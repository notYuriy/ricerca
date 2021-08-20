//! @file status.h
//! @brief File containing declarations of user routines status identifiers

#pragma once

enum
{
	//! @brief Operation successful
	USER_STATUS_SUCCESS = 0,
	//! @brief Out of bounds error
	USER_STATUS_OUT_OF_BOUNDS = 1,
	//! @brief Unsupported operation
	USER_STATUS_UNSUPPORTED = 2,
	//! @brief Out of memory error
	USER_STATUS_OUT_OF_MEMORY = 3,
	//! @brief Target unreachable
	USER_STATUS_TARGET_UNREACHABLE = 4,
	//! @brief Quota exceeded
	USER_STATUS_QUOTA_EXCEEDED = 5,
	//! @brief Mailbox shutdown
	USER_STATUS_MAILBOX_SHUTDOWN = 6,
	//! @brief Stream empty
	USER_STATUS_STREAM_EMPTY = 7,
	//! @brief Invalid message
	USER_STATUS_INVALID_MSG = 8,
	//! @brief Invalid handle
	USER_STATUS_INVALID_HANDLE = 9,
	//! @brief Invalid handle type
	USER_STATUS_INVALID_HANDLE_TYPE = 10,
};
