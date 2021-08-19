//! @file object.h
//! @brief File containing declarations of generic object-related types/functions

#pragma once

#include <mem/rc.h>

//! @brief Object type
enum
{
	//! @brief Invalid object
	USER_OBJ_TYPE_NONE = 0,
	//! @brief Stream producer
	USER_OBJ_TYPE_STREAM_PRODUCER = 1,
	//! @brief Stream consumer
	USER_OBJ_TYPE_STREAM_CONSUMER = 2,
	//! @brief Mailbox
	USER_OBJ_TYPE_MAILBOX = 3,
};

//! @brief Object reference
struct user_ref {
	union {
		//! @brief Ref-counted pointer to the object
		struct mem_rc *ref;
		//! @brief Pointer to the stream
		struct user_ipc_stream *stream;
		//! @brief Pointer to the mailbox
		struct user_mailbox *mailbox;
	};
	//! @brief Referenced object type
	int type;
};

//! @brief Drop owning reference to the object
//! @param ref Reference to drop
void user_drop_owning_ref(struct user_ref ref);
