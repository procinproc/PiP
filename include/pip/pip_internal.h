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

#include <pip/pip_config.h>
#include <pip/pip_dlfcn.h>
#include <pip/pip_machdep.h>
#include <pip/pip_clone.h>
#include <pip/pip_debug.h>

#define PIP_BASE_VERSION	(0x3100U)

#define PIP_API_VERSION		PIP_BASE_VERSION

#define PIP_ROOT_ENV		"PIP_ROOT"
#define PIP_TASK_ENV		"PIP_TASK"

#define PIP_MAGIC_WORD		"PrcInPrc"
#define PIP_MAGIC_WLEN		(8)

#define PIP_MAPS_PATH		"/proc/self/maps"

#define PIP_EXITED		(1)
#define PIP_EXIT_WAITED		(2)
#define PIP_ABORT		(9)

#define PIP_STACK_SIZE		(8*1024*1024LU) /* 8 MiB */
#define PIP_STACK_SIZE_MIN	(4*1024*1024LU) /* 1 MiB */
#define PIP_STACK_SIZE_MAX	(16*1024*1024*1024LU) /* 16 GiB */
#define PIP_STACK_ALIGN		(256)

#define PIP_MASK32		(0xFFFFFFFF)

#define PIP_MIDLEN		(64)

/* not to use heap when defined */
/* the ocnsequence of this may slow malloc() and free() substantially !! */
#define PIP_NO_MALLOPT

struct pip_root;
struct pip_task;

typedef struct pip_patch_list {
  char	*name;
  void	*addr;
} pip_patch_list_t;

typedef	int(*main_func_t)(int,char**,char**);
typedef	int(*start_func_t)(void*);
typedef int(*mallopt_t)(int,int);
typedef void(*pthread_init_t)(int,char**,char**);
typedef	void(*ctype_init_t)(void);
typedef void(*add_stack_user_t)(void);
typedef	void(*fflush_t)(FILE*);
typedef void (*exit_t)(int);
typedef void (*pthread_exit_t)(void*);

typedef void*(*dlopen_t)(const char*, int);
typedef void*(*dlmopen_t)(Lmid_t, const char*, int);
typedef void*(*dlinfo_t)(void*, int, void*);
typedef void*(*dlsym_t)(void*, const char*);
typedef int(*dladdr_t)(void*, void*);
typedef int(*dlclose_t)(void*);
typedef char*(*dlerror_t)(void);

typedef void*(*malloc_t)(size_t);
typedef void(*free_t)(void*);
typedef void*(*calloc_t)(size_t, size_t);
typedef void*(*realloc_t)(void*, size_t);
typedef int(*posix_memalign_t)(void**, size_t, size_t);

typedef int(*named_export_fin_t)(struct pip_task*);
typedef int(*pip_init_t)(struct pip_root*,struct pip_task*);
typedef
int(*pip_clone_syscall_t)(int(*)(void*), void*, int, void*, pid_t*, void*, pid_t*);
typedef int(*pip_patch_got_t)(char*, char**, pip_patch_list_t*);

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
  long long		*malloc_hook; /* not used */
  /* GLIBC functions */
  mallopt_t		mallopt;      /* to call GLIBC mallopt() */
  fflush_t		libc_fflush;  /* to call GLIBC fflush() at the end */
  exit_t		exit;	     /* call exit() from fork()ed process */
  pthread_exit_t	pthread_exit;	   /* (see above exit) */
  /* pip_patch_GOT */
  pip_patch_got_t	patch_got;
  /* pip_dlfcn */
  dlopen_t		dlopen;
  dlmopen_t		dlmopen;
  dlinfo_t		dlinfo;
  dlsym_t		dlsym;
  dladdr_t		dladdr;
  dlclose_t		dlclose;
  dlerror_t		dlerror;
  /* reserved for future use */
  void			*__reserved__[8]; /* reserved for future use */
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

#define PIP_ISA_ROOT(T)		( (T)->type & PIP_TYPE_ROOT )
#define PIP_ISA_TASK(T)		\
  ( (T)->type & ( PIP_TYPE_ROOT | PIP_TYPE_TASK ) )

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
  /* reserved for future use */
  void			*__reserved__[16];
} pip_task_t;

extern pip_task_t	*pip_task;

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

typedef struct pip_root {
  char			magic[PIP_MAGIC_WLEN];
  unsigned int		version;
  size_t		size_whole;
  size_t		size_root;
  size_t		size_task;
  void *volatile	export_root;
  pip_spinlock_t	lock_ldlinux; /* lock for dl*() functions */
  size_t		page_size;
  unsigned int		opts;
  unsigned int		actual_mode;
  int			ntasks;
  int			ntasks_count;
  int			ntasks_curr;
  int			ntasks_accum;
  int			pipid_curr;

  pip_clone_t		*cloneinfo;   /* only valid with process:preload */
  pip_sem_t		lock_glibc; /* lock for GLIBC functions */
  pip_sem_t		sync_spawn; /* Spawn synch */

  /* signal related members */
  sigset_t		old_sigmask;
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

  int			flag_debug;

  pip_sem_t		universal_lock;
  /* reserved for future use */
  void			*__reserved__[14];
  /* tasks */
  pip_task_t		tasks[];
} pip_root_t;

