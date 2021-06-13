//! @file ic.h
//! @brief File containing interrupt controller abstraction

#pragma once

#include <lib/target.h>

//! @brief Enable interrupt controller on AP
void ic_enable(void);

//! @brief Get interrupt controller ID
uint32_t ic_get_apic_id(void);

//! @brief Export interrupt controller BSP init target
EXPORT_TARGET(ic_bsp_target)
