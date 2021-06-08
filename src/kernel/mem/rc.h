//! @file rc.h
//! @brief File containing functions for manipulating refcounted objects

#pragma once

#include <misc/atomics.h> // For atomic operations
#include <misc/types.h>   // For size_t

struct mem_rc;

//! @brief Dispose callback type
typedef void (*mem_rc_dispose_t)(struct mem_rc *, void *);

//! @brief Header of a refcounted object
struct mem_rc {
	//! @brief Reference count
	size_t refcount;
	//! @brief Deallocation callback that is called whenever object refcount reaches 0
	mem_rc_dispose_t drop;
	//! @brief Opaque pointer passed to the callback
	void *opaque;
};

//! @brief Borrow refcounted object
//! @param obj Object to borrow
static inline struct mem_rc *mem_rc_borrow(struct mem_rc *obj) {
	// Increment reference count
	ATOMIC_FETCH_INCREMENT(&obj->refcount);
	return obj;
}

//! @brief Drop refcounted object
//! @param obj Object to drop
static inline void mem_rc_drop(struct mem_rc *obj) {
	// Decrement reference count
	const size_t refcount = ATOMIC_FETCH_DECREMENT(&obj->refcount);
	// If refcount is 1 (first fetch, then decrement, so if it became zero, we get 1), dispose
	// object. If callback is zero, data was allocated statically, so we can just silently drop it
	if (refcount == 1 && obj->drop != NULL) {
		obj->drop(obj, obj->opaque);
	}
}

//! @brief Initialize refcounted object
//! @param obj Object to initialize
//! @param callback Callback to be called on object disposal
static inline void mem_rc_init(struct mem_rc *obj, mem_rc_dispose_t callback, void *opaque) {
	obj->refcount = 1;
	obj->drop = callback;
	obj->opaque = opaque;
}

//! @brief Borrow refcounted reference to the object
//! @param x Reference to the object
//! @note Requires mem_rc to be right at the start of the pointed object
#define REF_BORROW(x) ((__typeof__(x))mem_rc_borrow((struct mem_rc *)x))

//! @brief Drop reference to the object
//! @param x Reference to the object
//! @note Requires mem_rc to be right at the start of the pointed object
#define REF_DROP(x) mem_rc_drop((struct mem_rc *)x)

//! @brief Initialize refcounted object
//! @param x Pointer to the object
//! @param callback Destructor callback. null if no deallocation is needed
//! @param opaque OPaque pointer passed to destructor
//! @note Requires mem_rc to be right at the start of the pointed object
#define REF_INIT(x, callback, opaque)                                                              \
	mem_rc_init((struct mem_rc *)x, (mem_rc_dispose_t)callback, opaque)
