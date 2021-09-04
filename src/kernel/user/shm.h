//! @file shm.h
//! @brief File containing declarations for shared memory functions

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <user/cookie.h>
#include <user/status.h>

//! @brief SHM object. Responsible for managing SHM buffer ownership
struct user_shm_owner;

//! @brief SHM reference. Can be used to access SHM object via reference to it
struct user_shm_ref;

//! @brief Create new SHM object
//! @param objbuf Buffer to store pointer to the SHM owner object in
//! @param idbuf Buffer to store SHM object ID in
//! @param size Memory size
//! @param cookie Entry cookie
//! @return API status
int user_shm_create(struct user_shm_owner **objbuf, size_t *idbuf, size_t size,
                    struct user_entry_cookie *cookie);

//! @brief Borrow SHM ref
//! @param owner SHM owner object
//! @return Borrowed SHM ref object
struct user_shm_ref *user_shm_create_ref(struct user_shm_owner *owner);

//! @brief Write to SHM given object ref
//! @param ref Pointer to the SHM ref
//! @param offset Offset in shared memory
//! @param len How many bytes to write
//! @param data Pointer to the data (can point to userspace)
//! @return API status
int user_shm_write_by_ref(struct user_shm_ref *ref, size_t offset, size_t len, const void *data);

//! @brief Read from SHM given object ref
//! @param ref Pointer to the SHM ref
//! @param offset Offset in shared memory
//! @param len How many bytes to read
//! @param data Pointer to the buffer (can point to userspace)
//! @return API status
int user_shm_read_by_ref(struct user_shm_ref *ref, size_t offset, size_t len, void *data);

//! @brief Write to SHM given SHM id
//! @param id Id of the SHM object
//! @param offset Offset in shared memory
//! @param len How many bytes to write
//! @param data Pointer to the data (can point to userspace)
//! @param cookie Entry cookie
//! @return API status
int user_shm_write_by_id(size_t id, size_t offset, size_t len, const void *data,
                         struct user_entry_cookie *cookie);

//! @brief Read from SHM given SHM id
//! @param id Id of the SHM object
//! @param offset Offset in shared memory
//! @param len How many bytes to read
//! @param data Pointer to the buffer (can point to userspace)
//! @param cookie Entry cookie
//! @return API status
int user_shm_read_by_id(size_t id, size_t offset, size_t len, void *data,
                        struct user_entry_cookie *cookie);

//! @brief Give access rights to all processes
//! @param object Pointer to the SHM owner object
//! @param rw Ownership type (true if R/W rights are dropped, false if R/O rights are dropped)
//! @return API status
int user_shm_drop_ownership(struct user_shm_owner *object, bool rw);

//! @brief Acquire self ownership
//! @param object Pointer to the SHM owner object
//! @param cookie Entry cookie
//! @param rw Ownership type (true if R/W rights are acquired, false if R/O rights are acquired)
//! @return API status
int user_shm_acquire_ownership(struct user_shm_owner *object, struct user_entry_cookie *cookie,
                               bool rw);

//! @brief Give ownership to group
//! @param object Pointer to the SHM owner object
//! @param group Group cookie
//! @param rw Ownership type (true if R/W rights are dropped, false if R/O rights are dropped)
//! @return API status
int user_shm_give_ownership_to_grp(struct user_shm_owner *object, struct user_group_cookie *group,
                                   bool rw);

//! @brief Export target for SHM subsystem initialization
EXPORT_TARGET(user_shms_available)
