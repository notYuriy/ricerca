//! @file object.c
//! @brief File containing definitions of generic object-related types/functions

#include <lib/list.h>
#include <lib/panic.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <thread/locking/mutex.h>
#include <user/ipc.h>
#include <user/object.h>

//! @brief Drop owning reference to the object
//! @param ref Reference to drop
void user_drop_owning_ref(struct user_ref ref) {
	switch (ref.type) {
	case USER_OBJ_TYPE_MAILBOX:
		user_destroy_mailbox(ref.mailbox);
		break;
	case USER_OBJ_TYPE_STREAM_PRODUCER:
		user_ipc_shutdown_stream_producer(ref.stream);
		break;
	case USER_OBJ_TYPE_STREAM_CONSUMER:
		user_ipc_shutdown_stream_consumer(ref.stream);
		break;
	case USER_OBJ_TYPE_NONE:
		break;
	default:
		MEM_REF_DROP(ref.ref);
		break;
	}
}
