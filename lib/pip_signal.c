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

int pip_kill( int pipid, int signo ) {
  int id = pipid;
  int err;

  ENTER;
  if( !pip_is_effective() || pip_root == NULL ) {
    err = EPERM;
  } else if( ( err = pip_check_pipid( &id ) ) == 0 ) {
    pip_task_t *task = pip_get_task_( id );
    if( task != NULL && PIP_IS_ALIVE( task ) ) {
      err = pip_raise_signal( task, signo );
    } else {
      err = ESRCH;
    }
  }
  RETURN( err );
}

int pip_sigmask( int how, const sigset_t *sigmask, sigset_t *oldmask ) {
  sigset_t sigset;
  int err = 0;

  if( !pip_is_effective() || pip_root == NULL ) {
    err = EPERM;
  } else {
    if( pip_root->task_root == pip_task &&
	sigismember( sigmask, SIGCHLD ) ) {
      /* do not block SIGCHLD in root */
      memcpy( &sigset, sigmask, sizeof(sigset) );
      sigdelset( &sigset, SIGCHLD );
      sigmask = &sigset;
    }
    if( pip_is_threaded_() ) {
      err = pthread_sigmask( how, sigmask, oldmask );
    } else {
      errno = 0;
      if( sigprocmask(  how, sigmask, oldmask ) != 0 ) {
	err = errno;
      }
    }
  }
  return( err );
}

int pip_signal_wait( int signo ) {
  sigset_t 	sigset;
  int 		sig, err = 0;

  if( !pip_is_effective() || pip_root == NULL ) {
    err = EPERM;
  } else {
    ASSERTD( sigemptyset( &sigset )      == 0 );
    ASSERTD( sigaddset( &sigset, signo ) == 0 );
    err = sigwait( &sigset, &sig );
#ifdef AH
    if( pip_is_threaded_() ) {
      errno = 0;
      sigwait( &sigset, &sig );
      err = errno;
    } else {
      (void) sigsuspend( &sigset ); /* always returns EINTR */
    }
#endif
  }
  return( err );
}

/* signal handlers */

void pip_set_signal_handler( int sig,
			     void(*handler)(),
			     struct sigaction *oldp ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigact.sa_sigaction = handler;
  ASSERTD( sigaddset( &sigact.sa_mask, sig ) == 0 );
  ASSERTD( sigaction( sig, &sigact, oldp   ) == 0 );
}

static void pip_sigchld_handler( int sig ) {
  DBG;
}

static void pip_exception_handler( int sig ) {
  pip_task_t *task = pip_task;

  pip_err_mesg( "Exception signal: %s (%d) !!", strsignal(sig), sig );
  if( task->debug_signals != NULL &&
      sigismember( task->debug_signals, sig ) ) {
    if( pip_root != NULL ) {
      pip_spin_lock( &pip_root->lock_bt );
    }
    pip_debug_info();
    if( pip_root != NULL ) {
      pip_spin_unlock( &pip_root->lock_bt );
    }
  }
  pip_set_exit_status( task, 0, sig );
  pip_do_exit( task, PIP_EXIT_EXIT, 0 );
  NEVER_REACH_HERE;
}

static void pip_set_exception_handler( int sig ) {
  /* FIXME: since the sigaltstack is allocated  */
  /* by a PiP task, there is no chance to free  */
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
#ifdef AH
  /* FIXME: if threaded, pthread_exit() */
  /*        on an altstack causes SEGV */
  if( pip_task->sigalt_stack == NULL ) {
    void	*altstack;
    stack_t	sigstack;

    pip_page_alloc( PIP_MINSIGSTKSZ, &altstack );
    ASSERTD( altstack != NULL );
    memset( &sigstack, 0, sizeof(sigstack) );
    sigstack.ss_sp   = altstack;
    sigstack.ss_size = PIP_MINSIGSTKSZ;
    ASSERTD( sigaltstack( &sigstack, NULL ) == 0 );
    pip_task->sigalt_stack = altstack;
    sigact.sa_flags  = SA_ONSTACK;
  }
#endif
  sigfillset( &sigact.sa_mask );
  sigact.sa_handler = pip_exception_handler;
  ASSERTD( sigaction( sig, &sigact, NULL ) == 0 );
}

void pip_abort_handler( int unused ) {
  void pip_unset_abort_handler( void );
  static int pip_aborted = 0;
  pip_task_t 	*task;
  int 		i;

  ENTERF( "sig:%d", unused );;

  if( pip_root != NULL && pip_task != NULL ) {
    if( PIP_ISA_ROOT( pip_task ) ) {
      if( pip_aborted ) return;
      pip_aborted = 1;
      for( i=0; i<pip_root->ntasks; i++ ) {
	task = &pip_root->tasks[i];
	if( PIP_IS_ALIVE( task ) ) {
	  pip_set_exit_status( task, 0, SIGABRT );
	  pip_raise_sigchld( task );
	  (void) pip_raise_signal( task, SIGKILL );
	}
      }
      pip_unset_abort_handler();
      abort();
    } else {			/* PiP task (not root) */
      pip_set_exit_status( pip_task, 0, SIGABRT );
      (void) pip_raise_signal( pip_root->task_root, SIGABRT );
      /* wait to be aborted by root */
      while( 1 ) sleep( 1 );
    }
  }
  RETURNV;
}

