//! @file usercopy.h
//! @brief File containing function for copying memory from userspace

#pragma once

#include <lib/string.h>
#include <misc/types.h>

//! @brief Copy memory from userspace
//! @param kernel_mem Pointer to the kernel memory
//! @param user_mem Pointer to the userspace memory
//! @param size Bytes to copy
//! @return True if copy has been successful
inline static bool mem_copy_from_user(void *kernel_mem, const void *user_mem, size_t size) {
	// TODO: Implement properly
	memcpy(kernel_mem, user_mem, size);
	return true;
}

//! @brief Copy memory to userspace
//! @param user_mem Pointer to the userspace memory
//! @param kernel_mem Pointer to the kernel memory
//! @param size Bytes to copy
//! @return True if copy has been successful
inline static bool mem_copy_to_user(void *user_mem, const void *kernel_mem, size_t size) {
	// TODO: implement properly
	memcpy(user_mem, kernel_mem, size);
	return true;
}
