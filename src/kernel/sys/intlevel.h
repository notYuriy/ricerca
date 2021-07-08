//! @file intlsevel.h
//! @brief File containing for interrupts enable/disable functions

#pragma once

#include <misc/attributes.h>

//! @brief Disable interrupts
//! @return True if interrupts were enabled
bool intlevel_elevate();

//! @brief Enable interrupts if true is passed
//! @param status True if interrupts should be enabled
void intlevel_recover(bool status);
