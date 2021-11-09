//! @file paging.h
//! @brief Declarations of paging functions

#pragma once

#include <mem/misc.h>
#include <misc/types.h>

//! @brief READ permissions
#define MEM_PAGING_READABLE 1

//! @brief WRITE permissions
#define MEM_PAGING_WRITABLE 2

//! @brief EXECUTABLE permissions
#define MEM_PAGING_EXECUTABLE 4

//! @brief USER permissions
#define MEM_PAGING_USER 8

//! @brief Paging hierarchy root. Reference-counted
struct mem_paging_root;

//! @brief Paging hierarchy mapper. Contains thread-local data required for lockless
//! mapping/unmapping.
struct mem_paging_mapper {
	//! @brief Physical address of beforehand zeroed page
	uintptr_t zeroed_pages[4];
};

//! @brief Create new paging root
//! @return Pointer to the new paging root or NULL on failure
struct mem_paging_root *mem_paging_new_root(void);

//! @brief Try to create a new paging mapper
//! @param mapper Pointer to the mapper
//! @return True on success and false on failure
bool mem_paging_init_mapper(struct mem_paging_mapper *mapper);

//! @brief Dispose paging mapper
//! @param mapper Pointer to the mapper
void mem_paging_deinit_mapper(struct mem_paging_mapper *mapper);

//! @brief Map 4k page at a given addresss
//! @param root Pointer to the paging root
//! @param vaddr Virtual address at which page should be mapped
//! @param paddr Physical address of the page to map
//! @param perms Permissions to map with
//! @return True on success, false on error
bool mem_paging_map_at(struct mem_paging_root *root, uintptr_t vaddr, uintptr_t paddr, int perms);

//! @brief Unmap 4k page at a given address
//! @param root Pointer to the paging root
//! @param vaddr Virtual address to be unmapped
//! @return Physical address that was unmapped or 0 if there was none
uintptr_t mem_paging_unmap_at(struct mem_paging_root *root, uintptr_t vaddr);

//! @brief Switch to a new paging hierarchy
//! @param root Pointer to the paging root
void mem_paging_switch_to(struct mem_paging_root *root);
