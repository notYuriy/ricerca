//! @file ipc.h
//! @brief Declarations related to IPC

#pragma once

#include <misc/attributes.h>
#include <misc/types.h>
#include <user/error.h>

//! @brief IPC token. Used as target of send operation.
struct user_ipc_token;

//! @brief IPC control token. Used to control token quota
struct user_ipc_control_token;

//! @brief IPC mailbox. Used to recieve messages from tokens
struct user_ipc_mailbox;

//! @brief Message types
enum
{
	//! @brief Empty marker
	USER_IPC_MSG_TYPE_NONE = 0,
	//! @brief Regular IPC message
	USER_IPC_MSG_TYPE_REGULAR = 1,
	//! @brief Token death IPC message.
	USER_IPC_MSG_TYPE_TOKEN_UNREACHABLE = 2,
};

//! @brief Size of IPC message payload
#define USER_IPC_MESSAGE_PAYLOAD_SIZE 48

//! @brief IPC message
struct user_ipc_message {
	union {
		//! @brief Control token pointer
		struct user_ipc_control_token *token;
		//! @brief Opaque value
		size_t opaque;
	};
	//! @brief Message index
	uint32_t index;
	//! @brief Message type
	uint32_t type;
	//! @brief Payload
	char payload[USER_IPC_MESSAGE_PAYLOAD_SIZE];
} attribute_packed;

//! @brief Create mailbox
//! @param quota Mailbox quota
//! @param mailbox Buffer to store reference to the mailbox
//! @return API error
int user_ipc_create_mailbox(uint32_t quota, struct user_ipc_mailbox **mailbox);

//! @brief Create token pair
//! @param mailbox Owning mailbox
//! @param quota Token quota
//! @param token Buffer to store reference to the token
//! @param ctrl Buffer to store reference to the control token
//! @param opaque Opaque value to be stored inside the token
//! @return API error
int user_ipc_create_token_pair(struct user_ipc_mailbox *mailbox, uint32_t quota,
                               struct user_ipc_token **token, struct user_ipc_control_token **ctrl,
                               size_t opaque);

//! @brief Send message to the token
//! @param token Target token
//! @param message Message to send
//! @return API error
int user_ipc_send(struct user_ipc_token *token, struct user_ipc_message *message);

//! @brief Recieve message from the mailbox
//! @param mailbox Mailbox to recieve messages from
//! @param message Buffer to recieve the message in
//! @return API error
int user_ipc_recieve(struct user_ipc_mailbox *mailbox, struct user_ipc_message *message);

//! @brief Ack message at index
//! @param mailbox Mailbox to which message has been sent
//! @param index Index of the message
//! @return API error
int user_ipc_ack(struct user_ipc_mailbox *mailbox, uint32_t index);

//! @brief Shutdown mailbox
//! @param mailbox Pointer to the mailbox
void user_ipc_shutdown_mailbox(struct user_ipc_mailbox *mailbox);
