/*
  * $RIKEN_copyright:$
  * $PIP_VERSION:$
  * $PIP_license:$
*/
/*
 * Written by Atsushi HORI <ahori@riken.jp>, 2017
 */

#ifndef _pip_machdep_x86_64_h
#define _pip_machdep_x86_64_h

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <stdint.h>

typedef volatile uint32_t	pip_spinlock_t;

inline static void pip_pause( void ) {
  asm volatile("wfe" :::"memory");
}
#define PIP_PAUSE

inline static void pip_write_barrier(void) {
  asm volatile("dsb st" :::"memory");
}
#define PIP_WRITE_BARRIER

inline static void pip_memory_barrier(void) {
  asm volatile("dsb sy" :::"memory");
}
#define PIP_MEMORY_BARRIER

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif
