//! @file rpc.c
//! @brief File containing code for RPC test

#include <lib/panic.h>
#include <lib/progress.h>
#include <lib/target.h>
#include <misc/atomics.h>
#include <test/tests.h>
#include <thread/tasking/balancer.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/task.h>
#include <user/entry.h>

MODULE("test/rpc")

//! @brief Number of messages to be passed
#define TEST_RPC_CALLS_NUM 10000000
//! @brief Progress bar size
#define PROGRESS_BAR_SIZE 50

//! @brief RPC server paramenters
struct test_rpc_server_params {
	//! @brief Pointer to the server user API entry
	struct user_api_entry *entry;
	//! @brief Mailbox handle
	size_t hmailbox;
	//! @brief Callee handle
	size_t hcallee;
	//! @brief Pointer to the main task
	struct thread_task *main_task;
};

//! @brief RPC server code
//! @param params Server parameters
void test_rpc_server(struct test_rpc_server_params *params) {
	log_printf("RPC calls recieved\r\t\t\t");
	for (size_t i = 0; i < TEST_RPC_CALLS_NUM; ++i) {
		progress_bar(i, TEST_RPC_CALLS_NUM, PROGRESS_BAR_SIZE);
		struct user_notification notification;
		int status = user_sys_get_notification(params->entry, params->hmailbox, &notification);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to recieve notification");
		// LOG_INFO("Recieved notification for call #%U", i);
		ASSERT(notification.type == USER_NOTE_TYPE_RPC_INCOMING, "Wrong notification type");
		ASSERT(notification.opaque == 0xdeadbeef, "Wrong notification opaque value");

		struct user_rpc_msg msg;
		status = user_sys_rpc_accept(params->entry, params->hcallee, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to accept RPC");
		// LOG_INFO("Accepted RPC #%U", i);
		ASSERT(msg.opaque == i, "Invalid key value");

		status = user_sys_rpc_return(params->entry, params->hcallee, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to return RPC");
		// LOG_INFO("Returned RPC #%U", i);
	}
	progress_bar(TEST_RPC_CALLS_NUM, TEST_RPC_CALLS_NUM, PROGRESS_BAR_SIZE);
	log_printf("\n");
	user_api_entry_deinit(params->entry);
	LOG_INFO("Server finished");
	thread_localsched_wake_up(params->main_task);
	thread_localsched_terminate();
}

//! @brief RPC client parameters
struct test_rpc_client_params {
	//! @brief Pointer to the client user API entry
	struct user_api_entry *entry;
	//! @brief Mailbox handle
	size_t hmailbox;
	//! @brief Caller handle
	size_t hcaller;
	//! @brief Token handle
	size_t htoken;
	//! @brief Set to 1 if client has finished
	int finished;
};

//! @brief RPC client code
//! @param params Client parameters
void test_rpc_client(struct test_rpc_client_params *params) {
	for (size_t i = 0; i < TEST_RPC_CALLS_NUM; ++i) {
		struct user_rpc_msg msg;
		msg.len = 0;
		msg.opaque = 0xabacaba;
		int status = user_sys_rpc_call(params->entry, params->hcaller, params->htoken, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to initiate RPC call");
		// LOG_INFO("Initiated RPC #%U", i);

		struct user_notification notification;
		status = user_sys_get_notification(params->entry, params->hmailbox, &notification);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to recieve notification");
		// LOG_INFO("Recieved notification for reply to call #%U", i);
		ASSERT(notification.type == USER_NOTE_TYPE_RPC_REPLY, "Wrong notification type");
		ASSERT(notification.opaque == 0xcafebabe, "Wrong notification opaque value");

		status = user_sys_rpc_recv_reply(params->entry, params->hcaller, &msg);
		ASSERT(status == USER_STATUS_SUCCESS, "Failed to recieve reply");
		// LOG_INFO("Recieved reply to call #%U", i);
		ASSERT(msg.opaque == 0xabacaba, "Wrong opaque value");
	}
	user_api_entry_deinit(params->entry);
	LOG_INFO("Client finished");
	ATOMIC_RELEASE_STORE(&params->finished, 1);
	thread_localsched_terminate();
}

//! @brief RPC test
void test_rpc(void) {
	struct user_api_entry client_entry, server_entry;
	struct test_rpc_client_params client_params;
	struct test_rpc_server_params server_params;
	if (user_api_entry_init(&client_entry) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize client user API entry");
	}
	if (user_api_entry_init(&server_entry) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize server user API entry");
	}
	client_params.entry = &client_entry;
	server_params.entry = &server_entry;
	if (user_sys_create_mailbox(&client_entry, false, &client_params.hmailbox) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize client mailbox");
	}
	if (user_sys_create_mailbox(&server_entry, false, &server_params.hmailbox) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize server mailbox");
	}
	int error;
	if ((error = user_sys_create_caller(&client_entry, client_params.hmailbox, 0xcafebabe,
	                                    &client_params.hcaller)) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize client caller (%d)", error);
	}
	if (user_sys_create_callee(&server_entry, server_params.hmailbox, 0xdeadbeef, 0,
	                           &server_params.hcallee,
	                           &client_params.htoken) != USER_STATUS_SUCCESS) {
		PANIC("Failed to initialize server callee");
	}
	struct user_ref token_ref;
	if (user_api_entry_move_handle_out(&server_entry, client_params.htoken, &token_ref) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to move token out");
	}
	if (user_api_entry_move_handle_in(&client_entry, token_ref, &client_params.htoken) !=
	    USER_STATUS_SUCCESS) {
		PANIC("Failed to move token in");
	}
	server_params.main_task = thread_localsched_get_current_task();
	client_params.finished = 0;
	struct thread_task *server_task, *client_task;
	client_task = thread_task_create_call(CALLBACK_VOID(test_rpc_client, &client_params));
	if (client_task == NULL) {
		PANIC("Failed to create client task");
	}
	server_task = thread_task_create_call(CALLBACK_VOID(test_rpc_server, &server_params));
	if (server_task == NULL) {
		PANIC("Failed to create server task");
	}
	thread_localsched_associate(0, client_task);
	thread_localsched_associate(0, server_task);
	thread_localsched_suspend_current(CALLBACK_VOID_NULL);
	while (ATOMIC_ACQUIRE_LOAD(&client_params.finished) != 1) {
		asm volatile("pause");
	}
}
