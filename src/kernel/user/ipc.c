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

//! @brief IPC control token. Used to control token quota
struct user_ipc_control_token {
	//! @brief Refcounting base
	struct mem_rc rc_base;
	//! @brief Max quota
	size_t max_quota;
	//! @brief Current quota
	size_t current_quota;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief Owning mailbox reference
	struct user_ipc_mailbox *owner;
	//! @brief Stored opaque value
	size_t opaque;
	//! @brief True if token has been shutdown
	bool is_shut_down;
	//! @brief True if token has been lost
	bool is_lost;
};

//! @brief IPC token. Used as a target of send operation
struct user_ipc_token {
	//! @brief Refcounting base
	struct mem_rc rc_base;
	// Pointer to the control token
	struct user_ipc_control_token *owner;
};

//! @brief Message queue node
struct user_ipc_msg_queue_node {
	//! @brief Queue node
	struct queue_node node;
};

//! @brief Waiting thread queue node
struct user_ipc_task_queue_node {
	//! @brief Queue node
	struct queue_node node;
	//! @brief Pointer to the waiting task
	struct thread_task *task;
	//! @brief Pointer to the recieved message
	struct user_ipc_message *msg;
};

//! @brief IPC mailbox. Used to recieve messages from tokens
struct user_ipc_mailbox {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief Max message quota
	size_t max_quota;
	//! @brief Current message quota
	size_t current_quota;
	//! @brief Message buffers
	struct user_ipc_message *messages;
	//! @brief Message nodes
	struct user_ipc_msg_queue_node *msg_nodes;
	//! @brief Free message queue
	struct queue free_queue;
	//! @brief Pending message queue
	struct queue pending_queue;
	//! @brief Pending waiting task queue
	struct queue task_queue;
	//! @brief True if mailbox was shutdown
	bool is_shut_down;
};

//! @brief Destroy mailbox
//! @param mailbox Pointer to the mailbox
//! @param opaque Ignored
static void user_ipc_destroy_mailbox(struct user_ipc_mailbox *mailbox, void *opaque) {
	(void)opaque;
	mem_heap_free(mailbox->msg_nodes, mailbox->max_quota * sizeof(struct user_ipc_msg_queue_node));
	mem_heap_free(mailbox->messages, mailbox->max_quota * sizeof(struct user_ipc_message));
	mem_heap_free(mailbox, sizeof(struct user_ipc_mailbox));
}

//! @brief Create mailbox
//! @param quota Mailbox quota
//! @param mailbox Buffer to store reference to the mailbox
//! @return API error
int user_ipc_create_mailbox(size_t quota, struct user_ipc_mailbox **mailbox) {
	// Allocate mailbox object, message buffers and message queue nodes
	struct user_ipc_mailbox *result = mem_heap_alloc(sizeof(struct user_ipc_mailbox));
	if (result == NULL) {
	}
	struct user_ipc_message *messages = mem_heap_alloc(quota * sizeof(struct user_ipc_message));
	if (messages == NULL) {
		goto free_result;
	}
	struct user_ipc_msg_queue_node *msg_nodes =
	    mem_heap_alloc(quota * sizeof(struct user_ipc_msg_queue_node));
	if (msg_nodes == NULL) {
		goto free_messages;
	}
	// Fill out mailbox fields
	MEM_REF_INIT(result, user_ipc_destroy_mailbox, NULL);
	result->lock = THREAD_SPINLOCK_INIT;
	result->max_quota = quota;
	result->messages = messages;
	result->msg_nodes = msg_nodes;
	result->free_queue = result->pending_queue = result->task_queue = QUEUE_INIT;
	result->is_shut_down = false;
	*mailbox = result;
	// Enqueue message buffers
	for (size_t i = 0; i < quota; ++i) {
		QUEUE_ENQUEUE(&result->free_queue, result->msg_nodes + i, node);
	}
	return USER_STATUS_SUCCESS;
free_messages:
	mem_heap_free(messages, quota * sizeof(struct user_ipc_message));
free_result:
	mem_heap_free(result, sizeof(struct user_ipc_mailbox));
	return USER_STATUS_OUT_OF_MEMORY;
}

