//! @file tasking.c
//! @brief Declaration of the tasking target

#include <thread/smp/boot_bringup.h>
#include <thread/tasking/balancer.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/tasking.h>

TARGET(thread_tasking_available, META_DUMMY,
       {thread_smp_ap_boot_bringup_available, thread_localsched_available,
        thread_balancer_available})
META_DEFINE_DUMMY()
