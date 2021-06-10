//! @file misc.h
//! @brief File for stuff that does not belong elsewhere

#pragma once

#include <misc/types.h>

//! @brief Get array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

//! @brief Align down
//! @param val Value to align down
//! @param align Alignment
//! @return Aligned value
static inline size_t align_down(size_t val, size_t align) {
	return (val / align) * align;
}

//! @brief Align up
//! @param val Value to align up
//! @param align Alignment
//! @return Aligned value
static inline size_t align_up(size_t val, size_t align) {
	return align_down(val + align - 1, align);
}
