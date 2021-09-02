//! @file cookie.h
//! @brief File containing declarations of security cookie functions

#pragma once

#include <misc/types.h>
#include <user/status.h>

//! @brief Group cookie object
struct user_group_cookie;

//! @brief API entry cookie. Used by sys_* functions to check perms
struct user_entry_cookie;

//! @brief Cookie key. Used to store info about required credentials.
typedef size_t user_cookie_key_t;

//! @brief Kernel cookie key. No user process can authentificate
#define USER_COOKIE_KEY_ONLY_KERNEL 0

//! @brief Universal cookie key. No restrictions on who can authentificate apply
#define USER_COOKIE_KEY_UNIVERSAL 1

//! @brief Create new group cookie object
//! @param buf Buffer to store pointer to the new security cookie in
//! @return API status
int user_group_cookie_create(struct user_group_cookie **buf);

//! @brief Create new API entry cookie object
//! @param buf Buffer to store pointer to the new API entry cookie in
//! @return API status
int user_entry_cookie_create(struct user_entry_cookie **buf);

//! @brief Transfer group cookie perms to entry cookie
//! @param entry Pointer to the entry cookie object
//! @param grp Pointer to the group cookie object
//! @return API status
int user_entry_cookie_add_to_grp(struct user_entry_cookie *entry, struct user_group_cookie *cookie);

//! @brief Remove group perms from entry cookie
//! @param entry Pointer to the entry cookie object
//! @param grp Pointer to the group cookie object
//! @return API status
int user_entry_cookie_remove_from_grp(struct user_entry_cookie *entry,
                                      struct user_group_cookie *cookie);

//! @brief Check perms for entry cookie
//! @param entry Pointer to the user API entry cookie
//! @param key Cookie key
//! @return True if authentification was successful, false otherwise
bool user_entry_cookie_auth(struct user_entry_cookie *entry, user_cookie_key_t key);

//! @brief Get group cookie key
//! @param grp Pointer to the group cookie
//! @return Cookie key
user_cookie_key_t user_group_cookie_get_key(struct user_group_cookie *grp);

//! @brief Get entry cookie key
//! @param entry Pointer to the user API entry cookie
//! @return Cookie key
user_cookie_key_t user_entry_cookie_get_key(struct user_entry_cookie *entry);
