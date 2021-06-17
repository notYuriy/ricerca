//! @file bootstrap.c
//! @brief Implementation of a simple bootstrap allocator

#include <init/init.h>
#include <lib/panic.h>
#include <mem/bootstrap.h>
#include <mem/misc.h>

MODULE("mem/bootstrap")
TARGET(mem_bootstrap_alloc_available, mem_bootstrap_alloc_init, {mem_misc_collect_info_available})

//! @brief Border of bootstrap allocated memory
//! @note Set to end of low physical memory so that low physmem remain unmodified
static uintptr_t mem_bootstrap_border_offset = PHYS_LOW;

//! @brief Current memory map entry
static size_t mem_bootstrap_memmap_index = 0;

//! @brief True if bootstrap allocator is usable
static bool mem_bootstrap_usable = false;

//! @brief Allocate memory on bootstrap
//! @param size Size of the allocation
void *mem_bootstrap_alloc(size_t size) {
	if (!mem_bootstrap_usable) {
		PANIC("Attempt to allocate bootstrap memory after bootstrap allocator was shut");
	}
	size_t real_size = align_up(size, 16);
	struct stivale2_struct_tag_memmap *memmap = init_memmap_tag;
	// Iterate over memory map until we find a usable entry
	while (mem_bootstrap_memmap_index < memmap->entries) {
		size_t index = mem_bootstrap_memmap_index;
		// If entry is not usable, we can ignore it
		if (memmap->memmap[index].type != STIVALE2_MMAP_USABLE) {
			goto next;
		}
		size_t entry_end = memmap->memmap[index].base + memmap->memmap[index].length;
		// If entry end is below mem_bootstrap_border_offset, entry can be ignored
		if (mem_bootstrap_border_offset >= entry_end) {
			goto next;
		}
		// IF border is below memmap->memmap[index].base, set it to memmap->memmap[index].base
		if (mem_bootstrap_border_offset < memmap->memmap[index].base) {
			mem_bootstrap_border_offset = align_up(memmap->memmap[index].base, 16);
		}
		// Allocate memory from this entry if we can
		if (entry_end - mem_bootstrap_border_offset >= real_size) {
			// Alright, allocate here
			void *result = (void *)(mem_wb_phys_win_base + mem_bootstrap_border_offset);
			mem_bootstrap_border_offset += real_size;
			return result;
		}
	next:
		mem_bootstrap_memmap_index++;
	}
	// No memory map entry to allocate from. At this point system will be unusable anyway, so we can
	// just panic
	PANIC("Failed to allocate %U bytes more", size);
}

//! @brief Return physical address beyond which no bootstrap allocations are placed
uintptr_t mem_bootstrap_terminate_allocator(void) {
	mem_bootstrap_usable = false;
	return align_up(mem_bootstrap_border_offset, PAGE_SIZE);
}

//! @brief Initialize bootstrap allocator
static void mem_bootstrap_alloc_init(void) {
	if (init_memmap_tag == NULL) {
		PANIC("No memory map!");
	}
	mem_bootstrap_usable = true;
}
