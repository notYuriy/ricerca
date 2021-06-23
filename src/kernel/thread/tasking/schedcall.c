//! @file schedcall.c
//! @brief File containing implementation of stack call routines

#include <lib/log.h>
#include <sys/arch/arch.h>
#include <sys/arch/interrupts.h>
#include <sys/arch/tss.h>
#include <thread/tasking/schedcall.h>

MODULE("thread/tasking/schedcall")
TARGET(thread_sched_call_available, thread_sched_call_init, {arch_available})

//! @brief Sched call vector
static uint8_t thread_sched_call_vec = 0x57;

//! @brief Interrupt handler for sched call gate
static void thread_sched_call_gate_handler(struct interrupt_frame *frame, void *ctx) {
	(void)ctx;
	interrupt_callback_t callback = (interrupt_callback_t)frame->rdi;
	void *arg = (void *)frame->rsi;
	callback(frame, arg);
}

//! @brief Call routine on scheduler stack
//! @param func Pointer to the function to be called
//! @param arg Pointer to the function argument
void thread_sched_call(interrupt_callback_t func, void *arg) {
	asm volatile("int $0x57" ::"D"(func), "S"(arg));
}

//! @brief Initialize sched stack call subsystem
static void thread_sched_call_init() {
	interrupt_register_handler(thread_sched_call_vec, thread_sched_call_gate_handler, NULL, 0, 0,
	                           true);
}
