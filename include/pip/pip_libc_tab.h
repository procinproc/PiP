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

#ifndef _pip_glibc_funcs_h_
#define _pip_glibc_funcs_h_

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <dlfcn.h>
#include <malloc.h>
#include <netdb.h>
#include <pthread.h>

typedef	int   (*libc_fflush_t)(FILE*);
typedef void  (*libc_exit_t)(int);
typedef void* (*libc_sbrk_t)(intptr_t);
typedef size_t(*libc_malloc_usable_size_t)(void*);
typedef int   (*libc_posix_memalign_t)(void**,size_t,size_t);
typedef int   (*libc_getaddrinfo_t)(const char*, 
				    const char*,
				    const struct addrinfo*,
				    struct addrinfo**);
typedef void  (*libc_freeaddrinfo_t)(struct addrinfo*);
typedef const char* (*libc_gai_strerror_t)(int);

typedef int   (*libc_pthread_create_t)(pthread_t *thread,
				       const pthread_attr_t *attr,
				       void *(*start_routine) (void *),
				       void *arg);
typedef void  (*libc_pthread_exit_t)(void*);

typedef void* (*libc_dlsym_t)(void*, const char*);
typedef void* (*libc_dlopen_t)(const char*,int);
typedef void* (*libc_dlmopen_t)(Lmid_t, const char*, int );
typedef int   (*libc_dlinfo_t)(void*,int,void*);
typedef char* (*libc_dlerror_t)(void);
typedef int   (*libc_dladdr_t)(const void*,Dl_info*);
typedef void* (*libc_dlvsym_t)(void*__restrict,const char*,const char*);

typedef struct pip_libc_ftab {
  /* libc.so */
  libc_fflush_t			fflush;
  libc_exit_t			exit;
  libc_sbrk_t			sbrk;
  libc_malloc_usable_size_t	malloc_usable_size;
  libc_posix_memalign_t		posix_memalign;
  libc_getaddrinfo_t		getaddrinfo;
  libc_freeaddrinfo_t		freeaddrinfo;
  libc_gai_strerror_t		gai_strerror;
  /* libpthread.so */
  libc_pthread_create_t		pthread_create;
  libc_pthread_exit_t		pthread_exit;
  /* libdl.so */
  libc_dlsym_t			dlsym;
  libc_dlopen_t			dlopen;
  libc_dlmopen_t		dlmopen;
  libc_dlinfo_t			dlinfo;
  libc_dladdr_t			dladdr;
  libc_dlvsym_t			dlvsym;
  libc_dlerror_t		dlerror;
} pip_libc_ftab_t;

#endif /* _pip_glibc_funcs_h_ */
