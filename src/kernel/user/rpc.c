//! @file rpc.c
//! @brief File containing RPC API implementation

#include <lib/containerof.h>
#include <lib/intmap.h>
#include <lib/queue.h>
#include <lib/string.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/rc.h>
#include <thread/locking/spinlock.h>
#include <user/rpc.h>

MODULE("user/rpc")

//! @brief RPC container. Contains all info about RPC request
struct user_rpc_container {
	//! @brief RPC message
	struct user_rpc_msg message;
	//! @brief Client opaque
	size_t client_opaque;
	//! @brief Queue node
	struct queue_node qnode;
	//! @brief Int map node
	struct intmap_node inode;
	//! @brief Reference to the caller that initiated RPC
	struct user_rpc_caller *caller;
};

//! @brief RPC token. Used as RPC target
struct user_rpc_token {
	//! @brief Discoverability RC base
	struct mem_rc rc_base;
};

//! @brief RPC caller. Allows to initiate RPC requests
struct user_rpc_caller {
	//! @brief Shutdown RC base
	struct mem_rc shutdown_rc_base;
	//! @brief Deallocation RC base
	struct mem_rc dealloc_rc_base;
	//! @brief On-reply notificaton raiser
	struct user_raiser *on_reply_raiser;
	//! @brief Queue of free RPC containers
	struct queue free_containers;
	//! @brief Queue of incoming RPC replies
	//! @note Moving token from client_opaque to message.opaque is done in user_rpc_get_result
	struct queue incoming_replies;
	//! @brief Caller lock
	struct thread_spinlock lock;
	//! @brief True if caller has been shut down
	bool is_shut_down;
};

//! @brief RPC callee. Allows to accept RPC requests
struct user_rpc_callee {
	//! @brief Shutdown RC base
	struct mem_rc shutdown_rc_base;
	//! @brief Deallocation RC base
	struct mem_rc dealloc_rc_base;
	//! @brief Attached token
	struct user_rpc_token token;
	//! @brief On-incoming notification raiser
	struct user_raiser *on_incoming_raiser;
	//! @brief Incoming RPC calls queue
	struct queue incoming_rpcs;
	//! @brief Hashmap with RPC calls waiting for reply
	struct intmap awaiting_reply;
	//! @brief Callee lock
	struct thread_spinlock lock;
	//! @brief Last allocated sequence number
	size_t seq;
	//! @brief True if callee has been shut down
	bool is_shut_down;
};
//! @brief Deallocate queue of inactive containers
//! @param queue Pointer to the queue
static void user_rpc_destroy_msg_queue(struct queue *queue) {
	struct user_rpc_container *current;
	while ((current = QUEUE_DEQUEUE(queue, struct user_rpc_container, qnode)) != NULL) {
		mem_heap_free(current, sizeof(struct user_rpc_container));
	}
}

//! @brief Shutdown RPC caller
//! @param shutdown_rc_base Pointer to the shutdown_rc_base member
static void user_rpc_shutdown_caller(struct mem_rc *shutdown_rc_base) {
	struct user_rpc_caller *caller =
	    CONTAINER_OF(shutdown_rc_base, struct user_rpc_caller, shutdown_rc_base);
	const bool int_state = thread_spinlock_lock(&caller->lock);
	ASSERT(!caller->is_shut_down, "Caller has already been shutdown");
	caller->is_shut_down = true;
	thread_spinlock_unlock(&caller->lock, int_state);
	MEM_REF_DROP(caller->on_reply_raiser);
	user_rpc_destroy_msg_queue(&caller->free_containers);
	MEM_REF_DROP(&caller->dealloc_rc_base);
}

//! @brief Deallocate resources associated with RPC caller
//! @param dealloc_rc_base Pointer to the dealloc_rc_base
static void user_rpc_dealloc_caller(struct mem_rc *dealloc_rc_base) {
	struct user_rpc_caller *caller =
	    CONTAINER_OF(dealloc_rc_base, struct user_rpc_caller, dealloc_rc_base);
	user_rpc_destroy_msg_queue(&caller->incoming_replies);
	mem_heap_free(caller, sizeof(struct user_rpc_caller));
}

