//! @file ipc.h
//! @brief Declarations related to IPC

#pragma once

#include <misc/attributes.h>
#include <misc/types.h>
#include <user/notifications.h>
#include <user/status.h>

//! @brief IPC message payload max size
#define USER_IPC_PAYLOAD_MAX 120

//! @brief IPC message
struct user_ipc_msg {
	//! @brief Message length
	size_t length;
	//! @brief Message payload
	char payload[USER_IPC_PAYLOAD_MAX];
} attribute_packed;

//! @brief IPC stream
struct user_ipc_stream;

//! @brief Create IPC stream
//! @param stream Buffer to store pointer to the new stream in
//! @param quota Max size of pending messages queue
//! @param mailbox Pointer to the mailbox for recieving notifications
//! @param opaque Opaque value to be passed in notifications
//! @return API status
//! @note Reference count of the returned stream is 1
int user_ipc_create_stream(struct user_ipc_stream **stream, size_t quota,
                           struct user_mailbox *mailbox, size_t opaque);

//! @brief Shutdown IPC stream on consumer side
//! @param stream Pointer to the stream
void user_ipc_shutdown_stream_consumer(struct user_ipc_stream *stream);

//! @brief Shutdown IPC stream on producer side
//! @param stream Pointer to the stream
void user_ipc_shutdown_stream_producer(struct user_ipc_stream *stream);

//! @brief Send signal over IPC stream
//! @param stream Pointer to the stream to send signal to
//! @return API status
int user_ipc_send_signal(struct user_ipc_stream *stream);

//! @brief Send message over IPC stream
//! @param stream Pointer to the stream to send message to
//! @param message Message to send
//! @return API status
int user_ipc_send_msg(struct user_ipc_stream *stream, const struct user_ipc_msg *msg);

//! @brief Attempt to recieve message over IPC stream
//! @param stream Pointer to the stream to recieve message from
//! @param buf Buffer to store message in
//! @return API status
int user_ipc_recieve_msg(struct user_ipc_stream *stream, struct user_ipc_msg *buf);
