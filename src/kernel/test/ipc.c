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
	//! @brief Token
	struct user_ipc_token *token;
};

//! @brief IPC server parameters
struct test_ipc_server_params {
	//! @brief Control token
	struct user_ipc_control_token *ctrl;
	//! @brief Mailbox
	struct user_ipc_mailbox *mailbox;
	//! @brief Pointer to the test task
	struct thread_task *test_task;
};

//! @brief Number of messages to send
#define TEST_IPC_MSGS_NUM 1000000

//! @brief IPC test server
static void test_ipc_server(struct test_ipc_server_params *params) {
	LOG_INFO("server: running");
	struct user_ipc_message msg;
	for (size_t i = 0; i < TEST_IPC_MSGS_NUM; ++i) {
		// progress_bar(i, TEST_IPC_MSGS_NUM, 50);
		// LOG_INFO("server: recieving message #%U", i);
		int recv_status = user_ipc_recieve(params->mailbox, &msg);
		ASSERT(recv_status == USER_STATUS_SUCCESS, "Invalid recv status");
		ASSERT(msg.opaque == 69, "Wrong opaque value");
		ASSERT(msg.type == USER_IPC_MSG_TYPE_REGULAR, "Invalid message type");
		// LOG_INFO("server: acking message #%U", i);
		int ack_status = user_ipc_ack(params->mailbox, msg.index);
		ASSERT(ack_status == USER_STATUS_SUCCESS, "Invalid ack status");
	}
	// progress_bar(TEST_IPC_MSGS_NUM, TEST_IPC_MSGS_NUM, 50);
	// log_putc('\n');
	LOG_INFO("server: recieving ping of death");
	int recv_status = user_ipc_recieve(params->mailbox, &msg);
	ASSERT(recv_status == USER_STATUS_SUCCESS, "Invalid recv status on ping of death");
	ASSERT(msg.opaque == 69, "Wrong opaque value on ping of death");
	ASSERT(msg.type == USER_IPC_MSG_TYPE_TOKEN_UNREACHABLE,
	       "Invalid message type on ping of death");
	user_ipc_shutdown_mailbox(params->mailbox);
	MEM_REF_DROP(params->ctrl);
	LOG_INFO("server: terminating");
	thread_localsched_wake_up(params->test_task);
	thread_localsched_terminate();
}

//! @brief IPC test client
static void test_ipc_client(struct test_ipc_client_params *params) {
	LOG_INFO("client: running");
	struct user_ipc_message msg;
	size_t skips = 0;
	for (size_t i = 0; i < TEST_IPC_MSGS_NUM; ++i) {
		// LOG_INFO("server: sending message #%U", i);
		while (true) {
			int status = user_ipc_send(params->token, &msg);
			switch (status) {
			case USER_STATUS_SUCCESS:
				goto finished;
			case USER_STATUS_QUOTA_EXCEEDED:
				skips++;
				asm volatile("pause");
				continue;
			default:
				PANIC("server: Failed to send the message");
				break;
			}
		}
	finished:
		continue;
	}
	MEM_REF_DROP(params->token);
	LOG_INFO("client: terminating");
	thread_localsched_terminate();
}

//! @brief IPC test
void test_ipc(void) {
	struct test_ipc_server_params server_params;
	struct test_ipc_client_params client_params;
	// Create mailbox
	ASSERT(user_ipc_create_mailbox(1025, &server_params.mailbox) == USER_STATUS_SUCCESS,
	       "Failed to create mailbox");
	// Create token pair
	ASSERT(user_ipc_create_token_pair(server_params.mailbox, 1024, &client_params.token,
	                                  &server_params.ctrl, 69) == USER_STATUS_SUCCESS,
	       "Failed to create token pair");
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
