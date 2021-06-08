//! @file handle_table.h
//! @brief Implementation of statically allocated handle table

#pragma once

#include <mem/rc.h>     // For mem_rc
#include <misc/misc.h>  // For ARRAY_SIZE
#include <misc/types.h> // For size_t

//! @brief Static handle table
struct static_handle_table {
	//! @brief Last allocated index
	size_t first_free_index;
	//! @brief Pointer to the handles
	struct mem_rc **handles;
	//! @brief Handles count
	size_t handles_count;
};

//! @brief Init value for handle table
//! @param handles struct mem_rc pointers array allocated for handles
//! @note Memory should be zeroed
#define STATIC_HANDLE_TABLE_INIT(handles)                                                          \
	{ 0, handles, ARRAY_SIZE(handles) }

//! @brief Static handle null. Returned when handle allocation has faied
#define STATIC_HANDLE_NULL 0

//! @brief Search for the first available slot to store ptr
//! @param table Pointer to the static_handle_table structure
//! @param handle Handle to be stored
//! @return 0 if allocation failed, index in table->handles + 1 if allocation succeeded
inline static size_t static_handle_table_alloc(struct static_handle_table *table,
                                               struct mem_rc *handle) {
	for (; table->first_free_index < table->handles_count; ++table->first_free_index) {
		if (table->handles[table->first_free_index] == NULL) {
			// Use slot
			table->handles[table->first_free_index] = handle;
			return table->first_free_index + 1;
		}
	}
	return STATIC_HANDLE_NULL;
}

//! @brief Free the handle
//! @param table Pointer to the static_handle_table structure
//! @param index Value returned from static_handle_table_alloc
inline static void static_handle_table_free(struct static_handle_table *table, size_t index) {
	size_t real_index = index - 1;
	// Update first_free_index field if needed
	if (table->first_free_index > real_index) {
		table->first_free_index = real_index;
	}
	// Dispose by setting cell to null
	table->handles[table->first_free_index] = NULL;
}

//! @brief Reserve a cell by a given index
//! @param table Static handle table
//! @param index Index in the table array
//! @param handle Handle to put in the reserved location
//! @return STATIC_HANDLE_NULL if reservation failed,
inline static size_t static_handle_table_reserve(struct static_handle_table *table, size_t index,
                                                 struct mem_rc *handle) {
	// Check for overflow
	if (table->handles_count <= index) {
		return STATIC_HANDLE_NULL;
	}
	// Check if place is available
	if (table->handles[index] != NULL) {
		return STATIC_HANDLE_NULL;
	}
	// Update first_free_index field if needed
	if (table->first_free_index == index) {
		table->first_free_index = index + 1;
	}
	// Store handle
	table->handles[index] = handle;
	return index + 1;
}
