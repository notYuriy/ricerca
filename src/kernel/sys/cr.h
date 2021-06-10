//! @file cr.h
//! @brief File containing functions to read from/write to control registers

#pragma once

#include <misc/types.h> // For uint64_t

//! @brief Read CR0 value
//! @return CR0
inline static uint64_t rdcr0() {
	uint64_t val;
	asm volatile("mov %%cr0, %0" : "=r"(val));
	return val;
}

//! @brief Write to CR0
//! @param val New CR0 value
inline static void wrcr0(uint64_t val) {
	asm volatile("mov %0, %%cr0" ::"r"(val));
}

//! @brief Read CR2 value
//! @return CR2
inline static uint64_t rdcr2() {
	uint64_t val;
	asm volatile("mov %%cr2, %0" : "=r"(val));
	return val;
}

//! @brief Read CR3 value
//! @return CR3
inline static uint64_t rdcr3() {
	uint64_t val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

//! @brief Write to CR3
//! @param val New CR3 value
inline static void wrcr3(uint64_t val) {
	asm volatile("mov %0, %%cr3" ::"r"(val) : "memory");
}

//! @brief Read CR4 value
//! @return CR4
inline static uint64_t rdcr4() {
	uint64_t val;
	asm volatile("mov %%cr4, %0" : "=r"(val));
	return val;
}

//! @brief Write to CR4
//! @param val New CR4 valiue
inline static void wrcr4(uint64_t val) {
	asm volatile("mov %0, %%cr4" ::"r"(val));
}
