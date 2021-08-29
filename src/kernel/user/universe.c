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
size_t user_universe_last_id = 2;

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
int user_universe_move_in_pair(struct user_universe *universe, struct user_ref *refs,
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
bool user_universe_check_ref_nolock(struct user_universe *universe, size_t cell) {
	if (cell > dynarray_len(universe->cells) || !universe->cells[cell].in_use) {
		return false;
	}
	return true;
}

//! @brief Drop reference at index
//! @param universe Universe to drop reference from
//! @param cell Index of cell
//! @param cookie Pin cookie
//! @return API status
int user_universe_drop_cell(struct user_universe *universe, size_t cell, size_t cookie) {
	thread_mutex_lock(&universe->lock);
	if (!user_universe_check_ref_nolock(universe, cell)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	struct user_ref ref = universe->cells[cell].ref;
	if (!user_unpinned_for(ref, cookie)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_PIN_COOKIE_MISMATCH;
	}
	universe->cells[cell].in_use = false;
	LIST_APPEND_TAIL(&universe->free_list, &universe->cells[cell], node);
	thread_mutex_unlock(&universe->lock);
	user_drop_ref(ref);
	return USER_STATUS_SUCCESS;
}

//! @brief Borrow reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_universe_borrow_out(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (!user_universe_check_ref_nolock(universe, cell)) {
		thread_mutex_unlock(&universe->lock);
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
int user_universe_move_out(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (!user_universe_check_ref_nolock(universe, cell)) {
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
//! @param universe1 First universe to lock
//! @param universe2 Second universe to lock
static void user_universe_lock_pair(struct user_universe *universe1,
                                    struct user_universe *universe2) {
	// If universe1 and universe2 are the same, we only need to lock once
	if (universe1 == universe2) {
		thread_mutex_lock(&universe1->lock);
	}
	// Lock universes in order based on their id
	if (universe1->universe_id < universe2->universe_id) {
		thread_mutex_lock(&universe1->lock);
		thread_mutex_lock(&universe2->lock);
	} else {
		thread_mutex_lock(&universe2->lock);
		thread_mutex_lock(&universe1->lock);
	}
}

//! @brief Unlock universes pair
//! @param universe1 First universe to unlock
//! @param universe2 Second universe to unlock
static void user_universe_unlock_pair(struct user_universe *universe1,
                                      struct user_universe *universe2) {
	// If universes are the same, unlock once
	thread_mutex_unlock(&universe1->lock);
	if (universe1 != universe2) {
		thread_mutex_unlock(&universe2->lock);
	}
}

//! @brief Move reference from one universe to the other
//! @param src Source universe
//! @param dst Destination universe
//! @param hsrc Handle in source universe
//! @param hdst Buffer to store handle for destination universe
//! @param cookie Pin cookie
//! @return API status
int user_universe_move_across(struct user_universe *src, struct user_universe *dst, size_t hsrc,
                              size_t *hdst, size_t cookie) {
	user_universe_lock_pair(src, dst);
	int status = USER_STATUS_SUCCESS;
	if (!user_universe_check_ref_nolock(src, hsrc)) {
		status = USER_STATUS_INVALID_HANDLE;
		goto cleanup;
	}
	struct user_ref moved_ref = src->cells[hsrc].ref;
	if (!user_unpinned_for(moved_ref, cookie)) {
		status = USER_STATUS_PIN_COOKIE_MISMATCH;
		goto cleanup;
	}
	status = user_universe_move_in_nolock(dst, moved_ref, hdst);
	if (status != USER_STATUS_SUCCESS) {
		goto cleanup;
	}
	src->cells[hsrc].in_use = false;
	LIST_APPEND_TAIL(&src->free_list, &src->cells[hsrc], node);
cleanup:
	user_universe_unlock_pair(src, dst);
	return status;
}

//! @brief Borrow reference from one universe to the other
//! @param src Source universe
//! @param dst Destination universe
//! @param hsrc Handle in source universe
//! @param hdst Buffer to store handle for destination universe
//! @return API status
int user_universe_borrow_across(struct user_universe *src, struct user_universe *dst, size_t hsrc,
                                size_t *hdst, size_t cookie) {
	user_universe_lock_pair(src, dst);
	int status = USER_STATUS_SUCCESS;
	if (!user_universe_check_ref_nolock(src, hsrc)) {
		user_universe_unlock_pair(src, dst);
		return USER_STATUS_INVALID_HANDLE;
	}
	struct user_ref moved_ref = user_borrow_ref(src->cells[hsrc].ref);
	if (!user_unpinned_for(moved_ref, cookie)) {
		status = USER_STATUS_PIN_COOKIE_MISMATCH;
		goto drop_and_cleanup;
	}
	status = user_universe_move_in_nolock(dst, moved_ref, hdst);
drop_and_cleanup:
	user_universe_unlock_pair(src, dst);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(moved_ref);
	}
	return status;
}

//! @brief Unpin reference
//! @param universe Pointer to the universe
//! @param handle Handle to unpin
//! @param cookie Security cookie
//! @return API status
int user_universe_unpin(struct user_universe *universe, size_t handle, size_t cookie) {
	thread_mutex_lock(&universe->lock);
	if (!user_universe_check_ref_nolock(universe, handle)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	if (!user_unpinned_for(universe->cells[handle].ref, cookie)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_PIN_COOKIE_MISMATCH;
	}
	universe->cells[handle].ref.pin_cookie = USER_COOKIE_UNPIN;
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Pin reference
//! @param universe Pointer to the universe
//! @param handle Handle to unpin
//! @param cookie Pin cookie
//! @return API status
int user_universe_pin(struct user_universe *universe, size_t handle, size_t cookie) {
	thread_mutex_lock(&universe->lock);
	if (!user_universe_check_ref_nolock(universe, handle)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	if (!user_unpinned_for(universe->cells[handle].ref, cookie)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_PIN_COOKIE_MISMATCH;
	}
	universe->cells[handle].ref.pin_cookie = cookie;
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Fork universe
//! @param src Pointer to the universe
//! @param dst Buffer to store reference to the new universe in
//! @param cookie Pin cookie
//! @return API status
int user_universe_fork(struct user_universe *src, struct user_universe **dst, size_t cookie) {
	struct user_universe *forked = mem_heap_alloc(sizeof(struct user_universe));
	if (forked == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	DYNARRAY(struct user_universe_cell) cells = DYNARRAY_NEW(struct user_universe_cell);
	if (cells == NULL) {
		mem_heap_free(forked, sizeof(struct user_universe));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	forked->free_list = LIST_INIT;
	forked->lock = THREAD_MUTEX_INIT;
	forked->universe_id = ATOMIC_FETCH_INCREMENT(&user_universe_last_id);
	MEM_REF_INIT(forked, user_universe_destroy);

	thread_mutex_lock(&src->lock);
	size_t length = dynarray_len(src->cells);
	DYNARRAY(struct user_universe_cell)
	resized = DYNARRAY_RESIZE(forked->cells, dynarray_len(src->cells));
	if (resized == NULL) {
		thread_mutex_unlock(&src->lock);
		MEM_REF_DROP(forked);
		return USER_STATUS_OUT_OF_MEMORY;
	}
	forked->cells = resized;
	for (size_t i = 0; i < length; ++i) {
		if (src->cells[i].in_use && user_unpinned_for(src->cells[i].ref, cookie)) {
			forked->cells[i].ref = user_borrow_ref(src->cells[i].ref);
			forked->cells[i].in_use = true;
		} else {
			forked->cells[i].in_use = false;
			LIST_APPEND_TAIL(&forked->free_list, forked->cells + i, node);
		}
	}
	thread_mutex_unlock(&src->lock);
	*dst = forked;
	return USER_STATUS_SUCCESS;
}
