//! @file init.c
//! @brief File containing kernel entrypoint and kernel init functions

#include <drivers/output/e9/e9.h>             // For e9 log subsystem
#include <drivers/output/stivale2/stivale2.h> // For stivale2 terminal wrapper
#include <init/stivale2.h>                    // For stivale2 protocol type definitions
#include <lib/log.h>                          // For LOG_* functions
#include <lib/panic.h>                        // For hang
#include <sys/acpi/acpi.h>                    // For acpi_early_init
#include <sys/numa/numa.h>                    // For NUMA early init

//! @brief Module name
#define MODULE "init"

//! @brief True if stivale2 terminal was registered
bool kernel_stivale2_term_loaded = false;

//! @brief Stack used by kernel on bootstrap
static uint8_t kernel_stack[65536];

//! @brief Stivale2 framebuffer tag
static struct stivale2_header_tag_framebuffer stivale2_fb_tag = {
    .tag = {.identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID, .next = 0},
    .framebuffer_height = 0,
    .framebuffer_width = 0,
    .framebuffer_bpp = 0,
};

//! @brief Stivale2 terminal tag
static struct stivale2_header_tag_terminal stivale2_term_tag = {
    .tag =
        {
            .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
            .next = (uint64_t)&stivale2_fb_tag,
        },
    .flags = 0};

//! @brief Stivale2 header
//! @note This part is taken from limine-bootloader/limine-barebones
__attribute__((section(".stivale2hdr"), used)) struct stivale2_header stivale_hdr = {
    .entry_point = 0,
    .stack = (uintptr_t)kernel_stack + sizeof(kernel_stack),
    .flags = 0b10,
    .tags = (uint64_t)&stivale2_term_tag,
};

//! @brief Query stivale for the tag
//! @param info Virtual pointer to stivale2 info structure
//! @param id Tag ID
struct stivale2_tag *stivale2_query(struct stivale2_struct *info, uint64_t id) {
	struct stivale2_tag *tag = (struct stivale2_tag *)info->tags;
	while (tag != NULL) {
		if (tag->identifier == id) {
			return tag;
		}
		tag = (struct stivale2_tag *)tag->next;
	}
	return tag;
}

//! @brief Try to initialize stivale2 terminal
//! @param info Info passed from bootloader
void kernel_load_stivale2_term(struct stivale2_struct *info) {
	struct stivale2_struct_tag_terminal *term_tag =
	    (struct stivale2_struct_tag_terminal *)stivale2_query(info,
	                                                          STIVALE2_STRUCT_TAG_TERMINAL_ID);
	if (term_tag != NULL) {
		kernel_stivale2_term_loaded = true;
		stivale2_term_register(term_tag);
		LOG_SUCCESS("Stivale2 terminal registered!");
	} else {
		LOG_WARN("Stivale 2 terminal was not found!");
	}
}

//! @brief Unregister stivale2 terminal if it was loaded
void kernel_unload_stivale2_term(void) {
	if (kernel_stivale2_term_loaded) {
		stivale2_term_unregister();
		LOG_SUCCESS("Stivale2 terminal unregistered!");
	}
}

//! @brief Kernel entrypoint
//! @param info Info passed from bootloader
void kernel_init(struct stivale2_struct *info) {
	// Load e9 logging subsystem
	e9_register();
	// Attempt to initialize stivale2 terminal
	kernel_load_stivale2_term(info);
	// Attempt early ACPI init
	struct stivale2_struct_tag_rsdp *rsdp_tag =
	    (struct stivale2_struct_tag_rsdp *)stivale2_query(info, STIVALE2_STRUCT_TAG_RSDP_ID);
	if (rsdp_tag == NULL) {
		PANIC("Machines without ACPI are not supported");
	}
	acpi_early_init(rsdp_tag);
	// Do NUMA init
	numa_init();
	hang();
}
