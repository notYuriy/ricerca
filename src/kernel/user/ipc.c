#include <lib/callback.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/queue.h>
#include <lib/string.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/rc.h>
#include <thread/locking/spinlock.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/task.h>
#include <user/ipc.h>

MODULE("user/ipc")

//! @brief IPC stream
struct user_ipc_stream {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Pointer to the message buffers
	struct user_ipc_msg *msgs;
	//! @brief Max message quota
	size_t max_quota;
	//! @brief Message buffers circular queue head
	size_t head;
	//! @brief Message buffers circular queue tail
	size_t tail;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief Owning mailbox
	struct user_mailbox *mailbox;
	//! @brief Opaque value
	size_t opaque;
	//! @brief True if stream has been shutdown
	bool shutdown;
	//! @brief True if event has been raised and not yet handled
	bool raised;
};

//! @brief Deallocate IPC stream
//! @param stream Stream to deallocate
//! @param opaque Unused
void user_ipc_cleanup_stream(struct user_ipc_stream *stream, void *opaque) {
	(void)opaque;
	MEM_REF_DROP(stream->mailbox);
	mem_heap_free(stream->msgs, sizeof(struct user_ipc_msg) * stream->max_quota);
	mem_heap_free(stream, sizeof(struct user_ipc_stream));
}

//! @brief Create IPC stream
//! @param stream Buffer to store pointer to the new stream in
//! @param quota Max size of pending messages queue
//! @param mailbox Pointer to the mailbox for recieving notifications
//! @param opaque Opaque value to be passed in notifications
//! @return API status
//! @note Reference count of the returned stream is 1
int user_ipc_create_stream(struct user_ipc_stream **stream, size_t quota,
                           struct user_mailbox *mailbox, size_t opaque) {
	struct user_ipc_stream *res_stream = mem_heap_alloc(sizeof(struct user_ipc_stream));
	if (res_stream == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	struct user_ipc_msg *msgs = mem_heap_alloc(sizeof(struct user_ipc_msg) * quota);
	if (msgs == NULL) {
		mem_heap_free(res_stream, sizeof(struct user_ipc_stream));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	int status = user_reserve_mailbox_slot(mailbox);
	if (status != USER_STATUS_SUCCESS) {
		mem_heap_free(res_stream, sizeof(struct user_ipc_stream));
		mem_heap_free(msgs, sizeof(struct user_ipc_msg) * quota);
		return status;
	}
	MEM_REF_INIT(res_stream, user_ipc_cleanup_stream, NULL);
	res_stream->head = 0;
	res_stream->lock = THREAD_SPINLOCK_INIT;
	res_stream->mailbox = MEM_REF_BORROW(mailbox);
	res_stream->max_quota = quota;
	res_stream->msgs = msgs;
	res_stream->opaque = opaque;
	res_stream->raised = false;
	res_stream->shutdown = false;
	res_stream->tail = 0;
	*stream = res_stream;
	return USER_STATUS_SUCCESS;
}

//! @brief Raise event on IPC stream without locking
//! @param stream Pointer to the stream
//! @return API status
static int user_ipc_raise_event_nolock(struct user_ipc_stream *stream) {
	if (!stream->raised) {
		stream->raised = true;
		struct user_notification note;
		note.opaque = stream->opaque;
		note.type = USER_NOTE_TYPE_IPC_STREAM_UPDATE;
		return user_send_notification(stream->mailbox, &note);
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Shutdown IPC stream on consumer side
//! @param stream Pointer to the stream
void user_ipc_shutdown_stream_consumer(struct user_ipc_stream *stream) {
	const bool int_state = thread_spinlock_lock(&stream->lock);
	stream->shutdown = true;
	thread_spinlock_unlock(&stream->lock, int_state);
	MEM_REF_DROP(stream);
}

//! @brief Shutdown IPC stream on producer side
//! @param stream Pointer to the stream
void user_ipc_shutdown_stream_producer(struct user_ipc_stream *stream) {
	const bool int_state = thread_spinlock_lock(&stream->lock);
	if (!stream->shutdown) {
		user_ipc_raise_event_nolock(stream);
	}
	stream->shutdown = true;
	thread_spinlock_unlock(&stream->lock, int_state);
	MEM_REF_DROP(stream);
}

//! @brief Send signal over IPC stream
//! @param stream Pointer to the stream to send signal to
//! @return API status
int user_ipc_send_signal(struct user_ipc_stream *stream) {
	const bool int_state = thread_spinlock_lock(&stream->lock);
	if (stream->shutdown) {
		thread_spinlock_unlock(&stream->lock, int_state);
		return USER_STATUS_TARGET_UNREACHABLE;
	}
	int status = user_ipc_raise_event_nolock(stream);
	thread_spinlock_unlock(&stream->lock, int_state);
	return status;
}

//! @brief Copy message
//! @param dst Destination message buffer
//! @param src Source message buffer
static inline void user_ipc_copy_msg(struct user_ipc_msg *dst, const struct user_ipc_msg *src) {
	memcpy(dst->payload, src->payload, src->length);
	dst->length = src->length;
}

//! @brief Send message over IPC stream
//! @param stream Pointer to the stream to send message to
//! @param message Message to send
//! @return API status
int user_ipc_send_msg(struct user_ipc_stream *stream, const struct user_ipc_msg *msg) {
	if (msg->length > USER_IPC_PAYLOAD_MAX) {
		return USER_STATUS_INVALID_MSG;
	}
	const bool int_state = thread_spinlock_lock(&stream->lock);
	if (stream->shutdown) {
		thread_spinlock_unlock(&stream->lock, int_state);
		return USER_STATUS_TARGET_UNREACHABLE;
	}
	if (stream->head - stream->tail == stream->max_quota) {
		thread_spinlock_unlock(&stream->lock, int_state);
		return USER_STATUS_QUOTA_EXCEEDED;
	}
	user_ipc_copy_msg(stream->msgs + (stream->head % stream->max_quota), msg);
	stream->head++;
	user_ipc_raise_event_nolock(stream);
	thread_spinlock_unlock(&stream->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Attempt to recieve message over IPC stream
//! @param stream Pointer to the stream to recieve message from
//! @param buf Buffer to store message in
//! @return API status
int user_ipc_recieve_msg(struct user_ipc_stream *stream, struct user_ipc_msg *buf) {
	const bool int_state = thread_spinlock_lock(&stream->lock);
	if (stream->shutdown) {
		thread_spinlock_unlock(&stream->lock, int_state);
		return USER_STATUS_TARGET_UNREACHABLE;
	}
	if (stream->head == stream->tail) {
		thread_spinlock_unlock(&stream->lock, int_state);
		return USER_STATUS_STREAM_EMPTY;
	}
	user_ipc_copy_msg(buf, stream->msgs + (stream->tail % stream->max_quota));
	stream->tail++;
	stream->raised = false;
	thread_spinlock_unlock(&stream->lock, int_state);
	return USER_STATUS_SUCCESS;
}
