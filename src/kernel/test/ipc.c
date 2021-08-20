//! @file ipc.c
//! @brief File containing IPC test

#include <lib/callback.h>
#include <lib/panic.h>
#include <lib/progress.h>
#include <lib/target.h>
#include <misc/atomics.h>
#include <thread/tasking/balancer.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/task.h>
#include <user/entry.h>

MODULE("test/ipc")

//! @brief IPC client parameters
struct test_ipc_client_params {
	//! @brief Client user api entry
	struct user_api_entry entry;
	//! @brief Mailbox handle
	size_t hmailbox;
	//! @brief Remote stream handle
	size_t hremote;
	//! @brief Local stream handle
	size_t hlocal;
};

//! @brief IPC server parameters
struct test_ipc_server_params {
	//! @brief Server user api entry
	struct user_api_entry entry;
	//! @brief Mailbox handle
	size_t hmailbox;
	//! @brief Remote stream handle
	size_t hremote;
	//! @brief Local stream handle
	size_t hlocal;
	//! @brief Pointer to the test task
	struct thread_task *test_task;
};

//! @brief Number of messages to send
#define TEST_IPC_MSGS_NUM 10000000

//! @brief IPC test server
static void test_ipc_server(struct test_ipc_server_params *params) {
	LOG_INFO("server: running");
	struct user_ipc_msg msg;
	struct user_notification note;
	for (size_t i = 0; i < TEST_IPC_MSGS_NUM; ++i) {
		// Get notification
		int status = user_api_get_notification(&params->entry, params->hmailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		// LOG_INFO("server: recieved #%U notification", i);
		// Get message from the client
		status = user_api_recv_msg(&params->entry, params->hlocal, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get message");
		// LOG_INFO("server: recieved #%U message from the client", i);
		// Send reply
		status = user_api_send_msg(&params->entry, params->hremote, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to send reply");
		// LOG_INFO("server: replied to #%U message from the client", i);
	}
	// Shut down remote stream
	user_api_drop_handle(&params->entry, params->hremote);
	// Wait for stream dead notification
	int status = user_api_get_notification(&params->entry, params->hmailbox, &note);
	ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
	LOG_INFO("server: got local_stream close notification");
	// Cleanup
	user_api_drop_handle(&params->entry, params->hlocal);
	user_api_drop_handle(&params->entry, params->hmailbox);
	// Exit
	LOG_SUCCESS("server: terminating");
	thread_localsched_wake_up(params->test_task);
	thread_localsched_terminate();
}

//! @brief IPC test client
static void test_ipc_client(struct test_ipc_client_params *params) {
	LOG_INFO("client: running");
	struct user_ipc_msg msg;
	msg.length = 0;
	struct user_notification note;
	bool got_dead_note = false;
	for (size_t i = 0; i < TEST_IPC_MSGS_NUM; ++i) {
		// Send request
		int status = user_api_send_msg(&params->entry, params->hremote, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to send request");
		// LOG_INFO("client: sent #%U message to the server", i);
		// Get notification
		status = user_api_get_notification(&params->entry, params->hmailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		// LOG_INFO("client: recieved #%U notification", i);
		// Get reply from the server
		status = user_api_recv_msg(&params->entry, params->hlocal, &msg);
		if (status != USER_STATUS_SUCCESS) {
			ASSERT(status == USER_STATUS_TARGET_UNREACHABLE, "Invalid status");
			got_dead_note = true;
			LOG_INFO("client: got local_stream close notification");
			break;
		} else {
			// LOG_INFO("client: recieved #%U message from the server", i);
		}
	}
	// Shut down remote stream
	user_api_drop_handle(&params->entry, params->hremote);
	// Wait for stream dead notification
	if (!got_dead_note) {
		int status = user_api_get_notification(&params->entry, params->hmailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		LOG_INFO("client: got local_stream close notification");
	}
	// Cleanup
	user_api_drop_handle(&params->entry, params->hlocal);
	user_api_drop_handle(&params->entry, params->hmailbox);
	// Exit
	LOG_SUCCESS("client: terminating");
	thread_localsched_terminate();
}

//! @brief IPC test
void test_ipc(void) {
	LOG_INFO("%u messages will be sent", (uint32_t)TEST_IPC_MSGS_NUM);
	struct test_ipc_server_params server_params;
	struct test_ipc_client_params client_params;
	// Create user API entries
	ASSERT(user_api_entry_init(&client_params.entry) == USER_STATUS_SUCCESS,
	       "Client user API entry init failed");
	ASSERT(user_api_entry_init(&server_params.entry) == USER_STATUS_SUCCESS,
	       "Server user API entry init failed");
	// Create mailboxes
	ASSERT(user_api_create_mailbox(&server_params.entry, 1, &server_params.hmailbox) ==
	           USER_STATUS_SUCCESS,
	       "Failed to create server mailbox");
	ASSERT(user_api_create_mailbox(&client_params.entry, 1, &client_params.hmailbox) ==
	           USER_STATUS_SUCCESS,
	       "Failed to create client mailbox");
	// Create streams
	ASSERT(user_api_create_stream(&server_params.entry, server_params.hmailbox, 1, 69,
	                              &server_params.hremote,
	                              &server_params.hlocal) == USER_STATUS_SUCCESS,
	       "Failed to create server token pair");
	ASSERT(user_api_create_stream(&client_params.entry, client_params.hmailbox, 1, 69,
	                              &client_params.hremote,
	                              &client_params.hlocal) == USER_STATUS_SUCCESS,
	       "Failed to create client token pair");
	// Do some magic to swap remote stream handles across universes, so that client
	// and server can talk to each other
	struct user_ref client_stream, server_stream;
	ASSERT(user_api_entry_move_handle_out(&server_params.entry, server_params.hremote,
	                                      &server_stream) == USER_STATUS_SUCCESS,
	       "Failed to move out server stream handle");
	ASSERT(user_api_entry_move_handle_out(&client_params.entry, client_params.hremote,
	                                      &client_stream) == USER_STATUS_SUCCESS,
	       "Failed to move out client stream handle");
	ASSERT(user_api_entry_move_handle_in(&server_params.entry, client_stream,
	                                     &server_params.hremote) == USER_STATUS_SUCCESS,
	       "Failed to move in client stream handle");
	ASSERT(user_api_entry_move_handle_in(&client_params.entry, server_stream,
	                                     &client_params.hremote) == USER_STATUS_SUCCESS,
	       "Failed to move in server stream handle");
	// Start client thread
	struct thread_task *client =
	    thread_task_create_call(CALLBACK_VOID(test_ipc_client, &client_params));
	ASSERT(client != NULL, "Failed to allocate client thread");
	thread_localsched_associate(2, client);
	// Start server thread
	server_params.test_task = thread_localsched_get_current_task();
	struct thread_task *server =
	    thread_task_create_call(CALLBACK_VOID(test_ipc_server, &server_params));
	ASSERT(server != NULL, "Failed to allocate server thread");
	thread_localsched_associate(2, server);
	// Wait for completition
	thread_localsched_suspend_current(CALLBACK_VOID_NULL);
	LOG_SUCCESS("IPC test done");
}
