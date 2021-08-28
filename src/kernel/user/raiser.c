//! @file raiser.c
//! @brief File containing implementations of notification raiser functions

#include <user/notifications.h>
#include <user/raiser.h>

//! @brief Initialize notifications raiser
//! @param raiser Pointer to the raiser
//! @param mailbox Pointer to the mailbox that will recieve notifications
//! @param template Notification that will be sent on raise
//! @return API status
int user_raiser_init(struct user_raiser *raiser, struct user_mailbox *mailbox,
                     struct user_notification template) {
	user_reserve_mailbox_slot(mailbox);
	raiser->mailbox = mailbox;
	raiser->template = template;
	raiser->raised = 0;
	raiser->acked = 0;
	return USER_STATUS_SUCCESS;
}

//! @brief Raise notification
//! @param raiser Pointer to the raiser
void user_raiser_raise(struct user_raiser *raiser) {
	raiser->raised++;
	if (raiser->acked == raiser->raised - 1) {
		user_send_notification(raiser->mailbox, &raiser->template);
	}
}

//! @brief Acknowledge notification
//! @param raiser Pointer to the raiser
void user_raiser_ack(struct user_raiser *raiser) {
	if (raiser->acked == raiser->raised) {
		return;
	}
	raiser->acked++;
	if (raiser->acked < raiser->raised) {
		user_send_notification(raiser->mailbox, &raiser->template);
	}
}

//! @brief Deinitialize notification raiser
//! @param raiser Pointer to the raiser
void user_raiser_deinit(struct user_raiser *raiser) {
	user_release_mailbox_slot(raiser->mailbox);
}
