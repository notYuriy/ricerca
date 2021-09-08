//! @file shm.c
//! @brief File containing implementation of SHM API

#include <lib/containerof.h>
#include <lib/dynarray.h>
#include <lib/intmap.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <mem/mem.h>
#include <mem/rc.h>
#include <mem/usercopy.h>
#include <misc/atomics.h>
#include <thread/locking/spinlock.h>
#include <user/shm.h>

MODULE("user/shm")
TARGET(user_shms_available, user_shm_init, {mem_all_available})

//! @brief Number of buckets in SHM intmap
#define USER_SHM_INTMAP_BUCKETS 1024

//! @brief SHM ref object. Can be used to access SHM via capability
struct user_shm_ref {
	//! @brief Dealloc RC base
	struct mem_rc rc_base;
};

//! @brief SHM owner object. Responsible for managing SHM buffer ownership
struct user_shm_owner {
	//! @brief Shutdown RC base
	struct mem_rc shutdown_rc_base;
	//! @brief SHM ref object
	struct user_shm_ref ref;
	//! @brief SHM lock
	struct thread_spinlock lock;
	//! @brief Pointer to the data
	uint8_t *data;
	//! @brief Memory size
	size_t size;
	//! @brief Intmap node
	struct intmap_node node;
	//! @brief RO cookie key
	user_cookie_key_t ro_key;
	//! @brief RW cookie key
	user_cookie_key_t rw_key;
};

//! @brief SHM buffers intmap
static struct intmap user_shm_intmap;

//! @brief Pointer to the array of spinlocks for each bucket
static struct thread_spinlock *user_shm_spinlocks;

//! @brief Last allocated SHM object ID
static size_t user_shm_last_allocated_id = 0;

//! @brief Shutdown SHM object
//! @param shm Pointer to the SHM object
static void user_shm_shutdown(struct user_shm_owner *shm) {
	const bool int_state =
	    thread_spinlock_lock(user_shm_spinlocks + (shm->node.key % USER_SHM_INTMAP_BUCKETS));
	intmap_remove(&user_shm_intmap, &shm->node);
	thread_spinlock_unlock(user_shm_spinlocks + (shm->node.key % USER_SHM_INTMAP_BUCKETS),
	                       int_state);
	MEM_REF_DROP(&shm->ref);
}

//! @brief Dealloc SHM object
//! @param ref Pointer to the SHM ref object
static void user_shm_dealloc(struct user_shm_ref *ref) {
	struct user_shm_owner *shm = CONTAINER_OF(ref, struct user_shm_owner, ref);
	mem_heap_free(shm->data, shm->size);
	mem_heap_free(shm, sizeof(struct user_shm_owner));
}

