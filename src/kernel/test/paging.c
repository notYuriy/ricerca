//! @file rpc.c
//! @brief File containing code for paging test

#include <thread/tasking/tasking.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/balancer.h>
#include <thread/smp/core.h>
#include <mem/virt/paging.h>
#include <mem/phys/phys.h>
#include <lib/target.h>
#include <lib/panic.h>
#include <misc/atomics.h>

MODULE("test/paging")

//! @brief Pointer to the paging root;
static struct mem_paging_root *test_paging_root;

//! @brief Number of threads yet to finish
size_t test_paging_yet_to_finish = 0;

//! @brief Paging test helper thread
static void test_paging_thread(uintptr_t vaddr) {
    // Allocate one page
    uintptr_t newpage = mem_phys_alloc_on_behalf(0x1000, PER_CPU(numa_id));
    ASSERT(newpage != PHYS_NULL, "Failed to allocate page for paging test");
    // Map it
    bool res = mem_paging_map_at(test_paging_root, vaddr, newpage, MEM_PAGING_READABLE | MEM_PAGING_USER | MEM_PAGING_WRITABLE);
    ASSERT(res, "Failed to map 0x%p at 0x%p", newpage, vaddr);
    // Yield
    thread_localsched_yield();
    // Unmap it
    uintptr_t val = mem_paging_unmap_at(test_paging_root, vaddr);
    ASSERT(val == newpage, "Invalid physical address returned from mem_paging_unmap_at (expected 0x%p, got 0x%p)", newpage, val);
    // Signal task termination to main test task
    ATOMIC_FETCH_INCREMENT(&test_paging_yet_to_finish);
    // Terminate current thread
    thread_localsched_terminate();
}

//! @brief Test thread count
#define TEST_PAGING_THREADS_NO 1

//! @brief Paging test
void test_paging() {
    // Create new paging context
    test_paging_root = mem_paging_new_root();
    ASSERT(test_paging_root != NULL, "Failed to allocate paging root for paging test");
    // Remember current cr3
    uintptr_t cr3 = rdcr3();
    // Switch to the new paging context
    mem_paging_switch_to(test_paging_root);
    // Start up TEST_PAGING_THREADS_NO threads
    for (size_t i = 0; i < TEST_PAGING_THREADS_NO; ++i) {
        struct thread_task *task = thread_task_create_call(CALLBACK_VOID(test_paging_thread, i * 0x1000));
        ASSERT(task != NULL, "Failed to allocate test thread for paging test");
        thread_balancer_allocate_to_any(task);
    }
    // Wait for threads to finish
    while (ATOMIC_ACQUIRE_LOAD(&test_paging_yet_to_finish) < TEST_PAGING_THREADS_NO) {
        asm volatile("pause");
    }
    // Recover CR3
    wrcr3(cr3);
    // Destroy test paging root
    MEM_REF_DROP(test_paging_root);
}