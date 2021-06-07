//! @file e9.h
//! @brief File containing stivale2 wrapper declarations

#include <init/stivale2.h> // For terminal structure tag

//! @brief Add logging subsystem for stivale2 terminal
//! @param term Pointer to terminal struct tag
void stivale2_term_register(struct stivale2_struct_tag_terminal *term);

//! @brief Unregister stivale2 logging subsystem
//! @note Called when kernel is in charge of the file buffer
void stivale2_term_unregister(void);
