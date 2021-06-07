//! @file e9.c
//! @brief File containing e9 driver

#include <drivers/output/e9/e9.h> // For interface declarations
#include <lib/log.h>              // For logging subsystem loading
#include <misc/types.h>           // For bool
#include <sys/ports.h>            // For port IO

//! @brief Module name
#define MODULE "e9"

//! @brief Print string to e9
//! @param self Pointer to the logging subsystem object
//! @param data Pointer to the string to be printed
//! @param size Size of the string
static void e9_puts(struct log_subsystem *self, const char *data, size_t size) {
	(void)self;
	for (size_t i = 0; i < size; ++i) {
		outb(0xe9, data[i]);
	}
}

//! @brief Detect e9 log presence
//! @return True if e9 log was detected
static bool e9_detect(void) {
	return inb(0xe9) == 0xe9;
}

//! @brief Register e9 log if present
//! @return True if e9 log is present
bool e9_register(void) {
	if (e9_detect()) {
		static struct log_subsystem instance;
		instance.callback = e9_puts;
		log_register_subsystem(&instance);
		LOG_SUCCESS("e9 log subsystem regsitered!");
		return true;
	}
	return false;
}
