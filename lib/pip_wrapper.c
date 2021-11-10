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
 * $PIP_VERSION: Version 2.1.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#ifdef AH

#define _GNU_SOURCE
#include <pip/pip_internal.h>
#include <dlfcn.h>
void *pip_dlsym_unsafe( void *handle, const char *symbol ) {
  return dlsym( handle, symbol );
}
void *pip_dlsym( void *handle, const char *symbol ) {
  return dlsym( handle, symbol );
}
static void *pip_dlsym_next( const char *symbol ) {
  return dlsym( RTLD_NEXT, symbol );
}
void *pip_dlopen_unsafe( const char *filename, int flag ) {
  return dlopen( filename, flag );
}
int pip_dlclose( void *handle ) {
  return dlclose( handle );
}
void *pip_dlmopen( long lmid, const char *path, int flag ) {
  return dlmopen( lmid, path, flag );
}
char *pip_dlerror( void ) {
  return dlerror();
}
int pip_dlinfo( void *handle, int request, void *info ) {
  return dlinfo( handle, request, info );
}
#endif

#include <pip/pip_internal.h>
#include <pip/pip_dlfcn.h>

/* safe (locked) dl* functions */

void *pip_dlsym_unsafe( void *handle, const char *symbol ) {
  extern void *_dl_sym(void *, const char *, void *);
  static void *(*pip_libc_dlsym)(void*, const char*)=NULL;

  if( pip_libc_dlsym == NULL ) {
    if( pip_task != NULL ) {
      pip_libc_dlsym = pip_task->symbols.dlsym;
    } else {
      pip_libc_dlsym = _dl_sym( RTLD_NEXT, "dlsym", pip_dlsym_unsafe );
    }
    ASSERT( pip_libc_dlsym != NULL );
  }
  return pip_libc_dlsym( handle, symbol );
}

void *pip_dlsym( void *handle, const char *symbol ) {
  pip_glibc_lock();
  void *addr = pip_dlsym_unsafe( handle, symbol );
  pip_glibc_unlock();
  return addr;
}

static void *pip_dlsym_next( const char *symbol ) {
  return pip_dlsym( RTLD_NEXT, symbol );
}

static void *pip_dlsym_next_unsafe( const char *symbol ) {
  return pip_dlsym_unsafe( RTLD_NEXT, symbol );
}

void *dlsym( void *handle, const char *symbol ) {
  return pip_dlsym( handle, symbol );
}

void *pip_dlmopen( long lmid, const char *path, int flag ) {
  static void *(*libc_dlmopen)( long,  const char*, int ) = NULL;
  pip_glibc_lock();
  if( libc_dlmopen == NULL ) {
    libc_dlmopen = pip_dlsym_next_unsafe( "dlmopen" );
    if( libc_dlmopen == NULL ) return NULL;
  }
  void *handle = libc_dlmopen( lmid, path, flag );
  DBG;
  pip_glibc_unlock();
  return handle;
}

void *dlmopen( long lmid, const char *path, int flag ) {
  return pip_dlmopen( lmid, path, flag );
}

void *pip_dlopen_unsafe( const char *filename, int flag ) {
static void *(*pip_libc_dlopen)( const char*, int ) = NULL;
  if( pip_libc_dlopen == NULL ) {
    ASSERT( ( pip_libc_dlopen = pip_dlsym_next_unsafe( "dlopen" ) ) != NULL ) ; 
  }
  void *rv = pip_libc_dlopen( filename, flag );
  return rv;
}

void *pip_dlopen( const char *filename, int flag ) {
  pip_glibc_lock();
  void *handle = pip_dlopen_unsafe( filename, flag );
  pip_glibc_unlock();
  return handle;
}

void *dlopen( const char *filename, int flag ) {
  void *handle;
  if( pip_task != NULL && filename != NULL ) {
    handle = pip_dlmopen( pip_task->lmid, filename, flag );
  } else {
    handle = pip_dlopen( filename, flag );
  }
  return handle;
}

int pip_dlinfo( void *handle, int request, void *info ) {
  static int (*pip_libc_dlinfo)( void*, int, void* ) = NULL;
  pip_glibc_lock();
  if( pip_libc_dlinfo == NULL ) {
    ASSERT( ( pip_libc_dlinfo = pip_dlsym_next_unsafe( "dlinfo" ) ) != NULL ) ; 
  }
  int rv = pip_libc_dlinfo( handle, request, info );
  pip_glibc_unlock();
  return rv;
}

