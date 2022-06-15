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

#define USE_TGKILL

#include <pip/pip_internal.h>
#include <pip/pip_gdbif.h>

int pip_is_initialized( void ) {
  return pip_initialized;
}

int pip_is_finalized( void ) {
  return pip_finalized;
}

int pip_is_effective( void ) {
  return pip_initialized && !pip_finalized;
}

int pip_isa_root( void ) {
  return pip_is_effective() && pip_task != NULL && PIP_ISA_ROOT( pip_task );
}

int pip_is_threaded_( void ) {
  return pip_initialized & PIP_MODE_PTHREAD;
}

#ifdef USE_TGKILL
static int tgkill( int tgid, int tid, int sig ) {
  return (int) syscall( (long int) SYS_tgkill, tgid, tid, sig );
}
#endif

int pip_raise_signal( pip_task_t *task, int sig ) {
  int err = 0;

  ENTERF( "raise signal (%d:%s) to PIPID:%d PID:%d TID:%d",
	  sig, strsignal(sig), task->pipid, task->pid, task->tid );

  if( !PIP_IS_ALIVE( task ) ) {
    err = ESRCH;
  } else if( pip_is_threaded_() ) {
#ifndef USE_TGKILL
    if( task->thread == 0 ) {
      err = ESRCH;
    } else {
      DBGF( "thread:%p", (void*) task->thread );
      err = pthread_kill( task->thread, sig );
    }
#else
    err = tgkill( task->pid, task->tid, sig );
#endif
  } else {			/* process mode */
    DBGF( "kill(%d,%d)", task->pid, sig );;
    if( kill( task->pid, sig ) != 0 ) err = errno;
  }
  RETURN( err );
}

void pip_abort( void ) PIP_NORETURN;
void pip_abort( void ) {
  ENTER;
  if( pip_is_threaded_() ) {
    abort();
  } else {
    void pip_abort_handler( int );
    pip_abort_handler( 0 );
  }
  NEVER_REACH_HERE;
}

pid_t pip_gettid( void ) {
  return (pid_t) syscall( (long int) SYS_gettid );
}

static int pip_is_magic_ok( pip_root_t *root ) {
  return strncmp( root->magic,
		  PIP_MAGIC_WORD,
		  PIP_MAGIC_LEN ) == 0;
}

static int pip_is_version_ok( pip_root_t *root ) {
  return( root->version == PIP_API_VERSION );
}

static int pip_are_sizes_ok( pip_root_t *root ) {
  return( root->size_root  == sizeof( pip_root_t ) &&
	  root->size_task  == sizeof( pip_task_t ) );
}

static int pip_check_root_and_task( pip_root_t *root, pip_task_t *task ) {
  int err = EPERM;

  if( root == NULL ) {
    pip_err_mesg( "Invalid PiP root" );
    return err;
  } else if( !pip_is_magic_ok( root ) ) {
    pip_err_mesg( "Magic number error" );
    return err;
  } else if( !pip_is_version_ok( root ) ) {
    pip_err_mesg( "API version miss-match between PiP root and task" );
    return err;
  } else if( !pip_are_sizes_ok( root ) ) {
    pip_err_mesg( "Size miss-match between PiP root and task" );
    return err;
  }
  return 0;
}

pip_task_t *pip_current_task( void ) {
  /* do not put any DBG macors in this function */
  static int		curr = 0;
  pid_t			tid   = pip_gettid();
  pip_root_t		*root = pip_root;
  pip_task_t 		*task = NULL;
  int 			i;

  if( root != NULL ) {
    for( i=curr; i<root->ntasks+1; i++ ) {
      task = &root->tasks[i];
      if( tid == task->tid ) goto found;
    }
    for( i=0; i<curr; i++ ) {
      task = &root->tasks[i];
      if( tid == task->tid ) goto found;
    }
  }
  return NULL;
 found:
  curr = i;
  return task;
}

static void pip_glibc_fin( pip_task_t *task ) {
  /* call fflush() in the target context to flush out messages */
  pip_libc_ftab(task)->fflush( NULL );
}

