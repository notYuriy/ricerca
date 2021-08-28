//! @file rpc.h
//! @brief File containing declarations related to RPC

#pragma once

#include <misc/attributes.h>
#include <misc/types.h>
#include <user/notifications.h>

//! @brief RPC message maximum payload size
#define USER_RPC_MAX_PAYLOAD_SIZE 112

//! @brief RPC noreply status
#define USER_RPC_STATUS_NOREPLY 0

//! @brief RPC message
struct user_rpc_msg {
	//! @brief Opaque member
	size_t opaque;
	//! @brief Status code (0 for error)
	uint32_t status;
	//! @brief Message length
	uint32_t len;
	//! @brief Message data
	char payload[USER_RPC_MAX_PAYLOAD_SIZE];
} attribute_packed;

//! @brief RPC caller. Allows to initiate RPC requests
struct user_rpc_caller;

//! @brief RPC token. Used as RPC target
struct user_rpc_token;

//! @brief RPC callee. Allows to accept RPC requests
struct user_rpc_callee;

//! @brief Create RPC caller
//! @param mailbox Pointer to the mailbox
//! @param opaque Opaque value (to be recieved in notifications)
//! @param caller Buffer to save pointer to the caller in
int user_rpc_create_caller(struct user_mailbox *mailbox, size_t opaque,
                           struct user_rpc_caller **caller);

//! @brief Create RPC callee
//! @param mailbox Pointer to the mailbox
//! @param opaque Opaque value (to be recieved in notifications)
//! @param buckets Number of buckets hint
//! @param callee Buffer to save pointer to the callee in
//! @param token buffer to save pointer to the token in
int user_rpc_create_callee(struct user_mailbox *mailbox, size_t opaque, size_t buckets,
                           struct user_rpc_callee **callee, struct user_rpc_token **token);

//! @brief Initiate RPC
//! @param caller Pointer to the caller
//! @param token Pointer to the token
//! @param msg RPC arguments
int user_rpc_initiate(struct user_rpc_caller *caller, const struct user_rpc_token *token,
                      const struct user_rpc_msg *msg);

//! @brief Accept RPC
//! @param callee Pointer to the callee
//! @param msg Buffer to store RPC arguments in
int user_rpc_accept(struct user_rpc_callee *callee, struct user_rpc_msg *msg);

//! @brief Return from RPC
//! @param callee Pointer to the callee
//! @param msg Buffer to store RPC result in
int user_rpc_return(struct user_rpc_callee *callee, const struct user_rpc_msg *msg);

//! @brief Get RPC result
//! @param caller Pointer to the caller
//! @param msg Buffer to store RPC result in
int user_rpc_get_result(struct user_rpc_caller *caller, struct user_rpc_msg *msg);
