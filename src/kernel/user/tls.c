//! @file tls.c
//! @brief File containing implementation of TLS API

#include <lib/intmap.h>
#include <lib/queue.h>
#include <mem/rc.h>
#include <user/tls.h>

//! @brief TLS intmap buckets count
#define USER_TLS_BUCKETS 16

//! @brief TLS table. Allows for thread-local key-value storage
struct user_tls_table {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Keys intmap
	struct intmap keys;
};

//! @brief TLS intmap node
struct user_tls_node {
	//! @brief Intmap node
	struct intmap_node node;
	//! @brief User-specified value
	size_t value;
};

//! @brief Destroy TLS table
//! @param table Pointer to the table
static void user_tls_table_destroy(struct user_tls_table *table) {
	for (size_t i = 0; i < USER_TLS_BUCKETS; ++i) {
		struct user_tls_node *node;
		while ((node = LIST_REMOVE_HEAD(table->keys.nodes + i, struct user_tls_node, node.node)) !=
		       NULL) {
			mem_heap_free(node, sizeof(struct user_tls_node));
		}
	}
	mem_heap_free(table, sizeof(struct user_tls_table));
}

//! @brief Create TLS table
//! @param table buffer to store pointer to the table in
//! @return API status
int user_tls_table_create(struct user_tls_table **table) {
	struct user_tls_table *res_table = mem_heap_alloc(sizeof(struct user_tls_table));
	if (res_table == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	if (!intmap_init(&res_table->keys, USER_TLS_BUCKETS)) {
		mem_heap_free(res_table, sizeof(struct user_tls_table));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	MEM_REF_INIT(res_table, user_tls_table_destroy);
	*table = res_table;
	return USER_STATUS_SUCCESS;
}

//! @brief Set TLS key
//! @param table Pointer to the table
//! @param key TLS key
//! @param value value
//! @return API status
int user_tls_table_set_key(struct user_tls_table *table, size_t key, size_t value) {
	struct intmap_node *node;
	if ((node = intmap_search(&table->keys, key)) != NULL) {
		struct user_tls_node *node = CONTAINER_OF(node, struct user_tls_node, node);
		node->value = value;
	} else {
		struct user_tls_node *node = mem_heap_alloc(sizeof(struct user_tls_node));
		if (node == NULL) {
			return USER_STATUS_OUT_OF_MEMORY;
		}
		node->value = value;
		node->node.key = key;
		intmap_insert(&table->keys, &node->node);
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Get TLS key
//! @param table Pointer to the table
//! @param key TLS key
//! @return Value at key or 0 if key is not present
size_t user_tls_table_get_key(struct user_tls_table *table, size_t key) {
	struct intmap_node *node;
	if ((node = intmap_search(&table->keys, key)) != NULL) {
		struct user_tls_node *node = CONTAINER_OF(node, struct user_tls_node, node);
		return node->value;
	}
	return 0;
}
