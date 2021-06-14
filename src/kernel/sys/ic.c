//! @file lapic.c
//! @brief Implementation of LAPIC functions

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuid.h>
#include <sys/ic.h>
#include <sys/msr.h>
#include <sys/pic.h>

MODULE("sys/ic");
TARGET(ic_bsp_target, ic_bsp_init, {pic_remap_target, mem_misc_collect_info_target})

//! @brief Interrupt controller state
static enum
{
	IC_X2APIC_USED,
	IC_XAPIC_USED,
	IC_PIC_USED,
} lapic_state = IC_PIC_USED;

//! @brief Pointer to LAPIC registers
static volatile uint32_t *lapic_xapic;

//! @brief IA32_APIC_BASE register as defined in Intel Software Developer's Manual
static const uint32_t LAPIC_IA32_APIC_BASE = 0x000000001b;

//! @brief Spurious LAPIC irq
static const uint8_t lapic_spur_irq = 127;

//! @brief xAPIC registers
enum
{
	//! @brief Spurious interrupt vector register
	LAPIC_XAPIC_SPUR_REG = 0x3c,
	//! @brief Local LAPIC id register
	LAPIC_XAPIC_ID_REG = 0x08,
};

//! @brief x2APIC MSRs
enum
{
	//! @brief Spurious interrupt vector MSR
	LAPIC_X2APIC_SPUR_REG = 0x80f,
	//! @brief Local LAPIC id register
	LAPIC_X2APIC_ID_REG = 0x802
};

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void) {
	if (lapic_state == IC_X2APIC_USED) {
		return rdmsr(LAPIC_X2APIC_ID_REG);
	} else if (lapic_state == IC_XAPIC_USED) {
		return lapic_xapic[LAPIC_XAPIC_ID_REG];
	} else {
		return 0;
	}
}

//! @brief Enable interrupt controller on AP
void ic_enable(void) {
	// Enable LAPIC
	if (lapic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_IA32_APIC_BASE, rdmsr(LAPIC_IA32_APIC_BASE) | (1ULL << 10ULL));
		wrmsr(LAPIC_X2APIC_SPUR_REG, 0x100 | lapic_spur_irq);
	} else if (lapic_state == IC_XAPIC_USED) {
		lapic_xapic[LAPIC_XAPIC_SPUR_REG] = 0x100 | lapic_spur_irq;
	} else {
		PANIC("lapic_enable in single-core mode got called");
	}
}

//! @brief Initialize interrupt controller on BSP
static void ic_bsp_init(void) {
	// First of all, let's determine if LAPIC is supported at all
	struct cpuid cpuid1h;
	cpuid(0x1, 0, &cpuid1h);
	if ((cpuid1h.edx & (1 << 9)) == 0) {
		return;
	}
	lapic_state = IC_XAPIC_USED;
	// Query x2APIC support
	if ((cpuid1h.ecx & (1 << 21)) != 0) {
		LOG_INFO("x2APIC support detected");
		lapic_state = IC_X2APIC_USED;
	}
	const uint64_t lapic_phys_base = rdmsr(LAPIC_IA32_APIC_BASE) & (~0xffffULL);
	if (lapic_phys_base >= INIT_PHYS_MAPPING_SIZE && !(lapic_state == IC_X2APIC_USED)) {
		PANIC("LAPIC unreachable until direct phys window set up");
	}
	lapic_xapic = (volatile uint32_t *)(mem_wb_phys_win_base + lapic_phys_base);
	LOG_INFO("xAPIC address: 0x%p", lapic_xapic);
	// Enable LAPIC
	ic_enable();
}
