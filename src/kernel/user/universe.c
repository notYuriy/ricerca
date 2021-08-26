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
	//! @brief Dynarray with all cells
	DYNARRAY(struct user_universe_cell) cells;
};

//! @brief Destroy universe
//! @param universe Pointer to the universe
static void user_destroy_universe(struct user_universe *universe) {
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
int user_create_universe(struct user_universe **universe) {
	struct user_universe *res_universe = mem_heap_alloc(sizeof(struct user_universe));
	if (res_universe == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	DYNARRAY(struct user_universe_cell) cells = DYNARRAY_NEW(struct user_universe_cell);
	if (cells == NULL) {
		mem_heap_free(res_universe, sizeof(struct user_universe));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	MEM_REF_INIT(res_universe, user_destroy_universe);
	res_universe->cells = cells;
	res_universe->free_list = LIST_INIT;
	res_universe->lock = THREAD_MUTEX_INIT;
	*universe = res_universe;
	return USER_STATUS_SUCCESS;
}

//! @brief Allocate cell without locking
//! @param universe Universe to allocate cell in
//! @param ref Reference to store
//! @param cell Buffer to store cell index in
//! @return API status
static int user_allocate_cell_nolock(struct user_universe *universe, struct user_ref ref,
                                     size_t *cell) {
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
int user_allocate_cell(struct user_universe *universe, struct user_ref ref, size_t *cell) {
	thread_mutex_lock(&universe->lock);
	int status = user_allocate_cell_nolock(universe, ref, cell);
	thread_mutex_unlock(&universe->lock);
	return status;
}

//! @brief Allocate two cells for two references
//! @param universe Universe to allocate cells in
//! @param refs Array of two references to store
//! @param cells Array of two cell indices to store results in
//! @return API status
int user_allocate_cell_pair(struct user_universe *universe, struct user_ref *refs, size_t *cells) {
	thread_mutex_lock(&universe->lock);
	const int status0 = user_allocate_cell_nolock(universe, refs[0], cells + 0);
	if (status0 != USER_STATUS_SUCCESS) {
		thread_mutex_unlock(&universe->lock);
		return status0;
	}
	const int status1 = user_allocate_cell_nolock(universe, refs[1], cells + 1);
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

//! @brief Drop reference at index
//! @param universe Universe to drop reference from
//! @param cell Index of cell
//! @return API status
void user_drop_cell(struct user_universe *universe, size_t cell) {
	thread_mutex_lock(&universe->lock);
	if (cell > dynarray_len(universe->cells)) {
		thread_mutex_unlock(&universe->lock);
		return;
	}
	if (!universe->cells[cell].in_use) {
		thread_mutex_unlock(&universe->lock);
		return;
	}
	user_drop_ref(universe->cells[cell].ref);
	universe->cells[cell].in_use = false;
	LIST_APPEND_TAIL(&universe->free_list, &universe->cells[cell], node);
	thread_mutex_unlock(&universe->lock);
	return;
}

//! @brief Borrow reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_borrow_ref(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (cell > dynarray_len(universe->cells)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	if (!universe->cells[cell].in_use) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	buf->type = universe->cells[cell].ref.type;
	buf->ref = MEM_REF_BORROW(universe->cells[cell].ref.ref);
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Move out reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_move_out_ref(struct user_universe *universe, size_t cell, struct user_ref *buf) {
	thread_mutex_lock(&universe->lock);
	if (cell > dynarray_len(universe->cells)) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	if (!universe->cells[cell].in_use) {
		thread_mutex_unlock(&universe->lock);
		return USER_STATUS_INVALID_HANDLE;
	}
	// Move out reference
	*buf = universe->cells[cell].ref;
	universe->cells[cell].in_use = false;
	LIST_APPEND_TAIL(&universe->free_list, &universe->cells[cell], node);
	thread_mutex_unlock(&universe->lock);
	return USER_STATUS_SUCCESS;
}
