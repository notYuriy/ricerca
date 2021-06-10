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
TARGET(mem_phys_target, mem_phys_init, mem_add_numa_ranges_target, numa_target)

//! @brief Pointer to array of mem_phys_object_data structures.
//! @note Used to store information about allocated physical objects
static struct mem_phys_object_data *mem_phys_objects_info;

//! @brief Allocate permanent physical memory on behalf of the given NUMA node without saving any
//! metadata
//! @param size Size of the memory area to be allocated
//! @param id Numa node on behalf of which memory will be allocated
//! @param buf Buffer to store reference to the borrowed memory pool in
//! @return Physical address of the allocated area, PHYS_NULL otherwise
static uintptr_t mem_phys_perm_alloc_on_behalf_nometa(size_t size, numa_id_t id,
                                                      struct mem_range **buf) {
	// Acquire NUMA subsystem lock
	const bool int_state = numa_acquire();
	// Get NUMA node data
	const struct numa_node *data = numa_query_data_no_borrow(id);
	// Iterate over all permanent neighbours
	for (size_t i = 0; i < data->permanent_used_entries; ++i) {
		const numa_id_t neighbour_id = data->permanent_neighbours[i];
		const struct numa_node *neighbour = numa_query_data_no_borrow(neighbour_id);
		// Iterate permanent memory ranges of the node
		for (struct mem_range *range = neighbour->permanent_ranges; range != NULL;
		     range = range->next_range) {
			// Try to allocate from range slub allocator
			uintptr_t result = mem_phys_slub_alloc(&range->slub, size);
			if (result != PHYS_NULL) {
				*buf = REF_BORROW(range);
				numa_release(int_state);
				return result;
			}
		}
	}
	numa_release(int_state);
	return PHYS_NULL;
}

//! @brief Allocate permanent physical memory on behalf of the given NUMA node
//! @param size Size of the memory area to be allocated
//! @param id Numa node on behalf of which memory will be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_perm_alloc_on_behalf(size_t size, numa_id_t id) {
	struct mem_range *buf;
	uintptr_t raw = mem_phys_perm_alloc_on_behalf_nometa(size, id, &buf);
	if (raw == PHYS_NULL) {
		return PHYS_NULL;
	}
	// Get allocation object
	struct mem_phys_object_data *obj = mem_phys_objects_info + (raw / PAGE_SIZE);
	// Store reference to the memory pool in it
	obj->range = buf;
	obj->size = size;
	return raw;
}

//! @brief Free permanent physical memory
//! @param addr Address returned from mem_phys_perm_alloc_on_behalf
void mem_phys_perm_free(uintptr_t addr) {
	// Acquire NUMA subsystem lock
	const bool int_state = numa_acquire();
	// Get allocation data
	struct mem_phys_object_data *obj = mem_phys_objects_info + (addr / PAGE_SIZE);
	// Free memory back to the memory region
	mem_phys_slub_free(&obj->range->slub, addr, obj->size);
	// Drop reference to the memory region
	REF_DROP(obj->range);
	numa_release(int_state);
}

//! @brief Initialize physical memory manager
//! @param memmap Memory map
static void mem_phys_init(void) {
	struct stivale2_struct_tag_memmap *memmap = init_memmap_tag;
	if (init_memmap_tag == NULL) {
		PANIC("No memory map tag");
	}
	// Query physical memory space size
	size_t phys_space_size = acpi_numa_query_phys_space_size();
	if (phys_space_size == 0) {
		// SRAT is not given, calculate manually by enumerating memory map
		for (size_t i = 0; i < memmap->entries; ++i) {
			size_t end = memmap->memmap[i].base + memmap->memmap[i].length;
			if (end > phys_space_size) {
				phys_space_size = end;
			}
		}
	}
	// Align phys_space_size
	phys_space_size = align_up(phys_space_size, PAGE_SIZE);
	// Calculate space needed to store info about allocations
	const size_t gran_units_size = phys_space_size / PAGE_SIZE;
	const size_t info_size = gran_units_size * sizeof(struct mem_phys_object_data);
	// Allocate space for info in boot domain
	const numa_id_t boot_domain = acpi_numa_boot_domain;
	struct mem_range *buf; // Unused
	uintptr_t info_phys = mem_phys_perm_alloc_on_behalf_nometa(info_size, boot_domain, &buf);
	if (info_phys == PHYS_NULL) {
		PANIC("Failed to allocate space to store info about physical allocations");
	}
	// Convert to higher half
	mem_phys_objects_info = (struct mem_phys_object_data *)(HIGH_PHYS_VMA + info_phys);
	LOG_INFO("mem_phys_objects_info at %p", mem_phys_objects_info);
}
