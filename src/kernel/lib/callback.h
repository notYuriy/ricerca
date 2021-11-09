//! @file callback.h
//! @brief Callback types

#include <misc/types.h>

#pragma once

//! @brief Callback with no return type
struct callback_void {
	//! @brief Function to run
	void (*func)(void *);
	//! @brief Context pointer
	void *ctx;
};

//! @brief Assemble callback
//! @param _func Function to run
//! @param _ctx Context pointer
#define CALLBACK_VOID(_func, _ctx)                                                                 \
	((struct callback_void){.func = (void *)(_func), .ctx = (void *)(_ctx)})

//! @brief Null void callback
#define CALLBACK_VOID_NULL ((struct callback_void){.func = NULL})

//! @brief Run void callback
//! @param callback Void callback
static inline void callback_void_run(struct callback_void callback) {
	if (callback.func != NULL) {
		callback.func(callback.ctx);
	}
}
