//! @brief misc.c
//! @file File containing basic memory-related features detection routine

#include <init/init.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuid.h>
#include <sys/cr.h>

MODULE("mem/misc")
TARGET(mem_misc_collect_info_available, mem_misc_collect_info, {});
TARGET(mem_phys_space_size_available, mem_calculate_phys_space_size, {acpi_available});

//! @brief Writeback physical window base
uintptr_t mem_wb_phys_win_base;

//! @brief True if 5 level paging is enabled
bool mem_5level_paging_enabled;

//! @brief True if 1 GB pages are supported
bool mem_1gb_pages_supported;

//! @brief Physical space size
size_t mem_phys_space_size;

//! @brief Collect memory-related info
void mem_misc_collect_info(void) {
	// Detect 1GB pages support
	struct cpuid buf;
	cpuid(0x80000001, 0, &buf);
	mem_1gb_pages_supported = (buf.edx & (1 << 26)) != 0;
	if (mem_1gb_pages_supported) {
		LOG_SUCCESS("Support for 1 GB pages detected");
	}
	// Get physical window base
	if ((rdcr4() & (1 << 12)) != 0) {
		// 5 level paging
		mem_wb_phys_win_base = 0xff00000000000000;
		LOG_SUCCESS("5 level paging support detected!");
		mem_5level_paging_enabled = true;
	} else {
		// 4 level paging
		mem_wb_phys_win_base = 0xffff800000000000;
		mem_5level_paging_enabled = false;
	}
	LOG_INFO("Physical base: 0x%p", mem_wb_phys_win_base);
}

//! @brief Calculate physical space size
void mem_calculate_phys_space_size(void) {
	// Query physical memory space size
	struct stivale2_struct_tag_memmap *memmap = init_memmap_tag;
	if (init_memmap_tag == NULL) {
		PANIC("No memory map tag");
	}
	mem_phys_space_size = acpi_query_phys_space_size();
	if (mem_phys_space_size == 0) {
		// SRAT is not given, calculate manually by enumerating memory map
		for (size_t i = 0; i < memmap->entries; ++i) {
			size_t end = memmap->memmap[i].base + memmap->memmap[i].length;
			if (end > mem_phys_space_size) {
				mem_phys_space_size = end;
			}
		}
	}
	LOG_INFO("Physical memory space size: 0x%p", mem_phys_space_size);
	// Align mem_phys_space_size
	mem_phys_space_size = align_up(mem_phys_space_size, PAGE_SIZE);
}
