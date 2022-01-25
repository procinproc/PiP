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

#ifndef _pip_internal_h_
#define _pip_internal_h_

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <ucontext.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <libgen.h>
#include <fcntl.h>
#include <link.h>

#define PIP_PRIVATE		__attribute__((visibility ("hidden")))

struct pip_root PIP_PRIVATE;
struct pip_task PIP_PRIVATE;

extern struct pip_root	*pip_root;
extern struct pip_task	*pip_task;
extern int		pip_initialized;
extern int		pip_finalized;
extern int 		pip_dont_wrap_malloc;

#include <pip/pip_config.h>
#include <pip/pip_dlfcn.h>
#include <pip/pip_machdep.h>
#include <pip/pip_clone.h>
#include <pip/pip_debug.h>
#include <pip/pip_gdbif_func.h>

#define PIP_BASE_VERSION	(0x3200U)

#define PIP_API_VERSION		PIP_BASE_VERSION

#define PIP_MAGIC_WORD		"PrcInPrc"
#define PIP_MAGIC_LEN		(8)

#define PIP_MAPS_PATH		"/proc/self/maps"

#define PIP_EXITED		(1)
#define PIP_EXIT_WAITED		(2)
#define PIP_ABORT		(9)

#define PIP_EXIT_EXIT		(0)
#define PIP_EXIT_PTHREAD	(1)

#define PIP_STACK_SIZE		(8*1024*1024LU) /* 8 MiB */
#define PIP_STACK_SIZE_MIN	(4*1024*1024LU) /* 1 MiB */
#define PIP_STACK_SIZE_MAX	(16*1024*1024*1024LU) /* 16 GiB */
#define PIP_STACK_ALIGN		(256)

#define PIP_MINSIGSTKSZ 	(MINSIGSTKSZ*16)

#define PIP_CLONE_LOCK_UNLOCKED		(0)
#define PIP_CLONE_LOCK_OTHERWISE	(0xFFFFFFFF)

#define PIP_MASK32		(0xFFFFFFFF)

#define PIP_MIDLEN		(64)

#define PIP_NORETURN		__attribute__((noreturn))

typedef struct pip_got_patch_list {
  char	*name;
  void	*addr;
} pip_got_patch_list_t;

typedef	int(*main_func_t)(int,char**,char**);
typedef	int(*start_func_t)(void*);
typedef void(*pthread_init_t)(int,char**,char**);
typedef	void(*ctype_init_t)(void);
typedef void(*add_stack_user_t)(void);
typedef	void(*fflush_t)(FILE*);
typedef void(*exit_t)(int);
typedef void (*pthread_exit_t)(void*);
typedef void*(*dlsym_t)(void*, const char*);
typedef void(*pip_set_opts_t)(char*,char*);

typedef int(*named_export_fin_t)(struct pip_task*);
typedef int(*pip_init_t)(struct pip_root*,struct pip_task*,char**);
typedef int(*pip_fin_t)(void);
typedef
int(*pip_clone_syscall_t)(int(*)(void*), void*, int, void*, pid_t*, void*, pid_t*);
typedef int(*pip_patch_got_t)(char*, char**, pip_got_patch_list_t*);
typedef
int(*clone_syscall_t)(int(*)(void*), void*, int, void*, pid_t*, void*, pid_t*);

