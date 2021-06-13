//! @file phys.h
//! @brief Physical memory management

#include <init/stivale2.h>
#include <mem/phys/slub.h>
#include <misc/types.h>
#include <sys/numa/numa.h>

//! @brief Physical allocation data
struct mem_phys_object_data {
	//! @brief Memory range object was allocated in
	struct mem_range *range;
	//! @brief Size of the memory block
	size_t size;
	//! @brief Next hotpluggable block within the range
	struct mem_phys_object_data *next;
	//! @brief Prev hotpluggable object within the range
	struct mem_phys_object_data *prev;
	//! @brief Reference count (used for page tables)
	size_t rc;
	//! @brief NUMA id of the owner
	numa_id_t node_id;
};

//! @brief Allocate permanent physical memory in the specific NUMA node without taking node's lock
//! @param size Size of the memory area to be allocated
//! @param id Numa node in which memory should be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_perm_alloc_specific_nolock(size_t size, numa_id_t id);

//! @brief Allocate permanent physical memory in the specific NUMA node
//! @param size Size of the memory area to be allocated
//! @param id Numa node in which memory should be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_perm_alloc_specific(size_t size, numa_id_t id);

//! @brief Allocate permanent physical memory on behalf of the given NUMA node
//! @param size Size of the memory area to be allocated
//! @param id Numa node on behalf of which memory will be allocated
//! @return Physical address of the allocated area, PHYS_NULL otherwise
uintptr_t mem_phys_perm_alloc_on_behalf(size_t size, numa_id_t id);

//! @brief Free permanent physical memory
//! @param addr Address returned from mem_phys_perm_alloc_on_behalf
void mem_phys_perm_free(uintptr_t addr);

//! @brief Get access to physical regions's data
//! @param addr Address of the physical
//! @return Pointer to physical region data
struct mem_phys_object_data *mem_phys_get_data(uintptr_t addr);

//! @brief Export PMM initializaton target
EXPORT_TARGET(mem_phys_target)
