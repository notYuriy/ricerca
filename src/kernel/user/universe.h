//! @file universe.h
//! @brief File containing declarations related to universes

#pragma once

#include <user/object.h>
#include <user/status.h>

//! @brief Universe. Addressable collection of object references
struct user_universe;

//! @brief Create new universe
//! @param universe Buffer to store reference to the new universe in
//! @return API status
int user_create_universe(struct user_universe **universe);

//! @brief Allocate cell for new reference
//! @param universe Universe to allocate cell in
//! @param ref Reference to store
//! @param cell Buffer to store cell index in
//! @return API status
int user_allocate_cell(struct user_universe *universe, struct user_ref ref, size_t *cell);

//! @brief Allocate two cells for two references
//! @param universe Universe to allocate cells in
//! @param refs Array of two references to store
//! @param cells Array of two cell indices to store results in
//! @return API status
int user_allocate_cell_pair(struct user_universe *universe, struct user_ref *refs, size_t *cells);

//! @brief Drop reference at index
//! @param universe Universe to drop reference from
//! @param cell Index of cell
//! @return API status
void user_drop_cell(struct user_universe *universe, size_t cell);

//! @brief Borrow reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_borrow_ref(struct user_universe *universe, size_t cell, struct user_ref *buf);

//! @brief Move out reference to the object at index
//! @param universe Universe to borrow reference from
//! @param cell Index of cell with the reference
//! @param buf Buffer to store borrowed reference in
//! @return API status
int user_move_out_ref(struct user_universe *universe, size_t cell, struct user_ref *buf);