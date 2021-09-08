//! @file tls.c
//! @brief File containing basic TLS tests

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>
#include <user/entry.h>

MODULE("test/tls")

//! @brief Basic TLS (thread-local storage) subsystem
void test_tls(void) {
	// Initialize user API entry
	struct user_api_entry entry;
	if (user_api_entry_init(&entry) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize user API entry");
	}
	// Add a few TLS keys
	if (user_sys_set_tls_key(&entry, 0xcafebabedeadbeef, 0xdeadbeefcafebabe) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to add TLS key #1");
	}
	if (user_sys_set_tls_key(&entry, 0x00000000ebadf000, 0xaaaaaaaabbbbbbbb) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to add TLS key #2");
	}
	// Check that those TLS keys are accessible
	if (user_sys_get_tls_key(&entry, 0xcafebabedeadbeef) != 0xdeadbeefcafebabe) {
		PANIC("Incorrect value returned for key 0xcafebabedeadbeef");
	}
	if (user_sys_get_tls_key(&entry, 0x00000000ebadf000) != 0xaaaaaaaabbbbbbbb) {
		PANIC("Incorrect value returned for key 0x00000000ebadf000");
	}
	// user_sys_get_tls_key should return 0 for keys that dont exist
	if (user_sys_get_tls_key(&entry, 0xaaaabbbbccccdddd) != 0) {
		PANIC("Non-zero value returned for the key that does not exist");
	}
	user_api_entry_deinit(&entry);
}
