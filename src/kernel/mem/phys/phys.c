//! @file phys.c
//! @brief Implementation of NUMA-aware physical memory allocation

#include <init/init.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>
#include <mem/mem.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <misc/misc.h>
#include <sys/acpi/numa.h>
#include <sys/numa/numa.h>

MODULE("mem/phys")
TARGET(mem_phys_available, mem_phys_init,
       {mem_add_numa_ranges_available, numa_available, mem_misc_collect_info_available,
        mem_phys_space_size_available})

//! @brief Pointer to array of mem_phys_object_data structures.
//! @note Used to store information about allocated physical objects
static struct mem_phys_object_data *mem_phys_objects_info = NULL;

//! @brief Safe info about newly allocated physical block
//! @param addr Physical address
//! @param size Size
//! @param range Weak ref to range
//! @param id NUMA node id
static void mem_phys_store_info(uintptr_t addr, size_t size, struct mem_range *range,
                                numa_id_t id) {
	// If object info pointer is NULL, do not store any info
	if (mem_phys_objects_info == NULL) {
		return;
	}
	// Get allocation object
	struct mem_phys_object_data *obj = mem_phys_objects_info + (addr / PAGE_SIZE);
	// Store reference to the memory pool in it
	obj->range = MEM_REF_BORROW(range);
	obj->size = size;
	obj->node_id = id;
}

//! @brief Allocate permanent physical memory in the specific NUMA node wihtout taking NUMA
//! lock
//! @param size Size of the memory area to be allocated
//! @param id Numa node in which memory should be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_alloc_specific_nolock(size_t size, numa_id_t id) {
	// Iterate rages belonging to the node
	const struct numa_node *node = numa_nodes + id;
	for (struct mem_range *range = node->ranges; range != NULL; range = range->next_range) {
		// Try to allocate from range slab allocator
		uintptr_t result = mem_phys_slab_alloc(&range->slab, size);
		ASSERT(result < mem_wb_phys_win_base, "Block in higher half");
		if (result != PHYS_NULL) {
			mem_phys_store_info(result, size, range, id);
			return result;
		}
	}
	return PHYS_NULL;
}

//! @brief Allocate permanent physical memory in the specific NUMA node
//! @param size Size of the memory area to be allocated
//! @param id Numa node in which memory should be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_alloc_specific(size_t size, numa_id_t id) {
	struct numa_node *node = numa_nodes + id;
	const bool int_state = thread_spinlock_lock(&node->lock);
	uintptr_t result = mem_phys_alloc_specific_nolock(size, id);
	thread_spinlock_unlock(&node->lock, int_state);
	return result;
}

//! @brief Allocate permanent physical memory on behalf of the given NUMA node
//! @param size Size of the memory area to be allocated
//! @param id Numa node on behalf of which memory will be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_alloc_on_behalf(size_t size, numa_id_t id) {
	// Get NUMA node data
	const struct numa_node *data = numa_nodes + id;
	// Iterate over all nodes
	for (size_t i = 0; i < numa_nodes_count; ++i) {
		const numa_id_t neighbour_id = data->neighbours[i];
		uintptr_t result = mem_phys_alloc_specific(size, neighbour_id);
		if (result != PHYS_NULL) {
			return result;
		}
	}
	return PHYS_NULL;
}

//! @brief Free permanent physical memory
//! @param addr Address returned from mem_phys_alloc_on_behalf
void mem_phys_free(uintptr_t addr) {
	// Get allocation data
	struct mem_phys_object_data *obj = mem_phys_objects_info + (addr / PAGE_SIZE);
	// Lock owning NUMA node
	struct numa_node *data = numa_nodes + obj->node_id;
	const bool int_state = thread_spinlock_lock(&data->lock);
	// Free memory back to the memory region
	mem_phys_slab_free(&obj->range->slab, addr, obj->size);
	// Unlock NUMA node
	thread_spinlock_unlock(&data->lock, int_state);
	// Drop reference to the memory region
	MEM_REF_DROP(obj->range);
}

//! @brief Get access to physical regions's data
//! @param addr Address of the physical
//! @return Pointer to physical region data
struct mem_phys_object_data *mem_phys_get_data(uintptr_t addr) {
	return mem_phys_objects_info + (addr / PAGE_SIZE);
}

//! @brief Initialize physical memory manager
static void mem_phys_init(void) {
	// Calculate space needed to store info about allocations
	const size_t gran_units_size = mem_phys_space_size / PAGE_SIZE;
	const size_t info_size = gran_units_size * sizeof(struct mem_phys_object_data);
	// Allocate space for info in boot domain
	const numa_id_t boot_domain = acpi_numa_boot_domain;
	uintptr_t info_phys = mem_phys_alloc_on_behalf(info_size, boot_domain);
	if (info_phys == PHYS_NULL) {
		PANIC("Failed to allocate space to store info about physical allocations");
	}
	// Convert to higher half
	mem_phys_objects_info = (struct mem_phys_object_data *)(mem_wb_phys_win_base + info_phys);
	LOG_INFO("mem_phys_objects_info at %p", mem_phys_objects_info);
}
