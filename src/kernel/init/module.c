//! @file module.c
//! @brief File containing implementation for boot modules related functions

#include <init/init.h>
#include <init/module.h>
#include <lib/string.h>

//! @brief Find module with a given cmdline
//! @param cmdline Module cmdline
//! @param buf Buffer to store struct init_module in
//! @return True if module with a given cmdline is present
bool init_module_lookup(const char *cmdline, struct init_module *buf) {
	if (init_modules_tag == NULL) {
		return false;
	}
	size_t cmdline_len = strlen(cmdline);
	for (uint64_t i = 0; i < init_modules_tag->module_count; ++i) {
		if (memcmp(cmdline, init_modules_tag->modules[i].string,
		           128 < cmdline_len ? 128 : cmdline_len) == 0) {
			buf->data = init_modules_tag->modules[i].begin;
			buf->size = init_modules_tag->modules[i].end - init_modules_tag->modules[i].begin;
			return true;
		}
	}
	return false;
}
