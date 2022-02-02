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
 * $PIP_VERSION: Version 2.3.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#include <pip/pip_internal.h>
#include <pip/pip_dlfcn.h>

/* safe (locked) dl* functions */

static void *pip_libc_dlsym_unsafe( void *handle, const char *symbol ) {
  static void*(*libc_dlsym)(void*,const char*) = NULL;
  if( libc_dlsym == NULL ) {
    libc_dlsym = pip_find_dso_symbol( handle, "libdl.so", "dlsym" );
    ASSERT( libc_dlsym != NULL );
  }
  return libc_dlsym( handle, symbol );
}

void *pip_dlsym_unsafe( void *handle, const char *symbol ) {
  return pip_libc_dlsym_unsafe( handle, symbol );
}

void *pip_dlsym_next_unsafe( const char *symbol ) {
  return pip_libc_dlsym_unsafe( RTLD_NEXT, symbol );
}

void *pip_dlsym_next( const char *symbol ) {
  pip_glibc_lock();
  void *addr = pip_libc_dlsym_unsafe( RTLD_NEXT, symbol );
  pip_glibc_unlock();
  return addr;
}

void *pip_dlsym( void *handle, const char *symbol ) {
  pip_glibc_lock();
  void *addr = pip_dlsym_unsafe( handle, symbol );
  pip_glibc_unlock();
  return addr;
}

void *dlsym( void *handle, const char *symbol ) {
  return pip_dlsym( handle, symbol );
}

void *pip_dlopen_unsafe( const char *filename, int flag ) {
  static void *(*libc_dlopen)( const char*, int ) = NULL;
  if( libc_dlopen == NULL ) {
    ASSERT( ( libc_dlopen = pip_dlsym_next_unsafe( "dlopen" ) ) != NULL ) ; 
  }
  void *rv = libc_dlopen( filename, flag );
  return rv;
}

void *pip_dlopen( const char *filename, int flag ) {
  pip_glibc_lock();
  void *handle = pip_dlopen_unsafe( filename, flag );
  pip_glibc_unlock();
  return handle;
}

void *dlopen( const char *filename, int flag ) {
  return pip_dlopen( filename, flag );
}

void *pip_dlmopen_unsafe( Lmid_t lmid, const char *filename, int flag ) {
  static void*(*libc_dlmopen)( Lmid_t, const char*, int ) = NULL;
  if( libc_dlmopen == NULL ) {
    ASSERT( ( libc_dlmopen = pip_dlsym_next_unsafe( "dlmopen" ) ) != NULL ) ; 
  }
  void *rv = libc_dlmopen( lmid, filename, flag );
  return rv;
}

void *pip_dlmopen( Lmid_t lmid, const char *filename, int flag ) {
  pip_glibc_lock();
  void *handle = pip_dlmopen_unsafe( lmid, filename, flag );
  pip_glibc_unlock();
  return handle;
}

void *dlmopen( Lmid_t lmid, const char *filename, int flag ) {
  return pip_dlmopen( lmid, filename, flag );
}

int pip_dlinfo( void *handle, int request, void *info ) {
  static int (*libc_dlinfo)( void*, int, void* ) = NULL;
  pip_glibc_lock();
  if( libc_dlinfo == NULL ) {
    ASSERT( ( libc_dlinfo = pip_dlsym_next_unsafe( "dlinfo" ) ) != NULL ) ; 
  }
  int rv = libc_dlinfo( handle, request, info );
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
  pip_glibc_lock();
  if( libc_dlerror == NULL ) {
    libc_dlerror = pip_dlsym_next_unsafe( "dlerror" );
  }
  char *dlerr = libc_dlerror();
  pip_glibc_unlock();
  return dlerr;
}

char *dlerror( void ) {
  return pip_dlerror();
}

int pip_dladdr(const void *addr, Dl_info *info) {
  static int(*libc_dladdr)(const void*, Dl_info*);
  pip_glibc_lock();
  if( libc_dladdr == NULL ) {
    libc_dladdr = pip_dlsym_next_unsafe( "dladdr" );
  }
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
  pip_glibc_lock();
  if( libc_dlvsym == NULL ) {
    libc_dlvsym = pip_dlsym_next_unsafe( "dlvsym" );
  }
  void *rv = libc_dlvsym( handle, symbol, version );
  pip_glibc_unlock();
  return rv;
}

void *dlvsym(void *__restrict handle, const char *symbol, const char *version) {
  return pip_dlvsym( handle, symbol, version );
}

/* misc. */

void *pip_sbrk( intptr_t inc ) {
  static void*(*libc_sbrk)(intptr_t) = NULL;
  pip_glibc_lock();
  if( libc_sbrk == NULL ) {
    libc_sbrk = pip_dlsym_next_unsafe( "sbrk" );
    if( libc_sbrk == NULL ) {
      errno = ENOSYS;
      return (void*)-1;
    }
  }
  void *rv = libc_sbrk( inc );
  pip_glibc_lock();
  return rv;
}

void *sbrk( intptr_t inc ) {
  return pip_sbrk( inc );
}

#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node, const char *service,
		const struct addrinfo *hints,
		struct addrinfo **res) {
  static int(*libc_getaddrinfo)(const char*, const char*,
				const struct addrinfo*,
				struct addrinfo**);
  pip_glibc_lock();
  if( libc_getaddrinfo == NULL ) {
    ASSERT( ( libc_getaddrinfo = 
	      pip_dlsym_next_unsafe( "getaddrinfo" ) ) != NULL ) ; 
  }
  int rv = libc_getaddrinfo( node, service, hints, res );
  pip_glibc_unlock();
  return rv;
}

void freeaddrinfo(struct addrinfo *res) {
  static void(*libc_freeaddrinfo)(struct addrinfo*);
  pip_glibc_lock();
  if( libc_freeaddrinfo == NULL ) {
    ASSERT( ( libc_freeaddrinfo = 
	      pip_dlsym_next_unsafe( "freeaddrinfo" ) ) != NULL ) ; 
  }
  libc_freeaddrinfo( res );
  pip_glibc_unlock();
}

const char *gai_strerror(int errcode) {
  static char*(*libc_gai_strerror)(int);
  pip_glibc_lock();
  if( libc_gai_strerror == NULL ) {
    ASSERT( ( libc_gai_strerror = 
	      pip_dlsym_next_unsafe( "gai_strerror" ) ) != NULL ) ; 
  }
  char *rv = libc_gai_strerror( errcode );
  pip_glibc_unlock();
  return rv;
}

void exit( int ) PIP_NORETURN;
void exit( int status ) {
  pip_do_exit( pip_task, PIP_EXIT_EXIT, (uintptr_t) status );
  NEVER_REACH_HERE;
}

void pip_pthread_exit( void* ) PIP_NORETURN;
void pip_pthread_exit( void *retval ) {
  pip_do_exit( pip_task, PIP_EXIT_PTHREAD, (uintptr_t) retval );
  NEVER_REACH_HERE;
}

void pthread_exit( void* ) PIP_NORETURN;
void pthread_exit( void *retval ) {
  pip_pthread_exit( retval );
  NEVER_REACH_HERE;
}
