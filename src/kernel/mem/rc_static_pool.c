//! @file static.c
//! @brief File containing implementations of static pool functions

#include <mem/rc_static_pool.h>

//! @brief Initialize static memory pool from a given memory range
//! @param pool Static memory pool to be initialized
//! @param object_size Size of the memory objects to be allocated
//! @param base Base of the virtual region allocated for the static memory pool
//! @param size Size of the virtual region allocated for the static memory pool
void mem_rc_static_pool_init(struct mem_rc_static_pool *pool, size_t object_size, uintptr_t base,
                             size_t size) {
	pool->brk_addr = base;
	pool->max_addr = base + size;
	pool->free_list = NULL;
	pool->size = object_size;
}

//! @brief Callback to deinit static objects by adding them to free list
static void mem_rc_static_pool_dispose(struct mem_rc *rc, struct mem_rc_static_pool *pool) {
	struct mem_rc_static_pool_free_node *node = (struct mem_rc_static_pool_free_node *)rc;
	node->next = pool->free_list;
	pool->free_list = node;
}

//! @brief Allocate memory object from static memory pool
//! @param pool Pool to allocate from
//! @return RC reference to the allocated object
struct mem_rc *mem_rc_static_pool_alloc(struct mem_rc_static_pool *pool) {
	if (pool->free_list != NULL) {
		struct mem_rc_static_pool_free_node *node = pool->free_list;
		pool->free_list = node->next;
		MEM_REF_INIT(node, mem_rc_static_pool_dispose, pool);
		return (struct mem_rc *)node;
	} else {
		if (pool->brk_addr == pool->max_addr) {
			return NULL;
		}
		struct mem_rc *res = (struct mem_rc *)pool->brk_addr;
		pool->brk_addr += pool->size;
		MEM_REF_INIT(res, mem_rc_static_pool_dispose, pool);
		return res;
	}
}
