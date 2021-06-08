//! @file atomics.h
//! @brief Header file defining various atomic operations

#pragma once

//! @brief Atomic fetch and increment
//! @param ptr Pointer to the variable to be incremented
//! @note Uses acquire&release ordering
#define ATOMIC_FETCH_INCREMENT(ptr) __atomic_fetch_add(ptr, 1, __ATOMIC_ACQ_REL)

//! @brief Atomic fetch and decrement
//! @param ptr Pointer to the variable to be decremented
//! @note Uses acquire&release ordering
#define ATOMIC_FETCH_DECREMENT(ptr) __atomic_fetch_sub(ptr, 1, __ATOMIC_ACQ_REL)

//! @brief Atomic acquire load
//! @param ptr Pointer to the value to be loaded
#define ATOMIC_ACQUIRE_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
