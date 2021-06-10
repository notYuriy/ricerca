//! @file lapic.h
//! @brief File containing LAPIC functions declarations

#pragma once

#include <lib/target.h>

//! @brief Initiailize lapic on AP
void lapic_ap_init(void);

//! @brief Export LAPIC BSP init target
EXPORT_TARGET(lapic_bsp_target)
