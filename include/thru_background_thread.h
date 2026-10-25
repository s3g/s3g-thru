#pragma once

#include <pthread.h>
#include <sched.h>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace s3g_daisy {
inline bool startBackgroundThread(pthread_t& thread, void* (*entry)(void*), void* context)
{
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) return false;
    sched_param priority {};
    const bool configured = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED) == 0
        && pthread_attr_setschedpolicy(&attributes, SCHED_OTHER) == 0
        && pthread_attr_setschedparam(&attributes, &priority) == 0;
    const bool started = configured && pthread_create(&thread, &attributes, entry, context) == 0;
    pthread_attr_destroy(&attributes);
    return started;
}

inline void configureBackgroundThread()
{
#if defined(__linux__)
    sched_param priority {};
    (void)pthread_setschedparam(pthread_self(), SCHED_OTHER, &priority);
    cpu_set_t cores;
    CPU_ZERO(&cores);
    CPU_SET(0, &cores); CPU_SET(1, &cores); CPU_SET(2, &cores);
    (void)sched_setaffinity(0, sizeof(cores), &cores);
    (void)setpriority(PRIO_PROCESS, 0, 10);
#endif
}
} // namespace s3g_daisy
