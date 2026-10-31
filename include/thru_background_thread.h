#pragma once

#include <pthread.h>
#include <sched.h>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace s3g_daisy {
inline bool startBackgroundThread(pthread_t& thread, void* (*entry)(void*), void* context, bool audioCompanion = false)
{
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) return false;
    sched_param priority {};
#if defined(__APPLE__)
    // Darwin SCHED_OTHER accepts 0..63; explicit zero starves audio lookahead
    // behind compilers and sanitizer work. File/analysis workers keep zero.
    if(audioCompanion)priority.sched_priority=(sched_get_priority_min(SCHED_OTHER)+sched_get_priority_max(SCHED_OTHER))/2;
#else
    (void)audioCompanion;
#endif
    const bool configured = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED) == 0
        && pthread_attr_setschedpolicy(&attributes, SCHED_OTHER) == 0
        && pthread_attr_setschedparam(&attributes, &priority) == 0;
    const bool started = configured && pthread_create(&thread, &attributes, entry, context) == 0;
    pthread_attr_destroy(&attributes);
    return started;
}

inline void configureBackgroundThread(int niceness = 10)
{
#if defined(__linux__)
    sched_param priority {};
    (void)pthread_setschedparam(pthread_self(), SCHED_OTHER, &priority);
    cpu_set_t cores;
    CPU_ZERO(&cores);
    CPU_SET(0, &cores); CPU_SET(1, &cores); CPU_SET(2, &cores);
    (void)sched_setaffinity(0, sizeof(cores), &cores);
    (void)setpriority(PRIO_PROCESS, 0, niceness);
#else
    (void)niceness;
#endif
}
} // namespace s3g_daisy
