//! @file containerof.h
//! @brief File with helper to get pointer to the container structure

#pragma once

//! @brief Get pointer to the container structure
//! @param ptr Pointer to the member
//! @param P Parent type
//! @param name Member name
//! @return Pointer to the structure that contains member structure at the specified address
#define CONTAINER_OF(ptr, P, name) ((P *)(((uintptr_t)ptr) - offsetof(P, name)))

//! @brief Get pointer to the container structure for nullable ptr
//! @param ptr Pointer to the member
//! @param P Parent type
//! @param name Member name
//! @return Pointer to the structure that contains member structure at the specified address
#define CONTAINER_OF_NULLABLE(ptr, P, name)                                                        \
	({                                                                                             \
		__auto_type _ptr = (ptr);                                                                  \
		(_ptr == NULL) ? NULL : CONTAINER_OF(_ptr, P, name);                                       \
	})