//! @brief Create new SHM object
//! @param objbuf Buffer to store pointer to the SHM owner object in
//! @param idbuf Buffer to store SHM object ID in
//! @param size Memory size
//! @param cookie Entry cookie
//! @return API status
int user_shm_create(struct user_shm_owner **objbuf, size_t *idbuf, size_t size,
                    struct user_entry_cookie *cookie) {
	struct user_shm_owner *shm = mem_heap_alloc(sizeof(struct user_shm_owner));
	if (shm == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	uint8_t *data = mem_heap_alloc(size);
	if (data == NULL) {
		mem_heap_free(shm, sizeof(struct user_shm_owner));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	memset(data, 0, size);
	shm->data = data;
	shm->size = size;
	shm->lock = THREAD_SPINLOCK_INIT;
	shm->node.key = ATOMIC_FETCH_INCREMENT(&user_shm_last_allocated_id);
	shm->ro_key = shm->rw_key = user_entry_cookie_get_key(cookie);
	MEM_REF_INIT(&shm->ref, user_shm_dealloc);
	MEM_REF_INIT(&shm->shutdown_rc_base, user_shm_shutdown);
	const bool int_state =
	    thread_spinlock_lock(user_shm_spinlocks + (shm->node.key % USER_SHM_INTMAP_BUCKETS));
	intmap_insert(&user_shm_intmap, &shm->node);
	thread_spinlock_unlock(user_shm_spinlocks + (shm->node.key % USER_SHM_INTMAP_BUCKETS),
	                       int_state);
	*objbuf = shm;
	*idbuf = shm->node.key;
	return USER_STATUS_SUCCESS;
}

//! @brief Borrow SHM ref
//! @param owner SHM owner object
//! @return Borrowed SHM ref object
struct user_shm_ref *user_shm_create_ref(struct user_shm_owner *owner) {
	return MEM_REF_BORROW(&owner->ref);
}

//! @brief Check if slice is withing SHM bounds
//! @param shm Pointer to the SHM object
//! @param offset Offset in SHM object
//! @param len Slice length
//! @return True if slice is valid, false otherwise
static bool user_shm_check_bounds(struct user_shm_owner *shm, size_t offset, size_t len) {
	size_t end;
	if (__builtin_add_overflow(offset, len, &end)) {
		return false;
	}
	return end <= shm->size;
}

//! @brief Write to SHM given object ref
//! @param ref Pointer to the SHM ref
//! @param offset Offset in shared memory
//! @param len How many bytes to write
//! @param data Pointer to the data (can point to userspace)
//! @return API status
int user_shm_write_by_ref(struct user_shm_ref *ref, size_t offset, size_t len, const void *data) {
	struct user_shm_owner *shm = CONTAINER_OF(ref, struct user_shm_owner, ref);
	if (!user_shm_check_bounds(shm, offset, len)) {
		return USER_STATUS_OUT_OF_BOUNDS;
	}
	if (!mem_copy_from_user(shm->data + offset, data, len)) {
		return USER_STATUS_INVALID_MEM;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Read from SHM given object ref
//! @param ref Pointer to the SHM ref
//! @param offset Offset in shared memory
//! @param len How many bytes to read
//! @param data Pointer to the buffer (can point to userspace)
//! @return API status
int user_shm_read_by_ref(struct user_shm_ref *ref, size_t offset, size_t len, void *data) {
	struct user_shm_owner *shm = CONTAINER_OF(ref, struct user_shm_owner, ref);
	if (!user_shm_check_bounds(shm, offset, len)) {
		return USER_STATUS_OUT_OF_BOUNDS;
	}
	if (!mem_copy_to_user(data, shm->data + offset, len)) {
		return USER_STATUS_INVALID_MEM;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Authentificate read operation on SHM object
//! @param shm Pointer to the SHM object
//! @param cookie Entry cookie
//! @return True if authentification has been successful, false otherwise
static bool user_shm_auth_read(struct user_shm_owner *shm, struct user_entry_cookie *cookie) {
	return user_entry_cookie_auth(cookie, shm->rw_key) ||
	       user_entry_cookie_auth(cookie, shm->ro_key);
}

//! @brief Authentificate write operation on SHM object
//! @param shm Pointer to the SHM object
//! @param cookie Entry cookie
//! @return True if authentification has been successful, false otherwise
static bool user_shm_auth_write(struct user_shm_owner *shm, struct user_entry_cookie *cookie) {
	return user_entry_cookie_auth(cookie, shm->rw_key);
}

//! @brief Find SHM object and borrow it
//! @param id Id of the SHM object
//! @return Pointer to the SHM object or NULL if not found
static struct user_shm_owner *user_shm_find_by_id(size_t id) {
	size_t mod = id % USER_SHM_INTMAP_BUCKETS;
	const bool int_state = thread_spinlock_lock(user_shm_spinlocks + mod);
	struct user_shm_owner *result =
	    CONTAINER_OF_NULLABLE(intmap_search(&user_shm_intmap, id), struct user_shm_owner, node);
	if (result != NULL) {
		MEM_REF_BORROW(&result->ref);
	}
	thread_spinlock_unlock(user_shm_spinlocks + mod, int_state);
	return result;
}

//! @brief Write to SHM given SHM id
//! @param id Id of the SHM object
//! @param offset Offset in shared memory
//! @param len How many bytes to write
//! @param data Pointer to the data (can point to userspace)
//! @param cookie Entry cookie
//! @return API status
int user_shm_write_by_id(size_t id, size_t offset, size_t len, const void *data,
                         struct user_entry_cookie *cookie) {
	struct user_shm_owner *shm = user_shm_find_by_id(id);
	if (shm == NULL) {
		return USER_STATUS_SECURITY_VIOLATION;
	}
	const bool int_state = thread_spinlock_lock(&shm->lock);
	bool auth = user_shm_auth_write(shm, cookie);
	thread_spinlock_unlock(&shm->lock, int_state);
	if (!auth) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_SECURITY_VIOLATION;
	}
	if (!user_shm_check_bounds(shm, offset, len)) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_OUT_OF_BOUNDS;
	}
	if (!mem_copy_from_user(shm->data + offset, data, len)) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_INVALID_MEM;
	}
	MEM_REF_DROP(&shm->ref);
	return USER_STATUS_SUCCESS;
}

//! @brief Read from SHM given SHM id
//! @param id Id of the SHM object
//! @param offset Offset in shared memory
//! @param len How many bytes to read
//! @param data Pointer to the buffer (can point to userspace)
//! @param cookie Entry cookie
//! @return API status
int user_shm_read_by_id(size_t id, size_t offset, size_t len, void *data,
                        struct user_entry_cookie *cookie) {
	struct user_shm_owner *shm = user_shm_find_by_id(id);
	if (shm == NULL) {
		return USER_STATUS_SECURITY_VIOLATION;
	}
	const bool int_state = thread_spinlock_lock(&shm->lock);
	bool auth = user_shm_auth_read(shm, cookie);
	thread_spinlock_unlock(&shm->lock, int_state);
	if (!auth) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_SECURITY_VIOLATION;
	}
	if (!user_shm_check_bounds(shm, offset, len)) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_OUT_OF_BOUNDS;
	}
	if (!mem_copy_to_user(data, shm->data + offset, len)) {
		MEM_REF_DROP(&shm->ref);
		return USER_STATUS_INVALID_MEM;
	}
	MEM_REF_DROP(&shm->ref);
	return USER_STATUS_SUCCESS;
}

//! @brief Initialize SHM subsystem
static void user_shm_init() {
	if (!intmap_init(&user_shm_intmap, USER_SHM_INTMAP_BUCKETS)) {
		LOG_PANIC("Failed to initialize SHM intmap");
	}
	user_shm_spinlocks = mem_heap_alloc(sizeof(struct thread_spinlock) * USER_SHM_INTMAP_BUCKETS);
	if (user_shm_spinlocks == NULL) {
		LOG_PANIC("Failed to allocate SHM intmap locks");
	}
	for (size_t i = 0; i < USER_SHM_INTMAP_BUCKETS; ++i) {
		user_shm_spinlocks[i] = THREAD_SPINLOCK_INIT;
	}
}

//! @brief Modify SHM's cookie key
//! @param shm Pointer to the SHM owner object
//! @param key New key
//! @param rw Ownership type (true if R/W rights are dropped, false if R/O rights are dropped)
static void user_shm_modify_perms(struct user_shm_owner *shm, user_cookie_key_t key, bool rw) {
	const bool int_state = thread_spinlock_lock(&shm->lock);
	if (rw) {
		shm->ro_key = key;
	} else {
		shm->rw_key = key;
	}
	thread_spinlock_unlock(&shm->lock, int_state);
}

//! @brief Give access rights to all processes
//! @param object Pointer to the SHM owner object
//! @param rw Ownership type (true if R/W rights are dropped, false if R/O rights are dropped)
//! @return API status
int user_shm_drop_ownership(struct user_shm_owner *object, bool rw) {
	user_shm_modify_perms(object, USER_COOKIE_KEY_UNIVERSAL, rw);
	return USER_STATUS_SUCCESS;
}

//! @brief Acquire self ownership
//! @param object Pointer to the SHM owner object
//! @param cookie Entry cookie
//! @param rw Ownership type (true if R/W rights are acquired, false if R/O rights are acquired)
//! @return API status
int user_shm_acquire_ownership(struct user_shm_owner *object, struct user_entry_cookie *cookie,
                               bool rw) {
	user_shm_modify_perms(object, user_entry_cookie_get_key(cookie), rw);
	return USER_STATUS_SUCCESS;
}

//! @brief Give ownership to group
//! @param object Pointer to the SHM owner object
//! @param group Group cookie
//! @param rw Ownership type (true if R/W rights are dropped, false if R/O rights are dropped)
//! @return API status
int user_shm_give_ownership_to_grp(struct user_shm_owner *object, struct user_group_cookie *group,
                                   bool rw) {
	user_shm_modify_perms(object, user_group_cookie_get_key(group), rw);
	return USER_STATUS_SUCCESS;
}
