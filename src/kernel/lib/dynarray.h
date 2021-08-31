//! @file dynarray.h
//! @brief Implementation of functions on dynamic resizable arrays

#pragma once

#include <lib/string.h>
#include <mem/heap/heap.h>
#include <stddef.h>

//! @brief Dynarray growth delta
#define DYNARRAY_GROWTH_DELTA 16

//! @brief Dynamic resizable array
#define DYNARRAY(T) T *

//! @brief Dynamic array metadata
struct dynarray_meta {
	//! @brief Number of elements in dynarray
	size_t count;
	//! @brief Number of preallocated slots
	size_t capacity;
	//! @brief Pointer to the allocated slots
	char data[];
};

//! @brief Get pointer to metadata from pointer to the dynarray
//! @param ptr Pointer to the dynarray
inline static struct dynarray_meta *dynarray_to_meta(void *ptr) {
	return ((struct dynarray_meta *)ptr) - 1;
}

//! @brief Create a new dynarray
//! @return Pointer to the dynarray or NULL
inline static void *dynarray_new() {
	struct dynarray_meta *meta =
	    (struct dynarray_meta *)mem_heap_alloc(sizeof(struct dynarray_meta));
	if (meta == NULL) {
		return NULL;
	}
	meta->capacity = meta->count = 0;
	return (void *)(meta->data);
}

//! @brief Generic dynarray creation method
//! @param T Type of dynarray cell
#define DYNARRAY_NEW(T) ((T *)dynarray_new())

//! @brief Try to move dynarray to a new capacity
//! @param meta Pointer to the dynarray metadata
//! @param len Size of one dynarray cell
//! @param newcap New capacity
//! @return Pointer to the new dynarray
inline static struct dynarray_meta *dynarray_change_cap(struct dynarray_meta *meta, size_t len,
                                                        size_t newcap) {
	size_t total_old_size = sizeof(struct dynarray_meta) + meta->capacity * len;
	size_t total_new_size = sizeof(struct dynarray_meta) + newcap * len;
	struct dynarray_meta *new_meta =
	    (struct dynarray_meta *)mem_heap_realloc(meta, total_new_size, total_old_size);
	if (new_meta == NULL) {
		if (newcap <= meta->capacity) {
			return meta;
		}
		return NULL;
	}
	new_meta->capacity = newcap;
	return new_meta;
}

//! @brief Try to push a new element
//! @param dynarray Pointer to the dynarray
//! @param elem Pointer to the elem
//! @param len Size of one dynarray element
//! @return Pointer to the new dynarray or NULL if push failed
inline static void *dynarray_push(void *dynarray, void *elem, size_t len) {
	struct dynarray_meta *meta = dynarray_to_meta(dynarray);
	if (meta->count < meta->capacity) {
		memcpy(meta->data + meta->count * len, elem, len);
		meta->count++;
		return dynarray;
	} else {
		struct dynarray_meta *new_meta =
		    dynarray_change_cap(meta, len, meta->capacity + DYNARRAY_GROWTH_DELTA);
		if (new_meta == NULL) {
			return NULL;
		}
		memcpy(new_meta->data + new_meta->count * len, elem, len);
		new_meta->count++;
		return &new_meta->data;
	}
}

//! @brief Dynarray generic push method
//! @param dynarray Pointer to the dynarray
//! @param elem Element to push
//! @return Pointer to the new dynarray or null
#define DYNARRAY_PUSH(dynarray, elem)                                                              \
	({                                                                                             \
		__auto_type _12083047Buf = (elem);                                                         \
		dynarray_push(dynarray, &_12083047Buf, sizeof(_12083047Buf));                              \
	})

//! @brief Resize dynarray
//! @param dynarray Pointer to the dynarray
//! @param len Size of one dynarray element
//! @param newisze New dynarray size
inline static void *dynarray_resize(void *dynarray, size_t len, size_t newsize) {
	struct dynarray_meta *meta = dynarray_to_meta(dynarray);
	size_t newcap = align_up(newsize, DYNARRAY_GROWTH_DELTA);
	if (newcap == meta->capacity) {
		meta->count = newsize;
		return dynarray;
	}
	struct dynarray_meta *new_dynarray = dynarray_change_cap(meta, len, newcap);
	if (new_dynarray == NULL) {
		return NULL;
	}
	new_dynarray->count = newsize;
	return new_dynarray;
}

//! @brief Dynarray generic resize method
//! @param dynarray Pointer to the dynarray
//! @param size New size
//! @return Pointer to the new dynarray or null
#define DYNARRAY_RESIZE(dynarray, size) dynarray_resize(dynarray, size, sizeof(*dynarray))

//! @brief Destroy dynarray
//! @param dynarray Pointer to the dynarray
//! @param len Length of one element
inline static void dynarray_destroy(void *dynarray, size_t len) {
	struct dynarray_meta *meta = dynarray_to_meta(dynarray);
	mem_heap_free(meta, meta->capacity * len + sizeof(struct dynarray_meta));
}

//! @brief Generic dynarray destroy macro
//! @param dynarray Pointer to the dynarray
#define DYNARRAY_DESTROY(dynarray) dynarray_destroy(dynarray, sizeof(*dynarray))

//! @brief Get dynarray length
//! @param dynarray Pointer to the dynarray
inline static size_t dynarray_len(void *dynarray) {
	struct dynarray_meta *meta = dynarray_to_meta(dynarray);
	return meta->count;
}
