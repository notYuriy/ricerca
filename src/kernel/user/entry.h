//! @file entry.h
//! @brief File containing declarations related to user API entry

#pragma once

#include <user/notifications.h>
#include <user/rpc.h>
#include <user/status.h>
#include <user/universe.h>

//! @brief User API entry. Thread-local structure that contains all data needed for handling
//! syscalls from owning thread
struct user_api_entry {
	//! @brief Pointer to the universe
	struct user_universe *universe;
};

//! @brief Initialize user API entry
//! @param entry Pointer to the user API entry
//! @return User API error
int user_api_entry_init(struct user_api_entry *entry);

//! @brief Move out handle at position
//! @param entry Pointer to the user API entry
//! @param handle Index in the universe
//! @param buf Buffer to store user_ref in
//! @return API status
int user_api_entry_move_handle_out(struct user_api_entry *entry, size_t handle,
                                   struct user_ref *buf);

//! @brief Allocate place for the reference within user entry
//! @param entry Pointer to the user API entry
//! @param ref Reference to store
//! @param buf Buffer to store handle in
//! @return API status
int user_api_entry_move_handle_in(struct user_api_entry *entry, struct user_ref ref, size_t *buf);

//! @brief Create mailbox
//! @param entry Pointer to the user API entry
//! @param quota Max pending notifications count
//! @param handle Buffer to store mailbox handle in
//! @return API status
int user_api_create_mailbox(struct user_api_entry *entry, size_t quota, size_t *handle);

//! @brief Wait for notification
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param buf Buffer to store recieved notification in
//! @return API status
//! @return API status
int user_api_get_notification(struct user_api_entry *entry, size_t hmailbox,
                              struct user_notification *buf);

//! @brief Create caller
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param hcaller Buffer to store caller handle in
//! @return API status
int user_api_create_caller(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t *hcaller);

//! @brief Create callee
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param buckets Number of buckets hint
//! @param hcallee Buffer to store callee handle in
//! @param htoken Buffer to store token in
//! @return API status
int user_api_create_callee(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t buckets, size_t *hcallee, size_t *htoken);

//! @brief Initiate RPC
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param htoken Token handle
//! @param args RPC args
//! @return API status
int user_api_rpc_call(struct user_api_entry *entry, size_t hcaller, size_t htoken,
                      const struct user_rpc_msg *args);

//! @brief Accept RPC call
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param args Buffer to store RPC args in
//! @return API status
int user_api_rpc_accept(struct user_api_entry *entry, size_t hcallee, struct user_rpc_msg *args);

//! @brief Reply to RPC
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param ret RPC return message
//! @return API status
int user_api_rpc_return(struct user_api_entry *entry, size_t hcallee,
                        const struct user_rpc_msg *ret);

//! @brief Recieve reply
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param ret Buffer to store returned message in
//! @return API status
int user_api_rpc_recv_reply(struct user_api_entry *entry, size_t hcaller, struct user_rpc_msg *ret);

//! @brief Drop cell at index
//! @param entry Pointer to the user API entry
//! @param handle Handle to drop
void user_api_drop_handle(struct user_api_entry *entry, size_t handle);

//! @brief Deinitialize user API entry
//! @param entry Pointer to the user API entry
//! @return API status
void user_api_entry_deinit(struct user_api_entry *entry);
