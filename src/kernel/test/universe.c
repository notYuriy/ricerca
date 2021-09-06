//! @file universe.c
//! @brief File containing basic test for universe user API functions

#include <lib/panic.h>
#include <lib/target.h>
#include <test/tests.h>
#include <user/entry.h>

MODULE("test/universe")

//! @brief Universes test
void test_universe(void) {
	// Initialize user API entry
	struct user_api_entry entry;
	if (user_api_entry_init(&entry) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize user API entry");
	}
	// Create universes
	size_t huniverse1, huniverse2;
	int status = user_sys_create_universe(&entry, &huniverse1);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to create 1st universe (status: %d)", status);
	}
	status = user_sys_create_universe(&entry, &huniverse2);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to create 2nd universe (status: %d)", status);
	}
	// Create mailbox handle to pass around
	size_t hmailbox;
	status = user_sys_create_mailbox(&entry, false, &hmailbox);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to create mailbox (status: %d)", status);
	}
	size_t inner1, inner2;
	status = user_sys_move_in(&entry, huniverse1, hmailbox, &inner1);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to move handle in the first universe (status: %d)", status);
	}
	// Check that hmailbox is no longer valid
	struct user_notification note_buf;
	status = user_sys_get_notification(&entry, hmailbox, &note_buf);
	if (status != USER_STATUS_INVALID_HANDLE) {
		PANIC("Handle accessible after user_sys_move_in (status: %d)", status);
	}
	// Move handle from the first universe to the second universe
	status = user_sys_move_across_universes(&entry, huniverse1, huniverse2, inner1, &inner2);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to move handle across universes (status: %d)", status);
	}
	// For second univserse
	size_t huniverse3;
	status = user_sys_fork_universe(&entry, huniverse2, &huniverse3);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to fork the universe (status: %d)", status);
	}
	// Drop 2nd version of the universe
	status = user_sys_drop(&entry, huniverse2);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to drop the 2nd universe (status: %d)", status);
	}
	// Move handle out of the 3rd universe
	status = user_sys_borrow_out(&entry, huniverse3, inner2, &hmailbox);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to move handle out of the second universe (status: %d)", status);
	}
	// Drop handle inside the 3rd universe
	status = user_sys_drop_in(&entry, huniverse3, inner2);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to drop handle in the 3rd universe (status: %d)", status);
	}
	// Drop mailbox handle
	status = user_sys_drop(&entry, hmailbox);
	if (status != USER_STATUS_SUCCESS) {
		PANIC("Failed to drop mailbox handle (status: %d)", status);
	}
	// Test universe ordering guarantees
	status = user_sys_move_in(&entry, huniverse3, huniverse1, &hmailbox);
	if (status != USER_STATUS_INVALID_UNIVERSE_ORDER) {
		PANIC("Universe order not enforced (status: %d)", status);
	}
	// Deinitialize user API entry
	user_api_entry_deinit(&entry);
}
