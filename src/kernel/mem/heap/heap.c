//! @brief heap.c
//! @file File containing implementation of kernel heap allocator

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/heap/heap.h>
#include <mem/heap/slab.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <sys/numa/numa.h>
#include <thread/smp/core.h>

// Overview
// 1. Slabs are aligned 64k regions for objects that are smaller that one page size. Each slab has a
// special area reserved for slab header, that stores owner NUMA id
// 2. Since physical memory allocation subsystem does not guarantee alignment above 4k, and heap
// slabs need 64k alignment (to calculate slab header address), slabs are allocated in a big chunks
// (64 slabs in a chunk in the best case, 63 in the worst) and padding is then leaked.
// 3. For each neighbour proximity domain (including self :^) allocator will first try to allocate
// from slabs, and then it will try to allocate new chunk
// 4. If there is no good block in free list, allocator will ask PMM for a new chunk. However, new
// slabs from the chunk are added to the node which PMM picked for allocation, not to the node ID of
// which was passed to mem_heap_alloc call
// 5. For objects larger than 4k, allocator will directly call PMM to satisfy allocation request
// TODO: maybe its a good idea to reclaim memory for slabs? We have slab headers anyway

MODULE("mem/heap")
TARGET(mem_heap_available, META_DUMMY,
       {mem_phys_available, mem_misc_collect_info_available, thread_smp_core_available})
META_DEFINE_DUMMY()

//! @brief Slab size
#define MEM_HEAP_SLAB_SIZE 65536

//! @brief Slab chunk size
#define MEM_HEAP_CHUNK_SIZE (64 * MEM_HEAP_SLAB_SIZE)

//! @brief Free object in the slab
struct mem_heap_obj {
	//! @brief Next free object
	struct mem_heap_obj *next;
};

//! @brief Slab header
struct mem_heap_slab_hdr {
	//! @brief NUMA domain of the owner
	numa_id_t owner;
	//! @brief Pointer to the next free slab
	struct mem_heap_slab_hdr *next_free;
};

//! @brief Allocate a new slabs chunk
//! @param id ID of the NUMA node in which allocation should be placed
//! @note NUMA lock should be acquired
static bool mem_allocate_new_slabs_chunk(numa_id_t id) {
	struct numa_node *self = numa_nodes + id;
	// Allocate backing memory on the given node
	uintptr_t backing_physmem = mem_phys_alloc_specific_nolock(MEM_HEAP_CHUNK_SIZE, id);
	if (backing_physmem == PHYS_NULL) {
		return false;
	}
	// Align begin and end to slab size
	uintptr_t backing_begin = align_up(backing_physmem, MEM_HEAP_SLAB_SIZE);
	uintptr_t backing_end = align_down(backing_physmem + MEM_HEAP_CHUNK_SIZE, MEM_HEAP_SLAB_SIZE);
	// Iterate over new slabs in backing memory
	for (uintptr_t physaddr = backing_begin; physaddr < backing_end;
	     physaddr += MEM_HEAP_SLAB_SIZE) {
		// Get higher half pointer to the slab
		ASSERT(physaddr % MEM_HEAP_SLAB_SIZE == 0, "Slab is not aligned");
		struct mem_heap_slab_hdr *new_slab =
		    (struct mem_heap_slab_hdr *)(mem_wb_phys_win_base + physaddr);
		// Add it to the list
		new_slab->next_free = self->slab_data.slabs;
		self->slab_data.slabs = new_slab;
	}
	return true;
}

//! @brief Create a new slab for a given order on the node
//! @param id Node ID
//! @param order Block size order
//! @note Requires empty slab list to be non-empty (lol)
static void mem_heap_add_slab(numa_id_t id, size_t order) {
	struct numa_node *self = numa_nodes + id;
	ASSERT(self->slab_data.slabs != NULL, "Slab list should be non-empty");
	// Take one slab
	struct mem_heap_slab_hdr *new_slab = self->slab_data.slabs;
	self->slab_data.slabs = new_slab->next_free;
	new_slab->owner = id;
	const uintptr_t start =
	    align_up((uintptr_t)new_slab + sizeof(struct mem_heap_slab_hdr), (1ULL << order));
	const uintptr_t end = (uintptr_t)new_slab + MEM_HEAP_SLAB_SIZE;
	for (uintptr_t addr = start; addr < end; addr += (1ULL << order)) {
		ASSERT(align_down(addr, MEM_HEAP_SLAB_SIZE) == (uintptr_t)new_slab,
		       "Object at addr does not belong to slab");
		struct mem_heap_obj *obj = (struct mem_heap_obj *)addr;
		obj->next = self->slab_data.free_lists[order];
		self->slab_data.free_lists[order] = obj;
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
		test *= 2;
		result += 1;
		if (result == max_order) {
			return max_order;
		}
	}
	return result;
}

