//! @file symmap.h
//! @brief File containing symbol map function declarations

#pragma once

#include <misc/types.h>

//! @brief Address info
struct symmap_addr_info {
	//! @brief Name of the symbol that address belongs to
	const char *name;
	//! @brief Offset from the start
	size_t offset;
};

//! @brief Query address info for a given address
//! @param addr Address to query info for
//! @param info Buffer to store info about the address in
//! @return True if symbol resolution was successful, false otherwise
bool symmap_query_addr_info(uintptr_t addr, struct symmap_addr_info *info);