typedef struct pip_symbol {
  main_func_t		main;	      /* main function address */
  start_func_t		start;	      /* strat function instead of main */
  /* PiP init functions */
  pip_init_t		pip_init;     /* implicit initialization */
  named_export_fin_t	named_export_fin; /* for free()ing hash entries */
  /* glibc variables */
  char			***libc_argvp; /* to set __libc_argv */
  int			*libc_argcp;   /* to set __libc_argc */
  char			**prog;	       /* to set __progname */
  char			**prog_full;   /* to set __progname_full */
  char			***environ;    /* to set the environ variable */
  /* GLIBC init funcs */
  ctype_init_t		ctype_init;   /* to call GLIBC __ctype_init() */
  /* GLIBC functions */
  void			*unused_slot0;	/* unused */
  void			*unused_slot1;	/* unused */
  fflush_t		libc_fflush;  /* to call GLIBC fflush() at the end */
  void			*unused_slot2;
  void			*unused_slot3;
  /* pip_patch_GOT */
  pip_patch_got_t	patch_got;
  /* pip_fin_task_implicitly */
  pip_fin_t		pip_fin;
  /* PiP-glibc */
  pip_set_opts_t	pip_set_opts;
  /* reserved for future use */
  void			*__reserved__[16]; /* reserved for future use */
} pip_symbols_t;

typedef struct pip_char_vec {
  char			**vec;
  char			*strs;
} pip_char_vec_t;

typedef struct pip_spawn_args {
  int			pipid;
  int			coreno;
  int			argc;
  char			*prog;
  char			*prog_full;
  char			*funcname;
  void			*start_arg;
  pip_char_vec_t	argvec;
  pip_char_vec_t	envvec;
  void			*__reserved__[16]; /* reserved for future use */
} pip_spawn_args_t;

#define PIP_TYPE_NULL	(0)
#define PIP_TYPE_ROOT	(1)
#define PIP_TYPE_TASK	(2)

#define PIP_ISA_ROOT(T)		( (T)->type == PIP_TYPE_ROOT )
#define PIP_IS_ALIVE(T)		( (T)->type != PIP_TYPE_NULL )
#define PIP_ISNTA_TASK(T)	( pip_gettid() != (T)->tid )

struct pip_gdbif_task;
struct pip_root;

typedef struct pip_task {
  int			pipid;	 /* PiP ID */
  int			type;	 /* PIP_TYPE_TASK or PIP_TYPE_ULP */

  void			*export;
  void			*import_root;
  void			*named_exptab;
  void			*aux;

  void			*loaded;
  pip_symbols_t		symbols;
  pip_spawn_args_t	args;	/* arguments for a PiP task */
  struct pip_root	*task_root;
  int			flag_exit;
  volatile int		flag_sigchld; /* termination in thread mode */
  volatile int32_t	status;	   /* exit value */
  int			retval;

  struct pip_gdbif_task	*gdbif_task;

  pid_t			pid;	/* PID in process mode */
  pid_t			tid;	/* TID in process mode */
  pthread_t		thread;	/* thread */
  pip_spawnhook_t	hook_before;
  pip_spawnhook_t	hook_after;
  void			*hook_arg;
  void			*sigalt_stack;
  /* stop_on_start fomr PiP 2.1 */
  pid_t			pid_onstart;
  char			*onstart_script;
  Lmid_t		lmid;
  /* malloc free list */
  pip_atomic_t		malloc_free_list;

  cpu_set_t 		*cpuset;
  sigset_t		*debug_signals;
  /* reserved for future use */
  void			*__reserved__[14];
} pip_task_t;

#define PIP_FILLER_SZ	(PIP_CACHE_SZ-sizeof(pip_spinlock_t))

typedef sem_t		pip_sem_t;

/* The following env vars must be copied */
/* if a PiP may have different env set.  */
typedef struct pip_env {
  char	*stop_on_start;
  char	*gdb_path;
  char	*gdb_command;
  char	*gdb_signals;
  char	*show_maps;
  char	*show_pips;
  char	*__reserved__[16];
} pip_env_t;

typedef struct pip_clone {
  pip_spinlock_t lock;	     /* lock */
} pip_clone_t;

typedef struct pip_recursive_lock {
  pip_atomic_t	count;
  pid_t		owner;
  int		nrecursive;
  pip_sem_t	semaphore;
} pip_recursive_lock_t;

INLINE void pip_sem_init( pip_sem_t *sem ) {
  (void) sem_init( sem, 1, 0 );
}

