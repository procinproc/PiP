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

#include <pip/pip_internal.h>
#include <pip/pip_dlfcn.h>

#if PIP_WRAPPER == 1

/* safe (locked) dl* functions */

void *pip_dlsym_unsafe( void *handle, const char *symbol ) {
  return pip_libc_ftab(NULL)->dlsym( handle, symbol );
}

void *pip_dlsym_next_unsafe( const char *symbol ) {
  return pip_dlsym_unsafe( RTLD_NEXT, symbol );
}

void *pip_dlsym_next( const char *symbol ) {
  pip_libc_lock();
  void *addr = pip_dlsym_unsafe( RTLD_NEXT, symbol );
  pip_libc_unlock();
  return addr;
}

void *pip_dlsym( void *handle, const char *symbol ) {
  pip_libc_lock();
  void *addr = pip_dlsym_unsafe( handle, symbol );
  pip_libc_unlock();
  return addr;
}

void *dlsym( void *handle, const char *symbol ) {
  return pip_dlsym( handle, symbol );
}

void *pip_dlopen_unsafe( const char *filename, int flag ) {
  return pip_libc_ftab(NULL)->dlopen( filename, flag );
}

void *pip_dlopen( const char *filename, int flag ) {
  pip_libc_lock();
  void *handle = pip_dlopen_unsafe( filename, flag );
  pip_libc_unlock();
  return handle;
}

void *dlopen( const char *filename, int flag ) {
  return pip_dlopen( filename, flag );
}

void *pip_dlmopen( Lmid_t lmid, const char *filename, int flag ) {
  pip_libc_lock();
  void *handle = pip_libc_ftab(NULL)->dlmopen( lmid, filename, flag );
  pip_libc_unlock();
  return handle;
}

void *dlmopen( Lmid_t lmid, const char *filename, int flag ) {
  return pip_dlmopen( lmid, filename, flag );
}

int pip_dlinfo( void *handle, int request, void *info ) {
  pip_libc_lock();
  int rv = pip_libc_ftab(NULL)->dlinfo( handle, request, info );
  pip_libc_unlock();
  return rv;
}

int dlinfo( void *handle, int request, void *info ) {
  return pip_dlinfo( handle, request, info );
}

int pip_dlclose( void *handle ) {
  extern int __libc_dlclose( void *handle );
  pip_libc_lock();
  int rv = __libc_dlclose( handle );
  pip_libc_unlock();
  return rv;
}

int dlclose( void *handle ) {
  return pip_dlclose( handle );
}

char *dlerror( void ) {
  pip_libc_lock();
  char *dlerr = pip_libc_ftab(NULL)->dlerror();
  pip_libc_unlock();
  return dlerr;
}

char *pip_dlerror( void ) {
  return dlerror();
}

int pip_dladdr(const void *addr, Dl_info *info) {
  pip_libc_lock();
  int rv = pip_libc_ftab(NULL)->dladdr( addr, info );
  pip_libc_unlock();
  return rv;
}

int dladdr(const void *addr, Dl_info *info) {
  return pip_dladdr( addr, info );
}

void*
pip_dlvsym(void *__restrict handle, const char *symbol, const char *version) {
  pip_libc_lock();
  void *rv = pip_libc_ftab(NULL)->dlvsym( handle, symbol, version );
  pip_libc_unlock();
  return rv;
}

void *dlvsym(void *__restrict handle, const char *symbol, const char *version) {
  return pip_dlvsym( handle, symbol, version );
}

/* misc. */

void *pip_sbrk( intptr_t inc ) {
  pip_libc_lock();
  void *rv = pip_libc_ftab(NULL)->sbrk( inc );
  pip_libc_unlock();
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
  pip_libc_lock();
  int rv = pip_libc_ftab(NULL)->getaddrinfo( node, service, hints, res );
  pip_libc_unlock();
  return rv;
}

void freeaddrinfo(struct addrinfo *res) {
  pip_libc_lock();
  pip_libc_ftab(NULL)->freeaddrinfo( res );
  pip_libc_unlock();
}

const char *gai_strerror(int errcode) {
  pip_libc_lock();
  const char *rv = pip_libc_ftab(NULL)->gai_strerror( errcode );
  pip_libc_unlock();
  return rv;
}

void exit( int ) PIP_NORETURN;
void exit( int status ) {
  pip_do_exit( pip_task, PIP_EXIT_EXIT, (uintptr_t) status );
  NEVER_REACH_HERE;
}

int pip_pthread_create( pthread_t *thread,
			const pthread_attr_t *attr,
			void *(*start_routine) (void *),
			void *arg ) {
  pip_libc_lock();
  int rv = pip_libc_ftab(NULL)->pthread_create( thread,
						attr,
						start_routine,
						arg );
  pip_libc_unlock();
  return rv;
}

int pthread_create( pthread_t *thread,
		    const pthread_attr_t *attr,
		    void *(*start_routine) (void *),
		    void *arg ) {
  return pip_pthread_create( thread, attr, start_routine, arg );
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

#else

/* safe (locked) dl* functions */

int pip_dlclose( void *handle ) {
  return dlclose( handle );
}

void *pip_dlopen_unsafe( const char *filename, int flag ) {
  return dlopen( filename, flag );
}

void *pip_dlmopen( Lmid_t lmid, const char *filename, int flag ) {
  pip_libc_lock();
  void *rv = dlmopen( lmid, filename, flag );
  pip_libc_unlock();
  return rv;
}

int pip_dlinfo( void *handle, int request, void *info ) {
  pip_libc_lock();
  int rv = dlinfo( handle, request, info );
  pip_libc_unlock();
  return rv;
}

void *pip_dlsym( void *handle, const char *symbol ) {
  pip_libc_lock();
  void *addr = dlsym( handle, symbol );
  pip_libc_unlock();
  return addr;
}

char *pip_dlerror( void ) {
  pip_libc_lock();
  char *dlerr = dlerror();
  pip_libc_unlock();
  return dlerr;
}

#endif /* PIP_WRAPPER */
