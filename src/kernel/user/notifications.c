//! @file notifications.c
//! @brief File containing notifications API implementation

#include <lib/containerof.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/queue.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/rc.h>
#include <thread/locking/spinlock.h>
#include <thread/smp/core.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/task.h>
#include <user/notifications.h>

MODULE("user/notifications")

//! @brief Local/global queue. Used as one global queue if is_per_cpu is false and as many local
//! queues if is_per_cpu is true
union user_local_global_queue {
	//! @brief Global wait queue
	struct queue global_queue;
	//! @brief Local wait queues
	struct queue *local_queues;
};

//! @brief Wait queue nodes
struct user_wait_queue_node {
	//! @brief Queue node
	struct queue_node node;
	//! @brief Pointer to the task
	struct thread_task *task;
	//! @brief Pointer to the raiser channel (set by waker)
	struct user_raiser_channel *channel;
};

//! @brief Notification raiser channel
struct user_raiser_channel {
	//! @brief Pointer to the owner
	struct user_raiser *owner;
	//! @brief Number of times notifications unhandled
	size_t pending;
	//! @brief Queue node
	struct queue_node node;
};

//! @brief Notifications raiser. Allows to raise notifications
struct user_raiser {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Notification
	struct user_notification notification;
	//! @brief Mailbox reference
	struct user_mailbox *mailbox_ref;
	union {
		//! @brief Single channel for global mailboxes
		struct user_raiser_channel global_channel;
		//! @brief Pointer to the array of per-cpu channels
		struct user_raiser_channel *local_channels;
	};
};

//! @brief Notifications mailbox. Allows to recieve notifications
struct user_mailbox {
	//! @brief Shutdown RC base
	struct mem_rc shutdown_rc_base;
	//! @brief Dealloc RC base
	struct mem_rc dealloc_rc_base;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief Message local/global queue
	union user_local_global_queue msg_queue;
	//! @brief Task local/global queue
	union user_local_global_queue task_queue;
	//! @brief True if mailbox has been shutdown
	bool is_shut_down;
	//! @brief True if mailbox has been created in per-cpu mode
	bool is_per_cpu;
};

//! @brief Initialize local/global queue
//! @param queue Pointer to the queue
//! @param is_per_cpu True if queues will be used in per-cpu mode
//! @return True if queue initialization was successful, false if OOM was returned instead
static bool user_local_global_init(union user_local_global_queue *queue, bool is_per_cpu) {
	if (is_per_cpu) {
		size_t core_count = thread_smp_core_max_cpus;
		struct queue *queues = mem_heap_alloc(sizeof(struct queue_node) * core_count);
		if (queues == NULL) {
			return false;
		}
		for (size_t i = 0; i < core_count; ++i) {
			queues[i] = QUEUE_INIT;
		}
		queue->local_queues = queues;
	} else {
		queue->global_queue = QUEUE_INIT;
	}
	return true;
}

//! @brief Enqueue node in local/global queue
//! @param queue Pointer to the queue
//! @param node Queue node
//! @param is_per_cpu True if queue is used in per-cpu mode
//! @param id Core index
static void user_local_global_enqueue(union user_local_global_queue *queue, struct queue_node *node,
                                      bool is_per_cpu, size_t id) {
	if (is_per_cpu) {
		queue_enqueue(queue->local_queues + id, node);
	} else {
		queue_enqueue(&queue->global_queue, node);
	}
}

//! @brief Dequeue node from local/global queue
//! @param queue Pointer to the queue
//! @param is_per_cpu True if queue is used in per-cpu mode
//! @param id Core index
static struct queue_node *user_local_global_dequeue(union user_local_global_queue *queue,
                                                    bool is_per_cpu, size_t id) {
	if (is_per_cpu) {
		return queue_dequeue(queue->local_queues + id);
	}
	return queue_dequeue(&queue->global_queue);
}

//! @brief Deinitialize local/global queue
//! @param queue Pointer to the queue
//! @param is_per_cpu True if queue is used in per-cpu mode
static void user_local_global_deinit(union user_local_global_queue *queue, bool is_per_cpu) {
	if (is_per_cpu) {
		mem_heap_free(queue->local_queues, sizeof(struct queue) * thread_smp_core_max_cpus);
	}
}

