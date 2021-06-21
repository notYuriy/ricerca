//! @file ic.h
//! @brief File containing interrupt controller abstraction

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <sys/timers/timer.h>

//! @brief Timer calibration period in milliseconds
#define IC_TIMER_CALIBRATION_PERIOD 10

//! @brief Per-cpu interrupt controller state
struct ic_core_state {
	//! @brief Number of ticks in one millisecond for IC timer
	uint32_t timer_ticks_per_ms;
	//! @brief TSC deadline mode support
	bool tsc_deadline_supported;
	//! @brief Number of ticks in one millisecond for invariant TSC
	uint64_t tsc_freq;
	//! @brief TSC buffer. Used for timer initialization in TSC mode
	uint64_t tsc_buf;
};

//! @brief Spurious interrupt vector
extern uint8_t ic_spur_vec;

//! @brief Timer interrupt vector
extern uint8_t ic_timer_vec;

//! @brief Handle spurious irq
void ic_handle_spur_irq(void);

//! @brief Enable interrupt controller on AP
void ic_enable(void);

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void);

//! @brief Send init IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
void ic_send_init_ipi(uint32_t id);

//! @brief Send startup IPI to the core with the given APIC ID
//! @param id APIC id of the core to be waken up
//! @param addr Trampoline address (should be <1MB and divisible by 0x1000)
void ic_send_startup_ipi(uint32_t id, uint32_t addr);

//! @brief Send IPI to the core with the given APIC ID
//! @param id APIC id of the core
//! @param vec Interrupt vector to be triggered when message is recieved
//! @note IPIs to the current core are ignored
void ic_send_ipi(uint32_t id, uint8_t vec);

//! @brief Initiate timer calibration process
void ic_timer_start_calibration(void);

//! @brief Finish timer calibration process. Should be called after IC_TIMER_CALIBRATION_PERIOD
//! millseconds passed from ic_timer_start_calibration call
void ic_timer_end_calibration(void);

//! @brief Prepare timer for one shot event
//! @param ms Number of milliseconds to wait
void ic_timer_one_shot(uint32_t ms);

//! @brief Acknowledge timer interrupt
void ic_timer_ack(void);

//! @brief Cancel one-shot timer event
void ic_timer_cancel_one_shot();

//! @brief Get timer counter delta.
//! @note Value of delta is reset after every ic_timer_one_shot

//! @brief Export interrupt controller BSP init target
EXPORT_TARGET(ic_bsp_available)
