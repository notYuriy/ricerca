//! @file intmap.h
//! @brief Implementation of map from integers to list nodes

#pragma once

#include <lib/list.h>
#include <mem/heap/heap.h>

//! @brief Map from integers to list nodes
struct intmap {
	//! @brief Number of buckets
	size_t buckets_count;
	//! @brief Buckets
	struct list *nodes;
};

//! @brief Int map node
struct intmap_node {
	//! @brief List node
	struct list_node node;
	//! @brief Key
	size_t key;
};

//! @brief Create new int map
//! @brief Intmap to initialize
//! @brief Buckets count
static inline bool intmap_init(struct intmap *intmap, size_t buckets) {
	struct list *nodes = (struct list *)mem_heap_alloc(buckets * sizeof(struct list));
	if (nodes == NULL) {
		return false;
	}
	for (size_t i = 0; i < buckets; ++i) {
		nodes[i] = LIST_INIT;
	}
	intmap->nodes = nodes;
	intmap->buckets_count = buckets;
	return true;
}

//! @brief Insert node into int map
//! @param intmap Pointer to the intmap
//! @param node Node to insert
static inline void intmap_insert(struct intmap *intmap, struct intmap_node *node) {
	size_t mod = node->key % intmap->buckets_count;
	LIST_APPEND_HEAD(intmap->nodes + mod, node, node);
}

//! @brief Find node in the int map
//! @param intmap Pointer to the intmap
//! @param key Key to search for
static inline struct intmap_node *intmap_search(struct intmap *intmap, size_t key) {
	size_t mod = key % intmap->buckets_count;
	struct list *list = intmap->nodes + mod;
	LIST_FOREACH(list, struct intmap_node, node, current) {
		if (current->key == key) {
			return current;
		}
	}
	return NULL;
}

//! @brief Remove node form the int map
//! @param intmap Pointer to the int map
//! @param node Pointer to the node
static inline void intmap_remove(struct intmap *intmap, struct intmap_node *node) {
	size_t mod = node->key % intmap->buckets_count;
	LIST_REMOVE(intmap->nodes + mod, node, node);
}

//! @brief Destroy intmap
//! @param intmap Pointer to the intmap
//! @note Does not deallocate nodes themselves!!!
static inline void intmap_destroy(struct intmap *intmap) {
	mem_heap_free(intmap->nodes, intmap->buckets_count * sizeof(struct list));
}
