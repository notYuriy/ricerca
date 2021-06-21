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
#include <sys/tsc.h>
#include <thread/smp/locals.h>

MODULE("sys/ic");
TARGET(ic_bsp_available, ic_bsp_init,
       {pic_remap_available, mem_misc_collect_info_available, acpi_available})

//! @brief Interrupt controller state
static enum
{
	IC_X2APIC_USED,
	IC_XAPIC_USED,
	IC_PIC8259_USED,
} ic_state = IC_PIC8259_USED;

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
	//! @brief LVT timer register
	LAPIC_XAPIC_LVT_TIMER_REG = 0xc8,
	//! @brief Initial count register
	LAPIC_XAPIC_INIT_CNT_REG = 0xe0,
	//! @brief Current count register
	LAPIC_XAPIC_CNT_REG = 0xe4,
	//! @brief Divide configuration register
	LAPIC_XAPIC_DCR_REG = 0xf8,
	//! @brief EOI register
	LAPIC_XAPIC_EOI_REG = 0x2c,
	//! @brief Task priority register
	LAPIC_XAPIC_TPR_REG = 0x20,
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
	//! @brief LVT timer register
	LAPIC_X2APIC_LVT_TIMER_REG = 0x832,
	//! @brief Initial count register
	LAPIC_X2APIC_INIT_CNT_REG = 0x838,
	//! @brief Current count register,
	LAPIC_X2APIC_CNT_REG = 0x839,
	//! @brief Divide configuration register
	LAPIC_X2APIC_DCR_REG = 0x83e,
	//! @brief EOI register
	LAPIC_X2APIC_EOI_REG = 0x80b,
	//! @brief Task priority register
	LAPIC_X2APIC_TPR_REG = 0x808,
};

enum
{
	//! @brief Enable bit in spurious register
	LAPIC_ENABLE = 1 << 8,
	//! @brief Enable X2APIC
	X2APIC_ENABLE = 1 << 10,
	//! @brief Init IPI mask
	LAPIC_INIT_IPI = 0x4500,
	//! @brief Startup IPI mask
	LAPIC_STARTUP_IPI = 0x4600,
	//! @brief Delievered bit for xAPIC
	XAPIC_DELIEVERED = 1 << 12,
	//! @brief LAPIC LVT disable mask
	LAPIC_LVT_DISABLE_MASK = 1 << 16,
	//! @brief One-shot LAPIC timer mode
	LAPIC_TMR_ONE_SHOT = 0,
	//! @brief TSC deadline LAPIC timer mode
	LAPIC_TMR_TSC = 0b10 << 17,
	//! @brief TSC deadline MSR
	LAPIC_IA32_TSC_DEADLINE_MSR = 0x6e0,
	//! @brief APIC base MSR
	LAPIC_IA32_APIC_BASE = 0x1b,
};

//! @brief Pointer to LAPIC registers
static volatile uint32_t *ic_xapic_base;

//! @brief Spurious LAPIC irq interrupt vector
uint8_t ic_spur_vec = 127;

//! @brief Timer interrupt vector
uint8_t ic_timer_vec = 32;

