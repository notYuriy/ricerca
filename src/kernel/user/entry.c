//! @file entry.c
//! @brief File containing user API entry implementation

#include <user/entry.h>

//! @brief Initialize user API entry
//! @param entry Pointer to the user API entry
//! @return User API error
int user_api_entry_init(struct user_api_entry *entry) {
	int status = user_universe_create(&entry->universe);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_entry_cookie_create(&entry->cookie);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(entry->universe);
		return status;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Move out handle at position
//! @param entry Pointer to the user API entry
//! @param handle Index in the universe
//! @param buf Buffer to store user_ref in
//! @return API status
int user_api_entry_move_handle_out(struct user_api_entry *entry, size_t handle,
                                   struct user_ref *buf) {
	return user_universe_move_out(entry->universe, handle, buf);
}

//! @brief Allocate place for the reference within user entry
//! @param entry Pointer to the user API entry
//! @param ref Reference to store
//! @param buf Buffer to store handle in
//! @return API status
int user_api_entry_move_handle_in(struct user_api_entry *entry, struct user_ref ref, size_t *buf) {
	return user_universe_move_in(entry->universe, ref, buf);
}

//! @brief Create mailbox
//! @param entry Pointer to the user API entry
//! @param quota Max pending notifications count
//! @param handle Buffer to store mailbox handle in
//! @return API status
int user_sys_create_mailbox(struct user_api_entry *entry, size_t quota, size_t *handle) {
	struct user_ref ref;
	ref.type = USER_OBJ_TYPE_MAILBOX;
	ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	int status = user_create_mailbox(&ref.mailbox, quota);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}

	size_t res_handle;
	status = user_universe_move_in(entry->universe, ref, &res_handle);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(ref.ref);
		return status;
	}
	*handle = res_handle;
	return USER_STATUS_SUCCESS;
}

