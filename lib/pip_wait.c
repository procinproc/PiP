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

//#define PIP_DEADLOCK_WARN

#include <pip/pip_internal.h>

void
pip_set_exit_status( pip_task_t *task, int exitno, int termsig ) {
  if( task != NULL ) {
    ENTERF( "PIPID:%d (exst:%d,termsig:%d)", 
	    task->pipid, exitno, termsig );
    if( !task->flag_exit ) {
      task->status    = PIP_W_EXITCODE( exitno, termsig );
      task->flag_exit = PIP_EXITED;
    }
    DBGF( "status: 0x%x", task->status );
    RETURNV;
  }
}

void pip_annul_task( pip_task_t *task ) {
  task->type   = PIP_TYPE_NULL;
  task->thread = 0;
  task->pid    = 0;
  task->tid    = 0;
}

static void pip_finalize_task( pip_task_t *task ) {
  ENTERF( "pipid=%d  status=0x%x", task->pipid, task->status );
  pip_gdbif_finalize_task( task );
  /* dlclose() and free() must be called only from the root process since */
  /* corresponding dlmopen() and malloc() is called by the root process   */
  if( task->loaded != NULL ) {
    /***** do not call dlclose() *****/
    //pip_dlclose( taski->loaded );
    //taski->loaded = NULL;
  }
  //pip_reset_task_struct( task );
}

static int pip_wait_thread( pip_task_t *task, int flag_blk ) {
  void *retval = NULL;
  int err = 0;

  ENTERF( "PIPID:%d", task->pipid );
  if( !task->thread ) {
    err = ECHILD;
  } else if( flag_blk ) {
    err = pthread_join( task->thread, &retval );
    DBGF( "pthread_join(): %s", pip_errname(err) );
  } else {
    err = pthread_tryjoin_np( task->thread, &retval );
    DBGF( "pthread_tryjoin_np(): %s", pip_errname(err) );
  }
  if( err ) {
    err = ECHILD;
  } else if( retval == PTHREAD_CANCELED ) {
    pip_warn_mesg( "PiP Task [%d] is canceled", task->pipid );
    pip_set_exit_status( task, ECANCELED, 0 );
  } else {
    pip_set_exit_status( task, 0, 0 );
  }
  /* workaround (make sure) */
  if( err != 0 && task->flag_sigchld ) {
    struct timespec ts;
    char path[128];
    struct stat stbuf;

    snprintf( path, 128, "/proc/%d/task/%d", getpid(), task->tid );
    DBGF( "path:%s", path );
    while( 1 ) {
      errno = 0;
      (void) stat( path, &stbuf );
      err = errno;
      DBGF( "stat(%s): %s", path, pip_errname( err ) );
      if( err == ENOENT ) {
	err = 0;
	break;
      }
      if( !flag_blk ) break;
      ts.tv_sec  = 0;
      ts.tv_nsec = 100*1000;
      nanosleep( &ts, NULL );
    }
  }
  RETURN( err );
}

#ifdef AH
static int pip_wait_proc( pip_task_t *task, int flag_blk ) {
  pid_t tid;
  int   status  = 0;
  int   options = 0;
  int   err     = 0;

  ENTER;
#ifdef __WCLONE
  /* __WALL: Wait for any processes/threads */
  options |= __WCLONE;
#else
  #ifdef __WALL
  /* __WALL: Wait for all children, regardless of type */
  /* ("clone" or "non-clone") [from the man page]      */
  options |= __WALL;
  #endif
#endif
  if( !flag_blk ) options |= WNOHANG;

  DBGF( "calling waitpid()  task:%p  tid:%d  pipid:%d",
	task, task->tid, task->pipid );
  while( 1 ) {
    tid = waitpid( task->tid, &status, options );
    err = errno;

    DBGF( "waitpid(tid=%d,status=0x%x)=%d (err=%d)",
	  task->tid, status, tid, err );

    if( tid == -1 ) {
      if( err == EINTR ) continue;
    } else if( tid == 0 ) {
      err = ECHILD;
    }
    break;
  }
  if( !err ) {
    pip_set_exit_status( task, status, 0 );
  }
  RETURN( err );
}
#endif

