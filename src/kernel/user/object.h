//! @file object.h
//! @brief File containing declarations of generic object-related types/functions

#pragma once

#include <mem/rc.h>
#include <user/cookie.h>

//! @brief Object type
enum
{
	//! @brief Invalid object
	USER_OBJ_TYPE_NONE = 0,
	//! @brief Caller
	USER_OBJ_TYPE_CALLER = 1,
	//! @brief Calee
	USER_OBJ_TYPE_CALLEE = 2,
	//! @brief Token
	USER_OBJ_TYPE_TOKEN = 3,
	//! @brief Mailbox
	USER_OBJ_TYPE_MAILBOX = 4,
	//! @brief Universe
	USER_OBJ_TYPE_UNIVERSE = 5,
	//! @brief Group cookie
	USER_OBJ_TYPE_GROUP_COOKIE = 6,
	//! @brief Entry cookie
	USER_OBJ_TYPE_ENTRY_COOKIE = 7,
	//! @brief SHM owner reference
	USER_OBJ_TYPE_SHM_OWNER = 8,
	//! @brief SHM RW reference
	USER_OBJ_TYPE_SHM_RW_REF = 9,
	//! @brief SHM RO reference
	USER_OBJ_TYPE_SHM_RO_REF = 10,
};

//! @brief Object reference
struct user_ref {
	union {
		//! @brief Ref-counted pointer to the object
		struct mem_rc *ref;
		//! @brief Pointer to the caller
		struct user_rpc_caller *caller;
		//! @brief Pointer to the callee
		struct user_rpc_callee *callee;
		//! @brief Pointer to the token
		struct user_rpc_token *token;
		//! @brief Pointer to the mailbox
		struct user_mailbox *mailbox;
		//! @brief Pointer to the universe
		struct user_universe *universe;
		//! @brief Pointer to the group cookie
		struct user_group_cookie *group_cookie;
		//! @brief Pointer to the entry cookie
		struct user_entry_cookie *entry_cookie;
		//! @brief Pointer to the SHM owner objcet
		struct user_shm_owner *shm_owner;
		//! @brief Pointer to the SHM ref object (shared by RW and RO ref types)
		struct user_shm_ref *shm_ref;
	};
	//! @brief Referenced object type
	int type;
	//! @brief Pin cookie key (controls who is allowed to duplicate/move/drop reference)
	user_cookie_key_t pin_cookie;
};

//! @brief Check if duplicate/move/drop operation could be performed on this reference
//! @param ref Reference to check
//! @param cookie Pointer to the user API entry cookie
static inline bool user_unpinned_for(struct user_ref ref, struct user_entry_cookie *cookie) {
	return user_entry_cookie_auth(cookie, ref.pin_cookie);
}

//! @brief Drop reference to the object
//! @param ref Reference to drop
static inline void user_drop_ref(struct user_ref ref) {
	MEM_REF_DROP(ref.ref);
}

//! @brief Borrow reference to the object
//! @param ref Reference to drop
static inline struct user_ref user_borrow_ref(struct user_ref ref) {
	struct user_ref result = ref;
	result.ref = MEM_REF_BORROW(ref.ref);
	return result;
}
