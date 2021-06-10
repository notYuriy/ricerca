//! @file attributes.h
//! @brief File containing commonly used attributes

#pragma once

//! @brief Packs the structure
#define attribute_packed __attribute__((__packed__))

//! @brief Indicates that definition may be not used
#define attribute_maybe_unused __attribute__((__unused__))
