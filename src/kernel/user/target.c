//! @file target.c
//! @brief File containing definition of target for userspace code initialization

#include <mem/mem.h>
#include <thread/tasking/tasking.h>
#include <user/shm.h>
#include <user/target.h>

MODULE("user/target")
TARGET(userspace_available, META_DUMMY,
       {thread_tasking_available, mem_all_available, user_shms_available})
META_DEFINE_DUMMY()
