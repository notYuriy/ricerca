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
#include <thread/smp/core.h>
#include <thread/smp/trampoline.h>

MODULE("sys/ic");
TARGET(ic_bsp_available, ic_bsp_init,
       {pic_remap_available, mem_misc_collect_info_available, acpi_available})

//! @brief Interrupt controller state
static enum
{
	IC_X2APIC_USED,
	IC_XAPIC_USED,
} ic_state = IC_XAPIC_USED;

//! @brief xAPIC registers
enum
{
	//! @brief Spurious interrupt vector register
	LAPIC_XAPIC_SPUR_REG = 0xf0 / 4,
	//! @brief LAPIC id register
	LAPIC_XAPIC_ID_REG = 0x20 / 4,
	//! @brief LAPIC version register
	LAPIC_XAPIC_VER_REG = 0x30 / 4,
	//! @brief ICR low register
	LAPIC_XAPIC_ICR_LOW_REG = 0x300 / 4,
	//! @brief ICR high register
	LAPIC_XAPIC_ICR_HIGH_REG = 0x310 / 4,
	//! @brief LVT CMCI register
	LAPIC_XAPIC_LVT_CMCI_REG = 0x2f0 / 4,
	//! @brief LVT timer register
	LAPIC_XAPIC_LVT_TIMER_REG = 0x320 / 4,
	//! @brief LVT thermal sensor register
	LAPIC_XAPIC_LVT_THERMAL_SENSOR_REG = 0x330 / 4,
	//! @brief LVT Performance monitoring register
	LAPIC_XAPIC_LVT_PERFMON_REG = 0x340 / 4,
	//! @brief LVT LINT0 register
	LAPIC_XAPIC_LVT_LINT0_REG = 0x350 / 4,
	//! @brief LVT LINT1 register
	LAPIC_XAPIC_LVT_LINT1_REG = 0x360 / 4,
	//! @brief LVT Error register
	LAPIC_XAPIC_LVT_ERR_REG = 0x370 / 4,
	//! @brief Initial count register
	LAPIC_XAPIC_INIT_CNT_REG = 0x380 / 4,
	//! @brief Current count register
	LAPIC_XAPIC_CNT_REG = 0x390 / 4,
	//! @brief Divide configuration register
	LAPIC_XAPIC_DCR_REG = 0x3e0 / 4,
	//! @brief EOI register
	LAPIC_XAPIC_EOI_REG = 0xb0 / 4,
	//! @brief Task priority register
	LAPIC_XAPIC_TPR_REG = 0x80 / 4,
};

//! @brief x2APIC MSRs
enum
{
	//! @brief Spurious interrupt vector MSR
	LAPIC_X2APIC_SPUR_REG = 0x80f,
	//! @brief LAPIC id register
	LAPIC_X2APIC_ID_REG = 0x802,
	//! @brief LAPIC version register
	LAPIC_X2APIC_VER_REG = 0x803,
	//! @brief ICR register
	LAPIC_X2APIC_ICR_REG = 0x830,
	//! @brief LVT CMCI register
	LAPIC_X2APIC_LVT_CMCI_REG = 0x82f,
	//! @brief LVT timer register
	LAPIC_X2APIC_LVT_TIMER_REG = 0x832,
	//! @brief LVT thermal sensor register
	LAPIC_X2APIC_LVT_THERMAL_SENSOR_REG = 0x833,
	//! @brief LVT Performance monitoring register
	LAPIC_X2APIC_LVT_PERFMON_REG = 0x834,
	//! @brief LVT LINT0 register
	LAPIC_X2APIC_LVT_LINT0_REG = 0x835,
	//! @brief LVT LINT1 register
	LAPIC_X2APIC_LVT_LINT1_REG = 0x836,
	//! @brief LVT Error register
	LAPIC_X2APIC_LVT_ERR_REG = 0x837,
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

//! @brief Physical base of LAPIC registers
static uint64_t ic_xapic_phys_base;

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
		} else {                                                                                   \
			val = (uint32_t)rdmsr(LAPIC_X2APIC_##reg##_REG);                                       \
		}                                                                                          \
		val;                                                                                       \
	})

