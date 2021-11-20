//! @file paging.c
//! @brief Implementation of paging functions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <mem/rc.h>
#include <mem/virt/paging.h>
#include <sys/cr.h>
#include <thread/locking/spinlock.h>
#include <thread/smp/core.h>

MODULE("mem/virt/paging")

//! @brief Flags mask
#define FLAGS_MASK (0777ULL | (1ULL << 63ULL))

//! @brief Present flag
#define FLAG_PRESENT 1ULL

//! @brief Writable flag
#define FLAG_WRITABLE 2ULL

//! @brief User flag
#define FLAGS_USER 4ULL

//! @brief No-exec flag
#define FLAGS_NOEXEC (1ULL << 63ULL)

//! @brief Paging root
struct mem_paging_root {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief CR3 value
	uintptr_t cr3;
};

//! @brief Get index for a given page level
//! @param addr Virtual address
//! @param lvl Level
uint16_t mem_paging_get_lvl_index(uintptr_t addr, uint8_t lvl) {
	return (addr >> (uintptr_t)(9 * lvl + 3)) & 0777ULL;
}

//! @brief Allocate zeroed page
//! @return New zeroed page or PHYS_NULL on failure
static uintptr_t mem_paging_new_zeroed() {
	uintptr_t res = mem_phys_alloc_on_behalf(PAGE_SIZE, PER_CPU(numa_id));
	if (res == PHYS_NULL) {
		return PHYS_NULL;
	}
	uintptr_t virt = res + mem_wb_phys_win_base;
	memset((void *)virt, 0, PAGE_SIZE);
	return res;
}

//! @brief Dispose paging table at level
//! @param addr Table physical address
//! @param lvl Level
static void mem_paging_dispose_at_level(uintptr_t addr, uint8_t level) {
	if (level == 0) {
		mem_phys_free(addr);
	}
	uintptr_t *table = (uintptr_t *)(addr + mem_wb_phys_win_base);
	for (uint16_t i = 0; i < 512; ++i) {
		if (table[i] != 0) {
			mem_paging_dispose_at_level(table[i] & (~FLAGS_MASK), level - 1);
		}
	}
}

//! @brief Dispose paging root
//! @param root Pointer to the paging root
static void mem_paging_dispose_root(struct mem_paging_root *root) {
	uintptr_t *root_table = (uintptr_t *)(root->cr3 + mem_wb_phys_win_base);
	for (uint16_t i = 0; i < 256; ++i) {
		if (root_table[i] != 0) {
			mem_paging_dispose_at_level(root_table[i] & (~FLAGS_MASK),
			                            mem_5level_paging_enabled ? 4 : 3);
		}
	}
	mem_phys_free(root->cr3);
	mem_heap_free(root, sizeof(struct mem_paging_root));
}

//! @brief Create new paging root
//! @return Pointer to the new paging root or NULL on failure
struct mem_paging_root *mem_paging_new_root(void) {
	struct mem_paging_root *res = mem_heap_alloc(sizeof(struct mem_paging_root));
	res->lock = THREAD_SPINLOCK_INIT;
	if (res == NULL) {
		return NULL;
	}
	res->cr3 = mem_paging_new_zeroed();
	if (res->cr3 == PHYS_NULL) {
		mem_heap_free(res, sizeof(struct mem_paging_root));
		return NULL;
	}
	MEM_REF_INIT(res, mem_paging_dispose_root);
	// Copy all higher half entries
	uintptr_t current_cr3 = rdcr3();
	uintptr_t *current_root_table = (uintptr_t *)(current_cr3 + mem_wb_phys_win_base);
	uintptr_t *new_root_table = (uintptr_t *)(res->cr3 + mem_wb_phys_win_base);
	for (size_t i = 256; i < 512; ++i) {
		new_root_table[i] = current_root_table[i];
	}
	return res;
}

//! @brief Try to create a new paging mapper
//! @param mapper Pointer to the mapper
//! @return True on success and false on failure
bool mem_paging_init_mapper(struct mem_paging_mapper *mapper) {
	for (size_t i = 0; i < (mem_5level_paging_enabled ? 4 : 3); ++i) {
		mapper->zeroed_pages[i] = mem_paging_new_zeroed();
		if (mapper->zeroed_pages[i] == PHYS_NULL) {
			for (size_t j = 0; j < i; ++j) {
				mem_phys_free(mapper->zeroed_pages[j]);
			}
			return false;
		}
	}
	return true;
}

//! @brief Dispose paging mapper
//! @param mapper Pointer to the mapper
void mem_paging_deinit_mapper(struct mem_paging_mapper *mapper) {
	for (size_t i = 0; i < (mem_5level_paging_enabled ? 4 : 3); ++i) {
		if (mapper->zeroed_pages[i] != PHYS_NULL) {
			mem_phys_free(mapper->zeroed_pages[i]);
		}
	}
}

