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
 * $PIP_VERSION: Version 3.1.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

//#define DEBUG
//#define PRINT_MAPS
//#define PRINT_FDS

/* the EVAL define symbol is to measure the time for calling dlmopen() */
//#define EVAL

#include <pip/pip_internal.h>
#include <pip/pip_dlfcn.h>
#include <pip/pip_gdbif_func.h>

#include <malloc.h> 		/* needed for mallopt(M_MMAP_THRESHOLD) */
#include <limits.h>		/* for PTHREAD_STACK_MIN */
#define PIP_TRAMPOLINE_STACKSZ	(PTHREAD_STACK_MIN)

extern char 		**environ;
extern pip_spinlock_t 	*pip_lock_clone;

 pip_clone_mostly_pthread_t pip_clone_mostly_pthread_ptr = NULL;

/*** note that the following static variables are   ***/
/*** located at each PIP task and the root process. ***/

static pip_clone_t*	pip_cloneinfo = NULL;

#ifndef PIP_NO_MALLOPT
  /* heap (using brk or sbrk) is not safe in PiP */
#ifdef M_MMAP_THRESHOLD
void pip_donot_use_heap( void ) __attribute__ ((constructor));
void pip_donot_usr_heap( void ) {
  if( mallopt( M_MMAP_THRESHOLD, 1 ) == 1 ) {
    DBGF( "mallopt(M_MMAP_THRESHOLD): succeeded" );
  } else {
    pip_warn_mesg( "mallopt(M_MMAP_THRESHOLD): failed !!!!!!" );
  }
}
#endif
#endif

static void pip_set_magic( pip_root_t *root ) {
  memcpy( root->magic, PIP_MAGIC_WORD, PIP_MAGIC_WLEN );
}

#define NITERS		(100)
#define FACTOR_INIT	(10)
static uint64_t pip_measure_yieldtime( void ) {
  double dt, xt;
  uint64_t c;
  int i;

  for( i=0; i<NITERS/10; i++ ) pip_system_yield();
  dt = -pip_gettime();
  for( i=0; i<NITERS; i++ ) pip_system_yield();
  dt += pip_gettime();
  DBGF( "DT:%g", dt );

  for( i=0; i<NITERS/10; i++ ) pip_pause();
  xt = 0.0;
  for( c=FACTOR_INIT; ; c*=2 ) {
    xt = -pip_gettime();
    for( i=0; i<NITERS*c; i++ ) pip_pause();
    xt += pip_gettime();
    DBGF( "c:%lu  XT:%g  DT:%g", c, xt, dt );
    if( xt > dt ) break;
  }
  c *= 10;
  DBGF( "yield:%lu", c );
  return c;
}

