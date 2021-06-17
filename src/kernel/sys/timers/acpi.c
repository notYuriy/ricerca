//! @file acpi.c
//! @brief File containing implementations of ACPI timing functions

#include <lai/drivers/timer.h>
#include <lib/log.h>
#include <sys/acpi/laihost.h>
#include <sys/timers/acpi.h>
#include <sys/timers/timer.h>

MODULE("sys/timer/acpi")
TARGET(acpi_timer_available, acpi_timer_init, {lai_available})

static struct timer acpi_timer;

static bool acpi_timer_wait_callback(struct timer *timer, uint32_t ms) {
	(void)timer;
	return lai_busy_wait_pm_timer(ms) == LAI_ERROR_NONE;
}

static void acpi_timer_init() {
	if (lai_start_pm_timer() != LAI_ERROR_NONE) {
		LOG_ERR("ACPI timer unsupported");
		return;
	}
	acpi_timer.wait_callback = acpi_timer_wait_callback;
	acpi_timer.coolness = TIMER_ACPI_COOLNESS;
	timer_register(&acpi_timer);
}
