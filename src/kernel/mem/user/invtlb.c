//! @file invtlb.c
//! @brief File containing code TLB maintenance subsystem

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <lib/target.h>
#include <mem/heap/heap.h>
#include <mem/user/invtlb.h>
#include <misc/atomics.h>
#include <misc/types.h>
#include <sys/cr.h>
#include <thread/locking/spinlock.h>
#include <thread/smp/core.h>

MODULE("mem/user/invtlb")
TARGET(mem_user_invtlb_available, mem_user_invtlb_init,
       {thread_smp_core_available, mem_heap_available})

//! @brief Idle state
#define MEM_USER_INVTLB_STATE_IDLE 2

//! @brief Current pending state
static uint8_t mem_user_invtlb_pending_state = 0;

//! @brief Core states
static uint8_t *mem_user_invtlb_states;

//! @brief Number of idle cores
static size_t mem_user_invtlb_idle_cores = 0;

//! @brief Number of cores pending TLB update
static size_t mem_user_invtlb_pending_tlb_updates = 0;

//! @brief Is global invalidation pending?
static bool mem_user_invtlb_pending = false;

//! @brief TLB subsystem lock
static struct thread_spinlock mem_user_invtlb_lock;

//! @brief Get pending state
static inline uint8_t mem_user_invtlb_get_pending_state() {
	return ATOMIC_ACQUIRE_LOAD(&mem_user_invtlb_pending_state);
}

//! @brief Flip state
static inline uint8_t mem_user_invtlb_flip_state(uint8_t pending) {
	return 1 - pending;
}

//! @brief Flip states
static inline void mem_user_invtlb_flip_states() {
	ATOMIC_RELEASE_STORE(&mem_user_invtlb_pending_state, 1 - mem_user_invtlb_pending_state);
}

//! @brief Initialize invtlb subsystem
static void mem_user_invtlb_init(void) {
	mem_user_invtlb_states = mem_heap_alloc(thread_smp_core_max_cpus);
	if (mem_user_invtlb_states == NULL) {
		PANIC("Failed to allocate core state table");
	}
	memset(mem_user_invtlb_states, mem_user_invtlb_pending_state, thread_smp_core_max_cpus);
	mem_user_invtlb_lock = THREAD_SPINLOCK_INIT;
}

//! @brief Ack pending global invalidation
//! @return ACK status
static enum
{
	//! @brief No invalidation required
	MEM_USER_INVTLB_NO_INVALIDATION_REQUIRED,
	//! @brief CR3 flush needed
	MEM_USER_INVTLB_FLUSH_CR3,
	//! @brief CR3 flush + generation update needed
	MEM_USER_INVTLB_GEN_UPDATE_PENDING,
} mem_user_invtlb_ack(void) {
	uint8_t pending_state = mem_user_invtlb_get_pending_state();
	if (mem_user_invtlb_states[PER_CPU(logical_id)] == pending_state) {
		mem_user_invtlb_states[PER_CPU(logical_id)] = mem_user_invtlb_flip_state(pending_state);
		size_t cores_left = ATOMIC_FETCH_DECREMENT(&mem_user_invtlb_pending_tlb_updates) - 1;
		if (cores_left == 0) {
			return MEM_USER_INVTLB_GEN_UPDATE_PENDING;
		}
		return MEM_USER_INVTLB_FLUSH_CR3;
	}
	return MEM_USER_INVTLB_NO_INVALIDATION_REQUIRED;
}

//! @brief Perform generation update without taking
static void mem_user_invtlb_gen_update_nolock(void) {
	mem_user_invtlb_pending = false;
	LOG_INFO("TODO: gen update code");
}

//! @brief Update cr3
//! @param old_cr3 Old CR3 value (can be set to 0 if unknown)
//! @param new_cr3 New CR3 value
//! @note Runs with ints disabled
void mem_user_invtlb_update_cr3(uint64_t old_cr3, uint64_t new_cr3) {
	switch (mem_user_invtlb_ack()) {
	case MEM_USER_INVTLB_GEN_UPDATE_PENDING:
		thread_spinlock_grab(&mem_user_invtlb_lock);
		mem_user_invtlb_gen_update_nolock();
		thread_spinlock_ungrab(&mem_user_invtlb_lock);
		// fall through
	case MEM_USER_INVTLB_FLUSH_CR3:
		wrcr3(new_cr3);
		break;
	default:
		if (old_cr3 != new_cr3) {
			wrcr3(new_cr3);
		}
		break;
	}
}

//! @brief Notify invtlb subsystem that core enters idle state
//! @note Runs with ints disabled
void mem_user_invtlb_on_idle_enter(void) {
	thread_spinlock_grab(&mem_user_invtlb_lock);
	if (mem_user_invtlb_ack() == MEM_USER_INVTLB_GEN_UPDATE_PENDING) {
		mem_user_invtlb_gen_update_nolock();
	}
	mem_user_invtlb_idle_cores++;
	mem_user_invtlb_states[PER_CPU(logical_id)] = MEM_USER_INVTLB_STATE_IDLE;
	thread_spinlock_ungrab(&mem_user_invtlb_lock);
}

//! @brief Notify invtlb subsystem that core exits idle state
//! @note Runs with ints disabled
void mem_user_invtlb_on_idle_exit() {
	thread_spinlock_grab(&mem_user_invtlb_lock);
	mem_user_invtlb_idle_cores--;
	mem_user_invtlb_states[PER_CPU(logical_id)] =
	    mem_user_invtlb_flip_state(mem_user_invtlb_pending_state);
	thread_spinlock_ungrab(&mem_user_invtlb_lock);
}

//! @brief Request global invalidation
void mem_user_invtlb_request(void) {
	const bool int_state = thread_spinlock_lock(&mem_user_invtlb_lock);
	if (mem_user_invtlb_pending) {
		thread_spinlock_unlock(&mem_user_invtlb_lock, int_state);
		return;
	}
	ATOMIC_RELEASE_STORE(&mem_user_invtlb_pending_tlb_updates,
	                     thread_smp_core_max_cpus - mem_user_invtlb_idle_cores);
	mem_user_invtlb_flip_states();
	thread_spinlock_ungrab(&mem_user_invtlb_lock);
	switch (mem_user_invtlb_ack()) {
	case MEM_USER_INVTLB_GEN_UPDATE_PENDING:
		thread_spinlock_grab(&mem_user_invtlb_lock);
		mem_user_invtlb_gen_update_nolock();
		thread_spinlock_ungrab(&mem_user_invtlb_lock);
		break;
	default:
		break;
	}
	intlevel_recover(int_state);
}
