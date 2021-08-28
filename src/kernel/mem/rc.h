//! @file rc.h
//! @brief File containing functions for manipulating refcounted objects

#pragma once

#include <lib/log.h>
#include <lib/panic.h>
#include <misc/atomics.h>
#include <misc/types.h>

struct mem_rc;

//! @brief Dispose callback type
typedef void (*mem_rc_dispose_t)(struct mem_rc *);

//! @brief Header of a refcounted object
struct mem_rc {
	//! @brief Reference count
	size_t refcount;
	//! @brief Deallocation callback that is called whenever object refcount reaches 0
	mem_rc_dispose_t drop;
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
	// If refcount was 0, there is a UAF bug.
#ifdef DEBUG
	if (refcount == 0) {
		log_logf(LOG_TYPE_PANIC, "mem/rc", "rc == 0 in mem_rc_drop for %p", obj);
		hang();
	}
#endif
	// If refcount is 1 (first fetch, then decrement, so if it became zero, we get 1), dispose
	// object. If callback is zero, data was allocated statically, so we can just silently drop it
	if (refcount == 1 && obj->drop != NULL) {
		obj->drop(obj);
	}
}

//! @brief Initialize refcounted object
//! @param obj Object to initialize
//! @param callback Callback to be called on object disposal
static inline void mem_rc_init(struct mem_rc *obj, mem_rc_dispose_t callback) {
	obj->refcount = 1;
	obj->drop = callback;
}

//! @brief Borrow refcounted reference to the object
//! @param x Reference to the object
//! @note Requires mem_rc to be right at the start of the pointed object
#define MEM_REF_BORROW(x) ((__typeof__(x))mem_rc_borrow((struct mem_rc *)x))

//! @brief Drop reference to the object
//! @param x Reference to the object
//! @note Requires mem_rc to be right at the start of the pointed object
#define MEM_REF_DROP(x) mem_rc_drop((struct mem_rc *)x)

//! @brief Initialize refcounted object
//! @param x Pointer to the object
//! @param callback Destructor callback. null if no deallocation is needed
//! @param opaque OPaque pointer passed to destructor
//! @note Requires mem_rc to be right at the start of the pointed object
#define MEM_REF_INIT(x, callback) mem_rc_init((struct mem_rc *)x, (mem_rc_dispose_t)callback)
