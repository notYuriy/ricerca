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
	};
	//! @brief Referenced object type
	int type;
};

//! @brief Drop borrowed reference to the object
//! @param ref Reference to drop
static inline void user_drop_ref(struct user_ref ref) {
	MEM_REF_DROP(ref.ref);
}
