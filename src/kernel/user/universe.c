//! @file universe.c
//! @brief File containing implementaions of universe functions

#include <lib/dynarray.h>
#include <lib/list.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>
#include <mem/rc.h>
#include <thread/locking/mutex.h>
#include <user/universe.h>

MODULE("user/universe")

//! @brief Last universe identifier
size_t user_universe_last_id = 0;

//! @brief Universe cell
struct user_universe_cell {
	//! @brief Free list node
	struct list_node node;
	//! @brief Is in use?
	bool in_use;
	//! @brief Reference itself
	struct user_ref ref;
};

//! @brief Universe. Addressable collection of object references
struct user_universe {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Universe lock
	struct thread_mutex lock;
	//! @brief List of free cells
	struct list free_list;
	//! @brief Universe id
	size_t universe_id;
	//! @brief Dynarray with all cells
	DYNARRAY(struct user_universe_cell) cells;
};

//! @brief Destroy universe
//! @param universe Pointer to the universe
static void user_universe_destroy(struct user_universe *universe) {
	for (size_t i = 0; i < dynarray_len(universe->cells); ++i) {
		if (universe->cells[i].in_use) {
			user_drop_ref(universe->cells[i].ref);
		}
	}
	DYNARRAY_DESTROY(universe->cells);
	mem_heap_free(universe, sizeof(struct user_universe));
}

