//! @file string.c
//! @brief File containing implementations of string functions

#include <lib/string.h>

//! @brief Calculate length of null-terminated string
//! @param str String, length of which should be calculated
size_t strlen(const char *str) {
	size_t result = 0;
	while (str[result] != '\0') {
		result++;
	}
	return result;
}

//! @brief Copy n bytes from src to dst
//! @param dest Pointer to destination buffer
//! @param src Pointer to data to be copied
//! @param n Number of bytes to be copied
//! @return Value of dest parameter
void *memcpy(void *dest, const void *src, size_t n) {
	uint8_t *cdest = dest;
	const uint8_t *csrc = src;
	for (size_t i = 0; i < n; ++i) {
		cdest[i] = csrc[i];
	}
	return dest;
}

//! @brief Fill area with bytes
//! @param dest Pointer to destination buffer
//! @param fill Byte to fill with
//! @param size Size of the destination buffer
void *memset(void *dest, int fill, size_t size) {
	uint8_t *cdest = dest;
	for (size_t i = 0; i < size; ++i) {
		cdest[i] = (uint8_t)fill;
	}
	return dest;
}

//! @brief Compare two memory areas for equality
//! @param ptr1 First memory area
//! @param ptr2 Second memory area
//! @param len Length of data to be compared
//! @return -1, if str1 > str2, 0 if str1 == str2, 1 if str2 > str1
int memcmp(const void *ptr1, const void *ptr2, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		if (((uint8_t *)ptr1)[i] > ((uint8_t *)ptr2)[i]) {
			return -1;
		} else if (((uint8_t *)ptr1)[i] < ((uint8_t *)ptr2)[i]) {
			return 1;
		}
	}
	return 0;
}