int dlinfo( void *handle, int request, void *info ) {
  return pip_dlinfo( handle, request, info );
}

int pip_dlclose( void *handle ) {
  extern int __libc_dlclose( void *handle );
  pip_glibc_lock();
  int rv = __libc_dlclose( handle );
  pip_glibc_unlock();
  return rv;
}

int dlclose( void *handle ) {
  return pip_dlclose( handle );
}

char *pip_dlerror( void ) {
  static char*(*libc_dlerror)( void ) = NULL;
  if( libc_dlerror == NULL ) {
    libc_dlerror = pip_dlsym_next( "dlerror" );
  }
  pip_glibc_lock();
  char *dlerr = libc_dlerror();
  pip_glibc_unlock();
  return dlerr;
}

char *dlerror( void ) {
  return pip_dlerror();
}

int pip_dladdr(const void *addr, Dl_info *info) {
  static int(*libc_dladdr)(const void*, Dl_info*);
  if( libc_dladdr == NULL ) {
    libc_dladdr = pip_dlsym_next( "dladdr" );
  }
  pip_glibc_lock();
  int rv = libc_dladdr( addr, info );
  pip_glibc_unlock();
  return rv;
}

int dladdr(const void *addr, Dl_info *info) {
  return pip_dladdr( addr, info );
}

void*
pip_dlvsym(void *__restrict handle, const char *symbol, const char *version) {
  static void*(*libc_dlvsym)(void*__restrict, const char*, const char* );
  if( libc_dlvsym == NULL ) {
    libc_dlvsym = pip_dlsym_next( "dlvsym" );
  }
  pip_glibc_lock();
  void *rv = libc_dlvsym( handle, symbol, version );
  pip_glibc_unlock();
  return rv;
}

void *dlvsym(void *__restrict handle, const char *symbol, const char *version) {
  return pip_dlvsym(handle, symbol, version);
}

/* misc. */

void *pip_sbrk( intptr_t inc ) {
  static void*(*pip_libc_sbrk)(intptr_t) = NULL;
  if( pip_libc_sbrk == NULL ) {
    pip_libc_sbrk = pip_dlsym( RTLD_NEXT, "sbrk" );
    if( pip_libc_sbrk == NULL ) {
      errno = ENOSYS;
      return (void*)-1;
    }
  }
  printf( ">> pip_sbrk()\n" );
  pip_glibc_lock();
  void *rv = pip_libc_sbrk( inc );
  pip_glibc_lock();
  printf( "<< pip_sbrk()\n" );
  return rv;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node, const char *service,
		const struct addrinfo *hints,
		struct addrinfo **res) {
  static int(*pip_libc_getaddrinfo)(const char*, const char*,
				    const struct addrinfo*,
				    struct addrinfo**);
  ENTER;
  if( pip_libc_getaddrinfo == NULL ) {
    ASSERT( ( pip_libc_getaddrinfo = 
	      pip_dlsym( RTLD_NEXT, "getaddrinfo" ) ) != NULL ) ; 
  }
  printf( ">> getaddrinfo()\n" );
  pip_glibc_lock();
  int rv = pip_libc_getaddrinfo( node, service, hints, res );
  pip_glibc_unlock();
  printf( "<< getaddrinfo()\n" );
  RETURN( rv );
}

void freeaddrinfo(struct addrinfo *res) {
  static void(*pip_libc_freeaddrinfo)(struct addrinfo*);
  if( pip_libc_freeaddrinfo == NULL ) {
    ASSERT( ( pip_libc_freeaddrinfo = 
	      pip_dlsym( RTLD_NEXT, "freeaddrinfo" ) ) != NULL ) ; 
  }
  pip_glibc_lock();
  pip_libc_freeaddrinfo( res );
  pip_glibc_unlock();
}

const char *gai_strerror(int errcode) {
  static char*(*pip_libc_gai_strerror)(int);
  if( pip_libc_gai_strerror == NULL ) {
    ASSERT( ( pip_libc_gai_strerror = 
	      pip_dlsym( RTLD_NEXT, "gai_strerror" ) ) != NULL ) ; 
  }
  pip_glibc_lock();
  char *rv = pip_libc_gai_strerror( errcode );
  pip_glibc_unlock();
  return rv;
}
