//! @file ports.h
//! @brief File containing x86 IO port functions

#pragma once

#include <misc/types.h>

//! @brief Output one byte to a given port
//! @param port Target port the byte is sent to
//! @param val Byte that is sent to the port
static inline void outb(uint16_t port, uint8_t val) {
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one byte from a given port
//! @param port Target port byte should be read from
//! @return Byte read from the port
static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

//! @brief Output one word (2 bytes) to a given port
//! @param port Target port the word (2 bytes) is sent to
//! @param val Word (2 bytes) that is sent to the port
static inline void outw(uint16_t port, uint16_t val) {
	asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one word (2 bytes) from a given port
//! @param port Target port word (2 bytes) should be read from
//! @return Word (2 bytes) read from the port
static inline uint16_t inw(uint16_t port) {
	uint16_t ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

//! @brief Output one dword (4 bytes) to a given port
//! @param port Target port the dword (4 bytes) is sent to
//! @param val Dword (4 bytes) that is sent to the port
static inline void outd(uint16_t port, uint32_t val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one dword (4 bytes) from a given port
//! @param port Target port dword (4 bytes) should be read from
//! @return Dword (4 bytes) read from the port
static inline uint32_t ind(uint16_t port) {
	uint32_t ret;
	asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
};
