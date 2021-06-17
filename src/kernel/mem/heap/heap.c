//! @brief heap.c
//! @file File containing implementation of kernel heap allocator

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/heap/heap.h>
#include <mem/heap/slub.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <sys/numa/numa.h>
#include <thread/smp/locals.h>

// Overview
// 1. Slubs are aligned 64k regions for objects that are smaller that one page size. Each slub has a
// special area reserved for slub header, that stores owner NUMA id
// 2. Since physical memory allocation subsystem does not guarantee alignment above 4k, and heap
// slubs need 64k alignment (to calculate slub header address), slubs are allocated in a big chunks
// (64 slubs in a chunk in the best case, 63 in the worst) and padding is then leaked.
// 3. For each neighbour proximity domain (including self :^) allocator will first try to allocate
// from slubs, and then it will try to allocate new chunk
// 4. If there is no good block in free list, allocator will ask PMM for a new chunk. However, new
// slubs from the chunk are added to the node which PMM picked for allocation, not to the node ID of
// which was passed to mem_heap_alloc call
// 5. For objects larger than 4k, allocator will directly call PMM to satisfy allocation request
// TODO: maybe its a good idea to reclaim memory for slubs? We have slub headers anyway

MODULE("mem/heap")
TARGET(mem_heap_available, META_DUMMY,
       {mem_phys_available, mem_misc_collect_info_available, thread_smp_locals_available})
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
//! @param owner NUMA node in which allocation should be placed
//! @note NUMA lock should be acquired
static bool mem_allocate_new_slubs_chunk(struct numa_node *owner) {
	// Allocate backing memory on the given node
	uintptr_t backing_physmem =
	    mem_phys_perm_alloc_specific_nolock(MEM_HEAP_CHUNK_SIZE, owner->node_id);
	if (backing_physmem == PHYS_NULL) {
		return false;
	}
	// Align begin and end to slub size
	uintptr_t backing_begin = align_up(backing_physmem, MEM_HEAP_SLUB_SIZE);
	uintptr_t backing_end = align_down(backing_physmem + MEM_HEAP_CHUNK_SIZE, MEM_HEAP_SLUB_SIZE);
	// Iterate over new slubs in backing memory
	for (uintptr_t physaddr = backing_begin; physaddr < backing_end;
	     physaddr += MEM_HEAP_SLUB_SIZE) {
		// Get higher half pointer to the slub
		struct mem_heap_slub_hdr *new_slub =
		    (struct mem_heap_slub_hdr *)(mem_wb_phys_win_base + physaddr);
		// Add it to the list
		new_slub->next_free = owner->slub_data.slubs;
		owner->slub_data.slubs = new_slub;
	}
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
//! @param max_order Biggest order
//! @return Block size order if it is less than max_order or
//! max_order
static size_t mem_heap_get_size_order(size_t size, size_t max_order) {
	if (size < 16) {
		size = 16;
	}
	size_t test = 16;
	size_t result = 4;
	while (size > test) {
		size *= 2;
		result += 1;
		if (result == max_order) {
			return max_order;
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

//! @brief Allocate memory
//! @param size Size of the memory to be allocated
//! @param id Locality to which memory will belong
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc(size_t size) {
	// Get NUMA id
	numa_id_t id = thread_smp_locals_get()->numa_id;
	// Get size order
	size_t order = mem_heap_get_size_order(size, MEM_HEAP_SLUB_ORDERS);
	if (order == MEM_HEAP_SLUB_ORDERS) {
		// Allocate directly using PMM and cast to upper half
		uintptr_t res = mem_phys_perm_alloc_on_behalf(size, id);
		if (res == PHYS_NULL) {
			return NULL;
		}
		return (void *)(mem_wb_phys_win_base + res);
	}
	// Allocate using slubs. Its a bit more intricate here
	// 1. Get NUMA node data
	struct numa_node *data = numa_query_data_no_borrow(id);
	// 2. Iterate ove all permanent neighbours
	for (size_t i = 0; i < data->permanent_used_entries; ++i) {
		const numa_id_t neighbour_id = data->permanent_neighbours[i];
		struct numa_node *neighbour = numa_query_data_no_borrow(neighbour_id);
		// 3. Take node lock
		const bool int_state = thread_spinlock_lock(&neighbour->lock);
		// 3. Check if corresponding free list has anything for us
		if (neighbour->slub_data.free_lists[order] != NULL) {
			// Cool, let's give that as a result
			struct mem_heap_obj *obj = neighbour->slub_data.free_lists[order];
			neighbour->slub_data.free_lists[order] = obj->next;
			thread_spinlock_unlock(&neighbour->lock, int_state);
			return (void *)obj;
		}
		// 4. Okey, time to make a new slub
		if (neighbour->slub_data.slubs != NULL) {
			// There are some empty slubs, just use them
			mem_heap_add_slub(neighbour, order);
			struct mem_heap_obj *obj = mem_allocate_from_slub(neighbour, order);
			thread_spinlock_unlock(&neighbour->lock, int_state);
			return (void *)obj;
		}
		// 5. Alright, let's allocate a new chunk
		if (!mem_allocate_new_slubs_chunk(neighbour)) {
			thread_spinlock_unlock(&neighbour->lock, int_state);
			continue;
		}
		mem_heap_add_slub(neighbour, order);
		struct mem_heap_obj *obj = mem_allocate_from_slub(neighbour, order);
		thread_spinlock_unlock(&neighbour->lock, int_state);
		return (void *)obj;
	}
	return NULL;
}

//! @brief Free memory on behalf of a given node
//! @param ptr Pointer to the previously allocated memory
//! @param size Size of the allocated memory
void mem_heap_free(void *mem, size_t size) {
	ASSERT(mem != NULL, "Attempt to free NULL");
	size_t order = mem_heap_get_size_order(size, MEM_HEAP_SLUB_ORDERS);
	if (order == MEM_HEAP_SLUB_ORDERS) {
		mem_phys_perm_free((uintptr_t)mem - mem_wb_phys_win_base);
		return;
	}
	// 1. Get pointer to slub header
	uintptr_t slub_hdr_addr = align_down((uintptr_t)mem, MEM_HEAP_SLUB_SIZE);
	struct mem_heap_slub_hdr *hdr = (struct mem_heap_slub_hdr *)slub_hdr_addr;
	// 2. Get owning NUMA node data
	numa_id_t owner_id = hdr->owner;
	struct numa_node *data = numa_query_data_no_borrow(owner_id);
	// 3. Acquire node's lock
	const bool int_state = thread_spinlock_lock(&data->lock);
	// 4. Enqueue node
	struct mem_heap_obj *obj = (struct mem_heap_obj *)mem;
	obj->next = data->slub_data.free_lists[order];
	data->slub_data.free_lists[order] = obj;
	// 5. Free NUMA lock
	thread_spinlock_unlock(&data->lock, int_state);
}

//! @brief Reallocate memory to a new region with new size
//! @param mem Pointer to the memory
//! @param newsize New size
//! @param oldsize Old size
//! @return Pointer to the new memory or NULL if realloc failed
void *mem_heap_realloc(void *mem, size_t newsize, size_t oldsize) {
	// If mem == NULL, use mem_heap_alloc
	if (mem == NULL) {
		return mem_heap_alloc(newsize);
	}
	// If newsize = 0, use mem_heap_free
	if (newsize == 0) {
		mem_heap_free(mem, oldsize);
	}
	// If orders match, we can just reuse mem. If not, reallocate to a new region
	// NOTE: We assume here that PMM also allocates orders of 2
	size_t oldorder = mem_heap_get_size_order(oldsize, 64);
	size_t neworder = mem_heap_get_size_order(newsize, 64);
	if (oldorder == neworder) {
		return mem;
	}
	size_t min = (oldsize < newsize) ? oldsize : newsize;
	// Allocate a new region
	void *result = mem_heap_alloc(newsize);
	if (result == NULL) {
		return NULL;
	}
	// Copy data from old region
	memcpy(result, mem, min);
	return result;
}
