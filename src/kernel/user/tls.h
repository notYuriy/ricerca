//! @file tls.h
//! @brief File containing thread-local storage API implementation for userspace

#pragma once

#include <misc/types.h>
#include <user/status.h>

//! @brief TLS table. Allows for thread-local key-value storage
struct user_tls_table;

//! @brief Create TLS table
//! @param table buffer to store pointer to the table in
//! @return API status
int user_tls_table_create(struct user_tls_table **table);

//! @brief Set TLS key
//! @param table Pointer to the table
//! @param key TLS key
//! @param value value
//! @return API status
int user_tls_table_set_key(struct user_tls_table *table, size_t key, size_t value);

//! @brief Get TLS key
//! @param table Pointer to the table
//! @param key TLS key
//! @return Value at key or 0 if key is not present
size_t user_tls_table_get_key(struct user_tls_table *table, size_t key);
