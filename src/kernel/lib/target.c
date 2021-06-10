//! @file target.h
//! @brief File containing subsystem initialization and dependency resolution helpers definitions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("initgraph")

//! @brief Reach subsystem target
void target_reach(struct target *target) {
	struct target *stack_top = target;
	target->next = NULL;
	while (stack_top != NULL) {
		struct target *current = stack_top;
		// Iterate over all dependencies and push them on resolution stack
		ASSERT(current->status != TARGET_RESOLVED, "Resolved target on resolution stack");
		if (current->status == TARGET_UNRESOVLED) {
			current->status = TARGET_WAITING_FOR_DEPS;
			for (size_t i = 0; i < current->deps_count; ++i) {
				struct target *dependency = current->deps[i];
				if (dependency->status == TARGET_WAITING_FOR_DEPS) {
					PANIC(
					    "Circular dependency detected while resolving dependency \"%s\" of \"%s\"",
					    dependency->name, target->name);
				}
				if (dependency->status == TARGET_UNRESOVLED) {
					dependency->next = stack_top;
					stack_top = dependency;
				}
			}
		} else if (current->status == TARGET_WAITING_FOR_DEPS) {
			stack_top = stack_top->next;
			current->callback();
			current->status = TARGET_RESOLVED;
			LOG_SUCCESS("Target \"%s\" reached", current->name);
		}
	}
}