//! @brief Shutdown mailbox
//! @param mailbox Pointer to the mailbox
static void user_shutdown_mailbox(struct user_mailbox *mailbox) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	mailbox->is_shut_down = true;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	struct queue_node *node;
	if (mailbox->is_per_cpu) {
		for (uint32_t i = 0; i < thread_smp_core_max_cpus; ++i) {
			while ((node = user_local_global_dequeue(&mailbox->msg_queue, true, i)) != NULL) {
				struct user_raiser *raiser =
				    CONTAINER_OF(node, struct user_raiser_channel, node)->owner;
				MEM_REF_DROP(raiser);
			}
		}
	} else {
		while ((node = user_local_global_dequeue(&mailbox->task_queue, false, 0)) != NULL) {
			struct user_raiser *raiser =
			    CONTAINER_OF(node, struct user_raiser_channel, node)->owner;
			MEM_REF_DROP(raiser);
		}
	}
	MEM_REF_DROP(&mailbox->dealloc_rc_base);
}

//! @brief Destroy mailbox
//! @param dealloc_rc_base Pointer to the mailbox's dealloc_rc_base
static void user_destroy_mailbox(struct mem_rc *dealloc_rc_base) {
	struct user_mailbox *maillbox =
	    CONTAINER_OF(dealloc_rc_base, struct user_mailbox, dealloc_rc_base);
	user_local_global_deinit(&maillbox->task_queue, maillbox->is_per_cpu);
	user_local_global_deinit(&maillbox->msg_queue, maillbox->is_per_cpu);
}

