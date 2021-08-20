//! @file entry.h
//! @brief File containing declarations related to user API entry

#pragma once

#include <user/ipc.h>
#include <user/notifications.h>
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
//! @param hmailbox Handle to the mailbox
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_api_get_notification(struct user_api_entry *entry, size_t hmailbox,
                              struct user_notification *buf);

//! @brief Create stream
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param quota Max pending messages count
//! @param opaque Opaque value stored inside the token
//! @param hproducer Buffer to store producer stream handle in
//! @param hconsumer Buffer to store consumer stream handle in
int user_api_create_stream(struct user_api_entry *entry, size_t hmailbox, size_t quota,
                           size_t opaque, size_t *hproducer, size_t *hconsumer);

//! @brief Send message
//! @param entry Pointer to the user API entry
//! @param hstream Producer stream handle
//! @param msg Message buffer
//! @return API status
int user_api_send_msg(struct user_api_entry *entry, size_t hstream, const struct user_ipc_msg *msg);

//! @brief Recieve message
//! @param entry Pointer to the user API entry
//! @param hstream Consumer stream handle
//! @param msg Message buffer
//! @return API status
int user_api_recv_msg(struct user_api_entry *entry, size_t hstream, struct user_ipc_msg *msg);

//! @brief Drop cell at index
//! @param entry Pointer to the user API entry
//! @param handle Handle to drop
void user_api_drop_handle(struct user_api_entry *entry, size_t handle);

//! @brief Deinitialize user API entry
//! @param entry Pointer to the user API entry
void user_api_entry_deinit(struct user_api_entry *entry);
