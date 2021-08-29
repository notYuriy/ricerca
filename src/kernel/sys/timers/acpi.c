//! @file acpi.c
//! @brief File containing implementations of ACPI timing functions

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/ports.h>
#include <sys/timers/acpi.h>
#include <sys/timers/timer.h>

MODULE("sys/timer/acpi")
TARGET(acpi_timer_available, acpi_timer_init, {acpi_available, mem_misc_collect_info_available})

//! @brief struct timer structure used to register ACPI timer
static struct timer acpi_timer;

//! @brief ACPI timer goal
static uint32_t acpi_timer_goal;

//! @brief Initial value
static uint32_t acpi_init_val;

//! @brief ACPI GAS timer register
static struct acpi_fadt_gas acpi_timer_gas;

//! @brief True if ACPI timer is 32-bit
static bool acpi_timer_32bit = false;

//! @brief Read ACPI timer counter
static uint32_t acpi_timer_read_counter() {
	switch (acpi_timer_gas.address_space) {
	case ACPI_GAS_PORT_IO_ADDRESS_SPACE:
		ASSERT(acpi_timer_gas.address < 65536, "Invalid Port IO address");
		return ind(acpi_timer_gas.address);
	case ACPI_GAS_MMIO_ADDRESS_SPACE: {
		volatile uint32_t *reg =
		    (volatile uint32_t *)(mem_wb_phys_win_base + acpi_timer_gas.address);
		return *reg;
	}
	default:
		PANIC("Unsupported GAS address space type");
	}
}

//! @brief ACPI timer make_goal callback
//! @param self Pointer to acpi_timer
//! @param ms Number of milliseconds to wait
//! @return True if operation is supported
static bool acpi_timer_make_goal(struct timer *self, uint32_t ms) {
	(void)self;
	acpi_init_val = acpi_timer_read_counter();
	acpi_timer_goal = acpi_init_val + (ms * 3580);
	if (!acpi_timer_32bit) {
		acpi_timer_goal &= 0xffffff;
	}
	return true;
}

//! @brief ACPI timer is_goal_reached callback
//! @param self Pointer to acpi_timer
//! @return True if goal was reached
static bool acpi_timer_is_goal_reached(struct timer *self) {
	(void)self;
	// TODO: handle wraparounds properly
	uint32_t current_val = acpi_timer_read_counter();
	if (acpi_init_val < acpi_timer_goal) {
		return current_val >= acpi_timer_goal || current_val < acpi_init_val;
	} else if (acpi_init_val == acpi_timer_goal) {
		return true;
	} else {
		return current_val >= acpi_timer_goal && current_val < acpi_init_val;
	}
}

static void acpi_timer_init() {
	if (acpi_boot_fadt == NULL) {
		return;
	}
	if (acpi_boot_fadt->pm_timer_len != 4) {
		return;
	}
	if (acpi_revision >= 2 && acpi_boot_fadt->pm_timer_blk_ex.address != 0) {
		acpi_timer_gas = acpi_boot_fadt->pm_timer_blk_ex;
	} else {
		acpi_timer_gas.address_space = ACPI_GAS_PORT_IO_ADDRESS_SPACE;
		acpi_timer_gas.address = acpi_boot_fadt->pm_timer_blk;
	}
	if ((acpi_boot_fadt->flags & (1 << 8)) != 0) {
		acpi_timer_32bit = true;
	}
	acpi_timer.make_goal = acpi_timer_make_goal;
	acpi_timer.is_goal_reached = acpi_timer_is_goal_reached;
	acpi_timer.coolness = TIMER_ACPI_COOLNESS;
	timer_register(&acpi_timer);
}