//! @brief Create new universe
//! @param universe Buffer to store reference to the new universe in
//! @return API status
int user_universe_create(struct user_universe **universe) {
	struct user_universe *res_universe = mem_heap_alloc(sizeof(struct user_universe));
	if (res_universe == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	DYNARRAY(struct user_universe_cell) cells = DYNARRAY_NEW(struct user_universe_cell);
	if (cells == NULL) {
		mem_heap_free(res_universe, sizeof(struct user_universe));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	MEM_REF_INIT(res_universe, user_universe_destroy);
	res_universe->cells = cells;
	res_universe->free_list = LIST_INIT;
	res_universe->lock = THREAD_MUTEX_INIT;
	res_universe->universe_id = ATOMIC_FETCH_INCREMENT(&user_universe_last_id);
	*universe = res_universe;
	return USER_STATUS_SUCCESS;
}

//! @brief Allocate cell without locking
//! @param universe Universe to allocate cell in
//! @param ref Reference to store
//! @param cell Buffer to store cell index in
//! @return API status
static int user_universe_move_in_nolock(struct user_universe *universe, struct user_ref ref,
                                        size_t *cell) {
	// Check for cyclical deps
	if (ref.type == USER_OBJ_TYPE_UNIVERSE && ref.universe->universe_id < universe->universe_id) {
		return USER_STATUS_INVALID_UNIVERSE_ORDER;
	}
	struct user_universe_cell *res_cell =
	    LIST_REMOVE_HEAD(&universe->free_list, struct user_universe_cell, node);
	if (res_cell != NULL) {
		ASSERT(!res_cell->in_use, "Free list has cell which is in use");
		res_cell->in_use = true;
		res_cell->ref = ref;
		*cell = res_cell - universe->cells;
		return USER_STATUS_SUCCESS;
	}
	size_t new_cell_index = dynarray_len(universe->cells);
	struct user_universe_cell new_cell;
	new_cell.in_use = true;
	new_cell.ref = ref;
	DYNARRAY(struct user_universe_cell) new_cells = DYNARRAY_PUSH(universe->cells, new_cell);
	if (new_cells == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	universe->cells = new_cells;
	*cell = new_cell_index;
	return USER_STATUS_SUCCESS;
}

//! @brief Allocate cell for new reference
//! @param universe Universe to allocate cell in
//! @param ref Reference to store
//! @param cell Buffer to store cell index in
//! @return API status
int user_universe_move_in(struct user_universe *universe, struct user_ref ref, size_t *cell) {
	thread_mutex_lock(&universe->lock);
	int status = user_universe_move_in_nolock(universe, ref, cell);
	thread_mutex_unlock(&universe->lock);
	return status;
}

//! @brief Allocate two cells for two references (neither of them can be universes)
//! @param universe Universe to allocate cells in
//! @param refs Array of two references to store
//! @param cells Array of two cell indices to store results in
//! @return API status
int user_universe_alloc_cell_pair(struct user_universe *universe, struct user_ref *refs,
                                  size_t *cells) {
	ASSERT(refs[0].type != USER_OBJ_TYPE_UNIVERSE, "Universe passed to user_allocate_ceil_pair");
	ASSERT(refs[1].type != USER_OBJ_TYPE_UNIVERSE, "Universe passed to user_allocate_ceil_pair");
	thread_mutex_lock(&universe->lock);
	const int status0 = user_universe_move_in_nolock(universe, refs[0], cells + 0);
	if (status0 != USER_STATUS_SUCCESS) {
		thread_mutex_unlock(&universe->lock);
		return status0;
	}
	const int status1 = user_universe_move_in_nolock(universe, refs[1], cells + 1);
	if (status1 != USER_STATUS_SUCCESS) {
		// Reclaim cell allocated for the first ref
		universe->cells[*cells].in_use = false;
		LIST_APPEND_TAIL(&universe->free_list, &universe->cells[*cells], node);
		thread_mutex_unlock(&universe->lock);
		return status1;
	}
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Check if there is a reference at given index
//! @param universe Universe
//! @param cell Cell index
//! @return True if reference at index is valid
bool user_check_ref_nolock(struct user_universe *universe, size_t cell) {
	if (cell > dynarray_len(universe->cells) || !universe->cells[cell].in_use) {
		thread_mutex_unlock(&universe->lock);
		return false;
	}
	return true;
}

//! @brief Drop reference at index
//! @param universe Universe to drop reference from
//! @param cell Index of cell
//! @return API status
void user_universe_drop_cell(struct user_universe *universe, size_t cell) {
	struct user_ref ref;
	int status = user_universe_move_ref(universe, cell, &ref);
	if (status == USER_STATUS_SUCCESS) {
		user_drop_ref(ref);
	}
}

//! @brief Borrow reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_universe_borrow_ref(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (!user_check_ref_nolock(universe, cell)) {
		return USER_STATUS_INVALID_HANDLE;
	}
	buf->type = universe->cells[cell].ref.type;
	buf->ref = MEM_REF_BORROW(universe->cells[cell].ref.ref);
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Move reference to the object at index
//! @param universe Universe to move reference from
//! @param cell Index of cell with the references
//! @param buf Buffer to store reference in
//! @return API status
int user_universe_move_ref(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (!user_check_ref_nolock(universe, cell)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	*buf = universe->cells[cell].ref;
	universe->cells[cell].in_use = false;
	LIST_APPEND_TAIL(&universe->free_list, &universe->cells[cell], node);
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Lock universes pair
static void user_universe_lock_pair(struct user_universe *universe1,
                                    struct user_universe *universe2) {
	// Lock universes in order based on their id
	if (universe1->universe_id < universe2->universe_id) {
		thread_mutex_lock(&universe1->lock);
		thread_mutex_lock(&universe2->lock);
	} else {
		thread_mutex_lock(&universe2->lock);
		thread_mutex_lock(&universe1->lock);
	}
}

//! @brief Move reference from one universe to the other
//! @param src Source universe
//! @param dst Destination universe
//! @param hsrc Handle in source universe
//! @param hdst Buffer to store handle in destination universe
int user_universe_move_across(struct user_universe *src, struct user_universe *dst, size_t hsrc,
                              size_t *hdst) {
	user_universe_lock_pair(src, dst);
	int status = USER_STATUS_SUCCESS;
	if (!user_check_ref_nolock(src, hsrc)) {
		status = USER_STATUS_INVALID_HANDLE;
		goto cleanup;
	}
	struct user_ref moved_ref = src->cells[hsrc].ref;
	status = user_universe_move_in_nolock(dst, moved_ref, hdst);
	if (status != USER_STATUS_SUCCESS) {
		goto cleanup;
	}
	src->cells[hsrc].in_use = false;
	LIST_APPEND_TAIL(&src->free_list, &src->cells[hsrc], node);
cleanup:
	thread_mutex_unlock(&src->lock);
	thread_mutex_unlock(&dst->lock);
	return status;
}

//! @brief Borrow reference from one universe to the other
//! @param src Source universe
//! @param dst Destination universe
//! @param hsrc Handle in source universe
//! @param hdst Buffer to store handle for destination universe
int user_universe_borrow_across(struct user_universe *src, struct user_universe *dst, size_t hsrc,
                                size_t *hdst) {
	user_universe_lock_pair(src, dst);
	int status = USER_STATUS_SUCCESS;
	if (!user_check_ref_nolock(src, hsrc)) {
		thread_mutex_unlock(&src->lock);
		thread_mutex_unlock(&dst->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	struct user_ref moved_ref = user_borrow_ref(moved_ref);
	status = user_universe_move_in_nolock(dst, moved_ref, hdst);
	if (status != USER_STATUS_SUCCESS) {
		thread_mutex_unlock(&src->lock);
		thread_mutex_unlock(&dst->lock);
		user_drop_ref(moved_ref);
		return status;
	}
	thread_mutex_unlock(&src->lock);
	thread_mutex_unlock(&dst->lock);
	return USER_STATUS_SUCCESS;
}
