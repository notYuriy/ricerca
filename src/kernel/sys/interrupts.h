//! @file interrupts.h
//! @brief File containing for interrupts enable/disable functions

#pragma once

#include <misc/attributes.h> // For bool

//! @brief Disable interrupts
//! @return True if interrupts were enabled
bool interrupts_disable();

//! @brief Enable interrupts if true is passed
//! @param status True if interrupts should be enabled
void interrupts_enable(bool status);
