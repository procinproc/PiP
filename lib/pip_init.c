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
#include <pip/pip_gdbif.h>

int 			pip_initialized      PIP_PRIVATE = 0;
int 			pip_finalized        PIP_PRIVATE = 1;
pip_sem_t		*pip_universal_lockp PIP_PRIVATE;
pip_root_t		*pip_root            PIP_PRIVATE;
pip_task_t		*pip_task            PIP_PRIVATE;
struct pip_gdbif_root	*pip_gdbif_root      PIP_PRIVATE;

static char 		*pip_path_gdb;
static char 		*pip_command_gdb;

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

int pip_is_magic_ok( pip_root_t *root ) {
  return strncmp( root->magic,
		  PIP_MAGIC_WORD,
		  PIP_MAGIC_LEN ) == 0;
}

int pip_is_version_ok( pip_root_t *root ) {
  return( root->version == PIP_API_VERSION );
}

int pip_are_sizes_ok( pip_root_t *root ) {
  return( root->size_root  == sizeof( pip_root_t ) &&
	  root->size_task  == sizeof( pip_task_t ) );
}

int pip_check_root_and_task( pip_root_t *root, pip_task_t *task ) {
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

#define PIP_MESGLEN		(512)
static void
pip_message( FILE *fp, char *tag, int dnl, const char *format, va_list ap ) {
  char mesg[PIP_MESGLEN];
  char idstr[PIP_MIDLEN];
  int len, fd;

  if( pip_root == NULL || !pip_root->flag_quiet ) {
    pip_idstr( idstr, PIP_MIDLEN );
    len = 0;
    if( dnl ) {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len, "\n" );
    }
    len += snprintf(    &mesg[len], PIP_MESGLEN-len, "%s%s ", tag, idstr );
    len += vsnprintf(   &mesg[len], PIP_MESGLEN-len, format, ap );
    if( dnl ) {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len, "\n\n" );
    } else {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len, "\n" );
    }
    fflush( fp );
    /* !!!! DON'T USE FPRINTF HERE !!!! */
    fd = fileno( fp );
    (void) write( fd, mesg, len );
  }
}

void pip_info_fmesg( FILE *fp, const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  if( fp == NULL ) fp = stderr;
  pip_message( fp, "PiP-INFO", 0, format, ap );
  va_end( ap );
}

void pip_info_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr, "PiP-INFO", 0, format, ap );
  va_end( ap );
}

void pip_warn_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr, "PiP-WARN", 0, format, ap );
  va_end( ap );
}

void pip_err_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr,
	       "PiP-ERR", 
#ifdef DEBUG
	       1,
#else
	       0,
#endif
	       format, ap );
  va_end( ap );
}