static int pip_check_opt_and_env( uint32_t *optsp ) {
  extern pip_spinlock_t pip_lock_got_clone;
  int opts   = *optsp;
  int mode   = ( opts & PIP_MODE_MASK );
  int newmod = 0;
  char *env  = getenv( PIP_ENV_MODE );

  enum PIP_MODE_BITS {
    PIP_MODE_PTHREAD_BIT          = 1,
    PIP_MODE_PROCESS_PRELOAD_BIT  = 2,
    PIP_MODE_PROCESS_GOT_BIT      = 4,
    PIP_MODE_PROCESS_PIPCLONE_BIT = 8
  } desired = 0;

  if( ( opts & ~PIP_VALID_OPTS ) != 0 ) {
    /* unknown option(s) specified */
    RETURN( EINVAL );
  }
  /* check if pip_preload.so is pre-loaded. if so, */
  /* PIP_MODE_PROCESS_PRELOAD is the only choice   */
  if( pip_cloneinfo == NULL ) {
    pip_cloneinfo = (pip_clone_t*) dlsym( RTLD_DEFAULT, "pip_clone_info");
  }
  DBGF( "cloneinfo:%p", pip_cloneinfo );
  if( pip_cloneinfo != NULL ) {
    DBGF( "mode:0x%x", mode );
    if( mode == 0 || 
	( mode & PIP_MODE_PROCESS_PRELOAD ) == mode ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      pip_lock_clone = &pip_cloneinfo->lock;
      goto done;
    }
    if( mode & ~PIP_MODE_PROCESS_PRELOAD ) {
      pip_err_mesg( "pip_preload.so is already loaded by LD_PRELOAD and "
		    "process:preload must be specified at pip_init()" );
      RETURN( EPERM );
    }
    if( env == NULL || env[0] == '\0' ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      pip_lock_clone = &pip_cloneinfo->lock;
      goto done;
    }
    if( strcasecmp( env, PIP_ENV_MODE_PROCESS_PRELOAD ) == 0 ||
	strcasecmp( env, PIP_ENV_MODE_PROCESS         ) == 0 ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      pip_lock_clone = &pip_cloneinfo->lock;
      goto done;
    } else {
      pip_err_mesg( "pip_preload.so is already loaded by LD_PRELOAD and "
		    "process:preload is the only valid choice of PIP_MODE environment" );
      RETURN( EPERM );
    }
  } else {
    /* pip_preload.so is not loaded. i.e., LD_PRELOAD does not include pip_preload.so */
    if( mode != 0 &&
	( mode & PIP_MODE_PROCESS_PRELOAD ) == PIP_MODE_PROCESS_PRELOAD ) {
      pip_err_mesg( "pip_preload.so is not loaded by LD_PRELOAD and "
		    "process:preload might be a wrong choice" );
      RETURN( EPERM );
    }
    if( env != NULL && strcasecmp( env, PIP_ENV_MODE_PROCESS_PRELOAD ) == 0 ) {
      pip_err_mesg( "pip_preload.so is not loaded by LD_PRELOAD and "
		    "process:preload is a wrong choice of PIP_MODE environment" );
      RETURN( EPERM );
    }
  }

  switch( mode ) {
  case 0:
    if( env == NULL || env[0] == '\0' ) {
      desired = PIP_MODE_PTHREAD_BIT     |
	        PIP_MODE_PROCESS_GOT_BIT |
	        PIP_MODE_PROCESS_PIPCLONE_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_THREAD  ) == 0 ||
	       strcasecmp( env, PIP_ENV_MODE_PTHREAD ) == 0 ) {
      desired = PIP_MODE_PTHREAD_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_PROCESS ) == 0 ) {
      desired = PIP_MODE_PROCESS_GOT_BIT |
	        PIP_MODE_PROCESS_PIPCLONE_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_PROCESS_GOT      ) == 0 ) {
      desired = PIP_MODE_PROCESS_GOT_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_PROCESS_PIPCLONE ) == 0 ) {
      desired = PIP_MODE_PROCESS_PIPCLONE_BIT;
    } else {
      pip_warn_mesg( "unknown environment setting PIP_MODE='%s'", env );
      RETURN( EPERM );
    }
    break;
  case PIP_MODE_PTHREAD:
    desired = PIP_MODE_PTHREAD_BIT;
    break;
  case PIP_MODE_PROCESS:
    if( env == NULL || env[0] == '\0' ) {
      desired = PIP_MODE_PROCESS_GOT_BIT |
	        PIP_MODE_PROCESS_PIPCLONE_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_PROCESS_GOT      ) == 0 ) {
      desired = PIP_MODE_PROCESS_GOT_BIT;
    } else if( strcasecmp( env, PIP_ENV_MODE_PROCESS_PIPCLONE ) == 0 ) {
      desired = PIP_MODE_PROCESS_PIPCLONE_BIT;
    } else {
      pip_warn_mesg( "unknown environment setting PIP_MODE='%s'", env );
      RETURN( EPERM );
    }
    break;
  case PIP_MODE_PROCESS_GOT:
    desired = PIP_MODE_PROCESS_GOT_BIT;
    break;
  case PIP_MODE_PROCESS_PIPCLONE:
    desired = PIP_MODE_PROCESS_PIPCLONE_BIT;
    break;
  default:
    RETURN( EINVAL );
  }

  if( desired & PIP_MODE_PROCESS_GOT_BIT ) {
    int pip_wrap_clone( void );
    if( pip_wrap_clone() == 0 ) {
      newmod = PIP_MODE_PROCESS_GOT;
      pip_lock_clone = &pip_lock_got_clone;
      goto done;
    } else if( !( desired & ( PIP_MODE_PTHREAD_BIT |
			      PIP_MODE_PROCESS_PIPCLONE_BIT ) ) ) {
      RETURN( EPERM );
    }
  }
  if( desired & PIP_MODE_PROCESS_PIPCLONE_BIT ) {
    if ( pip_clone_mostly_pthread_ptr == NULL )
      pip_clone_mostly_pthread_ptr =
	dlsym( RTLD_DEFAULT, "pip_clone_mostly_pthread" );
    if ( pip_clone_mostly_pthread_ptr != NULL ) {
      newmod = PIP_MODE_PROCESS_PIPCLONE;
      goto done;
    } else if( !( desired & PIP_MODE_PTHREAD_BIT) ) {
      pip_err_mesg( "%s mode is requested but pip_clone_mostly_pthread() "
		    "cannot not be found in (PiP-)glibc",
		    PIP_ENV_MODE_PROCESS_PIPCLONE );
      RETURN( EPERM );
    }
  }
  if( desired & PIP_MODE_PTHREAD_BIT ) {
    newmod = PIP_MODE_PTHREAD;
  }

 done:
  *optsp = ( opts & ~PIP_MODE_MASK ) | newmod;
  RETURN( 0 );
}

