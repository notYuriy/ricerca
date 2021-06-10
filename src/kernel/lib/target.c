//! @file target.h
//! @brief File containing subsystem initialization and dependency resolution helpers definitions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("initgraph")

//! @brief Reach subsystem target
void target_reach(struct target *target) {
	if (target->status == TARGET_RESOLVED) {
		return;
	}
	// Generate init plan
	struct target *stack_top = target;
	target->next = NULL;
	target->next_to_be_visited = NULL;
	struct target *run_list = NULL;
	struct target *run_list_tail = NULL;

	while (stack_top != NULL) {
		struct target *current = stack_top;
		if (current->status == TARGET_UNRESOVLED) {
			current->status = TARGET_WAITING_FOR_DEPS;
			// Iterate over all dependencies and push them on resolution stack
			for (size_t i = 0; i < current->deps_count; ++i) {
				struct target *dependency = current->deps[i];
				if (dependency->status == TARGET_WAITING_FOR_DEPS) {
					PANIC(
					    "Circular dependency detected while resolving dependency \"%s\" of \"%s\"",
					    dependency->name, target->name);
				}
				if (dependency->status == TARGET_UNRESOVLED) {
					dependency->next_to_be_visited = stack_top;
					stack_top = dependency;
				}
			}
		} else if (current->status == TARGET_WAITING_FOR_DEPS) {
			// Dependencies have been satisfied, so target can be added to run list and removed from
			// the stack
			current->status = TARGET_RESOLVED;
			current->next = NULL;
			if (run_list == NULL) {
				run_list = run_list_tail = current;
			} else {
				run_list_tail->next = current;
				run_list_tail = current;
			}
			stack_top = stack_top->next_to_be_visited;
		} else {
			PANIC("Resolved node enqueued");
		}
	}

	// Dump plan
	LOG_INFO("Running the following plan");
	struct target *current = run_list;
	while (current != NULL) {
		log_printf("* \033[33m\"%s\"\033[0m\n", current->name);
		current = current->next;
	}

	// Execute plan
	current = run_list;
	while (current != NULL) {
		current->callback();
		LOG_SUCCESS("Target \033[33m\"%s\"\033[0m reached", current->name);
		current = current->next;
	}
}