//! @brief Create RPC caller
//! @param mailbox Pointer to the mailbox
//! @param opaque Opaque value (to be recieved in notifications)
//! @param caller Buffer to save pointer to the caller in
int user_rpc_create_caller(struct user_mailbox *mailbox, size_t opaque,
                           struct user_rpc_caller **caller) {
	struct user_rpc_caller *res_caller = mem_heap_alloc(sizeof(struct user_rpc_caller));
	if (res_caller == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	struct user_notification template;
	template.opaque = opaque;
	template.type = USER_NOTE_TYPE_RPC_REPLY;
	int status = user_create_raiser(mailbox, &res_caller->on_reply_raiser, template);
	if (status != USER_STATUS_SUCCESS) {
		mem_heap_free(res_caller, sizeof(struct user_rpc_caller));
		return status;
	}
	*caller = res_caller;
	MEM_REF_INIT(&res_caller->dealloc_rc_base, user_rpc_dealloc_caller);
	MEM_REF_INIT(&res_caller->shutdown_rc_base, user_rpc_shutdown_caller);
	res_caller->free_containers = QUEUE_INIT;
	res_caller->incoming_replies = QUEUE_INIT;
	res_caller->is_shut_down = false;
	res_caller->lock = THREAD_SPINLOCK_INIT;
	return USER_STATUS_SUCCESS;
}

//! @brief Deallocate callee resources
//! @param dealloc_rc_base Pointer to the callee's dealloc_rc_base
static void user_rpc_dealloc_callee(struct mem_rc *dealloc_rc_base) {
	struct user_rpc_callee *callee =
	    CONTAINER_OF(dealloc_rc_base, struct user_rpc_callee, dealloc_rc_base);
	mem_heap_free(callee, sizeof(struct user_rpc_callee));
}

//! @brief Handler invoked when callee is no longer discoverable
//! @param token Pointer to the callee's token
static void user_rpc_notify_callee_undiscoverable(struct user_rpc_token *token) {
	struct user_rpc_callee *callee = CONTAINER_OF(token, struct user_rpc_callee, token);
	MEM_REF_DROP(&callee->dealloc_rc_base);
}

//! @brief Enqueue reply RPC container
//! @param container Pointer to the container
static void user_rpc_enqueue_reply_container(struct user_rpc_container *container) {
	struct user_rpc_caller *caller = container->caller;
	const bool int_state = thread_spinlock_lock(&caller->lock);
	QUEUE_ENQUEUE(&caller->incoming_replies, container, qnode);
	if (!caller->is_shut_down) {
		user_send_notification(caller->on_reply_raiser);
	}
	thread_spinlock_unlock(&caller->lock, int_state);
	MEM_REF_DROP(&caller->dealloc_rc_base);
}

//! @brief Shutdown callee
static void user_rpc_shutdown_callee(struct mem_rc *shutdown_rc_base) {
	struct user_rpc_callee *callee =
	    CONTAINER_OF(shutdown_rc_base, struct user_rpc_callee, shutdown_rc_base);
	const bool int_state = thread_spinlock_lock(&callee->lock);
	ASSERT(!callee->is_shut_down, "Callee has already been shutdown");
	callee->is_shut_down = true;
	thread_spinlock_unlock(&callee->lock, int_state);
	// Shutdown all requests from incoming queue
	struct user_rpc_container *incoming;
	while ((incoming = QUEUE_DEQUEUE(&callee->incoming_rpcs, struct user_rpc_container, qnode)) !=
	       NULL) {
		incoming->message.status = USER_RPC_STATUS_NOREPLY;
		incoming->message.len = 0;
		user_rpc_enqueue_reply_container(incoming);
	}
	// Shutdown all accepted RPCs in a similar fashion
	for (size_t i = 0; i < callee->awaiting_reply.buckets_count; ++i) {
		struct list *awaiting_list = callee->awaiting_reply.nodes + i;
		struct user_rpc_container *incoming;
		while ((incoming = LIST_REMOVE_HEAD(awaiting_list, struct user_rpc_container,
		                                    inode.node)) != NULL) {
			incoming->message.status = USER_RPC_STATUS_NOREPLY;
			incoming->message.len = 0;
			user_rpc_enqueue_reply_container(incoming);
		}
	}
	intmap_destroy(&callee->awaiting_reply);
	MEM_REF_DROP(callee->on_incoming_raiser);
	MEM_REF_DROP(&callee->dealloc_rc_base);
}

//! @brief Create RPC callee
//! @param mailbox Pointer to the mailbox
//! @param opaque Opaque value (to be recieved in notifications)
//! @param buckets Number of buckets hint
//! @param callee Buffer to save pointer to the callee in
//! @param token buffer to save pointer to the token in
int user_rpc_create_callee(struct user_mailbox *mailbox, size_t opaque, size_t buckets,
                           struct user_rpc_callee **callee, struct user_rpc_token **token) {
	struct user_rpc_callee *res_callee = mem_heap_alloc(sizeof(struct user_rpc_callee));
	if (res_callee == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	struct user_notification template;
	template.opaque = opaque;
	template.type = USER_NOTE_TYPE_RPC_INCOMING;
	int status = user_create_raiser(mailbox, &res_callee->on_incoming_raiser, template);
	if (status != USER_STATUS_SUCCESS) {
		mem_heap_free(res_callee, sizeof(struct user_rpc_callee));
		return status;
	}
	if (!intmap_init(&res_callee->awaiting_reply, buckets == 0 ? 1 : buckets)) {
		MEM_REF_DROP(res_callee->on_incoming_raiser);
		mem_heap_free(res_callee, sizeof(struct user_rpc_callee));
		return status;
	}
	*callee = res_callee;
	*token = &res_callee->token;
	MEM_REF_INIT(&res_callee->shutdown_rc_base, user_rpc_shutdown_callee);
	MEM_REF_INIT(&res_callee->dealloc_rc_base, user_rpc_dealloc_callee);
	MEM_REF_INIT(&res_callee->token, user_rpc_notify_callee_undiscoverable);
	res_callee->dealloc_rc_base.refcount = 2;
	res_callee->incoming_rpcs = QUEUE_INIT;
	res_callee->is_shut_down = false;
	res_callee->lock = THREAD_SPINLOCK_INIT;
	res_callee->seq = 0;
	return USER_STATUS_SUCCESS;
}

//! @brief Attempt to copy message contents with safety checks
//! @param dst Copy destination
//! @param src Copy source
//! @return API status
static inline int user_rpc_copy_contents_from_user(struct user_rpc_msg *dst,
                                                   const struct user_rpc_msg *src) {
	uint32_t len = src->len;
	if (len > USER_RPC_MAX_PAYLOAD_SIZE) {
		return USER_STATUS_INVALID_MSG;
	}
	dst->len = len;
	memcpy(dst->payload, src->payload, len);
	return USER_STATUS_SUCCESS;
}

//! @brief Attempt to copy message contents without safety checks
//! @param dst Copy destination
//! @param src Copy source
static inline void user_rpc_copy_contents_from_kernel(struct user_rpc_msg *dst,
                                                      const struct user_rpc_msg *src) {
	dst->len = src->len;
	memcpy(dst->payload, src->payload, dst->len);
}

//! @brief Initiate RPC
//! @param caller Pointer to the caller
//! @param token Pointer to the token
//! @param msg RPC arguments
int user_rpc_initiate(struct user_rpc_caller *caller, const struct user_rpc_token *token,
                      const struct user_rpc_msg *msg) {
	const bool int_state = thread_spinlock_lock(&caller->lock);
	ASSERT(!caller->is_shut_down, "Caller is shutdown while the ref to it is borrowed");
	// Try to allocate container from free containers pool
	struct user_rpc_container *container =
	    QUEUE_DEQUEUE(&caller->free_containers, struct user_rpc_container, qnode);
	if (container == NULL) {
		// If this fails, try to allocate on the heap
		container = mem_heap_alloc(sizeof(struct user_rpc_container));
		if (container == NULL) {
			thread_spinlock_unlock(&caller->lock, int_state);
			return USER_STATUS_OUT_OF_MEMORY;
		}
	}
	// Try to copy the message
	int status = user_rpc_copy_contents_from_user(&container->message, msg);
	if (status != USER_STATUS_SUCCESS) {
		QUEUE_ENQUEUE(&caller->free_containers, container, qnode);
		thread_spinlock_unlock(&caller->lock, int_state);
		return status;
	}
	// Borrow caller
	MEM_REF_BORROW(&caller->dealloc_rc_base);
	container->caller = caller;
	thread_spinlock_ungrab(&caller->lock);
	// Save client's opaque value
	container->client_opaque = msg->opaque;
	// Copy status value
	container->message.status = msg->status;
	// Lock callee
	struct user_rpc_callee *callee = CONTAINER_OF(token, struct user_rpc_callee, token);
	thread_spinlock_grab(&callee->lock);
	// Check if callee has been shutdown
	if (callee->is_shut_down) {
		// Unlock callee and lock caller
		thread_spinlock_ungrab(&callee->lock);
		thread_spinlock_grab(&caller->lock);
		// We still hold the reference to the caller
		ASSERT(!caller->is_shut_down, "Caller is shutdown while the ref to it is borrowed");
		QUEUE_ENQUEUE(&caller->free_containers, container, qnode);
		thread_spinlock_unlock(&caller->lock, int_state);
		return USER_STATUS_TARGET_UNREACHABLE;
	}
	// Enqueue in callee incoming queue
	QUEUE_ENQUEUE(&callee->incoming_rpcs, container, qnode);
	// Raise notification
	user_send_notification(callee->on_incoming_raiser);
	thread_spinlock_unlock(&callee->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Accept RPC
//! @param callee Pointer to the callee
//! @param msg Buffer to store RPC arguments in
int user_rpc_accept(struct user_rpc_callee *callee, struct user_rpc_msg *msg) {
	const bool int_state = thread_spinlock_lock(&callee->lock);
	ASSERT(!callee->is_shut_down, "Callee is shutdown while the ref to it is borrowed");
	struct user_rpc_container *container =
	    QUEUE_DEQUEUE(&callee->incoming_rpcs, struct user_rpc_container, qnode);
	if (container == NULL) {
		thread_spinlock_unlock(&callee->lock, int_state);
		return USER_STATUS_EMPTY;
	}
	size_t seq = callee->seq++;
	container->inode.key = seq;
	intmap_insert(&callee->awaiting_reply, &container->inode);
	user_rpc_copy_contents_from_kernel(msg, &container->message);
	msg->opaque = seq;
	msg->status = container->message.status;
	thread_spinlock_unlock(&callee->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Return from RPC
//! @param callee Pointer to the callee
//! @param msg Buffer to store RPC result in
int user_rpc_return(struct user_rpc_callee *callee, const struct user_rpc_msg *msg) {
	bool int_state = thread_spinlock_lock(&callee->lock);
	ASSERT(!callee->is_shut_down, "Callee is shutdown while the ref to it is borrowed");
	struct user_rpc_container *container = CONTAINER_OF_NULLABLE(
	    intmap_search(&callee->awaiting_reply, msg->opaque), struct user_rpc_container, inode);
	if (container == NULL) {
		thread_spinlock_unlock(&callee->lock, int_state);
		return USER_STATUS_INVALID_RPC_ID;
	}
	int status = user_rpc_copy_contents_from_user(&container->message, msg);
	if (status != USER_STATUS_SUCCESS) {
		thread_spinlock_unlock(&callee->lock, int_state);
		return status;
	}
	container->message.status = msg->status;
	intmap_remove(&callee->awaiting_reply, &container->inode);
	thread_spinlock_unlock(&callee->lock, int_state);
	user_rpc_enqueue_reply_container(container);
	return USER_STATUS_SUCCESS;
}

//! @brief Get RPC result
//! @param caller Pointer to the caller
//! @param msg Buffer to store RPC result in
int user_rpc_get_result(struct user_rpc_caller *caller, struct user_rpc_msg *msg) {
	bool int_state = thread_spinlock_lock(&caller->lock);
	ASSERT(!caller->is_shut_down, "Callee is shutdown while the ref to it is borrowed");
	struct user_rpc_container *container =
	    QUEUE_DEQUEUE(&caller->incoming_replies, struct user_rpc_container, qnode);
	if (container == NULL) {
		thread_spinlock_unlock(&caller->lock, int_state);
		return USER_STATUS_EMPTY;
	}
	user_rpc_copy_contents_from_kernel(msg, &container->message);
	msg->opaque = container->client_opaque;
	msg->status = container->message.status;
	QUEUE_ENQUEUE(&caller->free_containers, container, qnode);
	thread_spinlock_unlock(&caller->lock, int_state);
	return USER_STATUS_SUCCESS;
}
