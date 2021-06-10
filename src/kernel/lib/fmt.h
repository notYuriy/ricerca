//! @file log.c
//! @brief File containing declarations for formatting functions

#pragma once

#include <misc/types.h>

//! @brief Format data using format string and put it in the buffer
//! @param buf Buffer to store formatted string in
//! @param size Size of the buffer to store formatted string in
//! @param format Format string used as a template for the output
//! @param args Arguments for the format string
//! @param int Number of bytes used in the buffer
int vsnprintf(char *buf, size_t size, const char *format, va_list args);
