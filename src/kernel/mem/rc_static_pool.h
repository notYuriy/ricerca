//! @file static.h
//! @brief File containing declarations of static memory pool functions

#include <mem/rc.h>    // For reference-counted objects
#include <misc/misc.h> // For ARRAY_SIZE

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

//! @brief Declare static pool for the type
//! @param T Type to be stored in the pool
//! @param pool Pointer to the pool to be initialzied
//! @param array Array of T
//! @note Type should be refcounted
#define STATIC_POOL_INIT(T, array)                                                                 \
	{ (uintptr_t) array, ((uintptr_t)array + ARRAY_SIZE(array) * sizeof(T)), sizeof(T), NULL }

//! @brief Allocate type from the static pool configured for that type
//! @note Type should be refcounted
#define STATIC_POOL_ALLOC(pool, T) (T *)mem_rc_static_pool_alloc(pool)
