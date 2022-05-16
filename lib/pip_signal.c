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

static char 	*pip_path_gdb;
static char 	*pip_command_gdb;

static void pip_attach_gdb( void ) {
  pid_t	target = pip_gettid();
  pid_t	pid;

  ENTER;
  pip_info_mesg( "*** Attaching pip-gdb (%s)", pip_path_gdb );
  if( ( pid = fork() ) == 0 ) {
    extern char **environ;
    char attach[32];
    char *argv[32];
    int argc = 0;

    snprintf( attach,   sizeof(attach),   "%d", target );
    if( pip_command_gdb == NULL ) {
      /*  1 */ argv[argc++] = pip_path_gdb;
      /*  2 */ argv[argc++] = "-quiet";
      /*  3 */ argv[argc++] = "-p";
      /*  4 */ argv[argc++] = attach;
      /*  5 */ argv[argc++] = "-ex";
      /*  6 */ argv[argc++] = "set pagination off";
      /*  7 */ argv[argc++] = "-ex";
      /*  8 */ argv[argc++] = "set verbose off";
      /*  9 */ argv[argc++] = "-ex";
      /* 10 */ argv[argc++] = "set complaints 0";
      /* 11 */ argv[argc++] = "-ex";
      /* 12 */ argv[argc++] = "set confirm off";
      /* 13 */ argv[argc++] = "-ex";
      /* 14 */ argv[argc++] = "info inferiors";
      /* 15 */ argv[argc++] = "-ex";
      /* 16 */ argv[argc++] = "bt";
      /* 17 */ argv[argc++] = "-ex";
      /* 18 */ argv[argc++] = "detach";
      /* 19 */ argv[argc++] = "-ex";
      /* 20 */ argv[argc++] = "quit";
      /* 21 */ argv[argc++] = NULL;
    } else {
      /*  1 */ argv[argc++] = pip_path_gdb;
      /*  2 */ argv[argc++] = "-quiet";
      /*  3 */ argv[argc++] = "-p";
      /*  4 */ argv[argc++] = attach;
      /*  5 */ argv[argc++] = "-x";
      /*  6 */ argv[argc++] = pip_command_gdb;
      /*  7 */ argv[argc++] = "-batch";
      /*  8 */ argv[argc++] = NULL;
    }
    (void) close( 0 );		/* close STDIN */
    dup2( 2, 1 );
    execve( pip_path_gdb, argv, environ );
    pip_err_mesg( "Failed to execute PiP-gdb (%s)", pip_path_gdb );

  } else if( pid < 0 ) {
    pip_err_mesg( "Failed to fork PiP-gdb (%s)", pip_path_gdb );
  } else {
    waitpid( pid, NULL, 0 );
  }
  RETURNV;
}

#define PIP_DEBUG_BUFSZ		(4096)

void pip_fprint_maps( FILE *fp ) {
  int fd = open( PIP_MAPS_PATH, O_RDONLY );
  char buf[PIP_DEBUG_BUFSZ];

  if( fd >= 0 ) {
    while( 1 ) {
      ssize_t rc;
      size_t  wc;
      char *p;

      if( ( rc = read( fd, buf, PIP_DEBUG_BUFSZ ) ) <= 0 ) break;
      p = buf;
      do {
	wc = fwrite( p, 1, rc, fp );
	p  += wc;
	rc -= wc;
      } while( rc > 0 );
    }
    close( fd );
  } else {
    pip_err_mesg( "Unable to open %s", PIP_MAPS_PATH );
  }
}

static void pip_show_maps( void ) {
  ENTER;
  pip_info_mesg( "*** Show MAPS" );
  pip_fprint_maps( stderr );
  RETURNV;
}

static void pip_show_pips( char *prefix ) {
  ENTER;
  char *pips_comm = "/bin/pips";
  char *pips_opts = " x";
  char *pips_exec;
  char *p;

  sleep(1);
  ASSERT( ( pips_exec = alloca( strlen( prefix    ) +
				strlen( pips_comm ) + 
				strlen( pips_opts ) + 1 ) ) 
	  != NULL );
  p = pips_exec;
  p = stpcpy( p, prefix    );
  p = stpcpy( p, pips_comm );
  pip_info_mesg( "*** Show PIPS (%s)", pips_exec );
  stpcpy( p, pips_opts );
  system( pips_exec );
  sleep( 1 );			/* to flush out pips messages */
  RETURNV;
}