static pip_task_internal_t *pip_get_myself( void ) {
  pip_task_internal_t *taski;
  if( pip_isa_root() ) {
    taski = pip_root->task_root;
  } else {
    taski = pip_task;
  }
  return taski;
}

/* internal funcs */

void pip_reset_task_struct( pip_task_internal_t *taski ) {
  pip_task_annex_t 	*annex = AA(taski);
  pip_task_misc_t 	*misc = annex->misc;
  void			*stack_trampoline = annex->stack_trampoline;
  void			*namexp = annex->named_exptab;

  //memset( (void*) taski, 0, offsetof( pip_task_internal_t, annex ) );
  PIP_TASKQ_INIT( &TA(taski)->queue  );
  PIP_TASKQ_INIT( &TA(taski)->schedq );
  PIP_TASKQ_INIT( &TA(taski)->oodq   );
  TA(taski)->type  = PIP_TYPE_NULL;
  TA(taski)->pipid = PIP_PIPID_NULL;
  pip_spin_init( &TA(taski)->lock_oodq );

  //memset( (void*) annex, 0, sizeof( pip_task_annex_t ) );
  annex->flag_exit        = 0;
  annex->stack_trampoline = stack_trampoline;
  annex->named_exptab     = namexp;
  annex->tid              = -1; /* pip_gdbif_init_task_struct() refers this */
  annex->misc             = misc;
  pip_sem_init( &annex->sleep );
}

