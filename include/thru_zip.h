#pragma once
// Pinned miniz 3.1.2 amalgamation; see THIRD_PARTY_NOTICES.md.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC system_header
#endif
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.c"