void pip_debug_info( void ) {
  char *env, *prefix;

  ENTER;
  env = getenv( PIP_ENV_SHOW_MAPS );
  if( env != NULL && strcasecmp( env, "on" ) == 0 ) {
    pip_show_maps();
  }

  
  if( pip_root                              != NULL &&
      ( prefix = pip_root->prefixdir )      != NULL &&
      ( env = getenv( PIP_ENV_SHOW_PIPS ) ) != NULL &&
      strcasecmp( env, "on" ) == 0 ) {
    pip_show_pips( prefix );
  }
  if( pip_path_gdb != NULL ) {
    pip_attach_gdb();
  }
  RETURNV;
}

static int
pip_strncasecmp( const char *str0, const char *str1, const int len1 ) {
  int len0 = strlen( str0 );
  if( len0 != len1 ) return 1;
  len0 = ( len0 < len1 ) ? len0 : len1;
  int x = strncasecmp( str0, str1, len0 );
  return x;
}

struct sigtab {
  char	*name;
  int	signum;
} static const pip_gdb_sigtab[] =
  { { "HUP",  SIGHUP  },
    { "INT",  SIGINT  },
    { "QUIT", SIGQUIT },
    { "ILL",  SIGILL  },
    { "ABRT", SIGABRT },
    { "FPE",  SIGFPE  },
    { "INT",  SIGINT  },
    { "SEGV", SIGSEGV },
    { "PIPE", SIGPIPE },
    { "USR1", SIGUSR1 },
    { "USR2", SIGUSR2 },
    { NULL, 0 } };

static void pip_set_gdb_signal( sigset_t *sigs,
				char *token,
				int len,
				int(*sigman)(sigset_t*,int) ) {
  int i;
  if( pip_strncasecmp( "ALL", token, len ) == 0 ) {
    for( i=0; pip_gdb_sigtab[i].name!=NULL; i++ ) {
      ASSERT( sigman( sigs, pip_gdb_sigtab[i].signum ) == 0 );
    }
  } else {
    for( i=0; pip_gdb_sigtab[i].name!=NULL; i++ ) {
      if( pip_strncasecmp( pip_gdb_sigtab[i].name, token, len ) == 0 ) {
	ASSERT( sigman( sigs, pip_gdb_sigtab[i].signum ) == 0 );
	goto done;
      }
    }
    pip_warn_mesg( "%s: signal name '%.*s' is not acceptable for attaching PiP-gdb and ignored",
		   PIP_ENV_GDB_SIGNALS, len, token );
  }
 done:
  return;
}

static int pip_next_token( char *str, int idx ) {
  int i = idx;
  while( str[i] != '\0' &&
	 str[i] != '+'  &&
	 str[i] != '-' ) i ++;
  DBGF( "token:%.*s (%d)", i-idx, str, i-idx );
  return i;
}

