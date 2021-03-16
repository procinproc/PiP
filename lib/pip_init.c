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

#include <pip/pip_internal.h>
#include <pip/pip_gdbif.h>

pip_root_t		*pip_root PIP_PRIVATE;
pip_task_t		*pip_task PIP_PRIVATE;
struct pip_gdbif_root	*pip_gdbif_root PIP_PRIVATE;

int pip_is_initialized( void ) {
  return pip_task != NULL && pip_root != NULL;
}

int pip_isa_root( void ) {
  return pip_is_initialized() && PIP_ISA_ROOT( pip_task );
}

int pip_is_threaded_( void ) {
  return pip_root->opts & PIP_MODE_PTHREAD;
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
  if( task->flag_exit == 0 ) {	
    /* not yet terminated */
    if( pip_is_threaded_() ) {
#ifndef USE_TGKILL
      err = pthread_kill( task->thread, sig );
#else
      err = tgkill( task->pid, task->tid, sig );
#endif
    } else {
      if( kill( task->tid, sig ) != 0 ) {
	err = errno;
      }
    }
  }
  RETURN( err );
}

void pip_abort( void ) __attribute__((noreturn));
void pip_abort( void ) {
  /* thin function may be called either root or tasks */
  /* SIGTERM is delivered to root so that PiP tasks   */
  /* are forced to ternminate                         */
  ENTER;
  if( pip_root != NULL ) {
    (void) pip_raise_signal( pip_root->task_root, SIGTERM );
  } else {
    kill( pip_gettid(), SIGTERM );
  }
  while( 1 ) sleep( 1 );	/* wait for being killed */
  NEVER_REACH_HERE;
}

pid_t pip_gettid( void ) {
  return (pid_t) syscall( (long int) SYS_gettid );
}

int pip_is_magic_ok( pip_root_t *root ) {
  return strncmp( root->magic,
		  PIP_MAGIC_WORD,
		  PIP_MAGIC_WLEN ) == 0;
}

int pip_is_version_ok( pip_root_t *root ) {
  return( root->version == PIP_API_VERSION );
}

int pip_are_sizes_ok( pip_root_t *root ) {
  return( root->size_root  == sizeof( pip_root_t ) &&
	  root->size_task  == sizeof( pip_task_t ) );
}

static int pip_check_root( pip_root_t *root ) {
  if( root == NULL ) {
    return 1;
  } else if( !pip_is_magic_ok( root ) ) {
    return 2;
  } else if( !pip_is_version_ok( root ) ) {
    return 3;
  } else if( !pip_are_sizes_ok( root ) ) {
    return 4;
  }
  return 0;
}