int pip_check_sync_flag( uint32_t *optsp ) {
  int opts = *optsp;
  uint32_t f = opts & PIP_SYNC_MASK;

  DBGF( "flags:0x%x", f );
  if( f ) {
    if( pip_are_flags_exclusive( f, PIP_SYNC_AUTO     ) ) goto OK;
    if( pip_are_flags_exclusive( f, PIP_SYNC_BUSYWAIT ) ) goto OK;
    if( pip_are_flags_exclusive( f, PIP_SYNC_YIELD    ) ) goto OK;
    if( pip_are_flags_exclusive( f, PIP_SYNC_BLOCKING ) ) goto OK;
    return -1;
  } else {
    char *env = getenv( PIP_ENV_SYNC );
    if( env == NULL ) {
      f = PIP_SYNC_AUTO;
    } else if( strcasecmp( env, PIP_ENV_SYNC_AUTO     ) == 0 ) {
      f = PIP_SYNC_AUTO;
    } else if( strcasecmp( env, PIP_ENV_SYNC_BUSY     ) == 0 ||
	       strcasecmp( env, PIP_ENV_SYNC_BUSYWAIT ) == 0 ) {
      f = PIP_SYNC_BUSYWAIT;
    } else if( strcasecmp( env, PIP_ENV_SYNC_YIELD    ) == 0 ) {
      f = PIP_SYNC_YIELD;
    } else if( strcasecmp( env, PIP_ENV_SYNC_BLOCK    ) == 0 ||
	       strcasecmp( env, PIP_ENV_SYNC_BLOCKING ) == 0 ) {
      f = PIP_SYNC_BLOCKING;
    }
  }
 OK:
  *optsp = ( opts & ~PIP_SYNC_MASK ) | f;
  DBGF( "sync-flag %x | %x => %x", f, opts, *optsp );
  return 0;
}

void pip_set_signal_handler( int sig,
			     void(*handler)(),
			     struct sigaction *oldp ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigact.sa_sigaction = handler;
  ASSERT( sigemptyset( &sigact.sa_mask )    == 0 );
  ASSERT( sigaddset( &sigact.sa_mask, sig ) == 0 );
  ASSERT( sigaction( sig, &sigact, oldp )   == 0 );
}

void pip_unset_signal_handler( int sig, struct sigaction *oldp ) {
  ASSERT( sigaction( sig, oldp, NULL ) == 0 );
}

/* save PiP environments */

static void pip_save_debug_envs( pip_root_t *root ) {
  char *env;

  if( ( env = getenv( PIP_ENV_STOP_ON_START ) ) != NULL && *env != '\0' ) {
    root->envs.stop_on_start = strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_PATH      ) ) != NULL && *env != '\0' &&
      access( env, X_OK ) == 0 ) {
    root->envs.gdb_path      = strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_COMMAND   ) ) != NULL && *env != '\0' &&
      access( env, R_OK ) == 0 ) {
    root->envs.gdb_command   = strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_SIGNALS   ) ) != NULL && *env != '\0' ) {
    root->envs.gdb_signals   = strdup( env );
  }
  if( ( env = getenv( PIP_ENV_SHOW_MAPS     ) ) != NULL && *env != '\0' ) {
    root->envs.show_maps     = strdup( env );
  }
  if( ( env = getenv( PIP_ENV_SHOW_PIPS     ) ) != NULL && *env != '\0' ) {
    root->envs.show_pips    = strdup( env );
  }
}

/* signal handlers */

static void pip_sigchld_handler( int sig, siginfo_t *info, void *extra ) {}

void pip_set_sigmask( int sig ) {
  sigset_t sigmask;

  ASSERT( sigemptyset( &sigmask ) == 0);
  ASSERT( sigaddset(   &sigmask, sig ) == 0 );
  ASSERT( sigprocmask( SIG_BLOCK, &sigmask, &pip_root->old_sigmask ) == 0 );
}

void pip_unset_sigmask( void ) {
  ASSERT( sigprocmask( SIG_SETMASK, &pip_root->old_sigmask, NULL ) == 0 );
}