//! @brief Generic LAPIC read function
#define LAPIC_READ(reg)                                                                            \
	({                                                                                             \
		uint32_t val;                                                                              \
		if (ic_state == IC_XAPIC_USED) {                                                           \
			val = ic_xapic_base[LAPIC_XAPIC_##reg##_REG];                                          \
		} else if (ic_state == IC_X2APIC_USED) {                                                   \
			val = (uint32_t)rdmsr(LAPIC_X2APIC_##reg##_REG);                                       \
		} else {                                                                                   \
			PANIC("LAPIC_READ macro used in PIC8259 code");                                        \
		};                                                                                         \
		val;                                                                                       \
	})

//! @brief Generic LAPIC write function
#define LAPIC_WRITE(reg, val)                                                                      \
	do {                                                                                           \
		if (ic_state == IC_XAPIC_USED) {                                                           \
			ic_xapic_base[LAPIC_XAPIC_##reg##_REG] = (val);                                        \
		} else if (ic_state == IC_X2APIC_USED) {                                                   \
			wrmsr(LAPIC_X2APIC_##reg##_REG, (uint32_t)val);                                        \
		} else {                                                                                   \
			PANIC("LAPIC_READ macro used in PIC8259 code");                                        \
		}                                                                                          \
	} while (false)

//! @brief Check if LAPIC is used
//! @return True if LAPIC is used, false if IC driver should fallback to PIC8259
static bool ic_is_lapic_used(void) {
	return ic_state != IC_PIC8259_USED;
}

//! @brief Handle spurious irq
void ic_handle_spur_irq(void) {
	if (ic_is_lapic_used()) {
		LAPIC_WRITE(EOI, 0);
	}
}

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void) {
	if (ic_is_lapic_used()) {
		return LAPIC_READ(ID);
	} else {
		return 0;
	}
}

//! @brief Enable interrupt controller on AP
void ic_enable(void) {
	// Enable LAPIC
	if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_IA32_APIC_BASE, rdmsr(LAPIC_IA32_APIC_BASE) | X2APIC_ENABLE);
		wrmsr(LAPIC_X2APIC_SPUR_REG, LAPIC_ENABLE | ic_spur_vec);
	} else if (ic_state == IC_XAPIC_USED) {
		ic_xapic_base[LAPIC_XAPIC_SPUR_REG] = LAPIC_ENABLE | ic_spur_vec;
	} else {
		PANIC("lapic_enable in single-core mode got called");
	}
}

//! @brief Send raw IPI
//! @param id ID of the core message is being sent to
//! @param msg Message contents
static void ic_ipi_send_raw(uint32_t id, uint32_t msg) {
	if (ic_state == IC_XAPIC_USED) {
		ASSERT(id < 256, "Sending IPIs to the core with id > 256 in xAPIC mode");
		ic_xapic_base[LAPIC_XAPIC_ICR_HIGH_REG] = id << 24;
		ic_xapic_base[LAPIC_XAPIC_ICR_LOW_REG] = msg;
		while ((ic_xapic_base[LAPIC_XAPIC_ICR_LOW_REG] & XAPIC_DELIEVERED) != 0) {
			asm volatile("pause");
		}
	} else if (ic_state == IC_X2APIC_USED) {
		wrmsr(LAPIC_X2APIC_ICR_REG, (((uint64_t)id) << 32ULL) | msg);
	} else {
		PANIC("SMP is not supported with legacy PIC8259");
	}
}

//! @brief Send init IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
void ic_send_init_ipi(uint32_t id) {
	ic_ipi_send_raw(id, LAPIC_INIT_IPI);
}

//! @brief Send startup IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
//! @param addr Trampoline address (should be <1MB and divisible by 0x1000)
void ic_send_startup_ipi(uint32_t id, uint32_t addr) {
	ic_ipi_send_raw(id, LAPIC_STARTUP_IPI | (addr / 0x1000));
}

//! @brief Send IPI to the core with the given APIC ID
//! @param id APIC id of the core
//! @param vec Interrupt vector to be triggered when message is recieved
void ic_send_ipi(uint32_t id, uint8_t vec) {
	ic_ipi_send_raw(id, LAPIC_STARTUP_IPI | (uint32_t)vec);
}

//! @brief Detect TSC deadline mode support
static void ic_timer_tsc_deadline_detect() {
	PER_CPU(ic_state).tsc_deadline_supported = false;
	// Check TSC deadline support
	struct cpuid buf;
	cpuid(1, 0, &buf);
	if ((buf.ecx & (1 << 24)) != 0) {
		// Support is there
		PER_CPU(ic_state).tsc_deadline_supported = true;
	}
}

//! @brief Initiate timer calibration process
void ic_timer_start_calibration(void) {
	if (ic_is_lapic_used()) {
		ic_timer_tsc_deadline_detect();
		if (PER_CPU(ic_state).tsc_deadline_supported) {
			// Initialize timer LVT register
			LAPIC_WRITE(LVT_TIMER, LAPIC_TMR_TSC | ic_timer_vec);
			// Read currrent timestamp
			PER_CPU(ic_state).tsc_buf = tsc_read();
		} else {
			// Set divider to 16
			LAPIC_WRITE(DCR, 0b1010);
			// Initialize timer LVT register
			LAPIC_WRITE(LVT_TIMER, LAPIC_TMR_ONE_SHOT | ic_timer_vec);
			// Initialize counter to 0xffffffff
			LAPIC_WRITE(INIT_CNT, 0xffffffff);
		}
	} else {
		TODO();
	}
}

//! @brief Finish timer calibration process. Should be called after IC_TIMER_CALIBRATION_PERIOD
//! millseconds passed from ic_timer_start_calibration call
void ic_timer_end_calibration(void) {
	if (ic_is_lapic_used()) {
		if (PER_CPU(ic_state).tsc_deadline_supported) {
			// Read clock cycles difference
			PER_CPU(ic_state).tsc_freq =
			    (tsc_read() - PER_CPU(ic_state).tsc_buf) / IC_TIMER_CALIBRATION_PERIOD;

		} else {
			// Calculate number of ticks per ms
			uint32_t val = LAPIC_READ(CNT);
			PER_CPU(ic_state).timer_ticks_per_ms = (0xffffffff - val) / IC_TIMER_CALIBRATION_PERIOD;
			// Stop LAPIC timer
			ic_timer_cancel_one_shot();
		}
	} else {
		TODO();
	}
}

//! @brief Prepare timer for one shot event
//! @param ms Number of milliseconds to wait
void ic_timer_one_shot(uint32_t ms) {
	if (ic_is_lapic_used()) {
		if (PER_CPU(ic_state).tsc_deadline_supported) {
			wrmsr(LAPIC_IA32_TSC_DEADLINE_MSR, tsc_read() + PER_CPU(ic_state).tsc_freq * ms);
		} else {
			LAPIC_WRITE(INIT_CNT, PER_CPU(ic_state).timer_ticks_per_ms * ms);
		}
	} else {
		TODO();
	}
}

//! @brief Acknowledge timer interrupt
void ic_timer_ack(void) {
	if (ic_is_lapic_used()) {
		LAPIC_WRITE(EOI, 0);
	} else {
		pic_irq_ack(0);
	}
}
//! @brief Cancel one-shot timer event
void ic_timer_cancel_one_shot() {
	if (ic_is_lapic_used()) {
		if (PER_CPU(ic_state).tsc_deadline_supported) {
			wrmsr(LAPIC_IA32_TSC_DEADLINE_MSR, 0);
		} else {
			LAPIC_WRITE(INIT_CNT, 0);
		}
	} else {
		TODO();
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
	ic_xapic_base = (volatile uint32_t *)(mem_wb_phys_win_base + lapic_phys_base);
	LOG_INFO("xAPIC address: 0x%p", ic_xapic_base);
	// Enable LAPIC
	ic_enable();
}