//! @brief Convert message buffer pointer to message node pointer
//! @param mailbox Owning mailbox
//! @param buf Message buffer pointer
//! @return Message node pointer
static inline struct user_ipc_msg_queue_node *user_ipc_buf_to_node(struct user_ipc_mailbox *mailbox,
                                                                   struct user_ipc_message *buf) {
	size_t index = buf - mailbox->messages;
	return mailbox->msg_nodes + index;
}

//! @brief Convert message node pointer to message buffer pointer
//! @param mailbox Owning mailbox
//! @param node Message node pointer
//! @return Message buffer pointer
static inline struct user_ipc_message *user_ipc_node_to_buf(struct user_ipc_mailbox *mailbox,
                                                            struct user_ipc_msg_queue_node *node) {
	size_t index = node - mailbox->msg_nodes;
	return mailbox->messages + index;
}

//! @brief Send message to the mailbox with preallocated msg buffer
//! @param mailbox Mailbox to send message to
//! @param msg Message to send
static void user_ipc_send_message_prealloc(struct user_ipc_mailbox *mailbox,
                                           struct user_ipc_message *msg) {
	struct user_ipc_msg_queue_node *msg_node = user_ipc_buf_to_node(mailbox, msg);
	struct user_ipc_task_queue_node *task_node;
	if ((task_node = QUEUE_DEQUEUE(&mailbox->task_queue, struct user_ipc_task_queue_node, node)) !=
	    NULL) {
		task_node->msg = msg;
		thread_localsched_wake_up(task_node->task);
	} else {
		QUEUE_ENQUEUE(&mailbox->pending_queue, msg_node, node);
	}
}

//! @brief Destroy control token
//! @param tok Pointer to the token
//! @param opaque Unused
static void user_ipc_destroy_control_token(struct user_ipc_control_token *ctrl, void *opaque) {
	(void)opaque;
	ASSERT(!ctrl->is_shut_down, "Token has been shut down already");
	ctrl->is_shut_down = true;

	const bool int_state = thread_spinlock_lock(&ctrl->lock);
	bool dealloc = ctrl->is_lost;
	thread_spinlock_unlock(&ctrl->lock, int_state);

	if (dealloc) {
		MEM_REF_DROP(ctrl->owner);
		mem_heap_free(ctrl, sizeof(struct user_ipc_control_token));
	}
}

//! @brief Destroy token
//! @param tok Pointer to the token
//! @param opaque Unused
static void user_ipc_destroy_token(struct user_ipc_token *tok, void *opaque) {
	(void)opaque;
	struct user_ipc_control_token *ctrl = tok->owner;
	const bool int_state = thread_spinlock_lock(&ctrl->lock);
	ASSERT(!ctrl->is_lost, "Token has been lost already");
	ctrl->is_lost = true;

	thread_spinlock_grab(&ctrl->owner->lock);
	// Free quota in the mailbox
	if (!ctrl->owner->is_shut_down) {
		ctrl->owner->current_quota += ctrl->max_quota;
		if (!ctrl->is_shut_down) {
			// Send ping of death
			struct user_ipc_msg_queue_node *free_node =
			    QUEUE_DEQUEUE(&ctrl->owner->free_queue, struct user_ipc_msg_queue_node, node);
			ASSERT(free_node != NULL, "Failed to allocate slot for the ping of death");
			struct user_ipc_message *free_msg = user_ipc_node_to_buf(ctrl->owner, free_node);
			free_msg->opaque = ctrl->opaque;
			free_msg->type = USER_IPC_MSG_TYPE_TOKEN_UNREACHABLE;
			user_ipc_send_message_prealloc(ctrl->owner, free_msg);
		} else {
			// Space for ping of death can be reused for something else
			ctrl->owner->current_quota++;
		}
	}
	thread_spinlock_ungrab(&tok->owner->owner->lock);

	const bool dealloc_ctrl = ctrl->is_shut_down;
	thread_spinlock_unlock(&tok->owner->lock, int_state);
	mem_heap_free(tok, sizeof(struct user_ipc_token));
	if (dealloc_ctrl) {
		MEM_REF_DROP(ctrl->owner);
		mem_heap_free(ctrl, sizeof(struct user_ipc_control_token));
	}
}

