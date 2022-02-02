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
 * System Software Development Team, 2016-2021
 * $
 * $PIP_VERSION: Version 2.4.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#ifndef _pip_clone_h_
#define _pip_clone_h_

#ifdef DOXYGEN_SHOULD_SKIP_THIS
#ifndef DOXYGEN_INPROGRESS
#define DOXYGEN_INPROGRESS
#endif
#endif

#ifdef DOXYGEN_INPROGRESS
#ifndef INLINE
#define INLINE
#endif
#else
#ifndef INLINE
#define INLINE			inline static
#endif
#endif

#ifndef DOXYGEN_INPROGRESS

INLINE int pip_clone_flags( int flags ) {
  /* flags muxt be unset */
  flags &= ~(CLONE_FS);
  flags &= ~(CLONE_FILES);
  flags &= ~(CLONE_SIGHAND);
  flags &= ~(CLONE_PARENT);
  flags &= ~(CLONE_THREAD);
#ifdef CLONE_IO
  flags &= ~(CLONE_IO);
#endif
#ifdef CLONE_NEWNET
  flags &= ~(CLONE_NEWNET);
#endif
#ifdef CLONE_NEWNS
  flags &= ~(CLONE_NEWNS);
#endif
#ifdef CLONE_NEWPID
  flags &= ~(CLONE_NEWPID);
#endif
#ifdef CLONE_NEWUTS
  flags &= ~(CLONE_NEWUTS);
#endif

  /* flags must be set */
  flags |= CLONE_SETTLS;
  flags |= CLONE_VM;

  /* flags seem to be better to set */
  flags |= CLONE_PTRACE;
  flags |= CLONE_SYSVSEM;

  /* raise SIGCHLD when terminated */
  flags &= ~0xff;
  flags |= SIGCHLD;

  return flags;
}

#endif	/* DOXYGEN_SHOULD_SKIP_THIS */

#endif