char *pip_prefix_dir( pip_root_t *root ) {
  FILE	 *fp_maps = NULL;
  size_t  sz = 0;
  ssize_t l;
  char	 *line = NULL;
  char	 *libpip, *prefix, *p;
  char	 *updir = "/..";
  void	 *sta, *end;
  void	 *faddr = (void*) pip_prefix_dir;

  if( root->prefixdir != NULL ) {
    return root->prefixdir;
  }
  ASSERT( ( fp_maps = fopen( PIP_MAPS_PATH, "r" ) ) != NULL );
  DBGF( "faddr:%p", faddr );
  while( ( l = getline( &line, &sz, fp_maps ) ) > 0 ) {
    line[l] = '\0';
    DBGF( "l:%d sz:%d line(%p)\n\t%s", (int)l, (int)sz, line, line );
    prefix = NULL;
    int n = sscanf( line, "%p-%p %*4s %*x %*x:%*x %*d %ms", &sta, &end, &libpip );
    DBGF( "%d: %p-%p %p %s", n, sta, end, faddr, prefix );
    if( n == 3         && 
	libpip != NULL && 
	sta   <= faddr && 
	faddr <  end   &&
	( p = rindex( libpip, '/' ) ) != NULL ) {
      *p = '\0';
      ASSERT( ( prefix = (char*) malloc( strlen( libpip ) + strlen( updir ) + 1 ) )
	      != NULL );
      p = prefix;
      p = stpcpy( p, libpip );
      p = stpcpy( p, updir  );
      ASSERT( ( p = realpath( prefix, NULL ) ) != NULL );
      free( libpip );
      free( prefix );
      prefix = p;
      root->prefixdir = prefix;
      DBGF( "prefix: %s", prefix );
      return prefix;
    }
    free( libpip );
  }
  NEVER_REACH_HERE;
  return NULL;			/* dummy */
}

/* API */

