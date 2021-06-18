//! @file tables.c
//! @brief File containing definitions of routines for initialization of amd64 tables

#include <sys/tables/gdt.h>

//! @brief Initialize amd64 tables on this core
void tables_init(void) {
	gdt_load();
}
