//! @file mem.c
//! @brief Implementation of memory management initialization

#include <lib/log.h>            // Logging facilities
#include <mem/mem.h>            // For declaration of mem_init function
#include <mem/misc.h>           // For phys low
#include <mem/phys/slub.h>      // For physical memory slub
#include <mem/rc.h>             // For reference counting
#include <mem/rc_static_pool.h> // For static RC pool
#include <sys/acpi/numa.h>      // For memory areas iteration

#define MODULE "mem/init"

//! @brief Physical memory range
struct mem_range {
	//! @brief Refcounted base
	struct mem_rc rc_base;
	//! @brief Next memory range for this node
	struct mem_range *next_range;
	//! @brief Previous memory range for this ndoe
	struct mem_range *prev_range;
	//! @brief Physical memory slub
	struct mem_phys_slub slub;
	//! @brief True if memory range was offlined
	bool offlined;
};

//! @brief Backing array for static mem_range pool
static struct mem_range mem_range_backer[MEM_MAX_RANGES_STATIC];

//! @brief Static mem_range pool
static struct mem_rc_static_pool mem_ranges_pool =
    STATIC_POOL_INIT(struct mem_range, mem_range_backer);

//! @brief Initialize memory management
//! @param memmap Stivale2 memory map tag
void mem_init(struct stivale2_struct_tag_memmap *memmap) {
	LOG_INFO("Number of memory map entries: %U", memmap->entries);
	// Enumerate all usable memory map entries
	for (size_t i = 0; i < memmap->entries; ++i) {
		if (memmap->memmap[i].type != STIVALE2_MMAP_USABLE) {
			continue;
		}
		uintptr_t start = memmap->memmap[i].base;
		const uintptr_t end = start + memmap->memmap[i].length;
		// Ignore low memory
		if (end <= PHYS_LOW) {
			continue;
		}
		if (start < PHYS_LOW) {
			start = end;
		}
		struct acpi_numa_phys_range_iter iter = ACPI_NUMA_PHYS_RANGE_ITER_INIT(start, end);
		struct acpi_numa_memory_range buf;
		// Iterate all subregions of this memory range
		while (acpi_numa_get_memory_range(&iter, &buf)) {
			LOG_INFO("Usable memory range %p - %p belongs to domain %u", buf.start, buf.end,
			         (uint32_t)buf.node_id);
			// Allocate memory range object for this physical region
			struct mem_range *range = STATIC_POOL_ALLOC(&mem_ranges_pool, struct mem_range);
			if (range == NULL) {
				LOG_PANIC("Failed to statically allocate memory range object");
			}
			// Get corresponding NUMA node object
			struct numa_node *node = numa_query_data_no_borrow(buf.node_id);
			if (node == NULL) {
				LOG_PANIC("Unknown NUMA node");
			}
			// Initialize range info
			range->offlined = false;
			range->next_range = range->prev_range = NULL;
			mem_phys_slub_init(&range->slub, buf.start, buf.end - buf.start);
			// Add range to node's hotpluggable/permanent ranges list
			range->prev_range = NULL;
			if (buf.hotpluggable) {
				range->next_range = node->hotpluggable_ranges;
				node->hotpluggable_ranges->prev_range = range;
				node->hotpluggable_ranges = range;
			} else {
				range->next_range = node->permanent_ranges;
				node->permanent_ranges->prev_range = range;
				node->permanent_ranges = range;
			}
		}
	}
	LOG_SUCCESS("Initialization finished!");
}
