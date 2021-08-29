//! @file object.h
//! @brief File containing declarations of generic object-related types/functions

#pragma once

#include <mem/rc.h>

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
};

//! @brief Unpinned cookie (no restrictions on duplicating/moving/dropping ref)
#define USER_COOKIE_UNPIN 0

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
	};
	//! @brief Referenced object type
	int type;
	//! @brief Pin cookie (controls who is allowed to duplicate/move/drop reference)
	size_t pin_cookie;
};

//! @brief Check if duplicate/move/drop operation could be performed on this reference
static inline bool user_unpinned_for(struct user_ref ref, size_t cookie) {
	return ref.pin_cookie == USER_COOKIE_UNPIN || ref.pin_cookie == cookie;
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