static int pip_wait_proc( pip_task_t *task, int flag_blk ) {
  siginfo_t 	info;
  int   	options = WEXITED;
  int		status  = 0;
  int   	err     = 0;

  ENTER;
  if( !flag_blk ) options |= WNOHANG;

  DBGF( "calling waitid()  task:%p  pipid:%d  tid:%d",
	task, task->pipid, task->tid );
  while( 1 ) {
    memset( (void*) &info, 0, sizeof(info) );
    if( waitid( P_PID, task->tid, &info, options ) != 0 ) {
      if( errno == EINTR ) continue;
      err = errno;
    } else if( !flag_blk && info.si_pid == 0 ) {
      err = ECHILD;
    }
    if( !err ) {
      status = info.si_status;
      if( WIFEXITED( status ) ) {
	pip_set_exit_status( task, WEXITSTATUS(status), 0 );
      } else if( WIFSIGNALED( status ) ) {
	pip_set_exit_status( task, 0, WTERMSIG(status) );
      }
      if( task->pid_onstart > 0 ) {
	while( 1 ) {
	  if( waitpid( task->pid_onstart, NULL, 0 ) > 0 ) break;
	  switch( errno ) {
	  case EINTR:
	    continue;
	  case EINVAL:
	    DBGF( "waipid(onstart) failed !!!!" );
	  case ECHILD:
	    goto done;
	  default:
	    (void) kill( task->pid_onstart, SIGKILL );
	    continue;
	  }
	}
      done:
	task->pid_onstart = 0;
      }
    }
    DBGF( "waitid(tid=%d,status=0x%x) (err=%d)", task->tid, status, err );
    break;
  }
  RETURN( err );
}

static int pip_do_wait( pip_task_t *task, int flag_blk ) {
  int err = 0;

  ENTERF( "PIPID:%d", task->pipid );
  if( task->type == PIP_TYPE_NULL ||
      task->tid <= 0 ) {
    /* already waited */
    RETURN( ECHILD );
  }
  if( pip_is_threaded_() ) {	/* thread mode */
    err = pip_wait_thread( task, flag_blk );
  } else {			/* process mode */
    err = pip_wait_proc(   task, flag_blk );
  }
  if( !err ) {
    DBGF( "PIPID:%d terminated", task->pipid );
    if( task->pid_onstart > 0 ) {
      do {
	errno = 0;
	(void) waitpid( task->pid_onstart, NULL, 0 );
      } while( errno == EINTR );
      free( task->onstart_script );
      task->onstart_script = NULL;
      DBGF( "PIPID:%d on_start (%d) terminates", task->pipid, task->pid_onstart );
    }
    pip_annul_task( task );
  }
  RETURN( err );
}

static int pip_wait_task( pip_task_t *task ) {
  ENTERF( "PIPID:%d", task->pipid );
  if( task->type   != PIP_TYPE_NULL &&
      task->tid    != 0             &&
      task->thread != 0 ) {
    if( pip_is_threaded_() ) {
      if( task->flag_sigchld ) {
	/* if sigchld is really raised, then blocking wait */
	task->flag_sigchld = 0;
	if( pip_do_wait( task, 1 ) == 0 ) goto found;
      } else if( pip_do_wait( task, 0 ) == 0 ) goto found;
    } else {			/* process mode */
      if( pip_do_wait( task, 0 ) == 0 ) goto found;
    }
  }
  RETURN_NE( 0 );		/* not yet */
 found:
  RETURN_NE( 1 );		/* terminated */
}