//! @brief Create token pair
//! @param mailbox Owning mailbox
//! @param quota Token quota
//! @param token Buffer to store reference to the token
//! @param ctrl Buffer to store reference to the control token
//! @param opaque Opaque value to be stored inside the token
//! @return API error
int user_ipc_create_token_pair(struct user_ipc_mailbox *mailbox, size_t quota,
                               struct user_ipc_token **token, struct user_ipc_control_token **ctrl,
                               size_t opaque) {
	int error;
	// Allocate token object
	struct user_ipc_token *res_token = mem_heap_alloc(sizeof(struct user_ipc_token));
	if (res_token == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	// Allocate control token object
	struct user_ipc_control_token *res_ctrl = mem_heap_alloc(sizeof(struct user_ipc_control_token));
	if (res_ctrl == NULL) {
		error = USER_STATUS_OUT_OF_MEMORY;
		goto free_token;
	}
	// Allocate quota
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->current_quota < quota + 1) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		error = USER_STATUS_QUOTA_EXCEEDED;
		goto free_control_token;
	}
	mailbox->current_quota -= (quota + 1);
	thread_spinlock_unlock(&mailbox->lock, int_state);
	// Init refcounting
	MEM_REF_INIT(res_ctrl, user_ipc_destroy_control_token, NULL);
	MEM_REF_INIT(res_token, user_ipc_destroy_token, NULL);
	// Fill out fields
	res_ctrl->max_quota = quota;
	res_ctrl->current_quota = quota;
	res_ctrl->lock = THREAD_SPINLOCK_INIT;
	res_ctrl->owner = MEM_REF_BORROW(mailbox);
	res_ctrl->opaque = opaque;
	res_ctrl->is_shut_down = false;
	res_ctrl->is_lost = false;
	res_token->owner = res_ctrl;
	*token = res_token;
	*ctrl = res_ctrl;
	return USER_STATUS_SUCCESS;
free_control_token:
	mem_heap_free(res_ctrl, sizeof(struct user_ipc_control_token));
free_token:
	mem_heap_free(res_token, sizeof(struct user_ipc_token));
	return error;
}

