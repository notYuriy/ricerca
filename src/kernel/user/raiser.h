//! @file raiser.h
//! @brief File containing helpers for raising notifications

#pragma once

#include <user/notifications.h>
#include <user/status.h>

//! @brief Notifications raiser
struct user_raiser {
	//! @brief Pointer to the mailbox
	struct user_mailbox *mailbox;
	//! @brief Number of notifications raised
	size_t raised;
	//! @brief Number of notifications acked
	size_t acked;
	//! @brief Notification to send
	struct user_notification template;
};

//! @brief Initialize notifications raiser
//! @param raiser Pointer to the raiser
//! @param mailbox Pointer to the mailbox that will recieve notifications
//! @param template Notification that will be sent on raise
//! @return API status
int user_raiser_init(struct user_raiser *raiser, struct user_mailbox *mailbox,
                     struct user_notification template);

//! @brief Raise notification
//! @param raiser Pointer to the raiser
void user_raiser_raise(struct user_raiser *raiser);

//! @brief Acknowledge notification
//! @param raiser Pointer to the raiser
void user_raiser_ack(struct user_raiser *raiser);

//! @brief Deinitialize notification raiser
//! @param raiser Pointer to the raiser
void user_raiser_deinit(struct user_raiser *raiser);
