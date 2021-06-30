//! @file pairing_heap.c
//! @brief Tests for the pairing heap

#include <lib/containerof.h>
#include <lib/log.h>
#include <lib/pairing_heap.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("test/pairing_heap")

//! @brief Heap node with an integer key
struct int_node {
	struct pairing_heap_hook node;
	int key;
};

//! @brief Comparator for heap nodes
static bool pairing_heap_compare(struct pairing_heap_hook *left, struct pairing_heap_hook *right) {
	return CONTAINER_OF(left, struct int_node, node)->key <
	       CONTAINER_OF(right, struct int_node, node)->key;
}

//! @brief Pairing heap test
void test_pairing_heap(void) {
	struct pairing_heap heap;
	pairing_heap_init(&heap, pairing_heap_compare);
	struct int_node nodes[128];
	for (int ii = 128; ii > 0; ii -= 2) {
		int i = ii - 2;
		nodes[i].key = i;
		pairing_heap_insert(&heap, (struct pairing_heap_hook *)(nodes + i));
	}
	for (int ii = 129; ii > 1; ii -= 2) {
		int i = ii - 2;
		nodes[i].key = i;
		pairing_heap_insert(&heap, (struct pairing_heap_hook *)(nodes + i));
	}
	LOG_INFO("Insertions done");
	for (int i = 0; i < 128; ++i) {
		struct int_node *node = (struct int_node *)(pairing_heap_remove_min(&heap));
		if (node == NULL) {
			PANIC("Failed to dequeue node from the heap");
		}
		if (node->key != i) {
			PANIC("Incorrect minimum key (expected: %d found: %u)", i, node->key);
		}
	}
}
