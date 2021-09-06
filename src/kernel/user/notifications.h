//! @file notifications.h
//! @brief File containing declarations related to notifications API

#pragma once

#include <lib/queue.h>
#include <mem/rc.h>
#include <misc/attributes.h>
#include <misc/types.h>
#include <user/status.h>

//! @brief Notification type
enum
{
	//! @brief Incoming RPC call
	USER_NOTE_TYPE_RPC_INCOMING = 0,
	//! @brief RPC reply
	USER_NOTE_TYPE_RPC_REPLY = 1,
};

//! @brief One notification
struct user_notification {
	//! @brief Notification type
	uintptr_t type;
	//! @brief Opaque value supplied with notification
	uintptr_t opaque;
} attribute_packed;

//! @brief Notifications mailbox. Allows to recieve notifications
struct user_mailbox;

//! @brief Notifications raiser. Allows to raise notifications
struct user_raiser;

//! @brief Create mailbox
//! @param mailbox Buffer to store pointer to the new mailbox in
//! @param percpu True if waiting threads should only be able to recieve notifications from the same
//! CPU
//! @return API status
int user_create_mailbox(struct user_mailbox **mailbox, bool percpu);

//! @brief Recieve notification
//! @param mailbox Mailbox to recieve notification from
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_recieve_notification(struct user_mailbox *mailbox, struct user_notification *buf);

//! @brief Create raiser
//! @param mailbox Pointer to the mailbox
//! @param raiser Buffer to store pointer to the raiser in
//! @param notification Notification to send
//! @return API status
int user_create_raiser(struct user_mailbox *mailbox, struct user_raiser **raiser,
                       struct user_notification notification);

//! @brief Send notification
//! @param raiser Pointer to the raiser
void user_send_notification(struct user_raiser *raiser);

//! @brief Send notification with core id hint
//! @param raiser Pointer to the raiser
void user_send_notification_to_core(struct user_raiser *raiser, uint32_t id);
