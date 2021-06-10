//! @file log.c
//! @brief File containing definitions for formatting functions

#include <lib/fmt.h>
#include <lib/string.h>
#include <misc/types.h>

//! @brief Print 64 bit unsigned integer to the buffer
//! @param val Unsigned integer value to be printed
//! @param buf Buffer to store the result
//! @param size Size of the buffer
//! @param base Base from 2 to 16
//! @param width Width of the integer in the buffer. Padded with zeroes from
//! left if needed
static int fmt_putui(uint64_t val, char *buf, int size, uint8_t base, int width) {
	static const char *digits = "0123456789abcdef";
	// Number of bytes written to the buffer
	int bytes_written = 0;

	// Print int to buf in reverse
	// do while is used so that it prints one char '0' for zero
	do {
		// "val % buf" gets last digit of "val" in base "base"
		uint64_t mod = val % base;
		val = val / base;
		char c = digits[mod];
		// Add byte to the buffer if there is space left
		if (bytes_written < size) {
			buf[bytes_written] = c;
			bytes_written++;
		} else {
			break;
		}
	} while (val != 0);

	// Pad integer to width if we have space left
	// Padding is done by adding zeroes to the right
	// Third while(true) loop will reverse all bytes, and zeroes
	// will appear from the left
	while (bytes_written < width && bytes_written < size) {
		buf[bytes_written] = '0';
		bytes_written++;
	}

	// Reverse bytes in the buffer
	for (int i = 0; i < (bytes_written / 2); ++i) {
		char c = buf[i];
		buf[i] = buf[bytes_written - 1 - i];
		buf[bytes_written - 1 - i] = c;
	}
	return bytes_written;
}

//! @brief Print 64 bit signed integer to the buffer
//! @param val Unsigned integer value to be printed
//! @param buf Buffer to store the result
//! @param size Size of the buffer
//! @param base Base from 2 to 16
static int fmt_puti(int64_t val, char *buf, int size, uint8_t base) {
	// Calculate absolute value of the number
	uint64_t abs;
	bool negative = true;
	// -INT64_MIN won't fit in int64_t, so we handle this case separately
	if (val == INT64_MIN) {
		abs = ((uint64_t)(INT64_MAX)) + 1;
	} else if (val < 0) {
		abs = (uint64_t)(-val);
	} else {
		abs = (uint64_t)val;
		negative = false;
	}
	// Print sign if needed
	int bytes_written = 0;
	if (negative) {
		if (size == 0) {
			return 0;
		}
		buf[0] = '-';
		buf += 1;
		size -= 1;
		bytes_written++;
	}
	// Print integer
	bytes_written += fmt_putui(abs, buf, size, base, 0);
	return bytes_written;
}

//! @brief Helper macro to handle all unsigned int format specifiers in a
//! generic way
//! @param ch Format specifier (e.g. 'u')
//! @param va_type Type to be printed
//! @param base Base, in which integer should be printed
//! @param width Minimal width (integer representation is left-padded with
//! zeroes if not long enough)
#define UINT_FORMAT_CASE(ch, va_type, base, padding)                                               \
	case ch: {                                                                                     \
		uint64_t val = (uint64_t)va_arg(args, va_type);                                            \
		bytes_written +=                                                                           \
		    fmt_putui(val, buf + bytes_written, bytes_max - bytes_written, base, padding);         \
		break;                                                                                     \
	}

int vsnprintf(char *buf, size_t size, const char *format, va_list args) {
	// If buffer is empty, return
	if (size == 0) {
		return 0;
	}
	const int bytes_max = ((int)size) - 1;
	int bytes_written = 0;
	// Iterate over format string
	for (int i = 0; format[i] != '\0'; ++i) {
		// If there is no space left, break
		if (bytes_max == bytes_written) {
			break;
		}
		// If `buf[i]` is not a format specifier, print it directly
		if (format[i] != '%') {
			buf[bytes_written] = format[i];
			bytes_written++;
			continue;
		}
		// Skip to format specifier type
		char type = format[++i];
		switch (type) {
		// If "\0" is encountered, insert it and terminate
		case '\0':
			buf[bytes_written] = '\0';
			return bytes_written;
		// For %%, output %
		case '%':
			buf[bytes_written] = '%';
			bytes_written++;
			break;
		// For %c, cast vararg from int to char, and output character
		case 'c':
			buf[bytes_written] = (char)va_arg(args, int);
			bytes_written++;
			break;
		// For %s, calculate width with strlen and copy string
		case 's': {
			const char *str = va_arg(args, const char *);
			size_t length = strlen(str);
			size_t trimmed_length = (bytes_max < (int)length) ? bytes_max : (int)length;
			memcpy(buf + bytes_written, str, trimmed_length);
			bytes_written += trimmed_length;
			break;
		}
		// For %d, get 32 bit signed integer, cast to int64_t, and use fmt_puti
		case 'd': {
			int64_t val = (int64_t)va_arg(args, int32_t);
			bytes_written += fmt_puti(val, buf + bytes_written, bytes_max - bytes_written, 10);
			break;
		}
		// For %D, get 64 bit signed integer and use fmt_puti
		// clang-format off
		case 'D': {
			int64_t val = va_arg(args, int64_t);
			bytes_written += fmt_puti(val, buf + bytes_written,
									  bytes_max - bytes_written, 10);
			break;
		}
		// For %u, get 32 bit unsigned integer, cast to uint64_t, and print
		// in base 10
		UINT_FORMAT_CASE('u', uint32_t, 10, 0)
		// For %U, get 64 bit unsigned integer and use fmt_putui
		UINT_FORMAT_CASE('U', uint64_t, 10, 0)
		// For %x, get 32 bit unsigned integer, cast to uint64_t, and use
		// fmt_putui with base 16
		UINT_FORMAT_CASE('x', uint32_t, 16, 0)
		// For %X, get 64 bit unsigned integer and use fmt_putui with base
		// 16
		UINT_FORMAT_CASE('X', uint64_t, 16, 0)
		// For %p, get 64 bit unsigned integer and use fmt_putui with base
		// 16 and width 16
		UINT_FORMAT_CASE('p', uint64_t, 16, 16)
		}
		// clang-format on
	}
	buf[bytes_written] = '\0';
	return bytes_written;
}
