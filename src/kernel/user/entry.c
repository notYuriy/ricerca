//! @file entry.c
//! @brief File containing user API entry implementation

#include <user/entry.h>

//! @brief Initialize user API entry
//! @param entry Pointer to the user API entry
//! @return User API error
int user_api_entry_init(struct user_api_entry *entry) {
	int status = user_create_universe(&entry->universe);
	return status;
}

//! @brief Move out handle at position
//! @param entry Pointer to the user API entry
//! @param handle Index in the universe
//! @param buf Buffer to store user_ref in
//! @return API status
int user_api_entry_move_handle_out(struct user_api_entry *entry, size_t handle,
                                   struct user_ref *buf) {
	return user_move_out_ref(entry->universe, handle, buf);
}

//! @brief Allocate place for the reference within user entry
//! @param entry Pointer to the user API entry
//! @param ref Reference to store
//! @param buf Buffer to store handle in
//! @return API status
int user_api_entry_move_handle_in(struct user_api_entry *entry, struct user_ref ref, size_t *buf) {
	return user_allocate_cell(entry->universe, ref, buf);
}

//! @brief Create mailbox
//! @param entry Pointer to the user API entry
//! @param quota Max pending notifications count
//! @param handle Buffer to store mailbox handle in
//! @return API status
int user_api_create_mailbox(struct user_api_entry *entry, size_t quota, size_t *handle) {
	struct user_ref ref;
	ref.type = USER_OBJ_TYPE_MAILBOX;
	int status = user_create_mailbox(&ref.mailbox, quota);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}

	size_t res_handle;
	status = user_allocate_cell(entry->universe, ref, &res_handle);
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
int user_api_get_notification(struct user_api_entry *entry, size_t hmailbox,
                              struct user_notification *buf) {
	struct user_ref ref;
	int status = user_borrow_ref(entry->universe, hmailbox, &ref);
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

//! @brief Create caller
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param opaque Opaque value (will be sent in notifications)
//! @param hcaller Buffer to store caller handle in
//! @return API status
int user_api_create_caller(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t *hcaller) {
	struct user_ref mailbox_ref;
	int status = user_borrow_ref(entry->universe, hmailbox, &mailbox_ref);
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
	size_t result;
	status = user_allocate_cell(entry->universe, caller_ref, &result);
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
int user_api_create_callee(struct user_api_entry *entry, size_t hmailbox, size_t opaque,
                           size_t buckets, size_t *hcallee, size_t *htoken) {
	struct user_ref mailbox_ref;
	int status = user_borrow_ref(entry->universe, hmailbox, &mailbox_ref);
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
	refs[1].type = USER_OBJ_TYPE_TOKEN;
	refs[1].token = token;
	size_t cells[2];
	status = user_allocate_cell_pair(entry->universe, refs, cells);
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
int user_api_rpc_call(struct user_api_entry *entry, size_t hcaller, size_t htoken,
                      const struct user_rpc_msg *args) {
	struct user_ref caller_ref, token_ref;
	int status = user_borrow_ref(entry->universe, hcaller, &caller_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (caller_ref.type != USER_OBJ_TYPE_CALLER) {
		user_drop_ref(caller_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_borrow_ref(entry->universe, htoken, &token_ref);
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
int user_api_rpc_accept(struct user_api_entry *entry, size_t hcallee, struct user_rpc_msg *args) {
	struct user_ref callee_ref;
	int status = user_borrow_ref(entry->universe, hcallee, &callee_ref);
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
int user_api_rpc_return(struct user_api_entry *entry, size_t hcallee,
                        const struct user_rpc_msg *ret) {
	struct user_ref callee_ref;
	int status = user_borrow_ref(entry->universe, hcallee, &callee_ref);
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
int user_api_rpc_recv_reply(struct user_api_entry *entry, size_t hcaller,
                            struct user_rpc_msg *ret) {
	struct user_ref caller_ref;
	int status = user_borrow_ref(entry->universe, hcaller, &caller_ref);
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

//! @brief Drop cell at index
//! @param entry Pointer to the user API entry
//! @param handle Handle to drop
void user_api_drop_handle(struct user_api_entry *entry, size_t handle) {
	return user_drop_cell(entry->universe, handle);
}

//! @brief Deinitialize user API entry
//! @param entry Pointer to the user API entry
void user_api_entry_deinit(struct user_api_entry *entry) {
	MEM_REF_DROP(entry->universe);
}