INLINE void pip_sem_post( pip_sem_t *sem ) {
  errno = 0;
  (void) sem_post( sem );
  ASSERTD( errno == 0 );
}

INLINE void pip_sem_wait( pip_sem_t *sem ) {
  errno = 0;
  (void) sem_wait( sem );
  ASSERTD( errno == 0 || errno == EINTR );
}

INLINE void pip_sem_fin( pip_sem_t *sem ) {
  (void) sem_destroy( sem );
}

INLINE void pip_recursive_lock_init( pip_recursive_lock_t *lock ) {
  memset( lock, 0, sizeof(pip_recursive_lock_t) );
  pip_sem_init( &lock->semaphore );
}

INLINE void pip_recursive_lock_lock( pip_recursive_lock_t *lock ) {
  pid_t 	tid = pip_gettid();
  pip_atomic_t 	count = pip_atomic_fetch_and_add( &lock->count, 1 );
  if( count >= 1 ) {
    if( tid != lock->owner ) pip_sem_wait( &lock->semaphore );
  }
  lock->owner = tid;
  lock->nrecursive ++;
}

INLINE void pip_recursive_lock_unlock( pip_recursive_lock_t *lock ) {
  pid_t 	tid = pip_gettid();
  int		nrec;
  pip_atomic_t 	count;
  ASSERTD( tid == lock->owner );
  nrec = --lock->nrecursive;
  if( nrec == 0 ) lock->owner = 0;
  count = pip_atomic_sub_and_fetch( &lock->count, 1 );
  if( count > 0 && nrec == 0 ) {
    pip_sem_post( &lock->semaphore );
  }
}

INLINE void pip_recursive_lock_fin( pip_recursive_lock_t *lock ) {
  pip_sem_fin( &lock->semaphore );
  memset( lock, 0, sizeof(pip_recursive_lock_t) );
}

typedef struct pip_root {
  char			magic[PIP_MAGIC_LEN];
  unsigned int		version;
  size_t		size_whole;
  size_t		size_root;
  size_t		size_task;
  void *volatile	export_root;
  pip_spinlock_t	_unused_lock_ldlinux;
  size_t		page_size;
  unsigned int		opts;
  unsigned int		actual_mode;
  int			ntasks;
  int			ntasks_count;
  int			ntasks_curr;
  int			ntasks_accum;
  int			pipid_curr;

  pip_clone_t		*cloneinfo;   /* only valid with process:preload */
  pip_sem_t		lock_clone;   /* lock for clone */
  pip_sem_t		sync_spawn;   /* Spawn synch */

  /* signal related members */
  sigset_t		_unused_mask;
  /* for chaining signal handlers */
  struct sigaction	old_sigterm;
  struct sigaction	old_sigchld;
  /* environments */
  pip_env_t		envs;
  /* GDB Interface */
  struct pip_gdbif_root	*gdbif_root;

  pip_spinlock_t	lock_bt; /* lock for backtrace */
  size_t		stack_size;
  pip_task_t		*task_root; /* points to tasks[ntasks] */
  pip_spinlock_t	lock_tasks; /* lock for finding a new task id */

  char			*prefixdir;

  int			flag_debug; /* unused */

  pip_sem_t		universal_lock;
  pip_recursive_lock_t	glibc_lock; /* 5 64-bit words */

  cpu_set_t 		*cpuset;

  /* reserved for future use */
  void			*__reserved__[10];
  /* tasks */
  pip_task_t		tasks[];
} pip_root_t;

#ifndef __W_EXITCODE
#define __W_EXITCODE(retval,signal)	( (retval) << 8 | (signal) )
#endif
#define PIP_W_EXITCODE(X,S)	__W_EXITCODE(X,S)

extern void pip_after_fork( void ) PIP_PRIVATE;

