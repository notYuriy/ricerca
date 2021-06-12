//! @file target.h
//! @brief File containing subsystem initialization and dependency resolution helpers definitions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>

MODULE("initgraph")

//! @brief Compute plan to reach target
//! @param target Target to be reached
//! @return Linked list of targets to execute
//! @note Panics if circular dependency is detected
struct target *target_compute_plan(struct target *target) {
	struct target *head = NULL;
	struct target *tail = NULL;
	struct target *stack = target;
	target->next_to_be_visited = NULL;
	while (stack != NULL) {
		struct target *tos = stack;
		if (tos->dep_index == tos->deps_count) {
			// Resolved
			stack = stack->next_to_be_visited;
			tos->resolved = true;
			tos->next = NULL;
			if (head == NULL) {
				head = tos;
				tail = tos;
			} else {
				tail->next = tos;
				tail = tos;
			}
		} else {
			struct target *dependency = tos->deps[tos->dep_index++];
			if (dependency->resolved) {
				continue;
			}
			if (dependency->dep_index != 0) {
				PANIC("Circular dependency \"%s\" detected while resolving dependencies of \"%s\"",
				      dependency->name, tos->name);
			}
			dependency->next_to_be_visited = stack;
			stack = dependency;
		}
	}
	return head;
}

//! @brief Execute plan
//! @param plan Plan to be executed returned from target_compute_plan
void target_execute_plan(struct target *plan) {
	struct target *current = plan;
	while (current != NULL) {
		current->callback();
		LOG_INFO("Target \033[33m\"%s\"\033[0m reached", current->name);
		current = current->next;
	}
}

//! @brief Dump plan to kernel log
//! @param plan Plan to be printed
void target_plan_dump(struct target *plan) {
	LOG_INFO("Running the following plan");
	struct target *current = plan;
	while (current != NULL) {
		log_printf("* \033[33m\"%s\"\033[0m\n", current->name);
		current = current->next;
	}
}
