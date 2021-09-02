//! @file entry.h
//! @brief File containing declarations related to user API entry

#pragma once

#include <user/cookie.h>
#include <user/notifications.h>
#include <user/rpc.h>
#include <user/status.h>
#include <user/universe.h>

//! @brief User API entry. Thread-local structure that contains all data needed for handling
//! syscalls from owning thread
struct user_api_entry {
	//! @brief Pointer to the universe
	struct user_universe *universe;
	//! @brief Entry cookie
	struct user_entry_cookie *cookie;
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
int user_sys_create_mailbox(struct user_api_entry *entry, size_t quota, size_t *handle);

//! @brief Wait for notification
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param buf Buffer to store recieved notification in
//! @return API status
//! @return API status
int user_sys_get_notification(struct user_api_entry *entry, size_t hmailbox,
                              struct user_notification *buf);

//! @brief Create group cookie object
//! @param entry Pointer to the user API entry
//! @param handle Buffer to to store group cookie handle in
//! @return API status
int user_sys_create_group_cookie(struct user_api_entry *entry, size_t *handle);

//! @brief Create entry cookie object
//! @param entry Pointer to the user API entry
//! @param handle Buffer to to store entry cookie handle in
//! @return API status
int user_sys_create_entry_cookie(struct user_api_entry *entry, size_t *handle);

//! @brief Join the group
//! @param entry Pointer to the user API entry
//! @param hgrp Group cookie handle
//! @return API status
int user_sys_join_group(struct user_api_entry *entry, size_t hgrp);

//! @brief Leave the group
//! @param entry Pointer to the user API entry
//! @param hgrp Group cookie handle
//! @return API status
int user_sys_leave_group(struct user_api_entry *entry, size_t hgrp);

//! @brief Add entry cookie to the group
//! @param entry Pointer to the user API entry
//! @param hentry Entry cookie handle
//! @param hgrp Group handle
//! @return API status
int user_sys_add_entry_to_group(struct user_api_entry *entry, size_t hentry, size_t hgrp);

//! @brief Remove entry cookie from the group
//! @param entry Pointer to the user API entry
//! @param hentry Entry cookie handle
//! @param hgrp Group handle
//! @return API status
int user_sys_remove_entry_from_group(struct user_api_entry *entry, size_t hentry, size_t hgrp);

//! @brief Create caller
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param hcaller Buffer to store caller handle in
//! @return API status
int user_sys_create_caller(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t *hcaller);

//! @brief Create callee
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param buckets Number of buckets hint
//! @param hcallee Buffer to store callee handle in
//! @param htoken Buffer to store token in
//! @return API status
int user_sys_create_callee(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t buckets, size_t *hcallee, size_t *htoken);

//! @brief Initiate RPC
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param htoken Token handle
//! @param args RPC args
//! @return API status
int user_sys_rpc_call(struct user_api_entry *entry, size_t hcaller, size_t htoken,
                      const struct user_rpc_msg *args);

//! @brief Accept RPC call
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param args Buffer to store RPC args in
//! @return API status
int user_sys_rpc_accept(struct user_api_entry *entry, size_t hcallee, struct user_rpc_msg *args);

//! @brief Reply to RPC
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param ret RPC return message
//! @return API status
int user_sys_rpc_return(struct user_api_entry *entry, size_t hcallee,
                        const struct user_rpc_msg *ret);

//! @brief Recieve reply
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param ret Buffer to store returned message in
//! @return API status
int user_sys_rpc_recv_reply(struct user_api_entry *entry, size_t hcaller, struct user_rpc_msg *ret);

//! @brief Create universe
//! @param entry Pointer to the user API entry
//! @param huniverse Buffer to store handle to the universe in
int user_sys_create_universe(struct user_api_entry *entry, size_t *huniverse);

//! @brief Move handle across universes
//! @param entry Pointer to the user API entry
//! @param hsrc Handle to the source universe
//! @param hdst Handle to the destination universe
//! @param hsrci Handle IN the source universe
//! @param hdsti Buffer to store handle in the destination universe
int user_sys_move_across_universes(struct user_api_entry *entry, size_t hsrc, size_t hdst,
                                   size_t hsrci, size_t *hdsti);

//! @brief Borrow handle across universes
//! @param entry Pointer to the user API entry
//! @param hsrc Handle to the source universe
//! @param hdst Handle to the destination universe
//! @param hsrci Handle IN the source universe
//! @param hdsti Buffer to store handle in the destination universe
int user_sys_borrow_across_universes(struct user_api_entry *entry, size_t hsrc, size_t hdst,
                                     size_t hsrci, size_t *hdsti);

//! @brief Move handle and store it in universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param outer Handle in the outer universe
//! @param inner Buffer to store handle in inner universe
int user_sys_move_in(struct user_api_entry *entry, size_t huniverse, size_t outer, size_t *inner);

//! @brief Move handle out of universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Handle in the inner universe
//! @param outer Buffer to store handle in outer universe
int user_sys_move_out(struct user_api_entry *entry, size_t huniverse, size_t inner, size_t *outer);

//! @brief Move handle and store it in universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param outer Handle in the outer universe
//! @param inner Buffer to store handle in inner universe
int user_sys_borrow_in(struct user_api_entry *entry, size_t huniverse, size_t outer, size_t *inner);

//! @brief Move handle out of universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Handle in the inner universe
//! @param outer Buffer to store handle in outer universe
int user_sys_borrow_out(struct user_api_entry *entry, size_t huniverse, size_t inner,
                        size_t *outer);

//! @brief Unpin reference (allow everyone with universe handle to borrow/move/drop it)
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @return API status
int user_sys_unpin(struct user_api_entry *entry, size_t handle);

//! @brief Pin reference (restrict borrow/move/and drop operations to caller's cookie), leaving it
//! unpinned only for the aller
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @return API status
int user_sys_pin(struct user_api_entry *entry, size_t handle);

//! @brief Unpin reference (allow everyone with universe handle to borrow/move/drop it), that
//! previously was unpinned only for one group (via user_sys_pin_to_group)
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @param hgrp Group handle
//! @return API status
int user_sys_unpin_from_group(struct user_api_entry *entry, size_t handle, size_t hgrp);

//! @brief Pin reference (restrict borrow/move/and drop operations to caller's cookie), leaving it
//! unpinned only for one group
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @param hgrp Group handle
//! @return API status
int user_sys_pin_to_group(struct user_api_entry *entry, size_t handle, size_t hgrp);

//! @brief Fork universe (Create a new one and copy all accessible handles)
//! @param entry Pointer to the user API entry
//! @param hsrc Universe handle
//! @param hdst Buffer to store new universe handle in
//! @return API status
int user_sys_fork_universe(struct user_api_entry *entry, size_t hsrc, size_t *hdst);

//! @brief Drop handle in the universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Inner handle to drop
//! @return API status
int user_sys_drop_in(struct user_api_entry *entry, size_t huniverse, size_t inner);

//! @brief Drop cell at index
//! @param entry Pointer to the user API entry
//! @param handle Handle to drop
//! @return API status
int user_sys_drop(struct user_api_entry *entry, size_t handle);

//! @brief Deinitialize user API entry
//! @param entry Pointer to the user API entry
//! @return API status
void user_api_entry_deinit(struct user_api_entry *entry);
