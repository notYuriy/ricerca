//! @brief heap.c
//! @file File containing implementation of kernel heap allocator

#include <lib/panic.h>
#include <mem/heap/heap.h>
#include <mem/heap/slub.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <sys/numa/numa.h>

// Overview
// 1. Slubs are aligned 64k regions for objects that are smaller that one page size. Each slub has a
// special area reserved for slub header, that stores owner NUMA id
// 2. Since physical memory allocation subsystem does not guarantee alignment above 4k, and heap
// slubs need 64k alignment (to calculate slub header address), slubs are allocated in a big chunks
// (64 slubs in a chunk in the best case, 63 in the worst) and padding is then leaked.
// 3. Allocator only attempts to use slubs that belong to the host NUMA node. It won't use slubs of
// other proximity domains
// 4. If there is no good block in free list, allocator will ask PMM for a new chunk. However, new
// slubs from the chunk are added to the node which PMM picked for allocation, not to the node ID of
// which was passed to mem_heap_alloc call
// 5. For objects larger than 4k, allocator will directly call PMM to satisfy allocation request
// TODO: maybe its a good idea to reclaim memory for slubs? We have slub headers anyway

MODULE("mem/heap")
TARGET(mem_heap_target, META_DUMMY, {mem_phys_target})
META_DEFINE_DUMMY()

//! @brief Slub size
#define MEM_HEAP_SLUB_SIZE 65536

//! @brief Slub chunk size
#define MEM_HEAP_CHUNK_SIZE (64 * MEM_HEAP_SLUB_SIZE)

//! @brief Free object in the slub
struct mem_heap_obj {
	//! @brief Next free object
	struct mem_heap_obj *next;
};

//! @brief Slub header
struct mem_heap_slub_hdr {
	//! @brief NUMA domain of the owner
	numa_id_t owner;
	//! @brief Pointer to the next free slub
	struct mem_heap_slub_hdr *next_free;
};

//! @brief Allocate a new slubs chunk
//! @param node_id Node hint
//! @param node_ref Weak reference to the node in which chunk was actually allocated
//! @return False if allocation of a new chunk failed, true otherwise
//! @note NUMA lock should be acquired
static bool mem_allocate_new_slubs_chunk(numa_id_t id, struct numa_node **node_ref) {
	// Allocate backing memory
	uintptr_t backing_physmem = mem_phys_perm_alloc_on_behalf_nolock(MEM_HEAP_CHUNK_SIZE, id);
	if (backing_physmem == PHYS_NULL) {
		return false;
	}
	// Align begin and end to slub size
	uintptr_t backing_begin = align_up(backing_physmem, MEM_HEAP_SLUB_SIZE);
	uintptr_t backing_end = align_down(backing_physmem + MEM_HEAP_CHUNK_SIZE, MEM_HEAP_SLUB_SIZE);
	// Get NUMA node allocation was placed in
	struct mem_phys_object_data *data = mem_phys_get_data(backing_physmem);
	numa_id_t real_id = data->node_id;
	struct numa_node *node = numa_query_data_no_borrow(real_id);
	// Iterate over  new slubs in backing memory
	for (uintptr_t physaddr = backing_begin; physaddr < backing_end;
	     physaddr += MEM_HEAP_SLUB_SIZE) {
		// Get higher half pointer to the slub
		struct mem_heap_slub_hdr *new_slub =
		    (struct mem_heap_slub_hdr *)(mem_wb_phys_win_base + physaddr);
		// Add it to the list
		new_slub->next_free = node->slub_data.slubs;
		node->slub_data.slubs = new_slub;
	}
	// Store ref to the real owner
	*node_ref = node;
	return true;
}

//! @brief Create a new slub for a given order on the node
//! @param node Weak reference to the node
//! @param order Block size order
//! @note Requires empty slub list to be non-empty (lol)
static void mem_heap_add_slub(struct numa_node *node, size_t order) {
	ASSERT(node->slub_data.slubs != NULL, "Slub list should be non-empty");
	// Take one slub
	const struct mem_heap_slub_hdr *new_slub = node->slub_data.slubs;
	node->slub_data.slubs = new_slub->next_free;
	const uintptr_t start =
	    align_up((uintptr_t)new_slub + sizeof(struct mem_heap_slub_hdr), (1ULL << order));
	const uintptr_t end = (uintptr_t)new_slub + MEM_HEAP_SLUB_SIZE;
	for (uintptr_t addr = start; addr < end; addr += (1ULL << order)) {
		struct mem_heap_obj *obj = (struct mem_heap_obj *)addr;
		obj->next = node->slub_data.free_lists[order];
		node->slub_data.free_lists[order] = obj;
	}
}

