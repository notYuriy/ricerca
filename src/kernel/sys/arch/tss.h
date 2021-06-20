//! @file tss.h
//! @brief Declarations related to Task State Segment table

#pragma once

#include <lib/string.h>
#include <misc/attributes.h>
#include <misc/types.h>

//! @brief Task stack IST
#define TSS_TASK_IST 0

//! @brief Interrupt stack IST
#define TSS_INT_IST 1

//! @brief Scheduler stack IST
#define TSS_SCHED_IST 2

//! @brief TSS
struct tss {
	uint32_t reserved1;
	uint64_t rsp[3];
	uint64_t reserved2;
	uint64_t ist[7];
	uint64_t reserved3;
	uint32_t reserved4;
	uint16_t reserved5;
	uint16_t io_map_base_addr;
};

//! @brief Initilaize TSS
static inline void tss_fill(struct tss *tss) {
	memset(tss, 0, sizeof(struct tss));
	tss->io_map_base_addr = sizeof(struct tss);
}

//! @brief Set scheduler stack
//! @param tss Pointer to the TSS
//! @param stack Stack top
static inline void tss_set_sched_stack(struct tss *tss, uintptr_t stack) {
	tss->ist[TSS_SCHED_IST - 1] = stack;
}

//! @brief Set interrupt stack
//! @param tss Pointer to the TSS
//! @param stack Stack top
static inline void tss_set_int_stack(struct tss *tss, uintptr_t stack) {
	tss->ist[TSS_INT_IST - 1] = stack;
}

//! @brief Set task stack
//! @param tss Pointer to the TSS
//! @param stack Stack top
static inline void tss_set_task_stack(struct tss *tss, uintptr_t stack) {
	tss->rsp[0] = stack;
}
