//! @file heap.c
//! @brief File containing integrity test for heap

#include <lib/panic.h>
#include <lib/progress.h>
#include <lib/string.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <misc/types.h>

MODULE("test/heap")

//! @brief Maximum number of kept-alive pointers
#define MAX_OBJ 256
//! @brief Number of iterations (allocation/free attempts)
#define ITERATIONS 1048576
//! @brief Progress bar size
#define PROGRESS_BAR_SIZE 50

//! @brief Assert that memory range is filled with a given value
static void test_heap_assert_filled(uint8_t *start, size_t size, uint8_t val) {
	for (size_t i = 0; i < size; ++i) {
		if (start[i] != val) {
			PANIC("Heap corruption detected");
		}
	}
}

//! @brief Heap integrity test
void test_heap_integrity(void) {
	size_t prng_current = 3847;
	uint8_t *pointers[MAX_OBJ] = {NULL};
	size_t sizes[MAX_OBJ] = {0};
	log_printf("Heap integrity test: ");
	for (size_t i = 0; i < ITERATIONS; ++i) {
		progress_bar(i, ITERATIONS, PROGRESS_BAR_SIZE);
		uint8_t index = (uint8_t)prng_current;
		prng_current = ((prng_current + 1) * 17 + 19) % MAX_OBJ;
		if (pointers[index] == NULL) {
			size_t size = prng_current * 32;
			prng_current = ((prng_current + 1) * 17 + 19) % MAX_OBJ;
			sizes[index] = size;
			pointers[index] = mem_heap_alloc(size);
			// LOG_INFO("alloc at %u: %U -> %p", (uint32_t)index, sizes[index], pointers[index]);
			if (pointers[index] == NULL) {
				PANIC("Out of Memory");
			}
			memset(pointers[index], index, size);
		} else {
			test_heap_assert_filled(pointers[index], sizes[index], index);
			mem_heap_free(pointers[index], sizes[index]);
			// LOG_INFO("free at %u: %p, %U", (uint32_t)index, pointers[index], sizes[index]);
			pointers[index] = NULL;
		}
	}
	for (size_t i = 0; i < MAX_OBJ; ++i) {
		if (pointers[i] != NULL) {
			test_heap_assert_filled(pointers[i], sizes[i], i);
			mem_heap_free(pointers[i], sizes[i]);
		}
	}
	progress_bar(ITERATIONS, ITERATIONS, PROGRESS_BAR_SIZE);
	log_printf("\n");
}
