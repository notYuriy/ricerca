//! @file pairing_heap.h
//! @brief File containing implementation of the pairing heap

#pragma once

#include <misc/attributes.h>
#include <misc/types.h>

//! @brief Pairing heap node
struct pairing_heap_node {
	//! @brief Next node in the list
	struct pairing_heap_node *next;
	//! @brief Child list
	struct pairing_heap_node *child;
};

//! @brief Nodes comparator
//! @param left LHS side of the comparison
//! @param right RHS side of the comparison
//! @return True if LHS is less than RHS
typedef bool (*pairing_heap_cmp_t)(struct pairing_heap_node *left, struct pairing_heap_node *right);

//! @brief Pairing heap
struct pairing_heap {
	//! @brief Comparison callback
	pairing_heap_cmp_t cmp;
	//! @brief Heap root
	struct pairing_heap_node *heap_root;
};

//! @brief Initialize pairing heap
//! @param heap Pointer to the heap to initailize
//! @param cmp Comparator to compare heap nodes
attribute_maybe_unused void pairing_heap_init(struct pairing_heap *heap, pairing_heap_cmp_t cmp) {
	heap->cmp = cmp;
	heap->heap_root = NULL;
}

//! @brief Meld pairing heaps
//! @param heap1 Root of the first heap
//! @param heap2 Root of the second heap
//! @param cmp Comparator used for balancing
//! @return Pointer to the root of the melded heap
inline static struct pairing_heap_node *_pairing_heap_meld(struct pairing_heap_node *heap1,
                                                           struct pairing_heap_node *heap2,
                                                           pairing_heap_cmp_t cmp) {
	// If one heap is empty, return another one
	if (heap1 == NULL) {
		return heap2;
	} else if (heap2 == NULL) {
		return heap1;
	}
	// Find minimum and maximum nodes in [node1; node2] pair
	struct pairing_heap_node *min = cmp(heap1, heap2) ? heap1 : heap2;
	struct pairing_heap_node *max = min == heap1 ? heap2 : heap1;
	max->next = min->child;
	min->child = max;
	return min;
}

//! @brief Create a tree from the list of child nodes on deletition
//! @param children Head of the list to convert to the tree
//! @param cmp Comparator used for balancing
//! @return Pointer to the new tree
inline static struct pairing_heap_node *_pairing_heap_treeify(struct pairing_heap_node *children,
                                                              pairing_heap_cmp_t cmp) {
	if (children == NULL) {
		return NULL;
	}
	struct pairing_heap_node *next = children->next;
	children->next = NULL;
	if (next == NULL) {
		return children;
	}
	struct pairing_heap_node *nn = next->next;
	next->next = NULL;
	return _pairing_heap_meld(_pairing_heap_meld(children, next, cmp),
	                          _pairing_heap_treeify(nn, cmp), cmp);
}

//! @brief Insert node in the pairing heap
//! @param heap Pointer to the heap
//! @param node Node to insert
attribute_maybe_unused static inline void pairing_heap_insert(struct pairing_heap *heap,
                                                              struct pairing_heap_node *node) {
	node->next = NULL;
	node->child = NULL;
	heap->heap_root = _pairing_heap_meld(heap->heap_root, node, heap->cmp);
}

//! @brief Remove minimum from the pairing heap
//! @param heap Pointer to the heap
//! @return Minimum node in the heap or NULL if heap is empty
attribute_maybe_unused static inline struct pairing_heap_node *pairing_heap_remove_min(
    struct pairing_heap *heap) {
	struct pairing_heap_node *res = heap->heap_root;
	if (res == NULL) {
		return NULL;
	}
	heap->heap_root = _pairing_heap_treeify(res->child, heap->cmp);
	return res;
}
