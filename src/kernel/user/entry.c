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
		user_destroy_mailbox(ref.mailbox);
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
		user_drop_borrowed_ref(ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_recieve_notification(ref.mailbox, buf);
	user_drop_borrowed_ref(ref);
	return status;
}

//! @brief Create stream
//! @param entry Pointer to the user API entry
//! @param hmailbox Mailbox handle
//! @param quota Max pending messages count
//! @param opaque Opaque value stored inside the token
//! @param hproducer Buffer to store producer stream handle in
//! @param hconsumer Buffer to store consumer stream handle in
int user_api_create_stream(struct user_api_entry *entry, size_t hmailbox, size_t quota,
                           size_t opaque, size_t *hproducer, size_t *hconsumer) {
	struct user_ref mailbox_ref;
	int status = user_borrow_ref(entry->universe, hmailbox, &mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (mailbox_ref.type != USER_OBJ_TYPE_MAILBOX) {
		user_drop_borrowed_ref(mailbox_ref);
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	struct user_ipc_stream *stream;
	status = user_ipc_create_stream(&stream, quota, mailbox_ref.mailbox, opaque);
	user_drop_borrowed_ref(mailbox_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	struct user_ref stream_refs[2];
	stream_refs[0].type = USER_OBJ_TYPE_STREAM_CONSUMER;
	stream_refs[1].type = USER_OBJ_TYPE_STREAM_PRODUCER;
	stream_refs[0].stream = stream;
	stream_refs[1].stream = MEM_REF_BORROW(stream);
	size_t cells[2];
	status = user_allocate_cell_pair(entry->universe, stream_refs, cells);
	if (status != USER_STATUS_SUCCESS) {
		MEM_REF_DROP(stream);
		MEM_REF_DROP(stream);
		return status;
	}
	*hconsumer = cells[0];
	*hproducer = cells[1];
	return USER_STATUS_SUCCESS;
}

//! @brief Send message
//! @param entry Pointer to the user API entry
//! @param hstream Producer stream handle
//! @param msg Message buffer
//! @return API status
int user_api_send_msg(struct user_api_entry *entry, size_t hstream,
                      const struct user_ipc_msg *msg) {
	struct user_ref producer_ref;
	int status = user_borrow_ref(entry->universe, hstream, &producer_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (producer_ref.type != USER_OBJ_TYPE_STREAM_PRODUCER) {
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_ipc_send_msg(producer_ref.stream, msg);
	user_drop_borrowed_ref(producer_ref);
	return status;
}

//! @brief Recieve message
//! @param entry Pointer to the user API entry
//! @param hstream Consumer stream handle
//! @param msg Message buffer
//! @return API status
int user_api_recv_msg(struct user_api_entry *entry, size_t hstream, struct user_ipc_msg *msg) {
	struct user_ref consumer_ref;
	int status = user_borrow_ref(entry->universe, hstream, &consumer_ref);
	if (status != USER_STATUS_SUCCESS) {
		return status;
	}
	if (consumer_ref.type != USER_OBJ_TYPE_STREAM_CONSUMER) {
		return USER_STATUS_INVALID_HANDLE_TYPE;
	}
	status = user_ipc_recieve_msg(consumer_ref.stream, msg);
	user_drop_borrowed_ref(consumer_ref);
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
