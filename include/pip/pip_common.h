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

#ifndef _pip_common_h_
#define _pip_common_h_

#ifndef DOXYGEN_INPROGRESS

#include <pip/pip_internal.h>

#define SET_NAME_FMT "/proc/self/task/%u/comm"

/* the following function is to set the right */
/* name shown by the ps and top commands      */
#define SET_NAME_BODY(R,T)					\
  char *progname = NULL;					\
  char nam[16];							\
  char sym[] = "0*";						\
  char fname[sizeof(SET_NAME_FMT)+8];				\
  int  fd;							\
  if( task == (R)->task_root ) {				\
    sym[0] = 'R';						\
  } else {							\
    sym[0] += ( (T)->pipid % 10 );				\
    progname = (T)->args.prog;					\
  }								\
  switch( (R)->opts & PIP_MODE_MASK ) {				\
  case PIP_MODE_PROCESS_PRELOAD:				\
    sym[1] = ':';						\
    break;							\
  case PIP_MODE_PROCESS_PIPCLONE:				\
    sym[1] = ';';						\
    break;							\
  case PIP_MODE_PROCESS_GOT:					\
    sym[1] = '.';						\
    break;							\
  case PIP_MODE_PTHREAD:					\
    sym[1] = '|';						\
    break;							\
  default:							\
    sym[1] = '?';						\
    break;							\
  }								\
  if( progname == NULL ) {					\
    char prg[16];						\
    prctl( PR_GET_NAME, prg, 0, 0, 0 );				\
    snprintf( nam, 16, "%s%s", sym, prg );			\
  } else {							\
    char *p;							\
    if( ( p = strrchr( progname, '/' ) ) != NULL) {		\
      progname = p + 1;						\
    }								\
    snprintf( nam, 16, "%s%s", sym, progname );			\
  }								\
  if( sym[1] != '|' ) {					\
    (void) prctl( PR_SET_NAME, nam, 0, 0, 0 );			\
  } else {							\
    (void) pthread_setname_np( pthread_self(), nam );		\
  }								\
  sprintf( fname, SET_NAME_FMT, (unsigned int) task->tid );	\
  if( ( fd = open( fname, O_RDWR ) ) >= 0 ) {			\
    (void) write( fd, nam, strlen(nam) );			\
    (void) close( fd );						\
  }
   
#endif	/* DOXYGEN */
#endif
