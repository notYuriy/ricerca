//! @file tests.c
//! @brief File containing basic test driver

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("tests")

//! @brief Heap integrity test
void test_heap_integrity(void);

//! @brief Pairing heap test
void test_pairing_heap(void);

//! @brief Dynarray test
void test_dynarray(void);

//! @brief RPC test
void test_rpc(void);

//! @brief Universes test
void test_universe(void);

//! @brief Test unit
struct test_unit {
	//! @brief Test name
	const char *name;
	//! @brief Test function
	void (*callback)(void);
};

//! @brief Run test unit
//! @param unit Pointer to the test unit
static void tests_run_unit(struct test_unit *unit) {
	LOG_INFO("Running test \"%s\"...", unit->name);
	unit->callback();
	LOG_SUCCESS("Test \"%s\" finished without errors", unit->name);
}

//! @brief Test units
static struct test_unit units[] = {
    /*{.name = "Heap integrity test", .callback = test_heap_integrity},*/
    {.name = "Pairing heap test", .callback = test_pairing_heap},
    {.name = "Resizable arrays test", .callback = test_dynarray},
    {.name = "RPC test", .callback = test_rpc},
    {.name = "Universes test", .callback = test_universe}};

//! @brief Run tests
void tests_run(void) {
	LOG_INFO("Running kernel tests");
	for (size_t i = 0; i < ARRAY_SIZE(units); ++i) {
		tests_run_unit(units + i);
	}
	LOG_SUCCESS("Finished running kernel tests");
}
