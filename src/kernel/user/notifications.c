//! @file notifications.c
//! @brief File containing notifications API implementation

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/queue.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/rc.h>
#include <thread/locking/spinlock.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/task.h>
#include <user/notifications.h>

//! @brief Wait queue nodes
struct user_wait_queue_node {
	//! @brief Queue node
	struct queue_node node;
	//! @brief Pointer to the task
	struct thread_task *task;
	//! @brief Pointer to the notification buffer
	struct user_notification *notification;
	//! @brief True if mailbox has been shutdown
	bool shutdown;
};

//! @brief Notifications mailbox. Allows to recieve notifications
struct user_mailbox {
	//! @brief RC base
	struct mem_rc rc_base;
	//! @brief Lock
	struct thread_spinlock lock;
	//! @brief Notification buffers
	struct user_notification *notes;
	//! @brief Notification buffer head
	size_t head;
	//! @brief Notification buffer tail
	size_t tail;
	//! @brief Size of the notification pending queue
	size_t max_quota;
	//! @brief Remaining quota for new events
	size_t current_quota;
	//! @brief Sleep queue
	struct queue sleep_queue;
	//! @brief True if mailbox has been shutdown
	bool is_shut_down;
};

//! @brief Deallocate resources associated with the mailbox
//! @param mailbox Pointer to the mailbox
//! @param opaque Ignored
static void user_clear_mailbox(struct user_mailbox *mailbox, void *opaque) {
	(void)opaque;
	mem_heap_free(mailbox->notes, mailbox->max_quota * sizeof(struct user_notification));
	mem_heap_free(mailbox, sizeof(struct user_mailbox));
}

//! @brief Create mailbox
//! @param mailbox Buffer to store pointer to the new mailbox in
//! @param quota Pending notification queue max size
//! @return API status
int user_create_mailbox(struct user_mailbox **mailbox, size_t quota) {
	struct user_mailbox *res_mailbox = mem_heap_alloc(sizeof(struct user_mailbox));
	if (res_mailbox == NULL) {
		return USER_STATUS_OUT_OF_MEMORY;
	}
	MEM_REF_INIT(res_mailbox, user_clear_mailbox, NULL);
	res_mailbox->current_quota = quota;
	res_mailbox->head = 0;
	res_mailbox->is_shut_down = false;
	res_mailbox->lock = THREAD_SPINLOCK_INIT;
	res_mailbox->max_quota = quota;
	res_mailbox->sleep_queue = QUEUE_INIT;
	res_mailbox->tail = 0;
	res_mailbox->notes = mem_heap_alloc(quota * sizeof(struct user_notification));
	if (res_mailbox->notes == NULL) {
		mem_heap_free(mailbox, sizeof(struct user_mailbox));
		return USER_STATUS_OUT_OF_MEMORY;
	}
	*mailbox = res_mailbox;
	return USER_STATUS_SUCCESS;
}

//! @brief Destroy mailbox
//! @param mailbox Pointer to the mailbox
void user_destroy_mailbox(struct user_mailbox *mailbox) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	mailbox->is_shut_down = true;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	struct user_wait_queue_node *wait;
	while ((wait = QUEUE_DEQUEUE(&mailbox->sleep_queue, struct user_wait_queue_node, node)) !=
	       NULL) {
		wait->shutdown = true;
		thread_localsched_wake_up(wait->task);
	}
	MEM_REF_DROP(mailbox);
}

//! @brief Reserve one slot in circular notification buffer
//! @param mailbox Target mailbox
//! @return API status
int user_reserve_mailbox_slot(struct user_mailbox *mailbox) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_MAILBOX_SHUTDOWN;
	}
	if (mailbox->current_quota == 0) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_QUOTA_EXCEEDED;
	}
	mailbox->current_quota--;
	thread_spinlock_unlock(&mailbox->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Release one slot in circular notification buffer
//! @param mailbox Target mailbox
void user_release_mailbox_slot(struct user_mailbox *mailbox) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	mailbox->current_quota++;
	thread_spinlock_unlock(&mailbox->lock, int_state);
}

//! @brief Send notification
//! @param mailbox Target mailbox
//! @param payload Pointer to the notification to send
//! @return API status
int user_send_notification(struct user_mailbox *mailbox, const struct user_notification *payload) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_TARGET_UNREACHABLE;
	}
	struct user_wait_queue_node *wait;
	if ((wait = QUEUE_DEQUEUE(&mailbox->sleep_queue, struct user_wait_queue_node, node)) != NULL) {
		*wait->notification = *payload;
		wait->shutdown = false;
		thread_localsched_wake_up(wait->task);
	} else {
		mailbox->notes[mailbox->head % mailbox->max_quota] = *payload;
		mailbox->head += 1;
	}
	thread_spinlock_unlock(&mailbox->lock, int_state);
	return USER_STATUS_SUCCESS;
}

//! @brief Recieve notification
//! @param mailbox Mailbox to recieve notification from
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_recieve_notification(struct user_mailbox *mailbox, struct user_notification *buf) {
	const bool int_state = thread_spinlock_lock(&mailbox->lock);
	if (mailbox->is_shut_down) {
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_MAILBOX_SHUTDOWN;
	}
	if (mailbox->head != mailbox->tail) {
		*buf = mailbox->notes[mailbox->tail % mailbox->max_quota];
		mailbox->tail++;
		thread_spinlock_unlock(&mailbox->lock, int_state);
		return USER_STATUS_SUCCESS;
	}
	struct user_wait_queue_node wait;
	wait.notification = buf;
	wait.task = thread_localsched_get_current_task();
	QUEUE_ENQUEUE(&mailbox->sleep_queue, &wait, node);
	thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &mailbox->lock));
	int status = wait.shutdown ? USER_STATUS_MAILBOX_SHUTDOWN : USER_STATUS_SUCCESS;
	intlevel_recover(int_state);
	return status;
}