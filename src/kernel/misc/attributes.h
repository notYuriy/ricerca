//! @file attributes.h
//! @brief File containing commonly used attributes

#pragma once

//! @brief Packs the structure
#define attribute_packed __attribute__((__packed__))

//! @brief Indicates that definition may remain used
#define attribute_maybe_unused __attribute__((__unused__))

//! @brief Indicates that instrument calls should not be generated for this procedure
#define attribute_no_instrument __attribute__((no_instrument_function))

//! @brief Indicates that function can not be inlined
#define attribute_noinline __attribute__((noinline))

//! @brief Indicates that the function will never return
#define attribute_noreturn _Noreturn