static int pip_nonblocking_waitany( void ) {
  pip_task_t 	*task;
  static int	start = 0;
  int		id, pipid = PIP_PIPID_NULL;

  ENTER;
  for( id=start; id<pip_root->ntasks; id++ ) {
    task = &pip_root->tasks[id];
    if( PIP_IS_ALIVE( task ) && pip_wait_task( task ) ) {
      goto found;
    }
  }
  for( id=0; id<start; id++ ) {
    task = &pip_root->tasks[id];
    if( PIP_IS_ALIVE( task ) && pip_wait_task( task ) ) {
      goto found;
    }
  }
  goto not_found;

 found:
  pipid = id ++;
  start = ( id < pip_root->ntasks ) ? id : 0;

 not_found:
  /* chekc again if there is a live task or not */
  if( pipid == PIP_PIPID_NULL ) {
    for( id=0; id<pip_root->ntasks; id++ ) {
      task = &pip_root->tasks[id];
      if( PIP_IS_ALIVE( task ) ) goto live_task;
    }
    /* no live tasks */
    pipid = PIP_PIPID_ANY;
  }
 live_task:
  RETURN_NE( pipid );
}

static int pip_blocking_waitany( void ) {
  int	pipid;

  ENTER;
  while( 1 ) {
    pipid = pip_nonblocking_waitany();
    DBGF( "pip_nonblocking_waitany() = %d", pipid );
    if( pipid != PIP_PIPID_NULL ) break;
    ASSERT( pip_signal_wait( SIGCHLD ) == 0 );
  }
  RETURN( pipid );
}

int pip_wait( int pipid, int *statusp ) {
  pip_task_t	*task;
  int 		err = 0;

  ENTER;
  if( !pip_is_effective() || pip_root == NULL  ) RETURN( EPERM   );
  if( !pip_isa_root()                          ) RETURN( EPERM   );
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err     );
  if( pipid == PIP_PIPID_ROOT                  ) RETURN( EDEADLK );
  DBGF( "PIPID:%d", pipid );

  task = pip_get_task_( pipid );
  if( task->type == PIP_TYPE_NULL ) {
    /* already waited or already gone */
    err = ECHILD;
  } else {
    while( 1 ) {
      if( pip_wait_task( task ) ) {
	if( statusp != NULL ) *statusp = task->status;
	pip_finalize_task( task );
	break;
      }
      ASSERT( pip_signal_wait( SIGCHLD ) == 0 );
    }
  }
  RETURN( err );
}

int pip_trywait( int pipid, int *statusp ) {
  pip_task_t 	*task;
  int 		err;

  ENTER;
  if( !pip_is_effective() || pip_root == NULL  ) RETURN( EPERM   );
  if( !pip_isa_root() )                          RETURN( EPERM   );
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err     );
  if( pipid == PIP_PIPID_ROOT )                  RETURN( EDEADLK );
  DBGF( "PIPID:%d", pipid );

  task = pip_get_task_( pipid );
  if( ( err = pip_do_wait( task, 0 ) ) == 0 ) {
    if( statusp != NULL ) *statusp = task->status;
    pip_finalize_task( task );
  }
  RETURN( err );
}

int pip_wait_any( int *pipidp, int *statusp ) {
  int pipid, err = 0;

  ENTER;
  if( !pip_is_effective() || !pip_isa_root() ) RETURN( EPERM );

  pipid = pip_blocking_waitany();
  if( pipid == PIP_PIPID_ANY ) {
    err = ECHILD;
  } else {
    pip_task_t *task = pip_get_task_( pipid );
    if( pipidp  != NULL ) *pipidp  = pipid;
    if( statusp != NULL ) *statusp = task->status;
    pip_finalize_task( task );
  }
  RETURN( err );
}

int pip_trywait_any( int *pipidp, int *statusp ) {
  int pipid, err = 0;

  ENTER;
  if( !pip_is_effective() || 
      pip_root == NULL    ||
      !pip_isa_root() ) RETURN( EPERM   );

  pipid = pip_nonblocking_waitany();
  if( pipid == PIP_PIPID_NULL ) {
    err = ECHILD;
  } else if( pipid == PIP_PIPID_ANY ) {
    err = ECHILD;
  } else {
    pip_task_t *task = pip_get_task_( pipid );
    if( pipidp  != NULL ) *pipidp  = pipid;
    if( statusp != NULL ) *statusp = task->status;
    pip_finalize_task( task );
  }
  RETURN( err );
}