//! @brief Allocate object of a given order from slab
//! @param id ID of the NUMA node
//! @param order Order of the block to allocate
//! @return Pointer to allocated object
static struct mem_heap_obj *mem_allocate_from_slab(numa_id_t id, size_t order) {
	ASSERT(numa_nodes[id].slab_data.free_lists[order] != NULL, "Free-list is empty");
	struct mem_heap_obj *obj = numa_nodes[id].slab_data.free_lists[order];
	numa_nodes[id].slab_data.free_lists[order] = obj->next;
	return obj;
}

//! @brief Allocate memory
//! @param size Size of the memory to be allocated
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc(size_t size) {
	return mem_heap_alloc_on_behalf(size, PER_CPU(numa_id));
}

//! @brief Allocate memory
//! @param size Size of the memory to be allocated
//! @param id Locality to which memory will belong
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc_on_behalf(size_t size, numa_id_t id) {
	// Get size order
	size_t order = mem_heap_get_size_order(size, MEM_HEAP_SLAB_ORDERS);
	if (order == MEM_HEAP_SLAB_ORDERS) {
		// Allocate directly using PMM and cast to upper half
		uintptr_t res = mem_phys_alloc_on_behalf(size, id);
		if (res == PHYS_NULL) {
			return NULL;
		}
		return (void *)(mem_wb_phys_win_base + res);
	}
	// Allocate using slabs. Its a bit more intricate here
	// 1. Get NUMA node data
	struct numa_node *self = numa_nodes + id;
	// 2. Iterate over all nodes
	for (size_t i = 0; i < numa_nodes_count; ++i) {
		const numa_id_t neighbour_id = self->neighbours[i];
		struct numa_node *neighbour = numa_nodes + neighbour_id;
		// 3. Take node lock
		const bool int_state = thread_spinlock_lock(&neighbour->lock);
		// 3. Check if corresponding free list has anything for us
		if (neighbour->slab_data.free_lists[order] != NULL) {
			// Cool, let's give that as a result
			struct mem_heap_obj *obj = mem_allocate_from_slab(neighbour_id, order);
			thread_spinlock_unlock(&neighbour->lock, int_state);
			return (void *)obj;
		}
		// 4. Okey, time to make a new slab
		if (neighbour->slab_data.slabs != NULL) {
			// There are some empty slabs, just use them
			mem_heap_add_slab(neighbour_id, order);
			struct mem_heap_obj *obj = mem_allocate_from_slab(neighbour_id, order);
			thread_spinlock_unlock(&neighbour->lock, int_state);
			return (void *)obj;
		}
		// 5. Alright, let's allocate a new chunk
		if (!mem_allocate_new_slabs_chunk(neighbour_id)) {
			thread_spinlock_unlock(&neighbour->lock, int_state);
			continue;
		}
		mem_heap_add_slab(neighbour_id, order);
		struct mem_heap_obj *obj = mem_allocate_from_slab(neighbour_id, order);
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
	size_t order = mem_heap_get_size_order(size, MEM_HEAP_SLAB_ORDERS);
	if (order == MEM_HEAP_SLAB_ORDERS) {
		mem_phys_free((uintptr_t)mem - mem_wb_phys_win_base);
		return;
	}
	// 1. Get pointer to slab header
	uintptr_t slab_hdr_addr = align_down((uintptr_t)mem, MEM_HEAP_SLAB_SIZE);
	struct mem_heap_slab_hdr *hdr = (struct mem_heap_slab_hdr *)slab_hdr_addr;
	// 2. Get owning NUMA node data
	numa_id_t owner_id = hdr->owner;
	struct numa_node *data = numa_nodes + owner_id;
	// 3. Acquire node's lock
	const bool int_state = thread_spinlock_lock(&data->lock);
	// 4. Enqueue node
	struct mem_heap_obj *obj = (struct mem_heap_obj *)mem;
	obj->next = data->slab_data.free_lists[order];
	data->slab_data.free_lists[order] = obj;
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