void pip_do_exit( pip_task_t *task, int flag, uintptr_t extval ) {
  /* DONOT CALL pip_isa_root() in this function !!!! */
  pip_root_t	*root;
  int mode, is_threaded, flag_pip = 1;
  int i, err = 0;

  pip_free_all();

  if( task != NULL ) {
    ENTERF( "PIPID:%d  extval:%lu", task->pipid, extval );
  } else {
    ENTERF( "PIPID:(null)  extval:%lu", extval );
  }
  mode = ( pip_initialized != 0 ) ? pip_initialized : pip_finalized;
  is_threaded = ( mode == PIP_MODE_PTHREAD );

  if( pip_root == NULL ) goto call_exit;
  root = pip_root;

  if( task == NULL ) {
    if( pip_task == NULL ) goto force_exit;
    task = pip_task;
  }
  if( task != NULL && root == NULL ) root = task->task_root;

  if( PIP_ISNTA_TASK( task ) ) {
    /* when a PiP task fork()s and the forked process exits */
    DBGF( "returned from a fork()ed process or "
	  "pthread_create()ed thread? (%d/%d)", 
	  task->tid, pip_gettid() );
    flag_pip = 0;
    goto force_exit;
  } else {
    if( task->hook_after != NULL ) {
      if( ( err = task->hook_after( task->hook_arg ) ) != 0 ) {
	pip_err_mesg( "PIPID:%d after-hook returns %d", 
		      task->pipid, err );
      }
      if( flag != PIP_EXIT_PTHREAD && extval == 0 ) extval = err;
    }
    pip_gdbif_exit( task, WEXITSTATUS(extval) );
    pip_gdbif_hook_after( task );
    pip_glibc_fin( task );
    if( task->symbols.named_export_fin != NULL ) {
      task->symbols.named_export_fin( task );
    }
  }

  if( root != NULL && 
      task != NULL &&
      PIP_ISA_ROOT( task ) ) {
    for( i=0; i<root->ntasks; i++ ) {
      pip_task_t *t = &root->tasks[i];
      if( PIP_IS_ALIVE( t ) ) {
	/* sending signal 0 to check if the task 
	   is really alive or not */
	if( pip_raise_signal( t, 0 ) == 0 ) {
	  pip_info_mesg( "PiP task %d is still running and killed", 
			 t->pipid );
	  /* if so, kill the task */
	  (void) pip_raise_signal( t, SIGKILL );
	}
      }
    }
  }
 force_exit:
  if( task != NULL ) {
    pip_set_exit_status( task, extval, 0 );
    extval = task->status;
    if( extval != 0 ) {
      if( WIFEXITED( extval ) ) {
	extval = WEXITSTATUS( extval );
      } else {
	extval = WTERMSIG( extval );
      }
    }
  }
  DBGF( "FORCE EXIT:%lu", extval );
  if( flag_pip && is_threaded && !PIP_ISA_ROOT( task ) ) {	
    /* child task in thread mode */
    libc_pthread_exit_t pthrd_exit = 
      pip_libc_ftab(task)->pthread_exit;
    if( task != NULL && root != NULL     && 
	PIP_IS_ALIVE( root->task_root )  &&
	!PIP_ISNTA_TASK( task ) ) {
      pip_raise_sigchld( task );
    }
    if( task != NULL && PIP_ISA_ROOT(task) ) {
      pip_finalize_root( task->task_root );
      /* after calling above func., pip_root and pip_task are NOT accessible */
    }
    if( flag == PIP_EXIT_PTHREAD ) {
      pthrd_exit( (void*) extval );
    } else {
      pthrd_exit( NULL );
    }
  } else {			/* process mode */
  call_exit:
    {
      libc_exit_t libc_exit = pip_libc_ftab(task)->exit;
      DBGF( "libc_exit: %p", libc_exit );
      if( task != NULL && PIP_ISA_ROOT(task) ) {
	pip_finalize_root( task->task_root );
	/* after calling above func., pip_root is free()ed */
      }
      libc_exit( extval );
    }
  }
  NEVER_REACH_HERE;
}

void pip_after_fork( void ) {
  ENTER;
  pip_unset_signal_handlers();
  pip_root = NULL;
  pip_task = NULL;
  pip_initialized = 0;
  pip_finalized   = 1;
  RETURNV;
}

static int pip_init_task( pip_root_t *root, pip_task_t *task, char **envv ) {
  int err = EPERM;

  ENTER;
  if( ( err = pip_check_root_and_task( root, task ) ) == 0 ) {
    
    pip_set_signal_handlers();
    ASSERTD( pthread_atfork( NULL, NULL, pip_after_fork ) == 0 );
    
    pip_gdbif_task_commit( pip_task );
    pip_gdbif_hook_before( pip_task );
    
    DBGF( "pip_root: %p @ %p  piptask : %p @ %p", 
	  pip_root, &pip_root, pip_task, &pip_task );
  }
  RETURN( err );
}