//! @brief Create mailbox
//! @param mailbox Buffer to store pointer to the new mailbox in
//! @param percpu True if waiting threads should only be able to recieve notifications from the
//! same CPU
//! @return API status
int user_create_mailbox(struct user_mailbox **mailbox, bool percpu) {
	struct user_mailbox *res_mailbox = mem_heap_alloc(sizeof(struct user_mailbox));
	if (res_mailbox == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	if (!user_local_global_init(&res_mailbox->task_queue, percpu)) {
		mem_heap_free(res_mailbox, sizeof(struct user_mailbox));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	if (!user_local_global_init(&res_mailbox->msg_queue, percpu)) {
		user_local_global_deinit(&res_mailbox->task_queue, percpu);
		mem_heap_free(res_mailbox, sizeof(struct user_mailbox));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	res_mailbox->lock = THREAD_SPINLOCK_INIT;
	res_mailbox->is_per_cpu = percpu;
	res_mailbox->is_shut_down = false;
	MEM_REF_INIT(&res_mailbox->shutdown_rc_base, user_shutdown_mailbox);
	MEM_REF_INIT(&res_mailbox->dealloc_rc_base, user_destroy_mailbox);
	*mailbox = res_mailbox;
	return USER_STATUS_SUCCESS;
}

//! @brief Destroy raiser
//! @param raiser Pointer to the raiser
static void user_destroy_raiser(struct user_raiser *raiser) {
	bool is_per_cpu = raiser->mailbox_ref->is_per_cpu;
	if (is_per_cpu) {
		mem_heap_free(raiser->local_channels,
		              sizeof(struct user_raiser_channel) * thread_smp_core_max_cpus);
	}
	MEM_REF_DROP(raiser->mailbox_ref);
	mem_heap_free(raiser, sizeof(struct user_raiser));
}

//! @brief Create raiser
//! @param mailbox Pointer to the mailbox
//! @param raiser Buffer to store pointer to the raiser in
//! @param notification Notification to send
//! @return API status
int user_create_raiser(struct user_mailbox *mailbox, struct user_raiser **raiser,
                       struct user_notification notification) {
	struct user_raiser *res_raiser = mem_heap_alloc(sizeof(struct user_raiser));
	if (res_raiser == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	if (mailbox->is_per_cpu) {
		res_raiser->local_channels =
		    mem_heap_alloc(sizeof(struct user_raiser_channel) * thread_smp_core_max_cpus);
		if (res_raiser->local_channels == NULL) {
			mem_heap_free(res_raiser, sizeof(struct user_raiser));
			return USER_STATUS_OUT_OF_MEMORY;
		}
		for (uint32_t i = 0; i < thread_smp_core_max_cpus; ++i) {
			res_raiser->local_channels[i].pending = 0;
			res_raiser->local_channels[i].owner = res_raiser;
		}
	} else {
		res_raiser->global_channel.pending = 0;
		res_raiser->global_channel.owner = res_raiser;
	}
	MEM_REF_INIT(&res_raiser->rc_base, user_destroy_raiser);
	res_raiser->mailbox_ref = MEM_REF_BORROW(mailbox);
	res_raiser->notification = notification;
	*raiser = res_raiser;
	return USER_STATUS_SUCCESS;
}

//! @brief Enqueue notification
//! @param raiser Pointer to raiser
//! @param id Target core id
//! @note Mailbox (raiser->mailbox) should be locked in advance
static void user_raiser_enqueue_nolock(struct user_raiser *raiser, uint32_t id) {
	MEM_REF_BORROW(raiser);
	struct user_mailbox *mailbox = raiser->mailbox_ref;
	struct queue_node *wait_node;
	if ((wait_node = user_local_global_dequeue(&mailbox->task_queue, mailbox->is_per_cpu, id)) !=
	    NULL) {
		struct user_wait_queue_node *node =
		    CONTAINER_OF(wait_node, struct user_wait_queue_node, node);
		if (mailbox->is_per_cpu) {
			node->channel = raiser->local_channels + id;
		} else {
			node->channel = &raiser->global_channel;
		}
		thread_localsched_wake_up(node->task);
	} else if (mailbox->is_per_cpu) {
		user_local_global_enqueue(&mailbox->msg_queue, &raiser->local_channels[id].node, true, id);
	} else {
		user_local_global_enqueue(&mailbox->msg_queue, &raiser->global_channel.node, false, id);
	}
}

//! @brief Send notification with core id hint
//! @param raiser Pointer to the raiser
//! @param id Target core id
void user_send_notification_to_core(struct user_raiser *raiser, uint32_t id) {
	struct user_mailbox *mailbox = raiser->mailbox_ref;
	const bool is_per_cpu = mailbox->is_per_cpu;
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return;
	}
	if (is_per_cpu) {
		raiser->local_channels[id].pending++;
		if (raiser->local_channels[id].pending == 1) {
			user_raiser_enqueue_nolock(raiser, id);
		}
	} else {
		raiser->global_channel.pending++;
		if (raiser->global_channel.pending == 1) {
			user_raiser_enqueue_nolock(raiser, id);
		}
	}
	thread_spinlock_unlock(&mailbox->lock, int_state);
}

//! @brief Send notification
//! @param raiser Pointer to the raiser
void user_send_notification(struct user_raiser *raiser) {
	// Prevent migration by elevating interrupt level
	const bool int_state = intlevel_elevate();
	user_send_notification_to_core(raiser, PER_CPU(logical_id));
	intlevel_recover(int_state);
}

//! @brief Recieve notification
//! @param mailbox Mailbox to recieve notification from
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_recieve_notification(struct user_mailbox *mailbox, struct user_notification *buf) {
	const bool is_per_cpu = mailbox->is_per_cpu;
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	uint32_t id = PER_CPU(logical_id);
	struct queue_node *node;
	struct user_raiser_channel *channel;
	if ((node = user_local_global_dequeue(&mailbox->msg_queue, is_per_cpu, id)) != NULL) {
		channel = CONTAINER_OF(node, struct user_raiser_channel, node);
	} else {
		struct user_wait_queue_node node;
		node.task = thread_localsched_get_current_task();
		user_local_global_enqueue(&mailbox->task_queue, &node.node, is_per_cpu, id);
		thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &mailbox->lock));
		thread_spinlock_grab(&mailbox->lock);
		channel = node.channel;
	}
	struct user_raiser *raiser = channel->owner;
	channel->pending -= 1;
	if (channel->pending != 0) {
		// Re-enqueue to handle notification again
		user_raiser_enqueue_nolock(raiser, id);
	}
	*buf = raiser->notification;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	MEM_REF_DROP(raiser);
	return USER_STATUS_SUCCESS;
}