static void pip_set_gdb_sigset( char *env, sigset_t *sigs ) {
  int  sign, len, i, j;

  DBGF( "signals: %s", env );
  if( env[0] != '\0' ) {
    sign = '+';
    for( i=0; sign!='\0'; i=j+1 ) {
      DBGF( "i=%d", i );
      j = pip_next_token( env, i );
      DBGF( "j=%d", j );
      if( j == i ) continue;
      len = j - i;
      switch( sign ) {
      case '+':
	pip_set_gdb_signal( sigs, &env[i], len, sigaddset );
	DBGF( "%.*s is added", len, &env[i] );
	break;
      case '-':
	pip_set_gdb_signal( sigs, &env[i], len, sigdelset );
	DBGF( "%.*s is removed", len, &env[i] );
	break;
      case '\0':
	return;
      }
      sign = env[j];
      DBGF( "sign:%c (%d)", sign, j );
    }
  }
}

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
  int err = 0;

  if( !pip_is_effective() || pip_root == NULL ) {
    err = EPERM;
  } else {
    if( pip_is_threaded_() ) {
      err = pthread_sigmask( how, sigmask, oldmask );
    } else {
      if( sigprocmask( how, sigmask, oldmask ) != 0 ) {
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
    ASSERTD( sigemptyset( &sigset      ) == 0 );
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

static void pip_block_signal( int sig ) {
  sigset_t sigmask;
  if( sig > 0 ) {
    ASSERTD( sigemptyset( &sigmask )      == 0 );
    ASSERTD( sigaddset(   &sigmask, sig ) == 0 );
  } else {
    ASSERTD( sigfillset(  &sigmask )      == 0 );
  }
  ASSERTD( pthread_sigmask( SIG_BLOCK, &sigmask, NULL ) == 0 );
}

static void pip_unblock_signal( int sig ) {
  sigset_t sigmask;
  if( sig > 0 ) {
    ASSERTD( sigemptyset( &sigmask )      == 0 );
    ASSERTD( sigaddset(   &sigmask, sig ) == 0 );
  } else {
    ASSERTD( sigfillset(  &sigmask )      == 0 );
  }
  ASSERTD( pthread_sigmask( SIG_UNBLOCK, &sigmask, NULL ) == 0 );
}

void pip_set_signal_handler( int sig,
			     void(*handler)(),
			     struct sigaction *oldp ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigact.sa_sigaction = handler;
  sigact.sa_flags     = SA_RESTART;
  ASSERTD( sigaddset( &sigact.sa_mask, sig ) == 0 );
  ASSERTD( sigaction( sig, &sigact, oldp   ) == 0 );
}

void pip_unset_signal_handler( int sig, void *handler ) {
  struct sigaction oldact;

  if( handler == NULL ) {
    pip_set_signal_handler( sig, SIG_DFL, NULL );
  } else {
    ASSERTD( sigaction( SIGABRT, NULL, &oldact ) == 0 );
    if( oldact.sa_handler == handler ) {
      pip_set_signal_handler( sig, SIG_DFL, NULL );
    }
  }
}

#ifdef DEBUG
static void pip_sigchld_handler( int sig ) {  
  DBG;
}
#endif

static void pip_exception_handler( int sig ) {
  pip_task_t *task = NULL;

  if( pip_is_threaded_() && pip_root != NULL ) {
    pip_sem_wait( &pip_root->lock_sighand );
  }
  {
    ENTERF( "Signal=%d", sig );
    fflush( NULL );
    pip_err_mesg( "Exception signal: %s (%d) !!", 
		  strsignal(sig), sig );
    if( pip_is_threaded_() && pip_root != NULL ) {
      /* we cannot rely on the pip_task variable 
	 since this handler code is shared among PiP tasks */
      task = pip_current_task();
    } else {
      task = pip_task;
    }
    if( task != NULL                &&
	task->debug_signals != NULL &&
	sigismember( task->debug_signals, sig ) ) {
      if( pip_root != NULL ) {
	pip_spin_lock( &pip_root->lock_bt );
      }
      pip_debug_info();
      if( pip_root != NULL ) {
	pip_spin_unlock( &pip_root->lock_bt );
      }
    }
  }
  if( pip_is_threaded_() && pip_root != NULL ) {
    pip_sem_post( &pip_root->lock_sighand );
  }
  pip_set_exit_status( task, 0, sig );
  pip_do_exit( task, PIP_EXIT_EXIT, 0 );
  NEVER_REACH_HERE;
}

static void pip_set_exception_handler( int sig ) {
  struct sigaction	oldact, sigact;

  ASSERTD( sigaction( sig, NULL, &oldact ) == 0 );
  if( oldact.sa_handler != pip_exception_handler ) {
    //DBGF( "sig:%d handler is set", sig );
    memset( &sigact, 0, sizeof( sigact ) );
    sigact.sa_handler = pip_exception_handler;
    sigact.sa_flags   = SA_RESTART;
    ASSERTD( sigaddset( &sigact.sa_mask, sig ) == 0 );
    ASSERTD( sigaction( sig, &sigact, NULL   ) == 0 );
  }
}

static void pip_unset_abort_handler( void ) {
  pip_unset_signal_handler( SIGABRT, pip_abort_handler );
}

void pip_raise_sigchld( pip_task_t *task ) {
  if( pip_is_threaded_() ) {
    task->flag_sigchld = 1;
    ASSERT( pip_raise_signal( pip_root->task_root, SIGCHLD ) == 0 );
  }
}

int pip_kill_all_children_( int killsig ) {
  static int pip_aborted = 0;
  int 	 i;

  ENTER;
  if( pip_root != NULL && pip_task != NULL ) {
    if( PIP_ISA_ROOT( pip_task ) ) {
      if( !pip_aborted ) {
	pip_aborted = 1;

	for( i=0; i<pip_root->ntasks; i++ ) {
	  pip_task_t *task = &pip_root->tasks[i];
	  if( PIP_IS_ALIVE( task ) ) {
	    if( killsig != 0 ) {
	      pip_set_exit_status( task, 0, killsig );
	      pip_raise_sigchld( task );
	    }
	    (void) pip_raise_signal( task, killsig );
	  }
	  if( task->pid_onstart > 0 ) {
	    (void) kill( task->pid_onstart, killsig );
	  }
	}
      }
    } 
  }
  RETURN( 0 );
}

void pip_abort_handler( int unused ) {
  ENTERF( "sig:%d", unused );
  if( pip_task != NULL && pip_root != NULL ) {
    if( PIP_ISA_ROOT( pip_task ) ) {
      (void) pip_kill_all_children_( SIGKILL );
      pip_unset_abort_handler();
      abort();
    } else {
      (void) pip_raise_signal( pip_root->task_root, SIGABRT );
    }
    while( 1 ) sleep( 1 );
  }
  abort();
  NEVER_REACH_HERE;
}

static void pip_set_abort_handler( void ) {
  struct sigaction	sigact;

  memset( &sigact, 0, sizeof( sigact ) );
  sigact.sa_handler = pip_abort_handler;
  sigact.sa_flags   = SA_RESTART;
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
  0				/* end of list */
};

void pip_set_signal_handlers( void ) {
  /* setting PiP exceptional signal handler for    */
  /* the signals terminating PiP task (by default) */
  char		*path, *signals;
  sigset_t	*sigsetp, sigempty;
  int 		sig, i;

  ASSERTD( ( sigsetp = malloc( sizeof(sigset_t) ) ) != NULL );
  ASSERTD( sigemptyset( sigsetp   ) == 0 );
  ASSERTD( sigemptyset( &sigempty ) == 0 );
  if( ( signals = getenv( PIP_ENV_GDB_SIGNALS ) ) != NULL ) {
    pip_set_gdb_sigset( signals, sigsetp );
  } else {
    ASSERTD( sigaddset( sigsetp, SIGHUP  ) == 0 );
    ASSERTD( sigaddset( sigsetp, SIGSEGV ) == 0 );
  }
  pip_task->debug_signals = sigsetp;

  if( ( path = getenv( PIP_ENV_GDB_PATH ) ) != NULL ) {
    pip_path_gdb    = path;
    pip_command_gdb = getenv( PIP_ENV_GDB_COMMAND );
  }

  if( PIP_ISA_ROOT( pip_task ) ) {
    for( i=0; (sig=pip_exception_signals[i])>0; i++ ) {
      pip_set_exception_handler( sig );
    }
    if( !sigismember( sigsetp, SIGABRT ) ) {
      if( !pip_is_threaded_() ) {
	/* in process mode, all tasks must be aborted explicitly */
	pip_set_abort_handler();
      } else {
	/* in thread mode, no handler is required to kill tasks */
      }
    }
    pip_block_signal( SIGCHLD ); /* for sigwait(SIGCHLD) */
#ifdef DEBUG
    pip_set_signal_handler( SIGCHLD,
			    pip_sigchld_handler,
			    NULL );
#endif
  } else if( !pip_is_threaded_() ) { 
    /* tasks in process mode */
    for( i=0; (sig=pip_exception_signals[i])>0; i++ ) {
      pip_set_exception_handler( sig );
    }
    pip_set_abort_handler();
  } else {		
    /* tasks in thread mode  */
    pip_block_signal( SIGABRT );
    /* so that SIGABRT is forwarded to the root */
  }
}

static void pip_unset_exception_handler( int sig ) {
  pip_unset_signal_handler( SIGABRT, pip_exception_handler );
}

void pip_unset_signal_handlers( void ) {
  int sig, i;

  for( i=0; (sig=pip_exception_signals[i])>0; i++ ) {
    pip_unset_exception_handler( sig );
  }
  pip_unset_abort_handler();
  pip_unset_signal_handler( SIGCHLD, NULL );

  if( pip_is_threaded_() ) {
    if( !PIP_ISA_ROOT( pip_task ) ) {
      pip_unblock_signal( SIGABRT );
    }
  }
}

