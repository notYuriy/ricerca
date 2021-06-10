//! @file target.h
//! @brief File containing subsystem initialization and dependency resolution helpers declarations

#pragma once

#include <misc/attributes.h>
#include <misc/misc.h>
#include <misc/types.h>

//! @brief Initgraph target
struct target {
	//! @brief Pointer to array of pointers to depenent targets
	struct target **deps;
	//! @brief Target count
	size_t deps_count;
	//! @brief Target status
	enum {
		//! @brief Target is yet to be resolved
		TARGET_UNRESOVLED,
		//! @brief Target has been visited, but it has not been resolved
		TARGET_WAITING_FOR_DEPS,
		//! @brief Target has been satisfied
		TARGET_RESOLVED,
	} status;
	//! @brief Subsystem initialization callback
	void (*callback)();
	//! @brief Subsystem name
	const char *name;
	//! @brief Next to resolve
	struct target *next;
};

//! @brief Define module
//! @param name Module name
#define MODULE(name) static attribute_maybe_unused const char *module_name = name;

//! @brief Export subsystem milestone
//! @param target_name Target of the subsystem to be imported
#define EXPORT_TARGET(target_name) extern struct target target_name[1];

//! @brief Define misc target
//! @param target_name Target name
//! @param init_callback Init callback
//! @param ... Dependencies
#define TARGET(target_name, init_callback, ...)                                                    \
	static struct target *deps[] = {__VA_ARGS__};                                                  \
	static void init_callback(void);                                                               \
	struct target target_name[1] = {(struct target){                                               \
	    .deps = deps,                                                                              \
	    .deps_count = ARRAY_SIZE(deps),                                                            \
	    .status = TARGET_UNRESOVLED,                                                               \
	    .callback = init_callback,                                                                 \
	    .name = #init_callback,                                                                    \
	    .next = NULL,                                                                              \
	}};

//! @brief Reach subsystem target
void target_reach(struct target *target);
