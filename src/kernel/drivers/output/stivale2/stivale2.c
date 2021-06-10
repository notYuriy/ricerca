//! @file e9.c
//! @brief File containing implementation of stivale2 terminal wrapper

#include <init/stivale2.h>
#include <lib/log.h>
#include <sys/cr.h>

//! @brief Logging subsystem object for stivale2 terminal
static struct log_subsystem stivale2_term_subsystem;

//! @brief term_write function as provided by stivale2 terminal
static void (*stivale2_term_write)(const char *str, size_t len);

//! @brief Stivale2 terminal CR3
static uint64_t stivale2_term_cr3;

//! @brief Stivale2 logging subsystem callback
//! @param self Pointer to logging subsystem (ignored)
//! @param data Pointer to the string to be printed
//! @param len Length of the string to be printed
void stivale2_term_callback(struct log_subsystem *self, const char *str, size_t len) {
	(void)self;
	// Temporairly drop in stivale2 terminal cr3 to access lower half stivale terminal
	const uint64_t current_cr3 = rdcr3();
	wrcr3(stivale2_term_cr3);
	stivale2_term_write(str, len);
	wrcr3(current_cr3);
}

//! @brief Add logging subsystem for stivale2 terminal
//! @param term Pointer to terminal struct tag
void stivale2_term_register(struct stivale2_struct_tag_terminal *term) {
	stivale2_term_cr3 = rdcr3();
	stivale2_term_write = (void (*)(const char *, size_t))(term->term_write);
	stivale2_term_subsystem.callback = stivale2_term_callback;
	log_register_subsystem(&stivale2_term_subsystem);
}

//! @brief Unregister stivale2 logging subsystem
//! @note Called when kernel is in charge of the file buffer
void stivale2_term_unregister(void) {
	log_unregister_subsystem(&stivale2_term_subsystem);
}
