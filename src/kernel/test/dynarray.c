//! @file dynarray.c
//! @brief File containing basic dynarray test

#include <lib/dynarray.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("test/dynarray")

//! @brief Test dynarrays
void test_dynarray(void) {
	DYNARRAY(int) dyn = DYNARRAY_NEW(int);
	for (int i = 0; i < 128; ++i) {
		dyn = DYNARRAY_PUSH(dyn, i);
		if (dyn == NULL) {
			PANIC("OOM");
		}
	}
	ASSERT(dynarray_len(dyn) == 128, "dynarray_len function gives incorrect results");
	for (int i = 0; i < 128; ++i) {
		if (dyn[i] != i) {
			PANIC("dynarray corruption");
		}
	}
	DYNARRAY_DESTROY(dyn);
}
