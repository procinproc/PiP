/*
 * $PIP_license: <Simplified BSD License>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 
 *     Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * $
 * $RIKEN_copyright: Riken Center for Computational Sceience (R-CCS),
 * System Software Development Team, 2016-2022
 * $
 * $PIP_VERSION: Version 2.4.1$
 *
 * $Author: Atsushi Hori 
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#ifndef _pip_machdep_x86_64_h
#define _pip_machdep_x86_64_h

#ifndef DOXYGEN_INPROGRESS

#include <stdint.h>
#include <stdio.h>

typedef volatile uint32_t	pip_spinlock_t;
#define PIP_LOCK_TYPE

inline static void pip_pause( void ) {
  asm volatile("wfe" :::"memory");
}
#define PIP_PAUSE

inline static void pip_write_barrier(void) {
  asm volatile("dmb ishst" :::"memory");
}
#define PIP_WRITE_BARRIER

inline static void pip_memory_barrier(void) {
  asm volatile("dmb ish" :::"memory");
}
#define PIP_MEMORY_BARRIER

inline static void pip_print_fs_segreg( void ) {
  register unsigned long result asm ("x0");
  asm ("mrs %0, tpidr_el0; " : "=r" (result));
  fprintf( stderr, "TPIDR_EL0 REGISTER: 0x%lx\n", result );
}
#define PIP_PRINT_FSREG

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif
