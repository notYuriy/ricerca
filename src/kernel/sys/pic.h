//! @file pic.h
//! @brief File containing PIC remappping target

#pragma once

#include <lib/target.h>

//! @brief Acknowledge PIC interrupt
void pic_irq_ack(uint8_t irq);

//! @brief Export PIC remap target
EXPORT_TARGET(pic_remap_available)