//! @brief Calculate block size order
//! @param size Size of the memory block
//! @return Block size order if it is less than MEM_PHYS_SLUB_ORDERS_COUNT or
//! MEM_PHYS_SLUB_ORDERS_COUNT
static size_t mem_heap_get_size_order(size_t size) {
	if (size < 16) {
		size = 16;
	}
	size_t test = 16;
	size_t result = 4;
	while (size > test) {
		size *= 2;
		result += 1;
		if (result == MEM_HEAP_SLUB_ORDERS) {
			return result;
		}
	}
	return result;
}

//! @brief Allocate object of a given order from slub
//! @param node Pointer to owning NUMA node
//! @param order Order of the block to allocate
//! @return Pointer to allocated object
static struct mem_heap_obj *mem_allocate_from_slub(struct numa_node *node, size_t order) {
	ASSERT(node->slub_data.free_lists[order] != NULL, "Free-list is empty");
	struct mem_heap_obj *obj = node->slub_data.free_lists[order];
	node->slub_data.free_lists[order] = obj->next;
	return obj;
}

//! @brief Allocate memory on behalf of a given node
//! @param size Size of the memory to be allocated
//! @param id Locality to which memory will belong
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc(size_t size, numa_id_t id) {
	size_t order = mem_heap_get_size_order(size);
	if (order == MEM_PHYS_SLUB_ORDERS_COUNT) {
		// Allocate directly using PMM and cast to upper half
		uintptr_t res = mem_phys_perm_alloc_on_behalf(size, id);
		if (res == PHYS_NULL) {
			return NULL;
		}
		return (void *)(mem_wb_phys_win_base + res);
	}
	// Allocate using slubs. Its a bit more intricate here
	// 1. Acquire NUMA lock
	const bool int_state = numa_acquire();
	// 2. Get NUMA node data
	struct numa_node *data = numa_query_data_no_borrow(id);
	// 3. Check if corresponding free list has anything for us
	if (data->slub_data.free_lists[order] != NULL) {
		// Cool, let's give that as a result
		struct mem_heap_obj *obj = data->slub_data.free_lists[order];
		data->slub_data.free_lists[order] = obj->next;
		return (void *)obj;
	}
	// 4. Okey, time to make a new slub
	if (data->slub_data.slubs != NULL) {
		// There are some empty slubs, just use them
		mem_heap_add_slub(data, order);
		struct mem_heap_obj *obj = mem_allocate_from_slub(data, order);
		numa_release(int_state);
		return (void *)obj;
	}
	// 5. Alright, let's allocate a new chunk
	struct numa_node *real_owner;
	if (!mem_allocate_new_slubs_chunk(id, &real_owner)) {
		numa_release(int_state);
		return NULL;
	}
	mem_heap_add_slub(data, order);
	struct mem_heap_obj *obj = mem_allocate_from_slub(data, order);
	numa_release(int_state);
	return (void *)obj;
}

//! @brief Free memory on behalf of a given node
//! @param ptr Pointer to the previously allocated memory
//! @param size Size of the allocated memory
void mem_heap_free(void *mem, size_t size) {
	ASSERT(mem != NULL, "Attempt to free NULL");
	size_t order = mem_heap_get_size_order(size);
	if (order == MEM_PHYS_SLUB_ORDERS_COUNT) {
		mem_phys_perm_free((uintptr_t)mem - mem_wb_phys_win_base);
		return;
	}
	// 1. Acquire NUMA lock
	const bool int_state = numa_acquire();
	// 2. Get pointer to slub header
	uintptr_t slub_hdr_addr = align_down((uintptr_t)mem, MEM_HEAP_SLUB_SIZE);
	struct mem_heap_slub_hdr *hdr = (struct mem_heap_slub_hdr *)slub_hdr_addr;
	// 3. Get owning NUMA node data
	numa_id_t owner_id = hdr->owner;
	struct numa_node *data = numa_query_data_no_borrow(owner_id);
	// 4. Enqueue node
	struct mem_heap_obj *obj = (struct mem_heap_obj *)mem;
	obj->next = data->slub_data.free_lists[order];
	data->slub_data.free_lists[order] = obj;
	// 5. Free NUMA lock
	numa_release(int_state);
}
