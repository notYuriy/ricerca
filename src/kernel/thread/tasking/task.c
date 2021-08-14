//! @file task.c
//! @brief File containing helpers for task management

#include <lib/string.h>
#include <mem/heap/heap.h>
#include <sys/arch/gdt.h>
#include <thread/smp/core.h>
#include <thread/tasking/task.h>

//! @brief Create task with a given entrypoint
//! @param callback Void callback
//! @return Pointer to the created task or NULL on failure
struct thread_task *thread_task_create_call(struct callback_void callback) {
	struct thread_task *task = mem_heap_alloc(sizeof(struct thread_task));
	if (task == NULL) {
		return NULL;
	}
	void *stack = mem_heap_alloc(THREAD_TASK_STACK_SIZE);
	if (stack == NULL) {
		mem_heap_free(task, sizeof(struct thread_task));
		return NULL;
	}
	memset(task, 0, sizeof(struct thread_task));
	task->frame.cs = GDT_CODE64;
	task->frame.ss = GDT_DATA64;
	task->frame.rip = (uint64_t)callback.func;
	task->frame.rdi = (uint64_t)callback.ctx;
	task->frame.rflags = (1 << 9); // Enable interrupts

	task->stack = (uintptr_t)stack + THREAD_TASK_STACK_SIZE;
	task->frame.rsp = task->stack;
	return task;
}

//! @brief Dispose task
//! @param task Pointer to the task
void thread_task_dispose(struct thread_task *task) {
	// Free task stack
	mem_heap_free((void *)(task->stack - THREAD_TASK_STACK_SIZE), THREAD_TASK_STACK_SIZE);
	// Free task structure itself
	mem_heap_free(task, sizeof(struct thread_task));
}
