//! @file log.c
//! @brief File containing definitions for logging functions

#include <lib/fmt.h>
#include <lib/log.h>
#include <lib/string.h>
#include <sys/ports.h>
#include <thread/locking/spinlock.h>

//! @brief Maximum length of the buffer for log_printf
#define LOG_BUFFER_SIZE 4096

//! @brief Registered subsystem list root
static struct log_subsystem *first = NULL;

//! @brief Logging subsystem spinlock
static struct thread_spinlock log_spinlock = THREAD_SPINLOCK_INIT;

//! @brief Print one character to kernel log
//! @param character Character to be printed
void log_putc(char character) {
	log_write(&character, 1);
}

//! @brief Print string to kernel log without locking it
//! @param data Pointer to the first char of the string that is to be printed
//! @param size Size of the string
static void log_write_lockless(const char *data, size_t size) {
	// Iterate all subsystems and call their callbacks
	struct log_subsystem *current = first;
	while (current != NULL) {
		current->callback(current, data, size);
		current = current->next;
	}
}

//! @brief Print string to kernel log
//! @param data Pointer to the first char of the string that is to be printed
//! @param size Size of the string
void log_write(const char *data, size_t size) {
	const bool state = thread_spinlock_lock(&log_spinlock);
	log_write_lockless(data, size);
	thread_spinlock_unlock(&log_spinlock, state);
}

//! @brief Print formatted message to kernel log with no locking
//! @param format Format string for the message
//! @param args Arguments for the format string
void log_vaprintf_lockless(const char *fmt, va_list args) {
	// Buffer that messages will be formatted to
	static char buf[LOG_BUFFER_SIZE];
	// Format message
	int bytes_written = vsnprintf(buf, LOG_BUFFER_SIZE, fmt, args);
	// Write message
	log_write_lockless(buf, bytes_written);
}

//! @brief Print formatted message to kernel log without locking
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_printf_lockless(const char *fmt, ...) {
	// Initialize varargs
	va_list args;
	va_start(args, fmt);
	// Format and print
	log_vaprintf_lockless(fmt, args);
	// Deinitialize varargs
	va_end(args);
}

//! @brief Print formatted message to kernel log
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_printf(const char *fmt, ...) {
	const bool state = thread_spinlock_lock(&log_spinlock);
	// Initialize varargs
	va_list args;
	va_start(args, fmt);
	// Format and print
	log_vaprintf_lockless(fmt, args);
	// Deinitialize varargs
	va_end(args);
	thread_spinlock_unlock(&log_spinlock, state);
}

//! @brief Log formatted message to kernel log without locking
//! @param type Log type
//! @param subsystem String identifying kernel subsystem from which message comes from
//! @param format Format string for the message
//! @param args Arguments for the format string
void log_valogf_lockless(enum log_type type, const char *subsystem, const char *fmt, va_list args) {
	const char *prefix = "[";
	const char *suffix = "] ";
	switch (type) {
	case LOG_TYPE_SUCCESS:
		prefix = "[\033[32m";
		suffix = ":success\033[0m] ";
		break;
	case LOG_TYPE_WARN:
		prefix = "[\033[33m";
		suffix = ":warning\033[0m] ";
		break;
	case LOG_TYPE_ERR:
		prefix = "[\033[35m";
		suffix = ":error\033[0m] ";
		break;
	case LOG_TYPE_PANIC:
		prefix = "[\033[31m";
		suffix = ":panic\033[0m] ";
		break;
	case LOG_TYPE_INFO:
		prefix = "[\033[36m";
		suffix = ":info\033[0m] ";
		break;
	default:
		break;
	}
	// Write out log statement
	log_write_lockless(prefix, strlen(prefix));       // Color prefix
	log_write_lockless(subsystem, strlen(subsystem)); // Subsystem
	log_write_lockless(suffix, strlen(suffix));       // Color suffix
	// Buffer that message will be formatted to
	static char buf[LOG_BUFFER_SIZE];
	// Format message
	int bytes_written = vsnprintf(buf, LOG_BUFFER_SIZE, fmt, args);
	// Write message
	log_write_lockless(buf, bytes_written);
	// Print \n\r
	log_write_lockless("\n\r", 1);
}

//! @brief Log formatted message to kernel log without locking
//! @param type Log type
//! @param subsystem String identifying kernel subsystem from which message comes from
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_logf_lockless(enum log_type type, const char *subsystem, const char *fmt, ...) {
	// Initialize varargs
	va_list args;
	va_start(args, fmt);
	// Print log statement
	log_valogf_lockless(type, subsystem, fmt, args);
	// Deinitialize varargs
	va_end(args);
}

//! @brief Log formatted message to kernel log
//! @param type Log type
//! @param subsystem String identifying kernel subsystem from which message comes from
//! @param format Format string for the message
//! @param ... Arguments for the format string
void log_logf(enum log_type type, const char *subsystem, const char *fmt, ...) {
	const bool state = thread_spinlock_lock(&log_spinlock);
	// Initialize varargs
	va_list args;
	va_start(args, fmt);
	// Print log statement
	log_valogf_lockless(type, subsystem, fmt, args);
	// Deinitialize varargs
	va_end(args);
	thread_spinlock_unlock(&log_spinlock, state);
}

//! @brief Lock log subsystem
//! @return Interrupt state
bool log_lock(void) {
	return thread_spinlock_lock(&log_spinlock);
}

//! @brief Load logging subsystem
void log_register_subsystem(struct log_subsystem *subsystem) {
	const bool state = thread_spinlock_lock(&log_spinlock);
	// Add subsystem to subsystem list
	subsystem->next = first;
	first = subsystem;
	thread_spinlock_unlock(&log_spinlock, state);
}

//! @brief Unload loading subsystem
void log_unregister_subsystem(struct log_subsystem *subsystem) {
	const bool state = thread_spinlock_lock(&log_spinlock);
	// Iterate over all subsystems to find one we are unregistering
	struct log_subsystem *prev = NULL;
	struct log_subsystem *current = first;
	while (current != NULL) {
		if (current == subsystem) {
			// Erase it from the list
			if (prev == NULL) {
				first = subsystem->next;
			} else {
				prev->next = subsystem->next;
			}
			return;
		}
		prev = current;
		current = current->next;
	}
	thread_spinlock_unlock(&log_spinlock, state);
}
