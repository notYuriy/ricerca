//! @file target.h
//! @brief File containing subsystem initialization and dependency resolution helpers definitions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("initgraph")

//! @brief Reach subsystem target
void target_reach(struct target *target) {
	// Generate init plan
	struct target *stack_top = target;
	target->next = NULL;
	while (stack_top->status == TARGET_UNRESOVLED) {
		struct target *current = stack_top;
		current->status = TARGET_WAITING_FOR_DEPS;
		// Iterate over all dependencies and push them on resolution stack
		for (size_t i = 0; i < current->deps_count; ++i) {
			struct target *dependency = current->deps[i];
			if (dependency->status == TARGET_WAITING_FOR_DEPS) {
				PANIC("Circular dependency detected while resolving dependency \"%s\" of \"%s\"",
				      dependency->name, target->name);
			}
			if (dependency->status == TARGET_UNRESOVLED) {
				dependency->next = stack_top;
				stack_top = dependency;
			}
		}
	}
	// Dump plan
	LOG_INFO("Running the following plan");
	struct target *current = stack_top;
	while (current != NULL) {
		log_printf("* \033[33m\"%s\"\033[0m\n", current->name);
		current = current->next;
	}
	// Execute plan
	current = stack_top;
	while (current != NULL) {
		current->callback();
		LOG_SUCCESS("Target \033[33m\"%s\"\033[0m reached", current->name);
		current = current->next;
	}
}