#define PIP_MESGLEN		(512)
static void
pip_message( FILE *fp, char *tag, int dnl, const char *format, va_list ap ) {
  char mesg[PIP_MESGLEN];
  char idstr[PIP_MIDLEN];
  int len, fd;

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

#ifdef DEBUG
int pip_debug_env( void ) {
  static int flag = 0;
  if( !flag ) {
    if( getenv( "PIP_NODEBUG" ) ) {
      flag = -1;
    } else {
      flag = 1;
    }
  }
  return flag > 0;
}
#endif

static char *pip_path_gdb;
static char *pip_command_gdb;

pip_task_t *pip_current_task( int tid ) {
  /* do not put any DBG macors in this function */
  pip_root_t	*root = pip_root;
  pip_task_t 	*task;
  static int	curr = 0;
  int 			i;

  if( root != NULL ) {
    for( i=curr; i<root->ntasks+1; i++ ) {
      task = &root->tasks[i];
      if( tid == task->tid ) {
	curr = i;
	return task;
      }
    }
    for( i=0; i<curr; i++ ) {
      task = &root->tasks[i];
      if( tid == task->tid ) {
	curr = i;
	return task;
      }
    }
  }
  return NULL;
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

int pip_task_str( char *p, size_t sz, pip_task_t *task ) {
  int 	n = 0;

  if( task == NULL ) {
    n = snprintf( p, sz, "-" );
  } else if( task->type == PIP_TYPE_NULL ) {
    n = snprintf( p, sz, "*" );
  } else if( PIP_ISA_TASK( task ) ) {
    n = pip_pipid_str( p, sz, task->pipid, 1 );
  } else {
    n = snprintf( p, sz, "!" );
  }
  return n;
}

size_t pip_idstr( char *p, size_t s ) {
  pid_t		tid  = pip_gettid();
  pip_task_t	*ctx = pip_task;
  pip_task_t	*kc  = pip_current_task( tid );
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

void pip_describe( pid_t tid ) __attribute__ ((unused));
void pip_describe( pid_t tid ) {
  pip_task_t *task = pip_current_task( tid );
  char  *backtrace = "back-traced";
  char  *debugged  = "debugged";
  char	*trailer;

  if( pip_command_gdb == NULL ) {
    trailer = backtrace;
  } else {
    trailer = debugged;
  }
  if( task != NULL ) {
    int pipid = task->pipid;
    if( pipid == PIP_PIPID_ROOT ) {
      printf( "\nPiP-ROOT(TID:%d) is %s\n\n", tid, trailer );
    } else if( pipid >= 0 ) {
      printf( "\nPiP-TASK(PIPID:%d,TID:%d) is %s\n\n", pipid, tid, trailer );
    } else {
      printf( "\nPiP-TASK(PIPID:??,TID:%d) is %s\n\n", tid, trailer );
    }
  } else {
    printf( "\nPiP-unknown(TID:%d) is %s\n\n", tid, trailer );
  }
  fflush( NULL );
}

static void pip_attach_gdb( void ) {
  pid_t	target = pip_gettid();
  pid_t	pid;

  ENTER;
  if( pip_path_gdb != NULL ) {
    if( ( pid = fork() ) == 0 ) {
      extern char **environ;
      char attach[32];
      char describe[64];
      char *argv[32];
      int argc = 0;

      snprintf( attach,   sizeof(attach),   "%d", target );
      snprintf( describe, sizeof(describe), "call pip_describe(%d)", target );
      if( pip_command_gdb == NULL ) {
	/*  1 */ argv[argc++] = pip_path_gdb;
	/*  2 */ argv[argc++] = "-quiet";
	/*  3 */ argv[argc++] = "-ex";
	/*  4 */ argv[argc++] = "set verbose off";
	/*  5 */ argv[argc++] = "-ex";
	/*  6 */ argv[argc++] = "set complaints 0";
	/*  7 */ argv[argc++] = "-ex";
	/*  8 */ argv[argc++] = "set confirm off";
	/*  9 */ argv[argc++] = "-p";
	/* 10 */ argv[argc++] = attach;
	/* 11 */ argv[argc++] = "-ex";
	/* 12 */ argv[argc++] = "info inferiors";
	/* 13 */ argv[argc++] = "-ex";
	/* 14 */ argv[argc++] = describe;
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
  }
}

#define PIP_DEBUG_BUFSZ		(4096)
#define PIP_MAPS_PATH		"/proc/self/maps"

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

char *pip_get_prefix_dir( char *name ) {
  FILE	 *fp_maps;
  size_t sz, l;
  char	 *line, *fname, *prefix, *p;
  char	 *updir = "/..";
  void	 *sta, *end;
  void	 *faddr = (void*) pip_get_prefix_dir;

  fp_maps = NULL;
  line    = NULL;
  if( pip_root != NULL &&
      pip_root->installdir != NULL ) {
    prefix = pip_root->installdir;
  } else {
    sz = 0;
    ASSERT( ( fp_maps = fopen( "/proc/self/maps", "r" ) ) != NULL );
    while( ( l = getline( &line, &sz, fp_maps ) ) > 0 ) {
      //DBGF( "l:%d sz:%d line(%p):%s", (int)l, (int)sz, line, line );
      line[l] = '\0';
      prefix = NULL;
      if( sscanf( line, "%p-%p %*4s %*x %*d:%*d %*d %ms", &sta, &end, &prefix ) == 3 ) {
	DBGF( "%p-%p %p %s", sta, end, faddr, prefix );
	if( prefix != NULL && sta <= faddr && faddr < end ) {
	  if( pip_root != NULL ) {
	    prefix = dirname( prefix );
	    DBGF( "prefix: %s", prefix );
	    pip_root->installdir = prefix;
	  }
	  goto found;
	}
	free( prefix );
      }
    }
    fname = NULL;
    goto not_found;
  }
 found:
  ASSERT( ( fname = malloc( strlen( prefix ) +
			    strlen( updir )  +
			    strlen( name )   + 1 ) ) 
	  != NULL );
  p = stpcpy( fname, prefix );
  p = stpcpy( p, updir );
  p = stpcpy( p, name );
  DBGF( "fname: %s", fname );
 not_found:
  if( line != NULL ) free( line );
  fclose( fp_maps );
  return fname;
}

static void pip_show_pips( void ) {
  ENTER;
  char *env = pip_root->envs.show_pips;
  if( env != NULL && strcasecmp( env, "on" ) == 0 ) {
    char *pips_name = "/bin/pips";
    char *pips_path = pip_get_prefix_dir( pips_name );
    if( access( pips_path, X_OK ) == 0 ) {
      pip_info_mesg( "*** Show PIPS (%s)", pips_path );
      system( pips_path );
    } else {
      pip_err_mesg( "Unable to find pips %s", pips_path );
    }
    free( pips_path );
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

static void pip_exception_handler( int sig, siginfo_t *info, void *extra ) {
  ENTER;
  if( pip_root != NULL ) pip_spin_lock( &pip_root->lock_bt );
  {
    pip_err_mesg( "*** Exception signal: %s (%d) !!", strsignal(sig), sig );
    pip_debug_info();
  }
  if( pip_root != NULL ) pip_spin_unlock( &pip_root->lock_bt );
  (void) kill( pip_gettid(), sig );
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
  ASSERT( posix_memalign( allocp, pgsz, sz ) == 0 &&
	  *allocp != NULL );
}

#define PIP_MINSIGSTKSZ 	(MINSIGSTKSZ*2)

void pip_debug_on_exceptions( pip_task_t *task ) {
  char			*path, *command, *signals;
  sigset_t 		sigs, sigempty;
  struct sigaction	sigact;
  static int		done = 0;
  int			i;

  if( done ) return;
  done = 1;

  if( ( path = pip_root->envs.gdb_path ) != NULL &&
      *path != '\0' ) {
    ASSERT( sigemptyset( &sigs     ) == 0 );
    ASSERT( sigemptyset( &sigempty ) == 0 );
    ASSERT( sigaddset( &sigs, SIGHUP  ) == 0 );
    ASSERT( sigaddset( &sigs, SIGSEGV ) == 0 );

    if( !pip_is_threaded_() ) {
      if( access( path, X_OK ) != 0 ) {
	  pip_err_mesg( "Unable to execute (%s)", path );
      } else {
	pip_path_gdb = path;
	if( ( command = pip_root->envs.gdb_command ) != NULL &&
	    *command != '\0' ) {
	  if( access( command, R_OK ) == 0 ) pip_command_gdb = command;
	}
	if( ( signals = pip_root->envs.gdb_signals ) != NULL ) {
	  pip_set_gdb_sigset( signals, &sigs );
	}
	if( memcmp( &sigs, &sigempty, sizeof(sigs) ) != 0 ) {
	  /* FIXME: since the sigaltstack is allocated  */
	  /* by a PiP task, there is no chance to free  */
	  void		*altstack;
	  stack_t	sigstack;

	  pip_page_alloc( PIP_MINSIGSTKSZ, &altstack );
	  task->sigalt_stack = altstack;
	  memset( &sigstack, 0, sizeof( sigstack ) );
	  sigstack.ss_sp   = altstack;
	  sigstack.ss_size = PIP_MINSIGSTKSZ;
	  ASSERT( sigaltstack( &sigstack, NULL ) == 0 );

	  memset( &sigact, 0, sizeof( sigact ) );
	  sigact.sa_sigaction = pip_exception_handler;
	  sigact.sa_mask      = sigs;
	  sigact.sa_flags     = SA_RESETHAND | SA_ONSTACK;

	  for( i=0; sigtab[i].name!=NULL; i++ ) {
	    int signum = sigtab[i].signum;
	    if( sigismember( &sigs, signum ) ) {
	      DBGF( "PiP-gdb on signal: %s ", sigtab[i].name );
	      ASSERT( sigaction( signum, &sigact, NULL ) == 0 );
	    }
	  }
	}
      }
    } else {
      /* exception signals must be blocked in thread mode */
      /* so that root hanlder can catch them */
      ASSERT( sigaddset( &sigs, SIGTERM ) == 0 );
      ASSERT( sigaddset( &sigs, SIGCHLD ) == 0 );
      ASSERT( pthread_sigmask( SIG_BLOCK, &sigs, NULL ) == 0 );
    }
  }
}

/* the following function will be called implicitly */
/* this function is called by PiP tasks only        */
int pip_init_task_implicitly( pip_root_t *root,
			      pip_task_t *task )
  __attribute__ ((unused));	/* actually this is being used */
int pip_init_task_implicitly( pip_root_t *root,
			      pip_task_t *task ) {
  ENTER;
  int err = pip_check_root( root );
  if( !err ) {
    if( ( pip_root != NULL && pip_root != root ) ||
	( pip_task != NULL && pip_task != task ) ||
	( pip_gdbif_root != NULL &&
	  pip_gdbif_root != root->gdbif_root ) ) {
      err = ELIBSCN;
    } else {
      pip_root = root;
      pip_task = task;
      pip_gdbif_root = root->gdbif_root;
      if( !pip_is_threaded_() ) {
	pip_debug_on_exceptions( task );
      }
    }
  }
  RETURN( err );
}
