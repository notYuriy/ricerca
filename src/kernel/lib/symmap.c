//! @file symmap.c
//! @brief Implementation of symbol map functions

#include <init/module.h>
#include <lib/log.h>
#include <lib/symmap.h>
#include <misc/atomics.h>
#include <misc/symbol.h>

//! @brief Symmap cmdline
#define SYMMAP_CMDLINE "kernel-symbol-map"

//! @brief Symmap signature
#define SYMMAP_SIGN 0x1020304050607080

//! @brief Address of the symmap module
static uintptr_t symmap_module_addr = 0xffffffffffffffff;

//! @brief Start of .text section
extern symbol _kernel_text_start;

//! @brief End of .text section
extern symbol _kernel_text_end;

//! @brief Test if address belongs to section .text
//! @param addr Address to test
//! @return True if address is inside section text
static bool symmap_in_section_text(uintptr_t addr) {
	return addr >= (uintptr_t)_kernel_text_start && addr < (uintptr_t)_kernel_text_end;
}

//! @brief Find function to which address belongs using binary search
//! @param addr Address to search for
//! @param addrs Array of function starting addresses
//! @param cnt Functions count
//! @return 0 on failure, 1 + function index on success
static size_t symmap_find_by_addr(uintptr_t addr, uintptr_t *addrs, size_t cnt) {
	if (cnt == 0) {
		return 0;
	}
	size_t l = 0;
	size_t r = cnt;
	while (r - l > 1) {
		size_t m = (l + r) / 2;
		uintptr_t maddr = addrs[m];
		if (maddr > addr) {
			r = m;
		} else {
			l = m;
		}
	}
	if (addrs[l] > addr) {
		return 0;
	}
	return 1 + l;
}

//! @brief Query address info for a given address
//! @param addr Address to query info for
//! @param info Buffer to store info about the address in
//! @return True if symbol resolution was successful, false otherwise
bool symmap_query_addr_info(uintptr_t addr, struct symmap_addr_info *info) {
	// Linker map module is loaded lazily. If symmap_module_addr is -1, we havent attempted to load
	// it yet
	uintptr_t module_addr = ATOMIC_ACQUIRE_LOAD(&symmap_module_addr);
	if (module_addr == 0xffffffffffffffff) {
		struct init_module map;
		if (init_module_lookup(SYMMAP_CMDLINE, &map)) {
			module_addr = map.data;
			// Validate signature
			if (*((uint64_t *)module_addr) == SYMMAP_SIGN) {
				ATOMIC_RELEASE_STORE(&symmap_module_addr, map.data);
			} else {
				module_addr = 0;
				ATOMIC_RELEASE_STORE(&symmap_module_addr, 0);
			}
		} else {
			module_addr = 0;
			ATOMIC_RELEASE_STORE(&symmap_module_addr, 0);
		}
	}
	// If symmap module isn't present or if address is not in .text, return false
	if (module_addr == 0 || !symmap_in_section_text(addr)) {
		return false;
	}
	// Get function index
	size_t functions_count = ((uint64_t *)module_addr)[1];
	size_t fn_index = symmap_find_by_addr(addr, (uint64_t *)(module_addr + 16), functions_count);
	if (fn_index == 0) {
		return false;
	}
	fn_index--;
	uintptr_t fn_addr = ((uintptr_t *)(module_addr + 16))[fn_index];
	// Get pointer to the C-string
	uint32_t cstring_offset = ((uint32_t *)(module_addr + 16 + 8 * functions_count))[fn_index];
	const char *fn_name = (const char *)(module_addr + cstring_offset);
	info->name = fn_name;
	info->offset = addr - fn_addr;
	return true;
}
