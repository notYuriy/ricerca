//! @file static.h
//! @brief File containing declarations of static memory pool functions

#include <mem/rc.h>
#include <misc/misc.h>

//! @brief Static RC pool free list node
struct mem_rc_static_pool_free_node {
	//! @brief Next pointer
	struct mem_rc_static_pool_free_node *next;
};

//! @brief Static RC objects pool
struct mem_rc_static_pool {
	//! @brief Current brk virtual address
	uintptr_t brk_addr;
	//! @brief Max brk address
	uintptr_t max_addr;
	//! @brief Block size
	size_t size;
	//! @brief Free list
	struct mem_rc_static_pool_free_node *free_list;
};
//! @brief Allocate memory object from static memory pool
//! @param pool Pool to allocate from
//! @return RC reference to the allocated object
struct mem_rc *mem_rc_static_pool_alloc(struct mem_rc_static_pool *pool);

//! @brief Initialize static RC pool from a pointer to the backing array
//! @param T Type to be stored in the pool
//! @param pointer Pointer to the backing array
//! @param length Length of the backing array (in sizeof(T)s)
#define MEM_RC_STATIC_POOL_POINTER_INIT(T, pointer, length)                                        \
	(struct mem_rc_static_pool) {                                                                  \
		(uintptr_t) pointer, ((uintptr_t)pointer + length * sizeof(T)), sizeof(T), NULL            \
	}

//! @brief Declare static pool for the type
//! @param T Type to be stored in the pool
//! @param array Array of T
//! @note Type should be refcounted
#define MEM_RC_STATIC_POOL_INIT(T, array)                                                          \
	MEM_RC_STATIC_POOL_POINTER_INIT(T, array, ARRAY_SIZE(array))

//! @brief Allocate type from the static pool configured for that type
//! @note Type should be refcounted
#define MEM_RC_STATIC_POOL_ALLOC(pool, T) (T *)mem_rc_static_pool_alloc(pool)
