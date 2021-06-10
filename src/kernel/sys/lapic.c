//! @file lapic.c
//! @brief Implementation of LAPIC functions

#include <lib/log.h>
#include <lib/panic.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuid.h>
#include <sys/lapic.h>
#include <sys/pic.h>

MODULE("sys/lapic");
TARGET(lapic_bsp_target, lapic_bsp_init, pic_remap_target)

//! @brief True if x2APIC is supported
static bool lapic_x2apic_supported;

//! @brief Base of xAPIC
static uintptr_t lapic_xapic_base = 0xFEE00000;

//! @brief Initialize lapic on AP
void lapic_ap_init(void) {
}

//! @brief Initialize lapic on BSP
static void lapic_bsp_init(void) {
	// First of all, let's determine if LAPIC is supported at all
	struct cpuid cpuid1h;
	cpuid(0x1, 0, &cpuid1h);
	// Assert that LAPIC is available
	if ((cpuid1h.edx & (1 << 9)) == 0) {
		PANIC("No LAPIC support");
	}
	// Query x2APIC support
	lapic_x2apic_supported = (cpuid1h.ecx & (1 << 21)) != 0;
	if (lapic_x2apic_supported) {
		LOG_INFO("x2APIC support detected");
	}
	// Iterate over MADT to see if we get LAPIC addr overwrite
	if (acpi_boot_madt != NULL) {
		lapic_xapic_base = (uintptr_t)acpi_boot_madt->lapic_addr;
		// Calculate entries starting and ending address
		uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt);
		const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
		// Iterate over all entries
		while (address < end_address) {
			struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
			address += entry->length;
			if (entry->type != ACPI_MADT_LAPIC_ADDR_OVERRIDE_ENTRY) {
				continue;
			}
			struct acpi_madt_lapic_addr_override_entry *override =
			    (struct acpi_madt_lapic_addr_override_entry *)entry;
			lapic_xapic_base = (uintptr_t) override->override;
		}
	}
	LOG_INFO("xAPIC address: 0x%p", lapic_xapic_base);
}