extern int  pip_is_effective( void ) PIP_PRIVATE;
extern int  pip_is_finalized( void ) PIP_PRIVATE;
extern int  pip_fin_task_implicitly( void );
extern void pip_free_all( void ) PIP_PRIVATE;
extern void *pip_dlopen_unsafe( const char*, int ) PIP_PRIVATE;
extern void *pip_find_dso_symbol( void*, char*, char* ) PIP_PRIVATE;
extern void *pip_dlsym_unsafe( void*, const char* ) PIP_PRIVATE;
extern void pip_do_exit( pip_task_t*, int, uintptr_t ) PIP_PRIVATE;
extern void pip_named_export_fin_all( pip_root_t* );

extern void pip_reset_task_struct( pip_task_t* ) PIP_PRIVATE;
extern int  pip_tkill( int, int );

extern void pip_reset_task_struct( pip_task_t* ) PIP_PRIVATE;

extern int  pip_get_thread( int pipid, pthread_t *threadp );
extern int  pip_is_pthread( int *flagp );
extern int  pip_is_shared_fd( int *flagp );
extern int  pip_is_shared_sighand( int *flagp );

extern pip_task_t *pip_get_task_( int ) PIP_PRIVATE;

extern void pip_set_signal_handler( int sig, void(*)(),
				    struct sigaction* ) PIP_PRIVATE;
extern int  pip_signal_wait( int ) PIP_PRIVATE;
extern void pip_raise_sigchld( pip_task_t* ) PIP_PRIVATE;
extern void pip_set_signal_handlers( void ) PIP_PRIVATE;
extern void pip_unset_signal_handlers( void ) PIP_PRIVATE;
extern void pip_abort_handler( int ) PIP_PRIVATE;
extern void pip_kill_all_tasks_( int ) PIP_PRIVATE;

extern void pip_onstart( pip_task_t* ) PIP_PRIVATE;
extern void pip_set_exit_status( pip_task_t*, int, int ) PIP_PRIVATE;
extern void pip_annul_task( pip_task_t* ) PIP_PRIVATE;
extern void pip_pthread_exit( void* ) PIP_PRIVATE;

extern int pip_debug_env( void );

/* defined in libpip_init.so */
extern int  pip_check_root_and_task( pip_root_t*, pip_task_t* );
extern pid_t pip_gettid( void );
extern int  pip_is_threaded_( void );
extern void pip_page_alloc( size_t, void** );
extern int  pip_raise_signal( pip_task_t*, int );
extern void pip_debug_on_exceptions( pip_root_t*, pip_task_t* );
extern int  pip_patch_GOT( char*, char**, pip_got_patch_list_t* );
extern void pip_undo_patch_GOT( void );

extern void *__libc_malloc( size_t size );
extern void  __libc_free( void *ptr );
extern void *__libc_calloc( size_t nmemb, size_t size );
extern void *__libc_realloc( void *ptr, size_t size );
extern int   __libc_mallopt( int param, int value );

INLINE int pip_check_pipid( int *pipidp ) {
  if( !pip_is_effective() ) RETURN( EPERM  );
  if( pipidp   == NULL    ) RETURN( EINVAL );
  if( pip_root == NULL    ) RETURN( EPERM  );
  if( pip_task == NULL    ) RETURN( EPERM  );

  int pipid = *pipidp;
  switch( pipid ) {
  case PIP_PIPID_ROOT:
    if( !PIP_IS_ALIVE( pip_root->task_root ) ) {
      return EACCES;
    }
    break;
  case PIP_PIPID_ANY:
  case PIP_PIPID_NULL:
    return EINVAL;
    break;
  case PIP_PIPID_MYSELF:
    if( PIP_ISA_ROOT( pip_task ) ) {
      *pipidp = PIP_PIPID_ROOT;
    } else if( PIP_IS_ALIVE( pip_task ) ) {
      *pipidp = pip_task->pipid;
    } else {
      return ENXIO;		/* ???? */
    }
    break;
  default:
    if( pipid <  0                ) RETURN( ERANGE );
    if( pipid >= pip_root->ntasks ) RETURN( ERANGE );
  }
  return 0;
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* _pip_internal_h_ */
