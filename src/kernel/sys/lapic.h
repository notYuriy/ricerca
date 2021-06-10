//! @file lapic.h
//! @brief File containing LAPIC functions declarations

#pragma once

#include <lib/target.h>

//! @brief Enable LAPIC
void lapic_enable(void);

//! @brief Get APIC id
uint32_t lapic_get_apic_id(void);

//! @brief Export LAPIC BSP init target
EXPORT_TARGET(lapic_bsp_target)