static int pip_is_coefd( int fd ) {
  int flags = fcntl( fd, F_GETFD );
  return( flags > 0 && ( flags & FD_CLOEXEC ) );
}

static void pip_close_on_exec( void ) {
  DIR *dir;
  struct dirent *direntp;
  int fd;

#define PROCFD_PATH		"/proc/self/fd"
  if( ( dir = opendir( PROCFD_PATH ) ) != NULL ) {
    int fd_dir = dirfd( dir );
    while( ( direntp = readdir( dir ) ) != NULL ) {
      if( direntp->d_name[0] != '.' &&
	  ( fd = strtol( direntp->d_name, NULL, 10 ) ) >= 0 &&
	  fd != fd_dir &&
	  pip_is_coefd( fd ) ) {
	(void) close( fd );
	DBGF( "FD:%d is closed (CLOEXEC)", fd );
      }
    }
    (void) closedir( dir );
    (void) close( fd_dir );
  }
}

static void pip_libc_init( void ) {
  extern void __ctype_init( void );
  __ctype_init();
}

void *__pip_start_task( pip_root_t *root, 
			pip_task_t *task, 
			pip_spawn_args_t *args, 
			int err,
			char *err_mesg,
			char *warn_mesg ) {
  char **argv      = args->argvec.vec;
  char **envv      = args->envvec.vec;
  void *start_arg  = args->start_arg;
  pip_spawnhook_t before = task->hook_before;
  void *hook_arg         = task->hook_arg;
  int  extval;

  pip_libc_init();
  ENTERF( "err:%d  err_mesg:%s  warn_mesg:%s", err, err_mesg, warn_mesg );
  pip_root = root;
  pip_task = task;

  DBG;
  pip_set_libc_ftab( task->libc_ftabp );
  pip_dont_wrap_malloc = 0;
    
  if( root->opts & PIP_MODE_PTHREAD ) {
    pip_initialized = PIP_MODE_PTHREAD;
  } else {
    pip_initialized = PIP_MODE_PROCESS;
  }
  pip_finalized = 0;

  if( warn_mesg != NULL ) {
    pip_warn_mesg( "%s", warn_mesg );
    free( warn_mesg );
    warn_mesg = NULL;
  }
  if( err_mesg != NULL ) {
    pip_err_mesg( "%s", err_mesg );
    free( err_mesg );
    err_mesg = NULL;
  }
  if( err ) {
    extval = err;

  } else {
    (void) pip_dlerror();	/* reset error string */
    if( !err && ( err = pip_init_task( root, task, envv ) ) == 0 ) {
      if( task->onstart_script != NULL ) {
	/* PIP_STOP_ON_START (process mode only) */
	DBGF( "ONSTART: %s", task->onstart_script );
	pip_raise_signal( task, SIGSTOP );
      }
      /* calling the before hook, if any */
      if( before != NULL ) {
	if( ( err = before( hook_arg ) ) != 0 ) {
	  pip_err_mesg( "before hook at %p returns %d", before, err );
	}
      }
    }
    if( !( root->opts & PIP_MODE_PTHREAD ) ) pip_close_on_exec();
    if( err ) {
      DBG;
      extval = err;
      
    } else if( args->funcname == NULL ) {
      extern char **environ;
      main_func_t start_main = args->func_main;
      DBGF( ">> main@%p(%d,%s,%s,...)",
	    start_main, args->argc, argv[0], argv[1] );
      extval = start_main( args->argc, argv, environ );
      DBGF( "<< main@%p(%d,%s,%s,...) = %d",
	    start_main, args->argc, argv[0], argv[1], extval );
    } else {
      start_func_t start_func = args->func_user;
      DBGF( ">> %s@%p(%p)",
	    args->funcname, start_func, start_arg );
      extval = start_func( start_arg );
      DBGF( "<< %s@%p(%p) = %d",
	    args->funcname, start_func, start_arg, extval );
    }
  }
  pip_do_exit( task, PIP_EXIT_RETURN, extval );
  /* there is a cse to reach here */
  return NULL;
}