int pip_init( int *pipidp, int *ntasksp, void **rt_expp, uint32_t opts ) {
  pip_root_t		*root;
  pip_task_internal_t	*taski;
  pip_task_misc_t	*misc;
  size_t		sz;
  char			*envroot, *envtask;
  int			ntasks, pipid;
  int			i, err = 0;

#ifdef DO_MCHECK
  mcheck( NULL );
#endif

  if(( envroot = getenv( PIP_ROOT_ENV ) ) == NULL ) {
    /* root process */
    if( pip_is_initialized() ) RETURN( EBUSY );

    if( ntasksp == NULL ) {
      ntasks = PIP_NTASKS_MAX;
    } else {
      ntasks = *ntasksp;
    }
    if( ntasks <= 0             ) RETURN( EINVAL );
    if( ntasks > PIP_NTASKS_MAX ) RETURN( EOVERFLOW );

    if( ( err = pip_check_opt_and_env( &opts ) ) != 0 ) RETURN( err );
    if( pip_check_sync_flag(   &opts )  < 0 ) RETURN( EINVAL );

#ifndef PIP_CONCAT_STRUCT
    sz = sizeof(pip_root_t) +
      sizeof(pip_task_internal_t) * ( ntasks + 1 ) +
      sizeof(pip_task_annex_t   ) * ( ntasks + 1 ) +
      sizeof(pip_task_misc_t    ) * ( ntasks + 1 );
    pip_page_alloc( sz, (void**) &root );
    (void) memset( root, 0, sz );
    pip_task_annex_t *annex = (pip_task_annex_t*)
      ( ((intptr_t)root) +
	sizeof(pip_root_t) +
	sizeof(pip_task_internal_t) * ( ntasks + 1 ) );
    misc = (pip_task_misc_t*)
      ( ((intptr_t)root) +
	sizeof(pip_root_t) +
	sizeof(pip_task_internal_t) * ( ntasks + 1 ) +
	sizeof(pip_task_annex_t)    * ( ntasks + 1 ) );
    for( i=0; i<ntasks+1; i++ ) {
      root->tasks[i].annex       = &annex[i];
      root->tasks[i].annex->misc = &misc[i];
    }
#else
    sz = sizeof(pip_root_t) +
      sizeof(pip_task_internal_t) * ( ntasks + 1 ) +
      sizeof(pip_task_misc_t)     * ( ntasks + 1 );
    pip_page_alloc( sz, (void**) &root );
    (void) memset( root, 0, sz );
    pip_task_internal_t	*tasks = (pip_task_internal_t*)
      ( ((intptr_t)root) + sizeof(pip_root_t) );
    misc = (pip_task_misc_t*)
      ( ((intptr_t)tasks) + sizeof(pip_task_internal_t) * ( ntasks + 1 ) );
    for( i=0; i<ntasks+1; i++ ) {
      root->tasks[i].annex.misc = &misc[i];
    }
#endif
    root->size_whole = sz;
    root->size_root  = sizeof( pip_root_t );
    root->size_task  = sizeof( pip_task_internal_t );
    root->size_annex = sizeof( pip_task_annex_t );
    root->size_misc  = sizeof( pip_task_misc_t );

    DBGF( "ROOTROOT (%p)", root );

    pip_spin_init( &root->lock_tasks );
    pip_spin_init( &root->lock_bt    );

    (void) pip_prefix_dir( root );

    pip_sem_init( &root->lock_glibc );
    pip_sem_post( &root->lock_glibc );
    pip_sem_init( &root->sync_spawn );

    pipid = PIP_PIPID_ROOT;
    pip_set_magic( root );
    root->version               = PIP_API_VERSION;
    root->ntasks                = ntasks;
    root->ntasks_count          = 1; /* root is also a PiP task */
    root->cloneinfo             = pip_cloneinfo;
    root->opts                  = opts;
    root->yield_iters           = pip_measure_yieldtime();
    root->stack_size_trampoline = PIP_TRAMPOLINE_STACKSZ;
    root->task_root             = &root->tasks[ntasks];
    if( rt_expp != NULL ) {
      root->export_root = *rt_expp;
    }
    for( i=0; i<ntasks+1; i++ ) {
      pip_named_export_init( &root->tasks[i] );
      pip_reset_task_struct( &root->tasks[i] );
    }

    taski = root->task_root;
    TA(taski)->type       = PIP_TYPE_ROOT;
    TA(taski)->pipid      = pipid;
    TA(taski)->task_sched = taski;
    SETCURR( taski, taski );
    PIP_RUN( taski );

    AA(taski)->task_root = root;
    AA(taski)->tid       = pip_gettid();
    MA(taski)->loaded = dlopen( NULL, RTLD_NOW );
    MA(taski)->thread = pthread_self();

#ifdef PIP_SAVE_TLS
    pip_save_tls( &TA(taski)->tls );
#endif
    pip_page_alloc( root->stack_size_trampoline,
		    &AA(taski)->stack_trampoline );
    if( AA(taski)->stack_trampoline == NULL ) {
      free( root );
      RETURN( err );
    }
    pip_root = root;
    pip_task = taski;

    pip_set_name( taski );

    pip_set_sigmask( SIGCHLD );
    pip_set_signal_handler( SIGCHLD,
			    pip_sigchld_handler,
			    &root->old_sigchld );

    pip_gdbif_initialize_root( ntasks );
    pip_gdbif_task_commit( taski );
    if( !pip_is_threaded_() ) {
      pip_save_debug_envs( root );
      pip_debug_on_exceptions( root, taski );
    }
    /* fix me: only pip_preload.so must be excluded */
    unsetenv( "LD_PRELOAD" );

    DBGF( "PiP Execution Mode: %s", pip_get_mode_str() );

  } else if( ( envtask = getenv( PIP_TASK_ENV ) ) != NULL ) {
    /* child task */
    root  = (pip_root_t*) strtoll( envroot, NULL, 16 );
    pipid = (int) strtol( envtask, NULL, 10 );
    ASSERT( pipid >= 0 && pipid < root->ntasks );
    taski = &pip_root->tasks[pipid];
    if( ( err = pip_init_task_implicitly( root, taski ) ) == 0 ) {
      ntasks = root->ntasks;
      /* succeeded */
      if( ntasksp != NULL ) *ntasksp = ntasks;
      if( rt_expp != NULL ) {
	*rt_expp = AA(taski)->import_root;
      }
      DBGF( "UNSETENV ++++++++++++++++++++++++" );
      unsetenv( PIP_ROOT_ENV );
      unsetenv( PIP_TASK_ENV );
    } else {
      RETURN( err );
    }
  } else {
    RETURN( EPERM );
  }
  DBGF( "pip_root=%p  pip_task=%p", pip_root, pip_task );
  /* root and child */
  if( pipidp != NULL ) *pipidp = pipid;
  RETURN( err );
}

