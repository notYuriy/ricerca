//! @file module.h
//! @brief File containing functions for accessing boot modules

#pragma once

#include <misc/types.h>

//! @brief Kernel module
struct init_module {
	//! @brief Pointer to the module datat
	uintptr_t data;
	//! @brief Module size
	size_t size;
};

//! @brief Find module with a given cmdline
//! @param cmdline Module cmdline
//! @param buf Buffer to store struct init_module in
//! @return True if module with a given cmdline is present
bool init_module_lookup(const char *cmdline, struct init_module *buf);
