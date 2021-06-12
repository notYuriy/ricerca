//! @file progress.h
//! @brief Functions to show progress bars in kernel logs

#pragma once

#include <lib/log.h>
#include <misc/types.h>

//! @brief Draw progress bar
//! @param val Current value
//! @param max Maximum value
//! @param size Progress-bar size
inline static void progress_bar(size_t val, size_t max, size_t size) {
	size_t new_parts = 0;
	if (val != 0) {
		new_parts = (val * size) / max;
		const size_t old_parts = ((val - 1) * size) / max;
		if (new_parts == old_parts) {
			return;
		}
		for (size_t i = 0; i < size + 2; ++i) {
			log_write("\b", 1);
		}
	}
	log_write("[\033[32m", 6);
	for (size_t i = 0; i < new_parts; ++i) {
		log_write("*", 1);
	}
	for (size_t i = new_parts; i < size; ++i) {
		log_write(" ", 1);
	}
	log_write("\033[0m]", 5);
}