int pip_fin( void ) {
  int ntasks, i;

  ENTER;
  if( !pip_is_initialized() ) RETURN( EPERM );
  if( pip_isa_root() ) {		/* root */
    ntasks = pip_root->ntasks;
    for( i=0; i<ntasks; i++ ) {
      pip_task_internal_t *taski = &pip_root->tasks[i];
      if( TA(taski)->type != PIP_TYPE_NULL ) {
	DBGF( "%d/%d [pipid=%d (type=0x%x)] -- BUSY",
	      i, ntasks, TA(taski)->pipid, TA(taski)->type );
	RETURN( EBUSY );
      }
    }
    pip_named_export_fin_all();
    /* report accumulated timer values, if set */
    PIP_REPORT( time_load_dso  );
    PIP_REPORT( time_load_prog );
    PIP_REPORT( time_dlmopen   );

    pip_sem_fin( &pip_root->lock_glibc );
    pip_sem_fin( &pip_root->sync_spawn );
    /* SIGCHLD */
    pip_unset_sigmask();
    pip_unset_signal_handler( SIGCHLD,
			      &pip_root->old_sigchld );
    /* SIGTERM */
    pip_unset_signal_handler( SIGTERM,
			      &pip_root->old_sigterm );

    if( pip_root->envs.stop_on_start != NULL )
      free( pip_root->envs.stop_on_start );
    if( pip_root->envs.gdb_path      != NULL )
      free( pip_root->envs.gdb_path      );
    if( pip_root->envs.gdb_command   != NULL )
      free( pip_root->envs.gdb_command   );
    if( pip_root->envs.gdb_signals   != NULL )
      free( pip_root->envs.gdb_signals   );
    if( pip_root->envs.show_maps     != NULL )
      free( pip_root->envs.show_maps     );
    if( pip_root->envs.show_pips     != NULL )
      free( pip_root->envs.show_pips     );

    memset( pip_root, 0, pip_root->size_whole );
    /* after this point DBG(F) macros cannot be used */
    free( pip_root );
    pip_root = NULL;
    pip_task = NULL;

    pip_undo_patch_GOT();
  }
  RETURN( 0 );
}

int pip_export( void *exp ) {
  if( !pip_is_initialized() ) RETURN( EPERM );
  AA(pip_get_myself())->exp = exp;
  RETURN( 0 );
}

int pip_import( int pipid, void **expp  ) {
  pip_task_internal_t *taski;
  int err;

  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );
  taski = pip_get_task( pipid );
  if( expp != NULL ) *expp = (void*) AA(taski)->exp;
  RETURN( 0 );
}

int pip_isa_task( void ) {
  return
    pip_is_initialized() &&
    PIP_ISA_TASK( pip_task ) && /* root is also a task */
    !PIP_ISA_ROOT( pip_task );
}

int pip_get_dlmopen_info( int pipid, void **handle, long *lmidp ) {
  pip_task_internal_t *taski;
  int err;

  if( !pip_is_initialized() ) RETURN( EPERM );
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );
  taski = pip_get_task( pipid );
  if( handle != NULL ) *handle = MA(taski)->loaded;
  if( lmidp  != NULL ) *lmidp  = MA(taski)->lmid;
  RETURN( 0 );
}

int pip_get_pipid( int *pipidp ) {
  int pipid;
  pipid = pip_get_pipid_();
  if( pipid == PIP_PIPID_NULL ) RETURN( EPERM );
  if( pipidp != NULL ) *pipidp = pipid;
  RETURN( 0 );
}

