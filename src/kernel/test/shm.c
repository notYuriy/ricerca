//! @file shm.c
//! @brief File containing basic SHM test

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <lib/target.h>
#include <user/entry.h>

MODULE("test/shm")

//! @brief Basic SHM test
void test_shm(void) {
	// Create user API entry
	struct user_api_entry entry;
	if (user_api_entry_init(&entry) != USER_STATUS_SUCCESS) {
		PANIC("Failed to create user API entry");
	}
	// Create SHM object
	size_t hshm, shm_id;
	if (user_sys_create_shm_owned(&entry, &hshm, &shm_id, 4096) != USER_STATUS_SUCCESS) {
		PANIC("Failed to create SHM object");
	}
	// Check that buffer is zeroed
	uint8_t buf[4096];
	if (user_sys_read_from_shm_id(&entry, shm_id, 0, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to read from SHM object");
	}
	for (size_t i = 0; i < 4096; ++i) {
		if (buf[i] != 0) {
			PANIC("Shm buffer is not zeroed");
		}
	}
	// Test OOB checks
	if (user_sys_read_from_shm_id(&entry, shm_id, 128, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_OUT_OF_BOUNDS) {
		PANIC("OOB checks do not work");
	}
	// Create readonly and read/write references
	size_t hshmro, hshmrw;
	if (user_sys_borrow_shm_ro(&entry, hshm, &hshmro) != USER_STATUS_SUCCESS) {
		PANIC("Failed to borrow read-only SHM reference");
	}
	if (user_sys_borrow_shm_rw(&entry, hshm, &hshmrw) != USER_STATUS_SUCCESS) {
		PANIC("Failed to borrow read-write SHM reference");
	}
	// Attempt to write data from RO reference
	if (user_sys_write_to_shm_ref(&entry, hshmro, 0, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_INVALID_HANDLE_TYPE) {
		PANIC("Checks for writes to RO refs do not work");
	}
	// Write some data
	memset(buf, 0xaa, 4096);
	if (user_sys_write_to_shm_ref(&entry, hshmrw, 0, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to write data to SHM object using read-write ref");
	}
	// Read using RO ref
	if (user_sys_read_from_shm_ref(&entry, hshmro, 0, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to read from SHM object");
	}
	for (size_t i = 0; i < 4096; ++i) {
		if (buf[i] != 0xaa) {
			PANIC("SHM buffer corruption");
		}
	}
	// Check that data could be read with RW ref as well
	if (user_sys_read_from_shm_ref(&entry, hshmrw, 0, 4096, (uintptr_t)&buf) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to read from SHM object");
	}
	for (size_t i = 0; i < 4096; ++i) {
		if (buf[i] != 0xaa) {
			PANIC("SHM buffer corruption");
		}
	}
	user_api_entry_deinit(&entry);
}