pip_task_t *pip_current_task( void ) {
  /* do not put any DBG macors in this function */
  pid_t		tid   = pip_gettid();
  pip_root_t	*root = pip_root;
  pip_task_t 	*task;
  static int	curr = 0;
  int 		i;

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

static int
pip_pipid_str( char *p, size_t sz, int pipid, int upper ) {
  char	c;

  /* !! NEVER CALL CTYPE FUNCTION HERE !! */
  /* it may NOT be initialized and cause SIGSEGV */
  switch( pipid ) {
  case PIP_PIPID_ROOT:
    c = (upper)? 'R' : 'r'; goto one_char;
    break;
  case PIP_PIPID_MYSELF:
    c = (upper)? 'S' : 's'; goto one_char;
    break;
  case PIP_PIPID_ANY:
    c = (upper)? 'A' : 'a'; goto one_char;
    break;
  case PIP_PIPID_NULL:
    c = (upper)? 'U' : 'u'; goto one_char;
    break;
  default:
    c = (upper)? 'T' : 't';
    if( pip_root != NULL && pip_root->ntasks > 0 ) {
      if( 0 <= pipid && pipid < pip_root->ntasks ) {
	return snprintf( p, sz, "%c%d", c, pipid );
      } else {
	return snprintf( p, sz, "%c##", c );
      }
    }
  }
  c = '?';

 one_char:
  return snprintf( p, sz, "%c", c );
}

static int pip_task_str( char *p, size_t sz, pip_task_t *task ) {
  int 	n = 0;

  if( task == NULL ) {
    n = snprintf( p, sz, "-" );
  } else if( task->type == PIP_TYPE_NULL ) {
    n = snprintf( p, sz, "*" );
  } else if( PIP_IS_ALIVE( task ) ) {
    n = pip_pipid_str( p, sz, task->pipid, 1 );
  } else {
    n = snprintf( p, sz, "x" );
  }
  return n;
}

size_t pip_idstr( char *p, size_t s ) {
  pid_t		tid  = pip_gettid();
  pip_task_t	*ctx = pip_task;
  pip_task_t	*kc  = pip_current_task();
  char 		*opn = "[", *cls = "]", *delim = ":";
  int		n;

  n = snprintf( p, s, "%s", opn ); 	s -= n; p += n;
  {
    n = snprintf( p, s, "%d(", tid ); 	s -= n; p += n;
    n = pip_task_str( p, s, kc ); 	s -= n; p += n;
    n = snprintf( p, s, ")%s", delim );	s -= n; p += n;
    n = pip_task_str( p, s, ctx );	s -= n; p += n;
  }
  n = snprintf( p, s, "%s", cls ); 	s -= n; p += n;
  return s;
}

static void pip_attach_gdb( void ) {
  pid_t	target = pip_gettid();
  pid_t	pid;

  ENTER;
  if( pip_path_gdb != NULL ) {
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
  } else {			/* show backtrace instead */
#define BTBUF_SZ	(128)
    void *btbuf[BTBUF_SZ];
    int nbt;
    char **bt_str;

    if( ( nbt = backtrace( btbuf, BTBUF_SZ ) ) == 0 ) {
      pip_info_mesg( "No backtrace available (1)" );
    } else if( ( bt_str = backtrace_symbols( btbuf, nbt ) ) == 
	       NULL ) {
      pip_info_mesg( "No backtrace available (2)" );
    } else {
      int i;
      for( i=0; i<nbt; i++ ) {
	pip_info_mesg( "backtrace: %s", bt_str[i] );
      }
      free( bt_str );
    }
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
  char *env = pip_root->envs.show_maps;
  if( env != NULL && strcasecmp( env, "on" ) == 0 ) {
    pip_info_mesg( "*** Show MAPS" );
    pip_fprint_maps( stderr );
  }
  RETURNV;
}

static void pip_show_pips( void ) {
  ENTER;
  char *env    = pip_root->envs.show_pips;
  char *prefix = pip_root->prefixdir;
  if( env                     != NULL && 
      strcasecmp( env, "on" ) == 0    &&
      prefix                  != NULL ) {
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
  }
  RETURNV;
}

void pip_debug_info( void ) {
  ENTER;
  pip_show_maps();
  pip_show_pips();
  pip_attach_gdb();
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
} static const sigtab[] =
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
    for( i=0; sigtab[i].name!=NULL; i++ ) {
      ASSERT( sigman( sigs, sigtab[i].signum ) == 0 );
    }
  } else {
    for( i=0; sigtab[i].name!=NULL; i++ ) {
      if( pip_strncasecmp( sigtab[i].name, token, len ) == 0 ) {
	ASSERT( sigman( sigs, sigtab[i].signum ) == 0 );
	goto done;
      }
    }
    pip_warn_mesg( "%s: signal name '%.*s' is not acceptable and ignored",
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

#define ROUNDUP(X,Y)		((((X)+(Y)-1)/(Y))*(Y))
void pip_page_alloc( size_t sz, void **allocp ) {
  size_t pgsz;

  if( pip_root == NULL ) {
    pgsz = sysconf( _SC_PAGESIZE );
    pgsz = ( pgsz <= 0 ) ? 4096 : pgsz;
  } else if ( pip_root->page_size == 0 ) {
    pgsz = sysconf( _SC_PAGESIZE );
    pgsz = ( pgsz <= 0 ) ? 4096 : pgsz;
    pip_root->page_size = pgsz;
  } else {
    pgsz = pip_root->page_size;
  }
  sz = ROUNDUP( sz, pgsz );
  ASSERT( pip_posix_memalign( allocp, pgsz, sz ) == 0 &&
	  *allocp != NULL );
}

void pip_debug_on_exceptions( pip_root_t *root, pip_task_t *task ) {
  char			*path, *signals;
  sigset_t 		*sigsetp, sigempty;
  static int		done = 0;

  /* this function may be called twice (implicitly and explicitly) */
  if( done ) return;
  done = 1;

  ASSERTD( ( sigsetp = malloc( sizeof(sigset_t) ) ) != NULL );
  ASSERTD( sigemptyset( sigsetp   ) == 0 );
  ASSERTD( sigemptyset( &sigempty ) == 0 );
  if( ( signals = root->envs.gdb_signals ) != NULL ) {
    pip_set_gdb_sigset( signals, sigsetp );
  } else {
    ASSERTD( sigaddset( sigsetp, SIGHUP  ) == 0 );
    ASSERTD( sigaddset( sigsetp, SIGSEGV ) == 0 );
  }
  pip_task->debug_signals = sigsetp;

  if( ( path = pip_root->envs.gdb_path ) != NULL ) {
    pip_path_gdb = path;
    pip_command_gdb = root->envs.gdb_command;
  }
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

/* the following function will be called implicitly */
/* this function is called only by PiP root         */
int pip_init_task_implicitly( pip_root_t *root, pip_task_t *task, char **envv ) {
  int pipid, i, err = EPERM;

  ENTERF( "root: %p  task : %p", root, task );
  
  if( task == NULL ) {
    pip_err_mesg( "Invalid PiP task" );
  } else {
    pipid = task->pipid;
    if( &root->tasks[pipid] != task ) {
      pip_err_mesg( "Invalid PiP root and task" );

    } else if( ( err = pip_check_root_and_task( root, task ) ) == 0 ) {
      pip_root            = root;
      pip_task            = task;
      pip_universal_lockp = &root->universal_lock;

      environ = NULL;
      for( i=0; envv[i]!=NULL; i++ ) putenv( envv[i] );

      if( root->opts & PIP_MODE_PTHREAD ) {
	pip_initialized = PIP_MODE_PTHREAD;
      } else {
	pip_debug_on_exceptions( root, task );
	pip_initialized = PIP_MODE_PROCESS;
      }
      pip_finalized = 0;

      pip_set_signal_handlers();
      ASSERTD( pthread_atfork( NULL, NULL, pip_after_fork ) == 0 );

      DBGF( "pip_root: %p @ %p  piptask : %p @ %p", 
	    pip_root, &pip_root, pip_task, &pip_task );
    }
  }
  RETURN( err );
}

int pip_fin_task_implicitly( void ) {
  pip_root = NULL;
  pip_task = NULL;
  return 0;
}
