//! @file mem.c
//! @brief Implementation of memory management initialization

#include <init/init.h>
#include <lib/log.h>
#include <mem/heap/heap.h>
#include <mem/mem.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <mem/rc_static_pool.h>
#include <sys/acpi/numa.h>
#include <sys/numa/numa.h>

MODULE("mem");
TARGET(mem_add_numa_ranges_target, mem_add_numa_ranges, {numa_target, acpi_numa_target})
TARGET(mem_all_target, META_DUMMY, {mem_phys_target, mem_heap_target})
META_DEFINE_DUMMY()

//! @brief Backing array for static mem_range pool
static struct mem_range mem_range_backer[MEM_MAX_RANGES_STATIC];

//! @brief Static mem_range pool
static struct mem_rc_static_pool mem_ranges_pool =
    STATIC_POOL_INIT(struct mem_range, mem_range_backer);

//! @brief Initialize NUMA nodes with memory ranges
//! @param memmap Stivale2 memory map tag
static void mem_add_numa_ranges(void) {
	if (init_memmap_tag == NULL) {
		LOG_PANIC("No memory map");
	}
	struct stivale2_struct_tag_memmap *memmap = init_memmap_tag;
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
