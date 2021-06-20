//! @file acpi.c
//! @brief File containing implementations of ACPI timing functions

#include <lai/drivers/timer.h>
#include <lib/log.h>
#include <sys/acpi/laihost.h>
#include <sys/timers/acpi.h>
#include <sys/timers/timer.h>

MODULE("sys/timer/acpi")
TARGET(acpi_timer_available, acpi_timer_init, {lai_available})

//! @brief struct timer structure used to register ACPI timer
static struct timer acpi_timer;

//! @brief ACPI timer goal
static uint32_t acpi_timer_goal;

//! @brief ACPI timer make_goal callback
//! @param self Pointer to acpi_timer
//! @param ms Number of milliseconds to wait
//! @return True if operation is supported
static bool acpi_timer_make_goal(struct timer *self, uint32_t ms) {
	(void)self;
	acpi_timer_goal = lai_read_pm_timer_value() + (ms * 3580);
	return true;
}

//! @brief ACPI timer is_goal_reached callback
//! @param self Pointer to acpi_timer
//! @return True if goal was reached
static bool acpi_timer_is_goal_reached(struct timer *self) {
	(void)self;
	return lai_read_pm_timer_value() > acpi_timer_goal;
}

static void acpi_timer_init() {
	if (lai_start_pm_timer() != LAI_ERROR_NONE) {
		LOG_ERR("ACPI timer unsupported");
		return;
	}
	acpi_timer.make_goal = acpi_timer_make_goal;
	acpi_timer.is_goal_reached = acpi_timer_is_goal_reached;
	acpi_timer.coolness = TIMER_ACPI_COOLNESS;
	timer_register(&acpi_timer);
}