//! @brief Send message to the token
//! @param token Target token
//! @param message Message to send
//! @return API error
int user_ipc_send(struct user_ipc_token *token, struct user_ipc_message *message) {
	struct user_ipc_control_token *ctrl = token->owner;
	const bool int_state = thread_spinlock_lock(&ctrl->lock);
	if (ctrl->is_shut_down) {
		thread_spinlock_unlock(&ctrl->lock, int_state);
		return USER_STATUS_MAILBOX_UNREACHABLE;
	}
	if (ctrl->current_quota == 0) {
		thread_spinlock_unlock(&ctrl->lock, int_state);
		return USER_STATUS_QUOTA_EXCEEDED;
	}
	struct user_ipc_mailbox *mailbox = ctrl->owner;
	thread_spinlock_grab(&mailbox->lock);
	if (ctrl->owner->is_shut_down) {
		thread_spinlock_ungrab(&mailbox->lock);
		thread_spinlock_unlock(&ctrl->lock, int_state);
		return USER_STATUS_MAILBOX_UNREACHABLE;
	}
	ctrl->current_quota--;
	// Send message
	struct user_ipc_msg_queue_node *free_node =
	    QUEUE_DEQUEUE(&ctrl->owner->free_queue, struct user_ipc_msg_queue_node, node);
	ASSERT(free_node != NULL, "Failed to allocate slot for the message");
	struct user_ipc_message *free_msg = user_ipc_node_to_buf(ctrl->owner, free_node);
	memcpy(free_msg->payload, message->payload, USER_IPC_MESSAGE_PAYLOAD_SIZE);
	free_msg->token = MEM_REF_BORROW(ctrl);
	free_msg->type = USER_IPC_MSG_TYPE_REGULAR;
	free_msg->index = free_node - ctrl->owner->msg_nodes;
	user_ipc_send_message_prealloc(mailbox, free_msg);
	thread_spinlock_ungrab(&mailbox->lock);
	thread_spinlock_unlock(&ctrl->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Recieve message from the mailbox
//! @param mailbox Mailbox to recieve messages from
//! @param message Buffer to recieve the message in
//! @return API error
int user_ipc_recieve(struct user_ipc_mailbox *mailbox, struct user_ipc_message *recvbuf) {
	// Check if mailbox is shutdown
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_MAILBOX_SHUTDOWN;
	}
	// Check if there is a message waiting for us
	struct user_ipc_msg_queue_node *incoming_node;
	struct user_ipc_message *incoming_buf;
	if ((incoming_node = QUEUE_DEQUEUE(&mailbox->pending_queue, struct user_ipc_msg_queue_node,
	                                   node)) == NULL) {
		// If not, sleep
		struct user_ipc_task_queue_node wait_node;
		wait_node.task = thread_localsched_get_current_task();
		QUEUE_ENQUEUE(&mailbox->task_queue, &wait_node, node);
		thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &mailbox->lock));
		// We got a message! Or maybe not. Check if mailbox has been shutdown first
		thread_spinlock_grab(&mailbox->lock);
		if (mailbox->is_shut_down) {
			thread_spinlock_ungrab(&mailbox->lock);
			return USER_STATUS_MAILBOX_SHUTDOWN;
		}
		incoming_buf = wait_node.msg;
	} else {
		incoming_buf = user_ipc_node_to_buf(mailbox, incoming_node);
	}
	// Copy index, type and payload fields
	memcpy(recvbuf->payload, incoming_buf->payload, USER_IPC_MESSAGE_PAYLOAD_SIZE);
	recvbuf->type = incoming_buf->type;
	recvbuf->index = incoming_buf->index;
	// Handle opaque value based on message type
	switch (recvbuf->type) {
	case USER_IPC_MSG_TYPE_TOKEN_UNREACHABLE: {
		// Immediatelly ack the message and free message slot
		recvbuf->opaque = incoming_buf->opaque;
		incoming_buf->type = USER_IPC_MSG_TYPE_NONE;
		mailbox->current_quota += 1;
		QUEUE_ENQUEUE(&mailbox->free_queue, incoming_node, node);
		break;
	}
	default: {
		// Don't ack message for now, user has to do it with user_ipc_ack manually
		struct user_ipc_control_token *ctrl = incoming_buf->token;
		recvbuf->opaque = ctrl->opaque;
	}
	}
	thread_spinlock_unlock(&mailbox->lock, int_state);
	return USER_STATUS_SUCCESS;
}
//! @brief Ack message at index
//! @param mailbox Mailbox to which message has been sent
//! @param index Index of the message
//! @return API error
int user_ipc_ack(struct user_ipc_mailbox *mailbox, size_t index) {
	// Check if mailbox is shutdown
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_MAILBOX_SHUTDOWN;
	}
	if (index >= mailbox->max_quota) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_OUT_OF_BOUNDS;
	}
	if (mailbox->messages[index].type != USER_IPC_MSG_TYPE_REGULAR) {
		return USER_STATUS_INACTIVE_MESSAGE_SLOT;
	}
	struct user_ipc_control_token *ctrl = mailbox->messages[index].token;
	QUEUE_ENQUEUE(&mailbox->free_queue, mailbox->msg_nodes + index, node);
	mailbox->messages[index].type = USER_IPC_MSG_TYPE_NONE;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	thread_spinlock_grab(&ctrl->lock);
	ctrl->current_quota++;
	thread_spinlock_ungrab(&ctrl->lock);
	MEM_REF_DROP(ctrl);
	return USER_STATUS_SUCCESS;
}

//! @brief Shutdown mailbox
//! @param mailbox Pointer to the mailbox
void user_ipc_shutdown_mailbox(struct user_ipc_mailbox *mailbox) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	ASSERT(!mailbox->is_shut_down, "Mailbox has already been shutdown");
	mailbox->is_shut_down = true;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	struct user_ipc_task_queue_node *task_node;
	// Wake up threads polling on the mailbox
	while (
	    (task_node = QUEUE_DEQUEUE(&mailbox->task_queue, struct user_ipc_task_queue_node, node))) {
		thread_localsched_wake_up(task_node->task);
	}
	// Drop all referenced tokens
	for (size_t i = 0; i < mailbox->max_quota; ++i) {
		if (mailbox->messages[i].type == USER_IPC_MSG_TYPE_REGULAR) {
			MEM_REF_DROP(mailbox->messages[i].token);
		}
	}
	MEM_REF_DROP(mailbox);
}