//! @brief Regenerate page cache
static bool mem_paging_regen_cache(struct mem_paging_mapper *mapper) {
	for (size_t i = 0; i < (mem_5level_paging_enabled ? 4 : 3); ++i) {
		if (mapper->zeroed_pages[i] == PHYS_NULL) {
			mapper->zeroed_pages[i] = mem_paging_new_zeroed();
			if (mapper->zeroed_pages[i] == PHYS_NULL) {
				return false;
			}
		}
	}
	return true;
}

//! @brief Map 4k page at a given addresss
//! @param root Pointer to the paging root
//! @param vaddr Virtual address at which page should be mapped
//! @param paddr Physical address of the page to map
//! @param perms Permissions to map with
//! @return True on success, false on error
bool mem_paging_map_at(struct mem_paging_root *root, uintptr_t vaddr, uintptr_t paddr, int perms) {
	ASSERT(vaddr < mem_wb_phys_win_base, "Address 0x%p is not in lower half", vaddr);
	ASSERT(vaddr % PAGE_SIZE == 0, "Address 0x%p is not page size aligned", vaddr);

	struct mem_paging_mapper *mapper = &thread_localsched_get_current_task()->mapper;
	if (!mem_paging_regen_cache(mapper)) {
		return false;
	}

	const bool int_state = thread_spinlock_lock(&root->lock);

	uintptr_t current_phys = root->cr3;
	uint8_t lvls = mem_5level_paging_enabled ? 5 : 4;

	// Map all intermidiate pages
	for (uint8_t i = lvls; i > 1; --i) {
		uintptr_t *table = (uintptr_t *)(mem_wb_phys_win_base + current_phys);
		uint16_t index = mem_paging_get_lvl_index(vaddr, i);
		const uintptr_t interm_perms = FLAG_PRESENT | FLAG_WRITABLE | FLAGS_USER;
		if (table[index] == 0) {
			table[index] = mapper->zeroed_pages[i - 2] | interm_perms;
			current_phys = mapper->zeroed_pages[i - 2];
			mapper->zeroed_pages[i - 2] = PHYS_NULL;
		} else {
			current_phys = table[index] & (~FLAGS_MASK);
		}
	}

	// Map last level page
	uintptr_t *table = (uintptr_t *)(mem_wb_phys_win_base + current_phys);

	uintptr_t perms_mask = FLAG_PRESENT;
	if ((perms & MEM_PAGING_WRITABLE) != 0) {
		perms_mask |= FLAG_WRITABLE;
	}
	if ((perms & MEM_PAGING_EXECUTABLE) == 0) {
		perms_mask |= FLAGS_NOEXEC;
	}
	if ((perms & MEM_PAGING_USER) != 0) {
		perms_mask |= FLAGS_USER;
	}

	table[mem_paging_get_lvl_index(vaddr, 1)] = paddr | perms_mask;

	thread_spinlock_unlock(&root->lock, int_state);

	return true;
}

//! @brief Unmap 4k page at a given address
//! @param root Pointer to the paging root
//! @param vaddr Virtual address to be unmapped
//! @return Physical address that was unmapped or 0 if there was none
uintptr_t mem_paging_unmap_at(struct mem_paging_root *root, uintptr_t vaddr) {
	ASSERT(vaddr < mem_wb_phys_win_base, "Address 0x%p is not in lower half", vaddr);
	ASSERT(vaddr % PAGE_SIZE == 0, "Address 0x%p is not page size aligned", vaddr);

	uintptr_t current_phys = root->cr3;
	uint8_t lvls = mem_5level_paging_enabled ? 5 : 4;

	const bool int_state = thread_spinlock_lock(&root->lock);

	// Walk intermidiate pages
	for (uint8_t i = lvls; i > 1; --i) {
		uintptr_t *table = (uintptr_t *)(mem_wb_phys_win_base + current_phys);
		uintptr_t next = table[mem_paging_get_lvl_index(vaddr, i)] & (~FLAGS_MASK);
		current_phys = next;
	}

	uintptr_t *table = (uintptr_t *)(mem_wb_phys_win_base + current_phys);
	uintptr_t addr = table[mem_paging_get_lvl_index(vaddr, 1)] & (~FLAGS_MASK);
	table[mem_paging_get_lvl_index(vaddr, 1)] = 0;

	thread_spinlock_unlock(&root->lock, int_state);

	return addr;
}

//! @brief Switch to a new paging hierarchy
//! @param root Pointer to the paging root
void mem_paging_switch_to(struct mem_paging_root *root) {
	wrcr3(root->cr3);
}
