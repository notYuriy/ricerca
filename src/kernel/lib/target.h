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
	//! @brief Subsystem initialization callback
	void (*callback)();
	//! @brief Subsystem name
	const char *name;
	//! @brief Next to resolve
	struct target *next;
	//! @brief Next to be visited
	struct target *next_to_be_visited;
	//! @brief Current dependency being enumerated
	size_t dep_index;
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
	static struct target *target_name##deps[] = __VA_ARGS__;                                       \
	static void init_callback(void);                                                               \
	struct target target_name[1] = {(struct target){                                               \
	    .deps = target_name##deps,                                                                 \
	    .deps_count = ARRAY_SIZE(target_name##deps),                                               \
	    .callback = init_callback,                                                                 \
	    .name = #target_name,                                                                      \
	    .next = NULL,                                                                              \
	    .dep_index = 0,                                                                            \
	}};

//! @brief Compute plan to reach target
//! @param target Target to be reached
//! @return Linked list of targets to execute
//! @note Panics if circular dependency is detected
struct target *target_compute_plan(struct target *target);

//! @brief Execute plan
//! @param plan Plan to be executed returned from target_compute_plan
void target_execute_plan(struct target *plan);

//! @brief Dump plan to kernel log
//! @param plan Plan to be printed
void target_plan_dump(struct target *plan);

//! @brief Dummy callback name for meta packages
#define META_DUMMY target_dummy

//! @brief Define dummy callback
#define META_DEFINE_DUMMY()                                                                        \
	static void target_dummy() {                                                                   \
	}
