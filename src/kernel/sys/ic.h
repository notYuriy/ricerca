//! @file ic.h
//! @brief File containing interrupt controller abstraction

#pragma once

#include <lib/target.h>
#include <misc/types.h>

//! @brief Spurious interrupt vector
extern uint8_t ic_spur_vec;

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

//! @brief Export interrupt controller BSP init target
EXPORT_TARGET(ic_bsp_available)
