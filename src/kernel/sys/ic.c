//! @file ic.c
//! @brief Implementation of interrupt controller abstractions

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/misc.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuid.h>
#include <sys/ic.h>
#include <sys/msr.h>
#include <sys/pic.h>

MODULE("sys/ic");
TARGET(ic_bsp_available, ic_bsp_init,
       {pic_remap_available, mem_misc_collect_info_available, acpi_available})

//! @brief Interrupt controller state
static enum
{
	IC_X2APIC_USED,
	IC_XAPIC_USED,
	IC_PIC_USED,
} ic_state = IC_PIC_USED;

//! @brief Pointer to LAPIC registers
static volatile uint32_t *lapic_xapic;

//! @brief IA32_APIC_BASE register as defined in Intel Software Developer's Manual
static const uint32_t LAPIC_IA32_APIC_BASE = 0x0000001b;

//! @brief Spurious LAPIC irq
uint8_t ic_spur_vec = 127;

//! @brief xAPIC registers
enum
{
	//! @brief Spurious interrupt vector register
	LAPIC_XAPIC_SPUR_REG = 0x3c,
	//! @brief Local LAPIC id register
	LAPIC_XAPIC_ID_REG = 0x08,
	//! @brief ICR low register
	LAPIC_XAPIC_ICR_LOW_REG = 0xc0,
	//! @brief ICR high register
	LAPIC_XAPIC_ICR_HIGH_REG = 0xc4,
};

//! @brief x2APIC MSRs
enum
{
	//! @brief Spurious interrupt vector MSR
	LAPIC_X2APIC_SPUR_REG = 0x80f,
	//! @brief Local LAPIC id register
	LAPIC_X2APIC_ID_REG = 0x802,
	//! @brief ICR register
	LAPIC_X2APIC_ICR_REG = 0x830,
};

//! @brief Handle spurious irq
void ic_handle_spur_irq(void) {
}

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void) {
	if (ic_state == IC_X2APIC_USED) {
		return rdmsr(LAPIC_X2APIC_ID_REG);
	} else if (ic_state == IC_XAPIC_USED) {
		return lapic_xapic[LAPIC_XAPIC_ID_REG];
	} else {
		return 0;
	}
}

//! @brief Enable interrupt controller on AP
void ic_enable(void) {
	// Enable LAPIC
	if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_IA32_APIC_BASE, rdmsr(LAPIC_IA32_APIC_BASE) | (1ULL << 10ULL));
		wrmsr(LAPIC_X2APIC_SPUR_REG, 0x100 | ic_spur_vec);
	} else if (ic_state == IC_XAPIC_USED) {
		lapic_xapic[LAPIC_XAPIC_SPUR_REG] = 0x100 | ic_spur_vec;
	} else {
		PANIC("lapic_enable in single-core mode got called");
	}
}

//! @brief Send init IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
void ic_send_init_ipi(uint32_t id) {
	if (ic_state == IC_PIC_USED) {
		PANIC("Sending INIT IPIs is not implemented for legacy PIC8259");
	} else if (ic_state == IC_XAPIC_USED) {
		ASSERT(id < 256, "Sending IPIs to the core with id > 256 in xAPIC mode");
		lapic_xapic[LAPIC_XAPIC_ICR_HIGH_REG] = id << 24;
		lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] = 0x4500;
		while ((lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] & (1 << 12)) != 0) {
			asm volatile("pause");
		}
	} else if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_X2APIC_ICR_REG, (((uint64_t)id) << 32ULL) | 0x4500ULL);
	}
}

//! @brief Send startup IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
//! @param addr Trampoline address (should be <1MB and divisible by 0x1000)
void ic_send_startup_ipi(uint32_t id, uint32_t addr) {
	uint64_t page = addr / 0x1000;
	if (ic_state == IC_PIC_USED) {
		PANIC("Sending startup IPIs is not implemented for legacy PIC8259");
	} else if (ic_state == IC_XAPIC_USED) {
		ASSERT(id < 256, "Sending IPIs to the core with id > 256 in xAPIC mode");
		lapic_xapic[LAPIC_XAPIC_ICR_HIGH_REG] = id << 24;
		lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] = 0x4600 | page;
		while ((lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] & (1 << 12)) != 0) {
			asm volatile("pause");
		}
	} else if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_X2APIC_ICR_REG, (((uint64_t)id) << 32ULL) | 0x4600ULL | page);
	}
}

//! @brief Send IPI to the core with the given APIC ID
//! @param id APIC id of the core
//! @param vec Interrupt vector to be triggered when message is recieved
void ic_send_ipi(uint32_t id, uint8_t vec) {
	if (ic_state == IC_PIC_USED) {
		PANIC("Sending IPIs is not implemented for legacy PIC8259");
	} else if (ic_state == IC_XAPIC_USED) {
		ASSERT(id < 256, "Sending IPIs to the core with id > 256 in xAPIC mode");
		lapic_xapic[LAPIC_XAPIC_ICR_HIGH_REG] = id << 24;
		lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] = (uint32_t)vec;
		while ((lapic_xapic[LAPIC_XAPIC_ICR_LOW_REG] & (1 << 12)) != 0) {
			asm volatile("pause");
		}
	} else if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_X2APIC_ICR_REG, (((uint64_t)id) << 32ULL) | (uint64_t)vec);
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
	ic_state = IC_XAPIC_USED;
	// Query x2APIC support
	if ((cpuid1h.ecx & (1 << 21)) != 0) {
		LOG_INFO("x2APIC support detected");
		ic_state = IC_X2APIC_USED;
	}
	const uint64_t lapic_phys_base = rdmsr(LAPIC_IA32_APIC_BASE) & (~0xfffULL);
	if (lapic_phys_base >= INIT_PHYS_MAPPING_SIZE && !(ic_state == IC_X2APIC_USED)) {
		PANIC("LAPIC unreachable until direct phys window set up");
	}
	lapic_xapic = (volatile uint32_t *)(mem_wb_phys_win_base + lapic_phys_base);
	LOG_INFO("xAPIC address: 0x%p", lapic_xapic);
	// Enable LAPIC
	ic_enable();
}
