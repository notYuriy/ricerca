//! @file laihost.c
//! @brief Implementation of LAI host API

#include <lai/core.h>
#include <lai/host.h>
#include <lib/panic.h>
#include <mem/heap/heap.h>
#include <mem/mem.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/ports.h>

MODULE("sys/acpi/lai")
TARGET(lai_available, laihost_init,
       {
           acpi_available,
           mem_all_available,
       })

//! @brief Allocate memory
//! @param size Memory size
//! @return NULL if allocation failed, pointer to the allocated memory otherwise
void *laihost_malloc(size_t size) {
	return mem_heap_alloc(size);
}

//! @brief Reallocate memory
//! @param newsize New memory size
//! @param oldsize Old memory size
//! @return Pointer to reallocated memory, NULL if reallocation failed
void *laihost_realloc(void *mem, size_t newsize, size_t oldsize) {
	return mem_heap_realloc(mem, newsize, oldsize);
}

//! @brief Free memory
//! @param mem Memory area to be freeed
//! @param size Size of the memory area
void laihost_free(void *mem, size_t size) {
	mem_heap_free(mem, size);
}

//! @brief Find ACPI table
//! @param name Name of the ACPI table
//! @param index Index of the ACPI table
//! @return Pointer to the table found or NULL
void *laihost_scan(const char *name, size_t index) {
	return acpi_find_table(name, index);
}

//! @brief Map physical memory
//! @param offset Physical offset
//! @param size Size of the mapping
void *laihost_map(size_t offset, size_t size) {
	(void)size;
	return (void *)(mem_wb_phys_win_base + offset);
}

//! @brief Unmap physical memory mapping
//! @param mem Pointer to the mapping
//! @param size Size of the mapping
void laihost_unmap(void *mem, size_t size) {
	(void)mem;
	(void)size;
}

//! @brief Output one byte to a given port
//! @param port Target port the byte is sent to
//! @param val Byte that is sent to the port
void laihost_outb(uint16_t port, uint8_t val) {
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one byte from a given port
//! @param port Target port byte should be read from
//! @return Byte read from the port
uint8_t laihost_inb(uint16_t port) {
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

//! @brief Output one word (2 bytes) to a given port
//! @param port Target port the word (2 bytes) is sent to
//! @param val Word (2 bytes) that is sent to the port
void laihost_outw(uint16_t port, uint16_t val) {
	asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one word (2 bytes) from a given port
//! @param port Target port word (2 bytes) should be read from
//! @return Word (2 bytes) read from the port
uint16_t laihost_inw(uint16_t port) {
	uint16_t ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

//! @brief Output one dword (4 bytes) to a given port
//! @param port Target port the dword (4 bytes) is sent to
//! @param val Dword (4 bytes) that is sent to the port
void laihost_outd(uint16_t port, uint32_t val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

//! @brief Input one dword (4 bytes) from a given port
//! @param port Target port dword (4 bytes) should be read from
//! @return Dword (4 bytes) read from the port
uint32_t laihost_ind(uint16_t port) {
	uint32_t ret;
	asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
};

//! @brief lai log function
//! @param type Logging statement type
//! @param msg Message to be printed
void laihost_log(int type, const char *msg) {
	if (type == LAI_DEBUG_LOG) {
		LOG_INFO(msg);
	} else {
		LOG_WARN(msg);
	}
}

//! @brief lai panic function
//! @param msg Message to be printed
void laihost_panic(const char *msg) {
	PANIC(msg);
}

//! @brief Initialize LAI
static void laihost_init(void) {
	if (acpi_revision == 0) {
		return;
	}
	// LAI expects ACPI revision from RSDP, but we have already converted it to the sane number
	// Convert it back
	lai_set_acpi_revision(acpi_revision == 1 ? 0 : acpi_revision);
	lai_create_namespace();
}
