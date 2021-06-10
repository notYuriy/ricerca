//! @file e9.c
//! @brief File containing implementation of stivale2 terminal wrapper

#include <init/stivale2.h>
#include <lib/log.h>

//! @brief Logging subsystem object for stivale2 terminal
static struct log_subsystem subsystem;

//! @brief term_write function as provided by stivale2 terminal
static void (*term_write)(const char *str, size_t len);

//! @brief Stivale2 logging subsystem callback
//! @param self Pointer to logging subsystem (ignored)
//! @param data Pointer to the string to be printed
//! @param len Length of the string to be printed
void stivale2_term_callback(struct log_subsystem *self, const char *str, size_t len) {
	(void)self;
	term_write(str, len);
}

//! @brief Add logging subsystem for stivale2 terminal
//! @param term Pointer to terminal struct tag
void stivale2_term_register(struct stivale2_struct_tag_terminal *term) {
	term_write = (void (*)(const char *, size_t))(term->term_write);
	subsystem.callback = stivale2_term_callback;
	log_register_subsystem(&subsystem);
}

//! @brief Unregister stivale2 logging subsystem
//! @note Called when kernel is in charge of the file buffer
void stivale2_term_unregister(void) {
	log_unregister_subsystem(&subsystem);
}
