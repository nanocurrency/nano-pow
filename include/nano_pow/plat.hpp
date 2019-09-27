#pragma once

#ifdef _WIN32
#define NP_INLINE __forceinline
#else
#define NP_INLINE __attribute__ ((always_inline)) inline
#endif

#ifndef NP_INLINE
#define NP_INLINE
#endif
