//! @file log.c
//! @brief File containing declarations for logging functions

#pragma once

#include <misc/types.h>

//! @brief Log type
enum log_type {
	//! @brief Generic log
	LOG_TYPE_INFO,
	//! @brief Success
	LOG_TYPE_SUCCESS,
	//! @brief Warning
	LOG_TYPE_WARN,
	//! @brief Error
	LOG_TYPE_ERR,
	//! @brief Panic
	LOG_TYPE_PANIC,
};

//! @brief Log subsystem type
struct log_subsystem {
	//! @brief Point to the next registered logging subsystem
	struct log_subsystem *next;
	//! @brief Callback to print a string to the subsystem
	//! @param self Pointer to this structure
	//! @param str Pointer to the string to be printed
	//! @param len Length of the string to be printed
	void (*callback)(struct log_subsystem *self, const char *str, size_t len);
};

//! @brief Print one character to kernel log
//! @param character Character to be printed
void log_putc(char character);

//! @brief Print string to kernel log
//! @param data Pointer to the first char of the string that is to be printed
//! @param size Size of the string
void log_write(const char *data, size_t size);

//! @brief Print raw message to kernel log
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_printf(const char *fmt, ...);

//! @brief Log formatted message to kernel log
//! @param type Log type
//! @param subsystem String identifying kernel subsystem from which message comes from
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_logf(enum log_type type, const char *subsystem, const char *fmt, ...);

//! @brief Local info log statement.
//! @note Requires module_name macro to be defined
#define LOG_INFO(...) log_logf(LOG_TYPE_INFO, module_name, __VA_ARGS__)

//! @brief Local sucess log statement
//! @note Requires module_name macro to be defined
#define LOG_SUCCESS(...) log_logf(LOG_TYPE_SUCCESS, module_name, __VA_ARGS__)

//! @brief Local warning log statement
//! @note Requires module_name macro to be defined
#define LOG_WARN(...) log_logf(LOG_TYPE_WARN, module_name, __VA_ARGS__)

//! @brief Local error log statement
//! @note Requires module_name macro to be defined
#define LOG_ERR(...) log_logf(LOG_TYPE_ERR, module_name, __VA_ARGS__)

//! @brief Local panic log statement
//! @note Requires module_name macro to be defined
#define LOG_PANIC(...) log_logf(LOG_TYPE_PANIC, module_name, __VA_ARGS__)

//! @brief Load logging subsystem
void log_register_subsystem(struct log_subsystem *subsystem);

//! @brief Unload loading subsystem
void log_unregister_subsystem(struct log_subsystem *subsystem);
