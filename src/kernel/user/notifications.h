//! @file notifications.h
//! @brief File containing declarations related to notifications API

#pragma once

#include <misc/attributes.h>
#include <misc/types.h>
#include <user/status.h>

//! @brief Notification type
enum
{
	//! @brief New message for the stream has been recieved/other side of the stream has been closed
	USER_NOTE_TYPE_IPC_STREAM_UPDATE = 0,
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

//! @brief Create mailbox
//! @param mailbox Buffer to store pointer to the new mailbox in
//! @param quota Pending notification queue max size
//! @return API status
int user_create_mailbox(struct user_mailbox **mailbox, size_t quota);

//! @brief Reserve one slot in circular notification buffer and increment reference count
//! @param mailbox Target mailbox
//! @return API status
int user_reserve_mailbox_slot(struct user_mailbox *mailbox);

//! @brief Release one slot in circular notification buffer and decrement reference count
//! @param mailbox Target mailbox
void user_release_mailbox_slot(struct user_mailbox *mailbox);

//! @brief Send notification
//! @param mailbox Target mailbox
//! @param payload Pointer to the notification to send
//! @return API status
int user_send_notification(struct user_mailbox *mailbox, const struct user_notification *payload);

//! @brief Recieve notification
//! @param mailbox Mailbox to recieve notification from
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_recieve_notification(struct user_mailbox *mailbox, struct user_notification *buf);
