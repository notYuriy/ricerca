//! @file invtlb.h
//! @brief File containing declarations related to TLB maintenance infrastructure

#pragma once

#include <lib/target.h>
#include <misc/types.h>

//! @brief Update cr3
//! @param old_cr3 Old CR3 value (can be set to 0 if unknown)
//! @param new_cr3 New CR3 value
//! @note Runs with ints disabled
void mem_virt_invtlb_update_cr3(uint64_t old_cr3, uint64_t new_cr3);

//! @brief Notify invtlb subsystem that core enters idle state
//! @note Runs with ints disabled
void mem_virt_invtlb_on_idle_enter(void);

//! @brief Notify invtlb subsystem that core exits idle state
//! @note Runs with ints disabled
void mem_virt_invtlb_on_idle_exit();

//! @brief Request global invalidation
//! @note Caller should switch CR3 on its own
void mem_virt_invtlb_request(void);

//! @brief Target for TLB maintenance subsystem initialization
EXPORT_TARGET(mem_virt_invtlb_available)