#ifndef __W_EXITCODE
#define __W_EXITCODE(retval,signal)	( (retval) << 8 | (signal) )
#endif
#define PIP_W_EXITCODE(X,S)	__W_EXITCODE(X,S)

extern pip_root_t	*pip_root;

INLINE int pip_check_pipid( int *pipidp ) {
  int pipid;

  if( !pip_is_initialized() ) RETURN( EPERM  );
  if( pipidp == NULL        ) RETURN( EINVAL );
  pipid = *pipidp;

  switch( pipid ) {
  case PIP_PIPID_ROOT:
    break;
  case PIP_PIPID_ANY:
  case PIP_PIPID_NULL:
    RETURN( EINVAL );
    break;
  case PIP_PIPID_MYSELF:
    if( pip_isa_root() ) {
      *pipidp = PIP_PIPID_ROOT;
    } else if( pip_isa_task() ) {
      *pipidp = pip_task->pipid;
    } else {
      RETURN( ENXIO );		/* ???? */
    }
    break;
  default:
    if( pipid <  0                ) RETURN( EINVAL );
    if( pipid >= pip_root->ntasks ) RETURN( ERANGE );
  }
  return 0;
}

INLINE void pip_sem_init( pip_sem_t *sem ) {
  (void) sem_init( sem, 1, 0 );
}

INLINE void pip_sem_post( pip_sem_t *sem ) {
  errno = 0;
  (void) sem_post( sem );
  ASSERT( errno == 0 );
}

INLINE void pip_sem_wait( pip_sem_t *sem ) {
  errno = 0;
  (void) sem_wait( sem );
  ASSERT( errno == 0 || errno == EINTR );
}

INLINE void pip_sem_fin( pip_sem_t *sem ) {
  (void) sem_destroy( sem );
}

#define PIP_PRIVATE		__attribute__((visibility ("hidden")))

extern int __attribute__ ((visibility ("default")))
pip_init_task_implicitly( pip_root_t *root, pip_task_t *task );
extern void pip_reset_task_struct( pip_task_t* )PIP_PRIVATE;
extern pid_t pip_gettid( void );
extern int  pip_tkill( int, int );

extern void pip_page_alloc( size_t, void** ) PIP_PRIVATE;
extern void pip_reset_task_struct( pip_task_t* ) PIP_PRIVATE;

extern int  pip_get_thread( int pipid, pthread_t *threadp );
extern int  pip_is_pthread( int *flagp );
extern int  pip_is_shared_fd( int *flagp );
extern int  pip_is_shared_sighand( int *flagp );

extern pip_task_t *pip_get_task_( int ) PIP_PRIVATE;
extern int  pip_check_pipid( int* ) PIP_PRIVATE;
extern int  pip_is_threaded_( void ) PIP_PRIVATE;

extern void pip_set_signal_handler( int sig, void(*)(),
				    struct sigaction* ) PIP_PRIVATE;
extern int  pip_raise_signal( pip_task_t*, int ) PIP_PRIVATE;
extern void pip_set_sigmask( int ) PIP_PRIVATE;
extern void pip_unset_sigmask( void ) PIP_PRIVATE;
extern int  pip_signal_wait( int ) PIP_PRIVATE;

extern void pip_onstart( pip_task_t* ) PIP_PRIVATE;
extern void pip_set_exit_status( pip_task_t*, int ) PIP_PRIVATE;
extern void pip_task_signaled( pip_task_t*, int ) PIP_PRIVATE;
extern void pip_annul_task( pip_task_t* ) PIP_PRIVATE;

extern void pip_debug_on_exceptions( pip_root_t*, pip_task_t* ) PIP_PRIVATE;

extern int pip_debug_env( void );

extern int pip_patch_GOT( char*, char**, pip_patch_list_t* );

extern struct pip_gdbif_root	*pip_gdbif_root PIP_PRIVATE;

void pip_gdbif_load( pip_task_t* ) PIP_PRIVATE;
void pip_gdbif_exit( pip_task_t*, int ) PIP_PRIVATE;
void pip_gdbif_task_commit( pip_task_t* ) PIP_PRIVATE;
void pip_gdbif_task_new( pip_task_t* ) PIP_PRIVATE;
void pip_gdbif_initialize_root( int ) PIP_PRIVATE;
void pip_gdbif_finalize_task( pip_task_t* ) PIP_PRIVATE;
void pip_gdbif_hook_before( pip_task_t* ) PIP_PRIVATE;
void pip_gdbif_hook_after( pip_task_t* ) PIP_PRIVATE;

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* _pip_internal_h_ */
