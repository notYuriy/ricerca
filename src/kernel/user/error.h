//! @file error.h
//! @brief File containing declarations of user routines error identifiers

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
	//! @brief Mailbox unreachable
	USER_STATUS_MAILBOX_UNREACHABLE = 4,
	//! @brief Quota exceeded
	USER_STATUS_QUOTA_EXCEEDED = 5,
	//! @brief Mailbox shutdown
	USER_STATUS_MAILBOX_SHUTDOWN = 6,
	//! @brief Inactive message slot
	USER_STATUS_INACTIVE_MESSAGE_SLOT = 7,
};