//! @brief Wait for notification
//! @param entry Pointer to the user API entry
//! @param hmailbox Handle to the mailbox
//! @param buf Buffer to store recieved notification in
//! @return API status
int user_sys_get_notification(struct user_api_entry *entry, size_t hmailbox,
                              struct user_notification *buf) {
	struct user_ref ref;
	int status = user_universe_borrow_out(entry->universe, hmailbox, &ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (ref.type != USER_OBJ_TYPE_MAILBOX) {
		user_drop_ref(ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_recieve_notification(ref.mailbox, buf);
	user_drop_ref(ref);
	return status;
}

//! @brief Create group cookie object
//! @param entry Pointer to the user API entry
//! @param handle Buffer to to store group cookie handle in
//! @return API status
int user_sys_create_group_cookie(struct user_api_entry *entry, size_t *handle) {
	struct user_ref group_cookie_ref;
	group_cookie_ref.type = USER_OBJ_TYPE_GROUP_COOKIE;
	group_cookie_ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	int status = user_group_cookie_create(&group_cookie_ref.group_cookie);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_universe_move_in(entry->universe, group_cookie_ref, handle);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(group_cookie_ref);
		return status;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Create entry cookie object
//! @param entry Pointer to the user API entry
//! @param handle Buffer to to store entry cookie handle in
//! @return API status
int user_sys_create_entry_cookie(struct user_api_entry *entry, size_t *handle) {
	struct user_ref entry_cookie_ref;
	entry_cookie_ref.type = USER_OBJ_TYPE_ENTRY_COOKIE;
	entry_cookie_ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	int status = user_entry_cookie_create(&entry_cookie_ref.entry_cookie);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_universe_move_in(entry->universe, entry_cookie_ref, handle);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(entry_cookie_ref);
		return status;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Join the group
//! @param entry Pointer to the user API entry
//! @param hgrp Group cookie handle
//! @return API status
int user_sys_join_group(struct user_api_entry *entry, size_t hgrp) {
	struct user_ref group_ref;
	int status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_entry_cookie_add_to_grp(entry->cookie, group_ref.group_cookie);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Leave the group
//! @param entry Pointer to the user API entry
//! @param hgrp Group cookie handle
//! @return API status
int user_sys_leave_group(struct user_api_entry *entry, size_t hgrp) {
	struct user_ref group_ref;
	int status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_entry_cookie_remove_from_grp(entry->cookie, group_ref.group_cookie);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Add entry cookie to the group
//! @param entry Pointer to the user API entry
//! @param hentry Entry cookie handle
//! @param hgrp Group handle
//! @return API status
int user_sys_add_entry_to_group(struct user_api_entry *entry, size_t hentry, size_t hgrp) {
	struct user_ref entry_ref, group_ref;
	int status = user_universe_borrow_out(entry->universe, hentry, &entry_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (entry_ref.type != USER_OBJ_TYPE_ENTRY_COOKIE) {
		user_drop_ref(entry_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(entry_ref);
		return status;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(entry_ref);
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_entry_cookie_add_to_grp(entry_ref.entry_cookie, group_ref.group_cookie);
	user_drop_ref(entry_ref);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Remove entry cookie to the group
//! @param entry Pointer to the user API entry
//! @param hentry Entry cookie handle
//! @param hgrp Group handle
//! @return API status
int user_sys_remove_entry_from_group(struct user_api_entry *entry, size_t hentry, size_t hgrp) {
	struct user_ref entry_ref, group_ref;
	int status = user_universe_borrow_out(entry->universe, hentry, &entry_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (entry_ref.type != USER_OBJ_TYPE_ENTRY_COOKIE) {
		user_drop_ref(entry_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(entry_ref);
		return status;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(entry_ref);
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_entry_cookie_remove_from_grp(entry_ref.entry_cookie, group_ref.group_cookie);
	user_drop_ref(entry_ref);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Create caller
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param hcaller Buffer to store caller handle in
//! @return API status
int user_sys_create_caller(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t *hcaller) {
	struct user_ref mailbox_ref;
	int status = user_universe_borrow_out(entry->universe, hmailbox, &mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (mailbox_ref.type != USER_OBJ_TYPE_MAILBOX) {
		user_drop_ref(mailbox_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	struct user_rpc_caller *caller;
	status = user_rpc_create_caller(mailbox_ref.mailbox, opaque, &caller);
	user_drop_ref(mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	struct user_ref caller_ref;
	caller_ref.caller = caller;
	caller_ref.type = USER_OBJ_TYPE_CALLER;
	caller_ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	size_t result;
	status = user_universe_move_in(entry->universe, caller_ref, &result);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(caller);
		return status;
	}
	*hcaller = result;
	return USER_STATUS_SUCCESS;
}

//! @brief Create callee
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param buckets Number of buckets hint
//! @param hcallee Buffer to store callee handle in
//! @param htoken Buffer to store token in
//! @return API status
int user_sys_create_callee(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t buckets, size_t *hcallee, size_t *htoken) {
	struct user_ref mailbox_ref;
	int status = user_universe_borrow_out(entry->universe, hmailbox, &mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (mailbox_ref.type != USER_OBJ_TYPE_MAILBOX) {
		user_drop_ref(mailbox_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	struct user_rpc_callee *callee;
	struct user_rpc_token *token;
	status = user_rpc_create_callee(mailbox_ref.mailbox, opaque, buckets, &callee, &token);
	user_drop_ref(mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	struct user_ref refs[2];
	refs[0].type = USER_OBJ_TYPE_CALLEE;
	refs[0].callee = callee;
	refs[0].pin_cookie = user_entry_cookie_get_key(entry->cookie);
	refs[1].type = USER_OBJ_TYPE_TOKEN;
	refs[1].token = token;
	refs[1].pin_cookie = refs[0].pin_cookie;
	size_t cells[2];
	status = user_universe_move_in_pair(entry->universe, refs, cells);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(callee);
		MEM_REF_DROP(token);
		return status;
	}
	*hcallee = cells[0];
	*htoken = cells[1];
	return USER_STATUS_SUCCESS;
}

//! @brief Initiate RPC
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param htoken Token handle
//! @param args RPC args
//! @return API status
int user_sys_rpc_call(struct user_api_entry *entry, size_t hcaller, size_t htoken,
                      const struct user_rpc_msg *args) {
	struct user_ref caller_ref, token_ref;
	int status = user_universe_borrow_out(entry->universe, hcaller, &caller_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (caller_ref.type != USER_OBJ_TYPE_CALLER) {
		user_drop_ref(caller_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_out(entry->universe, htoken, &token_ref);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(caller_ref);
		return status;
	}
	if (token_ref.type != USER_OBJ_TYPE_TOKEN) {
		user_drop_ref(token_ref);
		user_drop_ref(caller_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_rpc_initiate(caller_ref.caller, token_ref.token, args);
	user_drop_ref(token_ref);
	user_drop_ref(caller_ref);
	return status;
}

//! @brief Accept RPC call
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param args Buffer to store RPC args in
//! @return API status
int user_sys_rpc_accept(struct user_api_entry *entry, size_t hcallee, struct user_rpc_msg *args) {
	struct user_ref callee_ref;
	int status = user_universe_borrow_out(entry->universe, hcallee, &callee_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (callee_ref.type != USER_OBJ_TYPE_CALLEE) {
		user_drop_ref(callee_ref);
		return USER_STATUS_INVALID_HANDLE;
	}
	status = user_rpc_accept(callee_ref.callee, args);
	user_drop_ref(callee_ref);
	return status;
}

//! @brief Reply to RPC
//! @param entry Pointer to the user API entry
//! @param hcallee Pointer to the callee
//! @param ret RPC return message
//! @return API status
int user_sys_rpc_return(struct user_api_entry *entry, size_t hcallee,
                        const struct user_rpc_msg *ret) {
	struct user_ref callee_ref;
	int status = user_universe_borrow_out(entry->universe, hcallee, &callee_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (callee_ref.type != USER_OBJ_TYPE_CALLEE) {
		user_drop_ref(callee_ref);
		return USER_STATUS_INVALID_HANDLE;
	}
	status = user_rpc_return(callee_ref.callee, ret);
	user_drop_ref(callee_ref);
	return status;
}

//! @brief Recieve reply
//! @param entry Pointer to the user API entry
//! @param hcaller Caller handle
//! @param ret Buffer to store returned message in
//! @return API status
int user_sys_rpc_recv_reply(struct user_api_entry *entry, size_t hcaller,
                            struct user_rpc_msg *ret) {
	struct user_ref caller_ref;
	int status = user_universe_borrow_out(entry->universe, hcaller, &caller_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (caller_ref.type != USER_OBJ_TYPE_CALLER) {
		user_drop_ref(caller_ref);
		return USER_STATUS_INVALID_HANDLE;
	}
	status = user_rpc_get_result(caller_ref.caller, ret);
	user_drop_ref(caller_ref);
	return status;
}

//! @brief Create universe
//! @param entry Pointer to the user API entry
//! @param huniverse Buffer to store handle to the universe in
int user_sys_create_universe(struct user_api_entry *entry, size_t *huniverse) {
	struct user_universe *universe;
	int status = user_universe_create(&universe);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	struct user_ref universe_ref;
	universe_ref.type = USER_OBJ_TYPE_UNIVERSE;
	universe_ref.universe = universe;
	universe_ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	size_t cell;
	status = user_universe_move_in(entry->universe, universe_ref, &cell);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(universe);
		return status;
	}
	*huniverse = cell;
	return USER_STATUS_SUCCESS;
}

//! @brief Extract universe pair
//! @param entry Pointer to the user API entry
//! @param hsrc Handle to the source universe
//! @param hdst Handle to the dest universe
//! @param src_ref Buffer to store reference to the source universe in
//! @param dst_ref Buffer to store reference to the destination universe in
//! @return API status
static int user_entry_extract_universe_pair(struct user_api_entry *entry, size_t hsrc, size_t hdst,
                                            struct user_ref *src_ref, struct user_ref *dst_ref) {
	int status = user_universe_borrow_out(entry->universe, hsrc, src_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (src_ref->type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(*src_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_out(entry->universe, hdst, dst_ref);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(*src_ref);
		return status;
	}
	if (dst_ref->type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(*src_ref);
		user_drop_ref(*dst_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Move handle across universes
//! @param entry Pointer to the user API entry
//! @param hsrc Handle to the source universe
//! @param hdst Handle to the destination universe
//! @param hsrci Handle IN the source universe
//! @param hdsti Buffer to store handle in the destination universe
int user_sys_move_across_universes(struct user_api_entry *entry, size_t hsrc, size_t hdst,
                                   size_t hsrci, size_t *hdsti) {
	struct user_ref source_ref, dest_ref;
	int status = user_entry_extract_universe_pair(entry, hsrc, hdst, &source_ref, &dest_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_universe_move_across(source_ref.universe, dest_ref.universe, hsrci, hdsti,
	                                   entry->cookie);
	user_drop_ref(source_ref);
	user_drop_ref(dest_ref);
	return status;
}

//! @brief Borrow handle across universes
//! @param entry Pointer to the user API entry
//! @param hsrc Handle to the source universe
//! @param hdst Handle to the destination universe
//! @param hsrci Handle IN the source universe
//! @param hdsti Buffer to store handle in the destination universe
int user_sys_borrow_across_universes(struct user_api_entry *entry, size_t hsrc, size_t hdst,
                                     size_t hsrci, size_t *hdsti) {
	struct user_ref source_ref, dest_ref;
	int status = user_entry_extract_universe_pair(entry, hsrc, hdst, &source_ref, &dest_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_universe_borrow_across(source_ref.universe, dest_ref.universe, hsrci, hdsti,
	                                     entry->cookie);
	user_drop_ref(source_ref);
	user_drop_ref(dest_ref);
	return status;
}

//! @brief Move handle and store it in universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param outer Handle in the outer universe
//! @param inner Buffer to store handle in inner universe
int user_sys_move_in(struct user_api_entry *entry, size_t huniverse, size_t outer, size_t *inner) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, huniverse, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_move_across(entry->universe, universe_ref.universe, outer, inner,
	                                   entry->cookie);
	user_drop_ref(universe_ref);
	return status;
}

//! @brief Move handle out of universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Handle in the inner universe
//! @param outer Buffer to store handle in outer universe
int user_sys_move_out(struct user_api_entry *entry, size_t huniverse, size_t inner, size_t *outer) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, huniverse, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_move_across(universe_ref.universe, entry->universe, inner, outer,
	                                   entry->cookie);
	user_drop_ref(universe_ref);
	return status;
}

//! @brief Move handle and store it in universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param outer Handle in the outer universe
//! @param inner Buffer to store handle in inner universe
int user_sys_borrow_in(struct user_api_entry *entry, size_t huniverse, size_t outer,
                       size_t *inner) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, huniverse, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_across(entry->universe, universe_ref.universe, outer, inner,
	                                     entry->cookie);
	user_drop_ref(universe_ref);
	return status;
}

//! @brief Move handle out of universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Handle in the inner universe
//! @param outer Buffer to store handle in outer universe
int user_sys_borrow_out(struct user_api_entry *entry, size_t huniverse, size_t inner,
                        size_t *outer) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, huniverse, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_borrow_across(universe_ref.universe, entry->universe, inner, outer,
	                                     entry->cookie);
	user_drop_ref(universe_ref);
	return status;
}

//! @brief Unpin reference (allow everyone with universe handle to borrow/move/drop it)
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @return API status
int user_sys_unpin(struct user_api_entry *entry, size_t handle) {
	return user_universe_unpin(entry->universe, handle, entry->cookie);
}

//! @brief Pin reference (restrict borrow/move/and drop operations to caller's cookie)
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @return API status
int user_sys_pin(struct user_api_entry *entry, size_t handle) {
	return user_universe_pin(entry->universe, handle, entry->cookie);
}

//! @brief Unpin reference (allow everyone with universe handle to borrow/move/drop it), that
//! previously was unpinned only for one group (via user_sys_pin_to_group)
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @param hgrp Group handle
//! @return API status
int user_sys_unpin_from_group(struct user_api_entry *entry, size_t handle, size_t hgrp) {
	struct user_ref group_ref;
	int status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		return USER_STATUS_INVALID_HANDLE;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_unpin_from_group(entry->universe, handle, entry->cookie,
	                                        group_ref.group_cookie);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Pin reference (restrict borrow/move/and drop operations to caller's cookie), leaving it
//! unpinned only for one group
//! @param entry Pointer to the user API entry
//! @param handle Handle
//! @param hgrp Group handle
//! @return API status
int user_sys_pin_to_group(struct user_api_entry *entry, size_t handle, size_t hgrp) {
	struct user_ref group_ref;
	int status = user_universe_borrow_out(entry->universe, hgrp, &group_ref);
	if (status != USER_STATUS_SUCCESS) {
		return USER_STATUS_INVALID_HANDLE;
	}
	if (group_ref.type != USER_OBJ_TYPE_GROUP_COOKIE) {
		user_drop_ref(group_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status =
	    user_universe_pin_to_group(entry->universe, handle, entry->cookie, group_ref.group_cookie);
	user_drop_ref(group_ref);
	return status;
}

//! @brief Fork universe (Create a new one and copy all accessible handles)
//! @param entry Pointer to the user API entry
//! @param hsrc Universe handle
//! @param hdst Buffer to store new universe handle in
//! @return API status
int user_sys_fork_universe(struct user_api_entry *entry, size_t hsrc, size_t *hdst) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, hsrc, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	struct user_ref forked_ref;
	forked_ref.type = USER_OBJ_TYPE_UNIVERSE;
	forked_ref.pin_cookie = user_entry_cookie_get_key(entry->cookie);
	status = user_universe_fork(entry->universe, &forked_ref.universe, entry->cookie);
	user_drop_ref(universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	status = user_universe_move_in(entry->universe, forked_ref, hdst);
	if (status != USER_STATUS_SUCCESS) {
		user_drop_ref(forked_ref);
		return status;
	}
	return USER_STATUS_SUCCESS;
}

//! @brief Drop handle in the universe
//! @param entry Pointer to the user API entry
//! @param huniverse Universe handle
//! @param inner Inner handle to drop
//! @return API status
int user_sys_drop_in(struct user_api_entry *entry, size_t huniverse, size_t inner) {
	struct user_ref universe_ref;
	int status = user_universe_borrow_out(entry->universe, huniverse, &universe_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (universe_ref.type != USER_OBJ_TYPE_UNIVERSE) {
		user_drop_ref(universe_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_universe_drop_cell(universe_ref.universe, inner, entry->cookie);
	user_drop_ref(universe_ref);
	return status;
}

//! @brief Drop cell at index
//! @param entry Pointer to the user API entry
//! @param handle Handle to drop
//! @return API status
int user_sys_drop(struct user_api_entry *entry, size_t handle) {
	return user_universe_drop_cell(entry->universe, handle, entry->cookie);
}

//! @brief Deinitialize user API entry
//! @param entry Pointer to the user API entry
void user_api_entry_deinit(struct user_api_entry *entry) {
	MEM_REF_DROP(entry->universe);
}
