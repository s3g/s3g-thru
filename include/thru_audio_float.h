#pragma once
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace s3g_daisy {
// Floating-point mode belongs to each thread. Audio helpers created by the
// non-audio module loader cannot rely on the host SPI thread's denormal mode.
inline bool enableAudioDenormalFlush() noexcept {
#if defined(__aarch64__)
    unsigned long mode;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(mode));
    mode|=1UL<<24;
    __asm__ __volatile__("msr fpcr, %0" :: "r"(mode));
    return true;
#elif defined(__SSE__)
    _mm_setcsr(_mm_getcsr()|0x8040u); // FTZ outputs and DAZ inputs.
    return true;
#else
    return false;
#endif
}
}
