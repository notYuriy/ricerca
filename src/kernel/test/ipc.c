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
#include <user/ipc.h>

MODULE("test/ipc")

//! @brief IPC client parameters
struct test_ipc_client_params {
	//! @brief Remote stream end
	struct user_ipc_stream *remote_stream;
	//! @brief Local stream end
	struct user_ipc_stream *local_stream;
	//! @brief Pointer to the local mailbox
	struct user_mailbox *mailbox;
};

//! @brief IPC server parameters
struct test_ipc_server_params {
	//! @brief Remote stream end
	struct user_ipc_stream *remote_stream;
	//! @brief Local stream end
	struct user_ipc_stream *local_stream;
	//! @brief Pointer to the local mailbox
	struct user_mailbox *mailbox;
	//! @brief Pointer to the test task
	struct thread_task *test_task;
};

//! @brief Number of messages to send
#define TEST_IPC_MSGS_NUM 100

//! @brief IPC test server
static void test_ipc_server(struct test_ipc_server_params *params) {
	LOG_INFO("server: running");
	struct user_ipc_msg msg;
	struct user_notification note;
	for (size_t i = 0; i < TEST_IPC_MSGS_NUM; ++i) {
		// Get notification
		int status = user_recieve_notification(params->mailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		LOG_INFO("server: recieved #%U notification", i);
		// Get message from the client
		status = user_ipc_recieve_msg(params->local_stream, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get message");
		LOG_INFO("server: recieved #%U message from the client", i);
		// Send reply
		status = user_ipc_send_msg(params->remote_stream, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to send reply");
		LOG_INFO("server: replied to #%U message from the client", i);
	}
	// Shut down remote stream
	user_ipc_shutdown_stream_producer(params->remote_stream);
	// Wait for stream dead notification
	int status = user_recieve_notification(params->mailbox, &note);
	ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
	LOG_INFO("server: got local_stream close notification");
	// Cleanup
	user_ipc_shutdown_stream_consumer(params->local_stream);
	user_destroy_mailbox(params->mailbox);
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
		int status = user_ipc_send_msg(params->remote_stream, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to send request");
		LOG_INFO("client: sent #%U message to the server", i);
		// Get notification
		status = user_recieve_notification(params->mailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		LOG_INFO("client: recieved #%U notification", i);
		// Get reply from the server
		status = user_ipc_recieve_msg(params->local_stream, &msg);
		if (status != USER_STATUS_SUCCESS) {
			ASSERT(status == USER_STATUS_TARGET_UNREACHABLE, "Invalid status");
			got_dead_note = true;
			LOG_INFO("client: got local_stream close notification");
			break;
		} else {
			LOG_INFO("client: recieved #%U message from the server", i);
		}
	}
	// Shut down remote stream
	user_ipc_shutdown_stream_producer(params->remote_stream);
	// Wait for stream dead notification
	if (!got_dead_note) {
		int status = user_recieve_notification(params->mailbox, &note);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to get notification");
		LOG_INFO("client: got local_stream close notification");
	}
	// Cleanup
	user_ipc_shutdown_stream_consumer(params->local_stream);
	user_destroy_mailbox(params->mailbox);
	// Exit
	LOG_SUCCESS("client: terminating");
	thread_localsched_terminate();
}

//! @brief IPC test
void test_ipc(void) {
	struct test_ipc_server_params server_params;
	struct test_ipc_client_params client_params;
	// Create mailboxes
	ASSERT(user_create_mailbox(&server_params.mailbox, 1) == USER_STATUS_SUCCESS,
	       "Failed to create mailbox");
	ASSERT(user_create_mailbox(&client_params.mailbox, 1) == USER_STATUS_SUCCESS,
	       "Failed to create mailbox");
	// Create streams
	ASSERT(user_ipc_create_stream(&server_params.local_stream, 1, server_params.mailbox, 69) ==
	           USER_STATUS_SUCCESS,
	       "Failed to create token pair");
	ASSERT(user_ipc_create_stream(&client_params.local_stream, 1, client_params.mailbox, 69) ==
	           USER_STATUS_SUCCESS,
	       "Failed to create token pair");
	// Borrow streams
	server_params.remote_stream = MEM_REF_BORROW(client_params.local_stream);
	client_params.remote_stream = MEM_REF_BORROW(server_params.local_stream);
	// Start client thread
	struct thread_task *client =
	    thread_task_create_call(CALLBACK_VOID(test_ipc_client, &client_params));
	ASSERT(client != NULL, "Failed to allocate client thread");
	thread_balancer_allocate_to_any(client);
	// Start server thread
	server_params.test_task = thread_localsched_get_current_task();
	struct thread_task *server =
	    thread_task_create_call(CALLBACK_VOID(test_ipc_server, &server_params));
	ASSERT(server != NULL, "Failed to allocate server thread");
	thread_balancer_allocate_to_any(server);
	// Wait for completition
	thread_localsched_suspend_current(CALLBACK_VOID_NULL);
	LOG_SUCCESS("IPC test done");
}
