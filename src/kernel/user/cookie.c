//! @file cookie.c
//! @brief File containing implementations of cookie functions

#include <lib/dynarray.h>
#include <mem/heap/heap.h>
#include <mem/rc.h>
#include <thread/locking/mutex.h>
#include <user/cookie.h>

//! @brief Last allocated key
static size_t user_cookie_last = 2;

//! @brief Group cookie object
struct user_group_cookie {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Cookie key
	user_cookie_key_t key;
};

//! @brief API entry cookie. Used by sys_* functions to check perms
struct user_entry_cookie {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Default key
	user_cookie_key_t key;
	//! @brief Entry locks
	struct thread_mutex lock;
	//! @brief Group cookie keys dynarray
	DYNARRAY(user_cookie_key_t) grp_keys;
};

//! @brief Free group cookie object
//! @param cookie Pointer to the cookie
static void user_group_cookie_destroy(struct user_group_cookie *cookie) {
	mem_heap_free(cookie, sizeof(struct user_group_cookie));
}

//! @brief Create new group cookie object
//! @param buf Buffer to store pointer to the new security cookie in
//! @return API status
int user_group_cookie_create(struct user_group_cookie **buf) {
	struct user_group_cookie *res = mem_heap_alloc(sizeof(struct user_group_cookie));
	if (res == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	res->key = ATOMIC_FETCH_INCREMENT(&user_cookie_last);
	MEM_REF_INIT(res->key, user_group_cookie_destroy);
	*buf = res;
	return USER_STATUS_SUCCESS;
}

//! @brief Free entry cookie object
//! @param cookie Pointer to the cookie
static void user_entry_cookie_destroy(struct user_entry_cookie *cookie) {
	DYNARRAY_DESTROY(cookie->grp_keys);
	mem_heap_free(cookie, sizeof(struct user_entry_cookie));
}

//! @brief Create new API entry cookie object
//! @param buf Buffer to store pointer to the new API entry cookie in
//! @return API status
int user_entry_cookie_create(struct user_entry_cookie **buf) {
	struct user_entry_cookie *res = mem_heap_alloc(sizeof(struct user_entry_cookie));
	if (res == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	DYNARRAY(user_cookie_key_t) keys = DYNARRAY_NEW(user_cookie_key_t);
	if (keys == NULL) {
		mem_heap_free(res, sizeof(struct user_entry_cookie));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	MEM_REF_INIT(res, user_entry_cookie_destroy);
	res->grp_keys = keys;
	res->lock = THREAD_MUTEX_INIT;
	res->key = ATOMIC_FETCH_INCREMENT(&user_cookie_last);
	*buf = res;
	return USER_STATUS_SUCCESS;
}

//! @brief Check if key is pressent in group keys list
//! @param entry Pointer to the entry cookie object
//! @param key Key to search for
bool user_entry_cookie_grp_key_present(struct user_entry_cookie *entry, user_cookie_key_t key) {
	for (size_t i = 0; i < dynarray_len(entry->grp_keys); ++i) {
		if (entry->grp_keys[i] == key) {
			return true;
		}
	}
	return false;
}

//! @brief Transfer group cookie perms to entry cookie
//! @param entry Pointer to the entry cookie object
//! @param grp Pointer to the group cookie object
//! @return API status
int user_entry_cookie_add_to_grp(struct user_entry_cookie *entry,
                                 struct user_group_cookie *cookie) {
	thread_mutex_lock(&entry->lock);
	if (user_entry_cookie_grp_key_present(entry, cookie->key)) {
		thread_mutex_unlock(&entry->lock);
		return USER_STATUS_SUCCESS;
	}
	for (size_t i = 0; i < dynarray_len(entry->grp_keys); ++i) {
		if (entry->grp_keys[i] == 0) {
			entry->grp_keys[i] = cookie->key;
			thread_mutex_unlock(&entry->lock);
			return USER_STATUS_SUCCESS;
		}
	}
	DYNARRAY(user_cookie_key_t) new_grp_keys = DYNARRAY_PUSH(entry->grp_keys, cookie->key);
	if (new_grp_keys == NULL) {
		thread_mutex_unlock(&entry->lock);
		return USER_STATUS_OUT_OF_MEMORY;
	}
	entry->grp_keys = new_grp_keys;
	thread_mutex_unlock(&entry->lock);
	return USER_STATUS_SUCCESS;
}

//! @brief Remove group perms from entry cookie
//! @param entry Pointer to the entry cookie object
//! @param grp Pointer to the group cookie object
//! @return API status
int user_entry_cookie_remove_from_grp(struct user_entry_cookie *entry,
                                      struct user_group_cookie *cookie) {
	thread_mutex_lock(&entry->lock);
	for (size_t i = 0; i < dynarray_len(entry->grp_keys); ++i) {
		if (entry->grp_keys[i] == cookie->key) {
			entry->grp_keys[i] = 0;
			thread_mutex_unlock(&entry->lock);
			break;
		}
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Check perms for entry cookie
//! @param entry Pointer to the user API entry cookie
//! @param key Cookie key
//! @return True if authentification was successful, false otherwise
bool user_entry_cookie_auth(struct user_entry_cookie *entry, user_cookie_key_t key) {
	if (key == USER_COOKIE_KEY_UNIVERSAL || key == entry->key) {
		return true;
	} else if (key == USER_COOKIE_KEY_ONLY_KERNEL) {
		return false;
	}
	thread_mutex_lock(&entry->lock);
	bool result = user_entry_cookie_grp_key_present(entry, key);
	thread_mutex_unlock(&entry->lock);
	return result;
}

//! @brief Get group cookie key
//! @param grp Pointer to the group cookie
//! @return Cookie key
user_cookie_key_t user_group_cookie_get_key(struct user_group_cookie *grp) {
	return grp->key;
}

//! @brief Get entry cookie key
//! @param entry Pointer to the user API entry cookie
//! @return Cookie key
user_cookie_key_t user_entry_cookie_get_key(struct user_entry_cookie *entry) {
	return entry->key;
}