int pip_get_ntasks( int *ntasksp ) {
  if( pip_root == NULL ) return( EPERM  ); /* intentionally small return */
  if( ntasksp != NULL ) {
    *ntasksp = pip_root->ntasks;
  }
  RETURN( 0 );
}

int pip_get_mode( int *modep ) {
  if( pip_root == NULL ) RETURN( EPERM  );
  if( modep != NULL ) {
    *modep = ( pip_root->opts & PIP_MODE_MASK );
  }
  RETURN( 0 );
}

const char *pip_get_mode_str( void ) {
  char *mode;

  if( pip_root == NULL ) return NULL;
  switch( pip_root->opts & PIP_MODE_MASK ) {
  case PIP_MODE_PTHREAD:
    mode = PIP_ENV_MODE_PTHREAD;
    break;
  case PIP_MODE_PROCESS:
    mode = PIP_ENV_MODE_PROCESS;
    break;
  case PIP_MODE_PROCESS_PRELOAD:
    mode = PIP_ENV_MODE_PROCESS_PRELOAD;
    break;
  case PIP_MODE_PROCESS_GOT:
    mode = PIP_ENV_MODE_PROCESS_GOT;
    break;
  case PIP_MODE_PROCESS_PIPCLONE:
    mode = PIP_ENV_MODE_PROCESS_PIPCLONE;
    break;
  default:
    mode = "(unknown)";
  }
  return mode;
}

int pip_is_threaded( int *flagp ) {
  if( pip_is_threaded_() ) {
    *flagp = 1;
  } else {
    *flagp = 0;
  }
  return 0;
}

int pip_is_shared_fd_( void ) {
  return pip_is_threaded_();
}

int pip_is_shared_fd( int *flagp ) {
  if( pip_root == NULL ) RETURN( EPERM  );
  if( pip_is_shared_fd_() ) {
    if( flagp != NULL ) *flagp = 1;
  } else {
    if( flagp != NULL ) *flagp = 0;
  }
  return 0;
}

int pip_kill_all_tasks( void ) {
  int pipid, i, err = 0;

  if( !pip_is_initialized() ) {
    err = EPERM;
  } else if( !pip_isa_root() ) {
    err = EPERM;
  } else {
    for( i=0; i<pip_root->ntasks; i++ ) {
      pipid = i;
      if( pip_check_pipid( &pipid ) == 0 ) {
	pip_task_internal_t *taski = &pip_root->tasks[pipid];
	if( pip_is_threaded_() ) {
	  AA(taski)->flag_sigchld = 1;
	  /* simulate process behavior */
	  ASSERT( pip_raise_signal( pip_root->task_root, SIGCHLD ) == 0 );
	}
	(void) pip_raise_signal( taski, SIGTERM );
      }
    }
  }
  return err;
}

int pip_get_system_id( int pipid, pip_id_t *idp ) {
  pip_task_internal_t *taski;
  int err;

  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );
  if( idp != NULL ) {
    taski = pip_get_task( pipid );
    /* Do not call gettid() nor pthread_self() for tbis                */
    /* if a task is a BLT then gettid() returns the scheduling task ID */
    if( pip_is_threaded_() ) {
      *idp = (intptr_t) AA(TA(taski)->task_sched)->misc->thread;
    } else {
      *idp = (intptr_t) AA(taski)->tid;
    }
  }
  RETURN( 0 );
}

/* energy-saving spin-lock */
void pip_glibc_lock( void ) __attribute__ ((unused));
/* actually this is being used */
void pip_glibc_lock( void ) {
  if( pip_root != NULL ) pip_sem_wait( &pip_root->lock_glibc );
}

void pip_glibc_unlock( void ) __attribute__ ((unused));
/* actually this is being used */
void pip_glibc_unlock( void ) {
  if( pip_root != NULL ) pip_sem_post( &pip_root->lock_glibc );
}