void pip_unset_abort_handler( void ) {
  struct sigaction	sigact, oldact;

  ASSERTD( sigaction( SIGABRT, NULL, &oldact ) == 0 );
  if( oldact.sa_handler == pip_abort_handler ) {
    memset( &sigact, 0, sizeof( sigact ) );
    sigact.sa_handler = SIG_DFL;
    ASSERTD( sigaction( SIGABRT, &sigact, NULL ) == 0 );
  }
}

void pip_raise_sigchld( pip_task_t *task ) {
  if( pip_is_threaded_() ) {
    task->flag_sigchld = 1;
    ASSERT( pip_raise_signal( pip_root->task_root, SIGCHLD ) == 0 );
  }
}

#ifdef AH
static void pip_set_abort_handler( void ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigfillset( &sigact.sa_mask );
  if( !pip_is_threaded_() || PIP_ISA_ROOT( pip_task ) ) {
    sigdelset(  &sigact.sa_mask, SIGABRT );
  }
  sigact.sa_handler = pip_abort_handler;
  ASSERTD( sigaction( SIGABRT, &sigact, NULL ) == 0 );
}
#endif
static void pip_set_abort_handler( void ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigfillset( &sigact.sa_mask );
  if( pip_task != NULL && PIP_ISA_ROOT( pip_task ) ) {
    sigdelset(  &sigact.sa_mask, SIGABRT );
  }
  sigact.sa_handler = pip_abort_handler;
  ASSERTD( sigaction( SIGABRT, &sigact, NULL ) == 0 );
}

static int pip_exception_signals[] = {
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGILL,
  SIGTRAP,
  SIGBUS,
  SIGFPE,
  SIGUSR1,
  SIGSEGV,
  SIGUSR2,
  SIGPIPE,
  SIGALRM,
  SIGTERM,
#ifdef SIGVTALRM
  SIGVTALRM,
#endif
#ifdef SIGXCPU
  SIGXCPU,
#endif
#ifdef SIGXFSZ
  SIGXFSZ,
#endif
#if defined(SIGIOT) && (SIGIOT!=SIGABRT)
  SIGIOT,
#endif
#ifdef SIGSTKFLT
  SIGSTKFLT,
#endif
#ifdef SIGIO
  SIGIO,
#endif
#ifdef SIGPWR
  SIGPWR,
#endif
#ifdef SIGEMT
  SIGEMT,
#endif
#ifdef SIGLOST
  SIGLOST,
#endif
  0
};

static void pip_block_signal( int sig ) {
  sigset_t sigmask;

  ASSERTD( sigemptyset( &sigmask )                                    == 0 );
  ASSERTD( sigaddset(   &sigmask, sig )                               == 0 );
  ASSERTD( sigprocmask( SIG_BLOCK, &sigmask, &pip_root->old_sigmask ) == 0 );
}

void pip_set_signal_handlers( void ) {
  /* setting PiP exceptional signal handler for    */
  /* the signals terminating PiP task (by default) */
  int sig, i;

  for( i=0; (sig=pip_exception_signals[i])>0; i++ ) {
    pip_set_exception_handler( sig );
  }
  if( pip_is_threaded_() ) {
    if( !PIP_ISA_ROOT( pip_task ) ) {
      /* so that SIGABRT is forwarded to the root */
      pip_block_signal( SIGABRT );
    }
  } else {
    pip_set_abort_handler();
  }
  if( PIP_ISA_ROOT( pip_task ) ) {
    pip_block_signal( SIGCHLD ); /* for sigwait(SIGCHLD) */
    pip_set_signal_handler( SIGCHLD,
			    pip_sigchld_handler,
			    NULL );
  } else {
    pip_set_signal_handler( SIGCHLD, SIG_DFL, NULL );
  }
}

static void pip_unset_exception_handler( int sig ) {
  struct sigaction	oldact, sigact;

  ASSERTD( sigaction( sig, NULL, &oldact ) == 0 );
  if( oldact.sa_handler == pip_exception_handler ) {
    memset( &sigact, 0, sizeof( sigact ) );
    sigact.sa_handler = SIG_DFL;
    ASSERTD( sigaction( sig, &sigact, NULL ) == 0 );
  }
}

void pip_unset_signal_handlers( void ) {
  /* setting PiP exceptional signal handler for    */
  /* the signals terminating PiP task (by default) */
  int sig, i;
  for( i=0; ; i++ ) {
    sig = pip_exception_signals[i];
    if( sig == 0 ) break;
    pip_unset_exception_handler( sig );
  }
  pip_unset_abort_handler();
  pip_set_signal_handler( SIGCHLD, SIG_DFL, NULL );
}

static void pip_unblock_signal( void ) {
  ASSERTD( sigprocmask( SIG_UNBLOCK, &pip_root->old_sigmask, NULL ) == 0 );
}


static void pip_sigquit_handler( int, void(*)(), struct sigaction* )
  PIP_NORETURN;
static void pip_sigquit_handler( int sig,
				 void(*handler)(),
				 struct sigaction *oldp ) {
  ENTER;
  pthread_exit( NULL );
  NEVER_REACH_HERE;
}

static void pip_reset_signal_handler( int sig ) {
  if( !pip_is_threaded_() ) {
    struct sigaction	sigact;
    memset( &sigact, 0, sizeof( sigact ) );
    sigact.sa_sigaction = (void(*)(int,siginfo_t*,void*)) SIG_DFL;
    ASSERTD( sigaction( sig, &sigact, NULL ) == 0 );
  } else {
    sigset_t sigmask;
    (void) sigemptyset( &sigmask );
    (void) sigaddset( &sigmask, sig );
    ASSERTD( pthread_sigmask( SIG_BLOCK, &sigmask, NULL ) == 0 );
  }
}
