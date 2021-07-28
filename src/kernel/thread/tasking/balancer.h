//! @file balancer.h
//! @brief File containing load balancing declarations

#include <lib/target.h>
#include <thread/tasking/task.h>

//! @brief Run task on any core
//! @param task Pointer to the task
void thread_balancer_allocate_to_any(struct thread_task *task);

//! @brief Export target for load balancer initialization
EXPORT_TARGET(thread_balancer_available)
