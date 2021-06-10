//! @file lapic.c
//! @brief Implementation of LAPIC functions

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuid.h>
#include <sys/lapic.h>
#include <sys/msr.h>
#include <sys/pic.h>

MODULE("sys/lapic");
TARGET(lapic_bsp_target, lapic_bsp_init, {pic_remap_target, mem_misc_collect_info_target})

//! @brief True if x2APIC is supported
static bool lapic_x2apic_supported;

//! @brief Pointer to LAPIC registers
static volatile uint32_t *lapic_xapic;

//! @brief IA32_APIC_BASE register as defined in Intel Software Developer's Manual
static const uint32_t LAPIC_IA32_APIC_BASE = 0x000000001b;

//! @brief Spurious LAPIC irq
static const uint8_t lapic_spur_irq = 127;

//! @brief xAPIC registers
enum {
	//! @brief Spurious interrupt vector register
	LAPIC_XAPIC_SPUR_REG = 0x3c,
	//! @brief Local LAPIC id register
	LAPIC_XAPIC_ID_REG = 0x08,
};

//! @brief x2APIC MSRs
enum {
	//! @brief Spurious interrupt vector MSR
	LAPIC_X2APIC_SPUR_REG = 0x80f,
	//! @brief Local LAPIC id register
	LAPIC_X2APIC_ID_REG = 0x802
};

//! @brief Get APIC id
uint32_t lapic_get_apic_id(void) {
	if (lapic_x2apic_supported) {
		return rdmsr(LAPIC_X2APIC_ID_REG);
	} else {
		return lapic_xapic[LAPIC_XAPIC_ID_REG];
	}
}

//! @brief Enable LAPIC
void lapic_enable(void) {
	// Enable LAPIC
	if (lapic_x2apic_supported) {
		wrmsr(LAPIC_IA32_APIC_BASE, rdmsr(LAPIC_IA32_APIC_BASE) | (1ULL << 10ULL));
		wrmsr(LAPIC_X2APIC_SPUR_REG, 0x100 | lapic_spur_irq);
	} else {
		lapic_xapic[LAPIC_XAPIC_SPUR_REG] = 0x100 | lapic_spur_irq;
	}
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
	const uint64_t lapic_phys_base = rdmsr(LAPIC_IA32_APIC_BASE) & (~0xffffULL);
	if (lapic_phys_base >= INIT_PHYS_MAPPING_SIZE && !lapic_x2apic_supported) {
		PANIC("LAPIC unreachable until direct phys window set up");
	}
	lapic_xapic = (volatile uint32_t *)(mem_wb_phys_win_base + lapic_phys_base);
	LOG_INFO("xAPIC address: 0x%p", lapic_xapic);
	// Enable LAPIC
	lapic_enable();
}