//! @brief Generic LAPIC write function
#define LAPIC_WRITE(reg, val)                                                                      \
	do {                                                                                           \
		if (ic_state == IC_XAPIC_USED) {                                                           \
			ic_xapic_base[LAPIC_XAPIC_##reg##_REG] = (val);                                        \
		} else {                                                                                   \
			wrmsr(LAPIC_X2APIC_##reg##_REG, (uint32_t)val);                                        \
		}                                                                                          \
	} while (false)

//! @brief Handle spurious irq
void ic_handle_spur_irq(void) {
	LAPIC_WRITE(EOI, 0);
}

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void) {
	return LAPIC_READ(ID);
}

//! @brief Enable interrupt controller on AP
void ic_enable(void) {
	// Enable LAPIC
	if (ic_state == IC_X2APIC_USED) {
		uint64_t flags = rdmsr(LAPIC_IA32_APIC_BASE) & 0xfff;
		wrmsr(LAPIC_IA32_APIC_BASE, ic_xapic_phys_base | flags | X2APIC_ENABLE);
		wrmsr(LAPIC_X2APIC_SPUR_REG, LAPIC_ENABLE | ic_spur_vec);
	} else {
		ic_xapic_base[LAPIC_XAPIC_SPUR_REG] = LAPIC_ENABLE | ic_spur_vec;
	}
	// Zero out TPR
	LAPIC_WRITE(TPR, 0);
	// Get LAPIC version
	uint32_t raw_ver = LAPIC_READ(VER);
	// Read max LVT entry
	uint32_t max_lvt_entry = (raw_ver >> 16) & 255;
	// Zero out LVT
	if (max_lvt_entry >= 3) {
		LAPIC_WRITE(LVT_ERR, LAPIC_LVT_DISABLE_MASK | 0xff);
	}
	LAPIC_WRITE(LVT_LINT0, LAPIC_LVT_DISABLE_MASK | 0xff);
	LAPIC_WRITE(LVT_LINT1, LAPIC_LVT_DISABLE_MASK | 0xff);
	LAPIC_WRITE(LVT_TIMER, LAPIC_LVT_DISABLE_MASK | 0xff);
	if (max_lvt_entry >= 4) {
		LAPIC_WRITE(LVT_PERFMON, LAPIC_LVT_DISABLE_MASK | 0xff);
	}
	if (max_lvt_entry >= 5) {
		LAPIC_WRITE(LVT_THERMAL_SENSOR, LAPIC_LVT_DISABLE_MASK | 0xff);
	}
	if (max_lvt_entry >= 6) {
		LAPIC_WRITE(LVT_CMCI, LAPIC_LVT_DISABLE_MASK | 0xff);
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
	} else {
		wrmsr(LAPIC_X2APIC_ICR_REG, (((uint64_t)id) << 32ULL) | msg);
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
	ic_ipi_send_raw(id, (uint32_t)vec);
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
	ic_timer_tsc_deadline_detect();
	if (PER_CPU(ic_state).tsc_deadline_supported) {
		// Initialize timer LVT register
		LAPIC_WRITE(LVT_TIMER, LAPIC_TMR_TSC | ic_timer_vec);
	} else {
		// Set divider to 16
		LAPIC_WRITE(DCR, 0b1010);
		// Initialize timer LVT register
		LAPIC_WRITE(LVT_TIMER, LAPIC_TMR_ONE_SHOT | ic_timer_vec);
		// Initialize counter to 0xffffffff
		LAPIC_WRITE(INIT_CNT, 0xffffffff);
	}
}

//! @brief Finish timer calibration process. Should be called after
//! THREAD_TRAMPOLINE_CALIBRATION_PERIOD millseconds passed from ic_timer_start_calibration call
void ic_timer_end_calibration(void) {
	// TSC frequency will be calculated in sys/tsc.c, so no need to do anything for TSC deadline
	if (!PER_CPU(ic_state).tsc_deadline_supported) {
		// Calculate number of ticks per ms
		uint32_t val = LAPIC_READ(CNT);
		PER_CPU(ic_state).timer_ticks_per_us =
		    (0xffffffff - val) / (THREAD_TRAMPOLINE_CALIBRATION_PERIOD * 1000);
		// Stop LAPIC timer
		ic_timer_cancel_one_shot();
	}
}

//! @brief Prepare timer for one shot event
//! @param us Number of microseconds to wait
void ic_timer_one_shot(uint64_t us) {
	if (PER_CPU(ic_state).tsc_deadline_supported) {
		// TSC frequency can be non-invariant, but we only rely on timer being precise when CPU
		// is not in some low power state.
		wrmsr(LAPIC_IA32_TSC_DEADLINE_MSR, tsc_read() + PER_CPU(tsc_freq) * us);
	} else {
		LAPIC_WRITE(INIT_CNT, PER_CPU(ic_state).timer_ticks_per_us * us);
	}
}

//! @brief Acknowledge IC interrupt
void ic_ack(void) {
	LAPIC_WRITE(EOI, 0);
}
//! @brief Cancel one-shot timer event
void ic_timer_cancel_one_shot() {
	if (PER_CPU(ic_state).tsc_deadline_supported) {
		wrmsr(LAPIC_IA32_TSC_DEADLINE_MSR, 0);
	} else {
		LAPIC_WRITE(INIT_CNT, 0);
	}
}

//! @brief Initialize interrupt controller on BSP
static void ic_bsp_init(void) {
	// First of all, let's determine if LAPIC is supported at all
	struct cpuid cpuid1h;
	cpuid(0x1, 0, &cpuid1h);
	if ((cpuid1h.edx & (1 << 9)) == 0) {
		PANIC("ricercaOS kernel requires LAPIC to run");
	}
	ic_state = IC_XAPIC_USED;
	// Query x2APIC support
	if ((cpuid1h.ecx & (1 << 21)) != 0) {
		LOG_INFO("x2APIC support detected");
		ic_state = IC_X2APIC_USED;
	}
	ic_xapic_phys_base = rdmsr(LAPIC_IA32_APIC_BASE) & (~0xfffULL);
	ic_xapic_base = (volatile uint32_t *)(mem_wb_phys_win_base + ic_xapic_phys_base);
	LOG_INFO("xAPIC address: 0x%p", ic_xapic_base);
	// Enable LAPIC
	ic_enable();
}
