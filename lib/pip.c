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
#include <pip/pip.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/prctl.h>

//#define DEBUG

/* the EVAL env. is to measure the time for calling dlmopen() */
//#define EVAL

#include <pip/pip.h>
#include <pip/pip_util.h>
#include <pip/pip_gdbif.h>

extern void *pip_dlsym_unsafe( void*, const char* );

static pip_spinlock_t 	*pip_clone_lock;
static pip_clone_t*	pip_cloneinfo   = NULL;

static int (*pip_clone_mostly_pthread_ptr) (
	pthread_t *newthread,
	int clone_flags,
	int core_no,
	size_t stack_size,
	void *(*start_routine) (void *),
	void *arg,
	pid_t *pidp) = NULL;

struct pip_gdbif_root	*pip_gdbif_root;

int pip_root_p_( void ) {
  return pip_root != NULL && pip_task != NULL &&
    pip_root->task_root == pip_task;
}

static void 
pip_set_name( char *symbol, char *progname ) {
#ifdef PR_SET_NAME
  /* the following code is to set the right */
  /* name shown by the ps and top commands  */
  char nam[16];

  if( progname == NULL ) {
    char prg[16];
    prctl( PR_GET_NAME, prg, 0, 0, 0 );
    snprintf( nam, 16, "%s%s", symbol, prg );
  } else {
    char *p;
    if( ( p = strrchr( progname, '/' ) ) != NULL) {
      progname = p + 1;
    }
    snprintf( nam, 16, "%s%s", symbol, progname );
  }
  if( symbol[1] != '|' ) {
    (void) prctl( PR_SET_NAME, nam, 0, 0, 0 );
  } else {
    (void) pthread_setname_np( pthread_self(), nam );
  }
#define FMT "/proc/self/task/%u/comm"
  char fname[sizeof(FMT)+8];
  int fd;
  sprintf( fname, FMT, (unsigned int) pip_gettid() );
  if( ( fd = open( fname, O_RDWR ) ) >= 0 ) {
    (void) write( fd, nam, strlen(nam) );
    (void) close( fd );
  }
#endif
}

static char pip_cmd_name_symbol( int opts ) {
  char sym;

  switch( opts & PIP_MODE_MASK ) {
  case PIP_MODE_PROCESS_PRELOAD:
    sym = ':';
    break;
  case PIP_MODE_PROCESS_PIPCLONE:
    sym = ';';
    break;
  case PIP_MODE_PROCESS_GOT:
    sym = '.';
    break;
  case PIP_MODE_PTHREAD:
    sym = '|';
    break;
  default:
    sym = '?';
    break;
  }
  return sym;
}

static int pip_count_vec( char **vecsrc ) {
  int n;
  for( n=0; vecsrc[n]!= NULL; n++ );
  return( n );
}

static void pip_set_magic( pip_root_t *root ) {
  memcpy( root->magic, PIP_MAGIC_WORD, PIP_MAGIC_LEN );
}

void pip_reset_task_struct( pip_task_t *task ) {
  void	*namexp = task->named_exptab;
  if( task->cpuset != NULL ) free( task->cpuset );
  memset( (void*) task, 0, sizeof(pip_task_t) );
  task->pipid = PIP_PIPID_NULL;
  task->type  = PIP_TYPE_NULL;
  task->named_exptab = namexp;
}

#include <elf.h>

int pip_check_pie( const char *path, int flag_verbose ) {
  struct stat stbuf;
  Elf64_Ehdr elfh;
  int fd;
  int err = 0;

  if( strchr( path, '/' ) == NULL ) {
    if( flag_verbose ) {
      pip_err_mesg( "'%s' is not a path (no slash '/')", path );
    }
    err = ENOENT;
  } else if( ( fd = open( path, O_RDONLY ) ) < 0 ) {
    err = errno;
    if( flag_verbose ) {
      pip_err_mesg( "'%s': open() fails (%s)", path, strerror( errno ) );
    }
  } else {
    if( fstat( fd, &stbuf ) < 0 ) {
      err = errno;
      if( flag_verbose ) {
	pip_err_mesg( "'%s': stat() fails (%s)", path, strerror( errno ) );
      }
    } else if( ( stbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH) ) == 0 ) {
      if( flag_verbose ) {
	pip_err_mesg( "'%s' is not executable", path );
      }
      err = EACCES;
    } else if( read( fd, &elfh, sizeof( elfh ) ) != sizeof( elfh ) ) {
      if( flag_verbose ) {
	pip_err_mesg( "Unable to read '%s'", path );
      }
      err = EUNATCH;
    } else if( elfh.e_ident[EI_MAG0] != ELFMAG0 ||
	       elfh.e_ident[EI_MAG1] != ELFMAG1 ||
	       elfh.e_ident[EI_MAG2] != ELFMAG2 ||
	       elfh.e_ident[EI_MAG3] != ELFMAG3 ) {
      if( flag_verbose ) {
	pip_err_mesg( "'%s' is not ELF", path );
      }
      err = ELIBBAD;
    } else if( elfh.e_type != ET_DYN ) {
      if( flag_verbose ) {
	pip_err_mesg( "'%s' is not PIE", path );
      }
      err = ELIBEXEC;
    }
    (void) close( fd );
  }
  return err;
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

typedef void(*pip_set_clone_t)(void);

static int pip_check_opt_and_env( int *optsp ) {
  extern pip_spinlock_t pip_lock_got_clone;
  pip_set_clone_t	set_clone;
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
    pip_cloneinfo = (pip_clone_t*) pip_dlsym( RTLD_DEFAULT, "pip_clone_info" );
  }
  set_clone = (pip_set_clone_t) pip_dlsym( RTLD_DEFAULT, "pip_set_clone" );
  DBGF( "cloneinfo:%p", pip_cloneinfo );

  if( pip_cloneinfo != NULL ) {
    DBGF( "mode:0x%x", mode );
    if( mode == 0 || 
	( mode & PIP_MODE_PROCESS_PRELOAD ) == mode ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      set_clone();
      pip_clone_lock = &pip_cloneinfo->lock;
      goto done;
    }
    if( mode & ~PIP_MODE_PROCESS_PRELOAD ) {
      pip_err_mesg( "pip_preload.so is already loaded by LD_PRELOAD and "
		    "process:preload is the only possible choice of PiP mode" );
      RETURN( EPERM );
    }
    if( env == NULL || env[0] == '\0' ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      set_clone();
      pip_clone_lock = &pip_cloneinfo->lock;
      goto done;
    }
    if( strcasecmp( env, PIP_ENV_MODE_PROCESS_PRELOAD ) == 0 ||
	strcasecmp( env, PIP_ENV_MODE_PROCESS         ) == 0 ) {
      newmod = PIP_MODE_PROCESS_PRELOAD;
      set_clone();
      pip_clone_lock = &pip_cloneinfo->lock;
      goto done;
    } else {
      pip_err_mesg( "pip_preload.so is already loaded by LD_PRELOAD and "
		    "process:preload is the only possible choice of PiP mode" );
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
  if( desired == 0 ) {
    RETURN( EPERM );
  }

  if( desired & PIP_MODE_PROCESS_GOT_BIT ) {
    int pip_wrap_clone( void );
    if( pip_wrap_clone() == 0 ) {
      newmod = PIP_MODE_PROCESS_GOT;
      pip_clone_lock = &pip_lock_got_clone;
      goto done;
    } else if( !( desired & ( PIP_MODE_PTHREAD_BIT |
			      PIP_MODE_PROCESS_PIPCLONE_BIT ) ) ) {
      RETURN( EPERM );
    }
  }
  if( desired & PIP_MODE_PROCESS_PIPCLONE_BIT ) {
    if ( pip_clone_mostly_pthread_ptr == NULL )
      pip_clone_mostly_pthread_ptr =
	pip_dlsym( RTLD_DEFAULT, "pip_clone_mostly_pthread" );
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

/* signal handlers */

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

static void pip_sigchld_handler( int sig, siginfo_t *info, void *extra ) {}

void pip_set_sigmask( int sig ) {
  sigset_t sigmask;

  ASSERT( sigemptyset( &sigmask )                                    == 0 );
  ASSERT( sigaddset(   &sigmask, sig )                               == 0 );
  ASSERT( sigprocmask( SIG_BLOCK, &sigmask, &pip_root->old_sigmask ) == 0 );
}

void pip_unset_sigmask( void ) {
  ASSERT( sigprocmask( SIG_SETMASK, &pip_root->old_sigmask, NULL ) == 0 );
}

/* save PiP environments */

static void pip_save_debug_envs( pip_root_t *root ) {
  char *env;

  if( ( env = getenv( PIP_ENV_STOP_ON_START ) ) != NULL && *env != '\0' ) {
    root->envs.stop_on_start = pip_strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_PATH      ) ) != NULL && *env != '\0' &&
      access( env, X_OK ) == 0 ) {
    root->envs.gdb_path      = pip_strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_COMMAND   ) ) != NULL && *env != '\0' &&
      access( env, R_OK ) == 0 ) {
    root->envs.gdb_command   = pip_strdup( env );
  }
  if( ( env = getenv( PIP_ENV_GDB_SIGNALS   ) ) != NULL && *env != '\0' ) {
    root->envs.gdb_signals   = pip_strdup( env );
  }
  if( ( env = getenv( PIP_ENV_SHOW_MAPS     ) ) != NULL && *env != '\0' ) {
    root->envs.show_maps     = pip_strdup( env );
  }
  if( ( env = getenv( PIP_ENV_SHOW_PIPS     ) ) != NULL && *env != '\0' ) {
    root->envs.show_pips     = pip_strdup( env );
  }
}

char *pip_prefix_dir( void ) {
  FILE	 *fp_maps = NULL;
  size_t  sz = 0;
  ssize_t l;
  char	 *line = NULL;
  char	 *libpip, *prefix, *p;
  char	 *updir = "/..";
  void	 *sta, *end;
  void	 *faddr = (void*) pip_prefix_dir;

  ASSERT( ( fp_maps = fopen( PIP_MAPS_PATH, "r" ) ) != NULL );
  //DBGF( "faddr:%p", faddr );
  while( ( l = getline( &line, &sz, fp_maps ) ) > 0 ) {
    line[l] = '\0';
    //DBGF( "l:%d sz:%d line(%p)\n\t%s", (int)l, (int)sz, line, line );
    prefix = NULL;
    int n = sscanf( line, "%p-%p %*4s %*x %*x:%*x %*d %ms", &sta, &end, &libpip );
    //DBGF( "%d: %p-%p %p %s", n, sta, end, faddr, prefix );
    if( n == 3             && 
	libpip    != NULL  && 
	libpip[0] != '\0'  &&
	sta       <= faddr && 
	faddr     <  end ) {
      ASSERT( ( p = rindex( libpip, '/' ) ) != NULL );
      *p = '\0';
      ASSERT( ( prefix = (char*) pip_malloc( strlen( libpip ) + 
					     strlen( updir ) + 1 ) )
	      != NULL );
      p = prefix;
      p = stpcpy( p, libpip );
      p = stpcpy( p, updir  );
      ASSERT( ( p = realpath( prefix, NULL ) ) != NULL );
      free( libpip );
      free( prefix );
      fclose( fp_maps );
      prefix = p;
      DBGF( "prefix: %s", prefix );
      return prefix;
    }
    free( libpip );
  }
  NEVER_REACH_HERE;
  return NULL;			/* dummy */
}

int pip_init( int *pipidp, int *ntasksp, void **rt_expp, int opts ) {
  void pip_named_export_init( pip_task_t* );
  pip_root_t	*root;
  size_t	sz;
  int		ntasks;
  int 		pipid;
  int 		i, err = 0;

  ENTERF( "pip_root: %p @ %p  piptask : %p @ %p", 
	  pip_root, &pip_root, pip_task, &pip_task );
  if( pip_root != NULL && 
      pip_task != NULL &&
      pip_task == pip_root->task_root ) {
    RETURN( EBUSY );

  } else if( pip_root == NULL && pip_task == NULL ) {
    DBGF( "ROOT ROOT ROOT" );
    /* root process */
    if( ntasksp == NULL ) {
      ntasks = PIP_NTASKS_MAX;
    } else {
      ntasks = *ntasksp;
    }
    if( ntasks <= 0             ) RETURN( EINVAL );
    if( ntasks > PIP_NTASKS_MAX ) RETURN( EOVERFLOW );

    if( ( err = pip_check_opt_and_env( &opts ) ) != 0 ) RETURN( err );
    sz = sizeof( pip_root_t ) + sizeof( pip_task_t ) * ( ntasks + 1 );
    pip_page_alloc( sz, (void**) &root );
    (void) memset( root, 0, sz );
    root->size_whole = sz;
    root->size_root  = sizeof( pip_root_t );
    root->size_task  = sizeof( pip_task_t );

    pip_spin_init( &root->lock_tasks   );

    root->prefixdir = pip_prefix_dir();
    pip_set_magic( root );
    root->version      = PIP_API_VERSION;
    root->ntasks       = ntasks;
    root->ntasks_count = 1; /* root is also a PiP task */
    root->cloneinfo    = pip_cloneinfo;
    root->opts         = opts;
    root->page_size    = sysconf( _SC_PAGESIZE );
    root->task_root    = &root->tasks[ntasks];
    pip_recursive_lock_init( &root->glibc_lock );
    pip_sem_init( &root->lock_clone );
    pip_sem_post( &root->lock_clone );
    pip_sem_init( &root->sync_spawn );
    pip_sem_init( &root->universal_lock );
    pip_sem_post( &root->universal_lock );
    for( i=0; i<ntasks+1; i++ ) {
      pip_reset_task_struct( &root->tasks[i] );
      pip_named_export_init( &root->tasks[i] );
    }

    if( rt_expp != NULL ) {
      root->export_root = *rt_expp;
    }
    pipid = PIP_PIPID_ROOT;
    root->task_root->pipid  = pipid;
    root->task_root->type   = PIP_TYPE_ROOT;
    root->task_root->loaded = pip_dlopen_unsafe( NULL, RTLD_NOW );
    root->task_root->thread = pthread_self();
    root->task_root->pid    = getpid();
    root->task_root->tid    = pip_gettid();
    pip_task = root->task_root;
    pip_root = root;
    {
      char sym[] = "R*";
      sym[1] = pip_cmd_name_symbol( root->opts );
      pip_set_name( sym, NULL );
    }
    pip_set_sigmask( SIGCHLD );
    pip_set_signal_handler( SIGCHLD,
			    pip_sigchld_handler,
			    &pip_root->old_sigchld );
    pip_gdbif_initialize_root( ntasks );
    pip_gdbif_task_commit( pip_task );
    if( !pip_is_threaded_() ) {
      pip_save_debug_envs( root );
      pip_debug_on_exceptions( root, pip_task );
    }
    DBGF( "PiP Execution Mode: %s", pip_get_mode_str() );

  } else {
    if( ( err = pip_check_root_and_task( pip_root, pip_task ) ) != 0 ) {
      RETURN( err );
    } else {
      DBGF( "TASK TASK TASK" );
      /* child task */
      DBGF( "pip_root: %p @ %p  piptask : %p @ %p", 
	    pip_root, &pip_root, pip_task, &pip_task );
      if( ntasksp != NULL ) *ntasksp = pip_root->ntasks;
      if( rt_expp != NULL ) *rt_expp = pip_task->import_root;
    }
  }
  /* root and child */
  if( pipidp != NULL ) *pipidp = pip_task->pipid;
  DBGF( "pip_root: %p @ %p  piptask : %p @ %p", 
	pip_root, &pip_root, pip_task, &pip_task );
  RETURN( err );
}

static int pip_is_shared_fd_( void ) {
  return pip_is_threaded_();
}

int pip_is_shared_fd( int *flagp ) {
  if( pip_root == NULL ) RETURN( EPERM  );
  if( flagp != NULL ) *flagp = pip_is_shared_fd_();
  RETURN( 0 );
}

int pip_isa_piptask( void ) {
  return( pip_task != NULL && pip_task->tid == pip_gettid() );
}

pip_task_t *pip_get_task_( int pipid ) {
  pip_task_t 	*task = NULL;

  switch( pipid ) {
  case PIP_PIPID_ROOT:
    task = pip_root->task_root;
    break;
  default:
    if( pipid >= 0 && pipid < pip_root->ntasks ) {
      task = &pip_root->tasks[pipid];
    }
    break;
  }
  return task;
}

int pip_get_dlmopen_info( int pipid, void **handle, long *lmidp ) {
  pip_task_t *task;
  int err;

  if( !pip_is_initialized() ) RETURN( EPERM );
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );
  task = pip_get_task_( pipid );
  if( handle != NULL ) *handle = task->loaded;
  if( lmidp  != NULL ) *lmidp  = task->lmid;
  RETURN( 0 );
}

int pip_get_pipid_( void ) {
  int pipid;
  if( pip_root == NULL ) {
    pipid = PIP_PIPID_ANY;
  } else if( pip_root_p_() ) {
    pipid = PIP_PIPID_ROOT;
  } else {
    pipid = pip_task->pipid;
  }
  DBGF( "pipid:%d", pipid );
  return pipid;
}

int pip_get_pipid( int *pipidp ) {
  if( !pip_is_initialized() ) RETURN( EPERM );
  if( pipidp != NULL ) *pipidp = pip_get_pipid_();
  RETURN( 0 );
}

int pip_get_ntasks( int *ntasksp ) {
  if( pip_root == NULL ) return( EPERM  ); /* intentionally using small return */
  if( ntasksp  != NULL ) *ntasksp = pip_root->ntasks;
  RETURN( 0 );
}

static pip_task_t *pip_get_myself( void ) {
  pip_task_t *task;

  if( pip_root_p_() ) {
    task = pip_root->task_root;
  } else {
    task = pip_task;
  }
  return task;
}

int pip_export( void *export ) {
  pip_task_t *task;

  if( !pip_is_initialized() ) RETURN( EPERM );
  if( export == NULL ) RETURN( EINVAL );
  task = pip_get_myself();
  if( task->export != NULL ) RETURN( EBUSY );
  task->export = export;
  RETURN( 0 );
}

int pip_import( int pipid, void **exportp ) {
  pip_task_t *task;
  int err;

  if( ( err = pip_check_pipid( &pipid ) ) == 0 ) {
    task = pip_get_task_( pipid );
    if( exportp != NULL ) *exportp = (void*) task->export;
    pip_memory_barrier();
  }
  RETURN( 0 );
}

#define PIP_GET_ADDR
#ifdef PIP_GET_ADDR
int pip_get_addr( int pipid, const char *name, void **addrp ) {
  void *handle, *addr;
  int err;

  if( name == NULL ) return EINVAL;
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) return err;
  if( pipid == PIP_PIPID_ROOT ) {;
    addr = pip_dlsym( pip_root->task_root->loaded, name );
  } else if( pipid == PIP_PIPID_MYSELF ) {
    addr = pip_dlsym( pip_task->loaded, name );
  } else if( ( handle = pip_root->tasks[pipid].loaded ) != NULL ) {
    addr = pip_dlsym( handle, name );
  } else {
    err = ESRCH;		/* tentative */
  }
  if( err == 0 && addrp != NULL ) *addrp = addr;
  return err;
}
#endif

static int pip_copy_vec( char **vecadd,
			 char **vecsrc,		   /* input */
			 pip_char_vec_t *cvecp ) { /* output */
  char 		**vecdst, *p, *strs;
  size_t	veccc, sz;
  int 		vecln, i, j;

  vecln = 0;
  veccc = 0;
  if( vecadd != NULL ) {
    for( i=0; vecadd[i]!=NULL; i++ ) {
      vecln ++;
      veccc += strlen( vecadd[i] ) + 1;
    }
  }
  for( i=0; vecsrc[i]!=NULL; i++ ) {
    vecln ++;
    veccc += strlen( vecsrc[i] ) + 1;
  }
  vecln ++;		/* plus final NULL */

  sz = sizeof(char*) * vecln;
  if( ( vecdst = (char**) pip_malloc( sz    ) ) == NULL ) {
    return ENOMEM;
  }
  if( ( strs   = (char*)  pip_malloc( veccc ) ) == NULL ) {
    free( vecdst );
    free( strs   );
    return ENOMEM;
  }
  p = strs;
  j = 0;
  if( vecadd != NULL ) {
    for( i=0; vecadd[i]!=NULL; i++ ) {
      vecdst[j++] = p;
      p = stpcpy( p, vecadd[i] ) + 1;
    }
  }
  for( i=0; vecsrc[i]!=NULL; i++ ) {
    vecdst[j++] = p;
    p = stpcpy( p, vecsrc[i] ) + 1;
  }
  vecdst[j] = NULL;
  cvecp->vec  = vecdst;
  cvecp->strs = strs;
  return 0;
}

static int pip_copy_env( char **envsrc, int pipid, pip_char_vec_t *vecp ) {
  char *addenv[] = { NULL };
  return pip_copy_vec( addenv, envsrc, vecp );
}

size_t pip_stack_size( void ) {
  char 		*env, *endptr;
  ssize_t 	s, sz, scale, smax;
  struct rlimit rlimit;

  if( ( sz = pip_root->stack_size ) == 0 ) {
    if( ( env = getenv( PIP_ENV_STACKSZ ) ) == NULL &&
	( env = getenv( "KMP_STACKSIZE" ) ) == NULL &&
	( env = getenv( "OMP_STACKSIZE" ) ) == NULL ) {
      sz = PIP_STACK_SIZE;	/* default */
    } else {
      if( ( sz = (ssize_t) strtoll( env, &endptr, 10 ) ) <= 0 ) {
	pip_err_mesg( "stacksize: '%s' is illegal and "
		      "default size (%lu KiB) is set",
		      env,
		      PIP_STACK_SIZE / 1024 );
	sz = PIP_STACK_SIZE;	/* default */
      } else {
	if( getrlimit( RLIMIT_STACK, &rlimit ) != 0 ) {
	  smax = PIP_STACK_SIZE_MAX;
	} else {
	  smax = rlimit.rlim_cur;
	}
	scale = 1;
	switch( *endptr ) {
	case 'T': case 't':
	  scale *= 1024;
	  /* fall through */
	case 'G': case 'g':
	  scale *= 1024;
	  /* fall through */
	case 'M': case 'm':
	  scale *= 1024;
	  /* fall through */
	case 'K': case 'k':
	case '\0':		/* default is KiB */
	  scale *= 1024;
	  sz *= scale;
	  break;
	case 'B': case 'b':
	  for( s=PIP_STACK_SIZE_MIN; s<sz && s<smax; s*=2 );
	  break;
	default:
	  sz = PIP_STACK_SIZE;
	  pip_err_mesg( "stacksize: '%s' is illegal and "
			"default size (%ldB) is used instead",
			env, sz );
	  break;
	}
	sz = ( sz < PIP_STACK_SIZE_MIN ) ? PIP_STACK_SIZE_MIN : sz;
	sz = ( sz > smax               ) ? smax               : sz;
      }
    }
    pip_root->stack_size = sz;
  }
  return sz;
}

int pip_is_coefd( int fd ) {
  int flags = fcntl( fd, F_GETFD );
  return( flags > 0 && ( flags & FD_CLOEXEC ) );
}

static void pip_close_on_exec( void ) {
  DIR *dir;
  struct dirent dirent, *direntp;
  int fd;

#define PROCFD_PATH		"/proc/self/fd"
  if( ( dir = opendir( PROCFD_PATH ) ) != NULL ) {
    int fd_dir = dirfd( dir );
    while( readdir_r( dir, &dirent, &direntp ) == 0 &&
	   direntp != NULL ) {
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

static int pip_find_user_symbols( pip_spawn_program_t *progp,
				  void *handle,
				  pip_task_t *task ) {
  pip_symbols_t *symp = &task->symbols;
  int err = 0;

  if( progp->funcname == NULL ) {
    symp->main  = pip_dlsym( handle, "main"          );
  } else {
    symp->start = pip_dlsym( handle, progp->funcname );
  }

  DBGF( "func(%s):%p  main:%p", progp->funcname, symp->start, symp->main );

  /* check start function */
  if( progp->funcname == NULL ) {
    if( symp->main == NULL ) {
      pip_err_mesg( "Unable to find main "
		    "(possibly not linked with '-rdynamic' option)" );
      err = ENOEXEC;
    }
  } else if( symp->start == NULL ) {
    pip_err_mesg( "Unable to find start function (%s)",
		  progp->funcname );
    err = ENOEXEC;
  }
  RETURN( err );
}

static int
pip_find_glibc_symbols( void *handle, pip_task_t *task ) {
  pip_symbols_t *symp = &task->symbols;
  int err = 0;

  pip_glibc_lock();		/* to protect dlsym */
  {
    /* the GLIBC _init() seems not callable. It seems that */
    /* dlmopen()ed name space does not setup VDSO properly */
    symp->ctype_init     = pip_dlsym_unsafe( handle, "__ctype_init"    );
    symp->libc_fflush    = pip_dlsym_unsafe( handle, "fflush"          );
    symp->exit	         = pip_dlsym_unsafe( handle, "exit"            );
    symp->pthread_exit   = pip_dlsym_unsafe( handle, "pthread_exit"    );
    /* GLIBC variables */
    symp->libc_argcp     = pip_dlsym_unsafe( handle, "__libc_argc"     );
    symp->libc_argvp     = pip_dlsym_unsafe( handle, "__libc_argv"     );
    symp->environ        = pip_dlsym_unsafe( handle, "environ"         );
    /* GLIBC misc. variables */
    symp->prog           = pip_dlsym_unsafe( handle, "__progname"      );
    symp->prog_full      = pip_dlsym_unsafe( handle, "__progname_full" );
  }
  pip_glibc_unlock();
  RETURN( err );
}

#ifdef RTLD_DEEPBIND
#define DLMOPEN_FLAGS	  (RTLD_NOW | RTLD_DEEPBIND)
#else
#define DLMOPEN_FLAGS	  (RTLD_NOW)
#endif

static int
pip_load_dsos( pip_spawn_program_t *progp, pip_task_t *task) {
  const char 	*path = progp->prog;
  Lmid_t	lmid;
  pip_init_t	impinit     = NULL;
  void 		*loaded     = NULL;
  int		err = 0;

  ENTERF( "path:%s", path );

  loaded = pip_dlmopen( LM_ID_NEWLM, path, DLMOPEN_FLAGS );
  if( loaded == NULL ) {
    if( ( err = pip_check_pie( path, 1 ) ) != 0 ) goto error;
    pip_err_mesg( "dlmopen(%s): %s", path, pip_dlerror() );
    err = ENOEXEC;
    goto error;
  }

  if( ( err = pip_find_glibc_symbols( loaded, task       ) ) ) goto error;
  if( ( err = pip_find_user_symbols( progp, loaded, task ) ) ) goto error;
  if( pip_dlinfo( loaded, RTLD_DI_LMID, &lmid ) != 0 ) {
    pip_err_mesg( "Unable to obtain Lmid - %s", pip_dlerror() );
    err = ELIBSCN;
    goto error;
  }
  DBGF( "lmid:%d", (int) lmid );

  impinit = (pip_init_t) pip_dlsym( loaded, "pip_init_task_implicitly" );
  if( impinit == NULL ) {
    DBGF( "dlsym: %s", pip_dlerror() );
    err = ELIBBAD;
    goto error;
  }
  task->lmid             = lmid;
  task->loaded           = loaded;
  task->symbols.pip_init = impinit;
  RETURN( err );

 error:
  if( loaded != NULL ) pip_dlclose( loaded     );
  RETURN( err );
}

static int pip_load_prog( pip_spawn_program_t *progp,
			  pip_spawn_args_t *args,
			  pip_task_t *task ) {
  int 	err;

  ENTERF( "prog=%s", progp->prog );

  PIP_ACCUM( time_load_dso,
	     ( err = pip_load_dsos( progp, task ) ) == 0 );
  if( !err ) pip_gdbif_load( task );
  RETURN( err );
}

static int pip_redo_corebind( pip_task_t *task ) {
  if( sched_setaffinity( task->tid, sizeof(cpu_set_t), task->cpuset ) != 0 ) {
    RETURN( errno );
  }
  RETURN( 0 );
}

static int pip_do_corebind( pip_task_t *task, uint32_t coreno, cpu_set_t *oldsetp ) {
  cpu_set_t cpuset;
  pid_t tid;
  int flags  = coreno & PIP_CPUCORE_FLAG_MASK;
  int i, err = 0;

  ENTER;
  /* PIP_CPUCORE_* flags are exclusive */
  if( ( flags & PIP_CPUCORE_ASIS ) != flags &&
      ( flags & PIP_CPUCORE_ABS  ) != flags ) RETURN( EINVAL );
  if( !( flags & PIP_CPUCORE_ASIS ) ) {
    coreno &= PIP_CPUCORE_CORENO_MASK;
    if( coreno >= PIP_CPUCORE_CORENO_MAX ) RETURN ( EINVAL );
  }

  task->cpuset = (cpu_set_t*) malloc( sizeof(cpu_set_t) );
  ASSERTD( task->cpuset != NULL );

  tid = pip_gettid();
  if( sched_getaffinity( tid, sizeof(cpu_set_t), oldsetp ) != 0 ) {
    err = errno;
    goto error;
  }

  if( flags & PIP_CPUCORE_ASIS ) {
    memcpy( task->cpuset, oldsetp, sizeof(cpu_set_t) );
    RETURN( 0 );
  }
  /* the coreno might be absolute or not. this is beacuse */
  /* the Fujist A64FX CPU has non-contiguoes cpu numbers  */
  if( flags == 0 ) {		/* not absolute */
    int ncores, nth;

    if( sched_getaffinity( tid, sizeof(cpu_set_t), &cpuset ) != 0 ) {
      err = errno;
      goto error;
    }
    if( ( ncores = CPU_COUNT( &cpuset ) ) ==  0 ) RETURN( 0 );
    coreno %= ncores;
    nth = coreno;
    for( i=0; ; i++ ) {
      if( !CPU_ISSET( i, &cpuset ) ) continue;
      if( nth-- == 0 ) {
	CPU_ZERO( &cpuset );
	CPU_SET( i, &cpuset );
	if( sched_setaffinity( tid, sizeof(cpu_set_t), &cpuset ) != 0 ) {
	  err = errno;
	}
	break;
      }
    }
  } else {
    CPU_ZERO( &cpuset );
    CPU_SET( coreno, &cpuset );
    /* here, do not call pthread_setaffinity(). This MAY fail */
    /* because pd->tid is NOT set yet.  I do not know why.    */
    /* But this is OK to call sched_setaffinity() with tid.   */
    if( sched_setaffinity( tid, sizeof(cpuset), &cpuset ) != 0 ) {
      err = errno;
    }
  }
  if( err ) {
  error:
    free( task->cpuset );
    task->cpuset = NULL;
  } else {
    memcpy( task->cpuset, &cpuset, sizeof(cpu_set_t) );
  }
  RETURN( err );
}

static int pip_undo_corebind( cpu_set_t *oldsetp ) {
  pid_t tid;
  int err = 0;

  ENTER;
  /* here, do not call pthread_setaffinity().  See above comment. */
  tid = pip_gettid();
  if( sched_setaffinity( tid, sizeof(cpu_set_t), oldsetp ) != 0 ) {
    err = errno;
  }
  RETURN( err );
}

static void pip_glibc_init( pip_symbols_t *symbols,
			    pip_spawn_args_t *args ) {
  /* setting GLIBC variables */
  if( symbols->libc_argcp != NULL ) {
    DBGF( "&__libc_argc=%p", symbols->libc_argcp );
    *symbols->libc_argcp = args->argc;
  }
  if( symbols->libc_argvp != NULL ) {
    DBGF( "&__libc_argv=%p", symbols->libc_argvp );
    *symbols->libc_argvp = args->argvec.vec;
  }
  if( symbols->prog != NULL ) {
    *symbols->prog = args->prog;
  }
  if( symbols->prog_full != NULL ) {
    *symbols->prog_full = args->prog_full;
  }
  if( symbols->ctype_init != NULL ) {
    DBGF( ">> __ctype_init@%p", symbols->ctype_init );
    symbols->ctype_init();
    DBGF( "<< __ctype_init@%p", symbols->ctype_init );
  }
}

static void pip_glibc_fin( pip_symbols_t *symbols ) {
  /* call fflush() in the target context to flush out messages */
  if( symbols->libc_fflush != NULL ) {
    DBGF( ">> fflush@%p()", symbols->libc_fflush );
    symbols->libc_fflush( NULL );
    DBGF( "<< fflush@%p()", symbols->libc_fflush );
  }
}

static void pip_return_from_start_func( pip_task_t *task, int extval ) {
  int err = 0;

  ENTER;
  if( pip_gettid() == task->tid ) {
    if( task->hook_after != NULL ) {
      err = task->hook_after( task->hook_arg );
      if( err ) {
	pip_err_mesg( "PIPID:%d after-hook returns %d", task->pipid, err );
      }
      if( extval == 0 ) extval = err;
    }
    pip_glibc_fin( &task->symbols );
    if( task->symbols.named_export_fin != NULL ) {
      task->symbols.named_export_fin( task );
    }
    pip_gdbif_exit( task, WEXITSTATUS(extval) );
    pip_gdbif_hook_after( task );
  } else {
    /* when a PiP task fork()s and returns */
    /* from main this case happens         */
    DBGF( "return from a fork()ed process?" );
    goto call_exit;
  }
  DBGF( "PIPID:%d -- FORCE EXIT:%d", task->pipid, extval );
  /* here we have to call the exit() in the same context, if possible */
  if( pip_is_threaded_() ) {	/* thread mode */
    task->flag_sigchld = 1;
    (void) pip_raise_signal( pip_root->task_root, SIGCHLD );

    if( task->symbols.pthread_exit != NULL ) {
      task->symbols.pthread_exit( NULL );
    } else {
      pthread_exit( NULL );
    }
  } else {			/* process mode */
  call_exit:
    if( task->symbols.exit != NULL ) {
      task->symbols.exit( extval );
    } else {
      exit( extval );
    }
  }
  NEVER_REACH_HERE;
}

static void pip_sigquit_handler( int, void(*)(), struct sigaction* )
  __attribute__((noreturn));
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
    ASSERT( sigaction( sig, &sigact, NULL ) == 0 );
  } else {
    sigset_t sigmask;
    (void) sigemptyset( &sigmask );
    (void) sigaddset( &sigmask, sig );
    ASSERT( pthread_sigmask( SIG_BLOCK, &sigmask, NULL ) == 0 );
  }
}

static char *pip_onstart_script( char *env, size_t len ) {
  struct stat stbuf;
  char *script;

  ENTERF( "env:%s", env );
  script = pip_strndup( env, len );
  if( stat( script, &stbuf ) != 0 ) {
    pip_warn_mesg( "Unable to find file: %s (%s=%s)", 
		   script, PIP_ENV_STOP_ON_START, env );
    free( script );
    script = NULL;
  } else if( ( stbuf.st_mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) == 0 ) {
    pip_warn_mesg( "Not an executable file: %s (%s=%s)", 
		   script, PIP_ENV_STOP_ON_START, env );
    free( script );
    script = NULL;
  }
  DBGF( "script:%s", script );
  return script;
}

static char *pip_onstart_target( pip_task_t *task, char *env ) {
  char *p, *q;
  int pipid;

  p = index( env, '@' );
  if( p == NULL ) {
    return pip_onstart_script( env, strlen( env ) );
  } else if( env[0] == '@' ) {		/* no script */
    p = &env[1];
    if( *p == '\0' ) return pip_strdup( "" );
    pipid = strtol( p, NULL, 10 );
    if( pipid < 0 || pipid == task->pipid ) {
      return pip_strdup( "" );
    }
  } else {
    q = p ++;			/* skip '@' */
    if( *p == '\0' ) {		/* no target */
      return pip_onstart_script( env, q - env );
    }
    pipid = strtol( p, NULL, 10 );
    if( pipid < 0 || pipid == task->pipid ) {
      return pip_onstart_script( env, q - env );
    }
  }
  return NULL;
}

static void *pip_do_spawn( void *thargs )  {
  pip_spawn_args_t *args = (pip_spawn_args_t*) thargs;
  int 	pipid      = args->pipid;
  char **argv      = args->argvec.vec;
  char **envv      = args->envvec.vec;
  void *start_arg  = args->start_arg;
  int coreno       = args->coreno;
  pip_task_t *self = &pip_root->tasks[pipid];
  pip_spawnhook_t before = self->hook_before;
  void *hook_arg         = self->hook_arg;
  int 	extval = 0, err = 0;

  ENTER;
  self->thread = pthread_self();
  self->pid    = getpid();
  self->tid    = pip_gettid();

  pip_gdbif_task_commit( self );
  pip_gdbif_hook_before( self );

  pip_sem_post( &pip_root->sync_spawn );

  if( ( err = pip_redo_corebind( self ) ) != 0 ) {
    pip_warn_mesg( "failed to bound CPU core:%d (%d)", coreno, err );
    err = 0;
  }
  {
    char sym[] = "0*";
    sym[0] += ( pipid % 10 );
    sym[1] = pip_cmd_name_symbol( pip_root->opts );
    pip_set_name( sym, args->prog );
  }
  if( !pip_is_threaded_() ) {
    pip_reset_signal_handler( SIGCHLD );
    (void) setpgid( 0, (pid_t) pip_root->task_root->tid );
  } else {
    pip_set_signal_handler( SIGQUIT, pip_sigquit_handler, NULL );
  }
#ifdef DEBUG
  if( pip_is_threaded_() ) {
    pthread_attr_t attr;
    size_t sz;
    int _err;
    if( ( _err = pthread_getattr_np( self->thread, &attr      ) ) != 0 ) {
      DBGF( "pthread_getattr_np()=%d", _err );
    } else if( ( _err = pthread_attr_getstacksize( &attr, &sz ) ) != 0 ) {
      DBGF( "pthread_attr_getstacksize()=%d", _err );
    } else {
      DBGF( "stacksize = %ld [KiB]", sz/1024 );
    }
  }
#endif
  if( !pip_is_shared_fd_() ) pip_close_on_exec();

  if( self->onstart_script != NULL ) {
    /* PIP_STOP_ON_START (process mode only) */
    DBGF( "ONSTART: %s", self->onstart_script );
    ASSERT( pip_raise_signal( self, SIGSTOP ) == 0 );
  }

  pip_glibc_init( &self->symbols, args );
  DBGF( "pip_init:%p", self->symbols.pip_init );
  if( ( err = self->symbols.pip_init( pip_root, self, envv ) ) != 0 ) {
    extval = err;
  } else {
    /* calling the before hook, if any */
    if( before != NULL ) {
      if( ( err = before( hook_arg ) ) ) {
	pip_warn_mesg( "try to spawn(%s), but the before hook at %p returns %d",
		       argv[0], before, err );
	extval = err;
      }
    }
    if( !err ) {
      if( self->symbols.start == NULL ) {
	/* calling hook function, if any */
	DBGF( "[%d] >> main@%p(%d,%s,%s,...)",
	      args->pipid, self->symbols.main, args->argc, argv[0], argv[1] );
	extval = self->symbols.main( args->argc, argv, *self->symbols.environ );
	DBGF( "[%d] << main@%p(%d,%s,%s,...) = %d",
	      args->pipid, self->symbols.main, args->argc, argv[0], argv[1],
	      extval );
      } else {
	DBGF( "[%d] >> %s:%p(%p)",
	      args->pipid, args->funcname,
	      self->symbols.start, start_arg );
	extval = self->symbols.start( start_arg );
	DBGF( "[%d] << %s:%p(%p) = %d",
	      args->pipid, args->funcname,
	      self->symbols.start, start_arg, extval );
      }
    }
  }
  pip_return_from_start_func( self, extval );
  NEVER_REACH_HERE;
  return NULL;			/* dummy */
}

static int pip_find_a_free_task( int *pipidp ) {
  int pipid = *pipidp;
  int err = 0;

  if( pip_root->ntasks_accum >= PIP_NTASKS_MAX ) RETURN( EOVERFLOW );
  if( pipid < PIP_PIPID_ANY || pipid >= pip_root->ntasks ) {
    DBGF( "pipid=%d", pipid );
    RETURN( EINVAL );
  }

  pip_spin_lock( &pip_root->lock_tasks );
  /*** begin lock region ***/
  do {
    if( pipid != PIP_PIPID_ANY ) {
      if( pip_root->tasks[pipid].pipid != PIP_PIPID_NULL ) {
	err = EAGAIN;
	goto unlock;
      }
    } else {
      int i;

      for( i=pip_root->pipid_curr; i<pip_root->ntasks; i++ ) {
	if( pip_root->tasks[i].pipid == PIP_PIPID_NULL ) {
	  pipid = i;
	  goto found;
	}
      }
      for( i=0; i<pip_root->pipid_curr; i++ ) {
	if( pip_root->tasks[i].pipid == PIP_PIPID_NULL ) {
	  pipid = i;
	  goto found;
	}
      }
      err = EAGAIN;
      goto unlock;
    }
  found:
    pip_root->tasks[pipid].pipid = pipid;	/* mark it as occupied */
    pip_root->pipid_curr = pipid + 1;
    *pipidp = pipid;

  } while( 0 );
 unlock:
  /*** end lock region ***/
  pip_spin_unlock( &pip_root->lock_tasks );

  RETURN( err );
}

static int pip_do_task_spawn( pip_spawn_program_t *progp,
			      int pipid,
			      int coreno,
			      uint32_t opts,
			      pip_task_t **tskp,
			      pip_spawn_hook_t *hookp ) {
  pip_spawn_args_t	*args = NULL;
  pip_task_t		*task = NULL;
  size_t		stack_size;
  char			*env_stop;
  int 			err = 0;

  ENTER;
  if( pip_root == NULL ) RETURN( EPERM );
  if( !pip_isa_root()  ) RETURN( EPERM );
  if( progp       == NULL ) RETURN( EINVAL );
  if( progp->prog == NULL ) RETURN( EINVAL );
  /* starting from main */
  if( progp->funcname == NULL &&
      ( progp->argv == NULL || progp->argv[0] == NULL ) ) {
    RETURN( EINVAL );
  }
  /* starting from an arbitrary func */
  if( progp->funcname == NULL && progp->prog == NULL ) {
    progp->prog = progp->argv[0];
  }
  if( pipid == PIP_PIPID_MYSELF ||
      pipid == PIP_PIPID_NULL ) {
    RETURN( EINVAL );
  }
  if( pipid != PIP_PIPID_ANY ) {
    if( pipid < 0 || pipid > pip_root->ntasks ) RETURN( EINVAL );
  }
  if( coreno < 0 ) RETURN( EINVAL );

  if( ( err = pip_find_a_free_task( &pipid ) ) != 0 ) goto error;
  task = &pip_root->tasks[pipid];
  pip_reset_task_struct( task );
  task->pipid     = pipid;	/* mark it as occupied */
  task->type      = PIP_TYPE_TASK;
  task->task_root = pip_root;

  args = &task->args;
  args->pipid  = pipid;
  args->coreno = coreno;
  { 				/* GLIBC/misc/init-misc.c */
    char *prog = progp->prog;
    char *p = strrchr( prog, '/' );
    if( p == NULL ) {
      args->prog      = pip_strdup( prog );
      args->prog_full = pip_strdup( prog );
    } else {
      args->prog      = pip_strdup( p + 1 );
      args->prog_full = pip_strdup( prog );
    }
    ASSERT( args->prog != NULL && args->prog_full != NULL );
    DBGF( "prog:%s full:%s", args->prog, args->prog_full );
  }
  err = pip_copy_env( progp->envv, pipid, &args->envvec );
  if( err ) ERRJ_ERR( err );

  if( progp->funcname == NULL ) {
    err = pip_copy_vec( NULL, progp->argv, &args->argvec );
    if( err ) ERRJ_ERR( err );
    args->argc = pip_count_vec( args->argvec.vec );
  } else {
    if( ( args->funcname = pip_strdup( progp->funcname ) ) == NULL ) {
      ERRJ_ERR( ENOMEM );
    }
    args->start_arg = progp->arg;
  }
  task->aux           = progp->aux;
  if( progp->exp != NULL ) {
    task->import_root = progp->exp;
  } else {
    task->import_root = pip_root->export_root;
  }
  if( hookp != NULL ) {
    task->hook_before = hookp->before;
    task->hook_after  = hookp->after;
    task->hook_arg    = hookp->hookarg;
  }
  if( ( env_stop = pip_root->envs.stop_on_start ) != NULL &&
      *env_stop != '\0' ) {
    if( !pip_is_threaded_() ) {
      task->onstart_script = pip_onstart_target( task, env_stop );
      if( task->onstart_script != NULL ) {
	pip_info_mesg( "PiP task[%d] will be SIGSTOPed (%s)",
		       task->pipid, PIP_ENV_STOP_ON_START );
      }
    } else {
      pip_warn_mesg( "%s=%s is NOT effective with (p)thread mode",
		     PIP_ENV_STOP_ON_START, env_stop );
    }
  }
  DBGF( "ONSTART: '%s'", task->onstart_script );
  /* must be called before calling dlmopen() */
  pip_gdbif_task_new( task );
  {
    cpu_set_t cpuset;
    if( ( err = pip_do_corebind( task, coreno, &cpuset ) ) == 0 ) {
      /* core-binding should take place before loading solibs,    */
      /* hoping anon maps would be mapped onto the same numa node */
      PIP_ACCUM( time_load_prog,
		 ( err = pip_load_prog( progp, args, task ) ) == 0 );
      /* and of course, the corebinding must be undone */
      (void) pip_undo_corebind( &cpuset );
    }
  }
  if( err != 0 ) goto error;

  stack_size = pip_stack_size();
  if( ( pip_root->opts & PIP_MODE_PROCESS_PIPCLONE ) ==
      PIP_MODE_PROCESS_PIPCLONE ) {
    int flags = pip_clone_flags( CLONE_PARENT_SETTID |
				 CLONE_CHILD_CLEARTID |
				 CLONE_SYSVSEM |
				 CLONE_PTRACE );
    pid_t pid;

    /* we need lock on ldlinux. supposedly glibc does someting */
    /* before calling main function */
    err = pip_clone_mostly_pthread_ptr( &task->thread,
					flags,
					-1,
					stack_size,
					(void*(*)(void*)) pip_do_spawn,
					args,
					&pid );
    DBGF( "pip_clone_mostly_pthread_ptr()=%d", err );
    if( err ) pip_glibc_unlock();
  } else {
    pthread_attr_t 	attr;
    pid_t 		tid = pip_gettid();

    if( ( err = pthread_attr_init( &attr ) ) != 0 ) goto error;
    ASSERT( pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE ) == 0 );
    ASSERT( pthread_attr_setstacksize( &attr, stack_size )                == 0 );

    DBGF( "tid=%d  cloneinfo@%p", tid, pip_root->cloneinfo );
    do {
      if( pip_clone_lock != NULL ) {
	/* lock is needed, because the pip_clone()
	   might also be called from outside of PiP lib. */
	pip_spin_lock_wv( pip_clone_lock, tid );
	/* unlock is done in the wrapper function */
      }
      err = pthread_create( &task->thread,
			    &attr,
			    (void*(*)(void*)) pip_do_spawn,
			    (void*) args );
      DBGF( "pthread_create()=%d", err );
      if( err ) pip_glibc_unlock();
      /* unlock is done in the created thread */
    } while( 0 );
  }
  if( err == 0 ) {
    pip_sem_wait( &pip_root->sync_spawn );
    pip_root->ntasks_count ++;
    pip_root->ntasks_accum ++;
    pip_root->ntasks_curr  ++;

    if( task->onstart_script != NULL && task->onstart_script[0] != '\0' ) {
      pip_onstart( task );
    }
    *tskp = task;

  } else {
  error:			/* undo */
    if( args != NULL ) {
      if( args->argvec.vec  != NULL ) free( args->argvec.vec  );
      if( args->argvec.strs != NULL ) free( args->argvec.strs );
      if( args->envvec.vec  != NULL ) free( args->envvec.vec  );
      if( args->envvec.strs != NULL ) free( args->envvec.strs );
    }
    if( task != NULL ) {
      if( task->loaded != NULL ) (void) pip_dlclose( task->loaded );
      pip_reset_task_struct( task );
    }
  }
  RETURN( err );
}

int pip_task_spawn( pip_spawn_program_t *progp,
		    uint32_t coreno,
		    uint32_t opts,
		    int *pipidp,
		    pip_spawn_hook_t *hookp ) {
  pip_task_t	*task = NULL;
  int 		pipid, err;

  ENTER;
  if( pipidp == NULL ) {
    pipid = PIP_PIPID_ANY;
  } else {
    pipid = *pipidp;
  }
  err = pip_do_task_spawn( progp, pipid, coreno, opts, &task, hookp );
  if( !err ) {
    if( pipidp != NULL ) *pipidp = task->pipid;
  }
  RETURN( err );
}

int pip_spawn( char *prog,
	       char **argv,
	       char **envv,
	       int   coreno,
	       int  *pipidp,
	       pip_spawnhook_t before,
	       pip_spawnhook_t after,
	       void *hookarg ) {
  pip_spawn_program_t program;
  pip_spawn_hook_t hook;

  ENTER;
  if( prog == NULL ) return EINVAL;
  pip_spawn_from_main( &program, prog, argv, envv, NULL, NULL );
  pip_spawn_hook( &hook, before, after, hookarg );
  RETURN( pip_task_spawn( &program, coreno, 0, pipidp, &hook ) );
}

int pip_fin( void ) {
  int i, err = 0;

  ENTER;
  if( !pip_is_initialized() ) RETURN( EPERM );

  pip_free_all();
  if( !pip_root_p_() ) {
    pip_root = NULL;
    pip_task = NULL;
  } else {
    for( i=0; i<pip_root->ntasks; i++ ) {
      if( pip_root->tasks[i].type != PIP_TYPE_NULL ) {
	DBGF( "%d/%d [%d] -- BUSY", 
	      i, 
	      pip_root->ntasks, 
	      pip_root->tasks[i].pipid );
	err = EBUSY;
	break;
      }
    }
    if( err == 0 ) {
      void pip_named_export_fin_all( void );
      pip_root_t *root = pip_root;
      /* SIGCHLD */
      pip_unset_sigmask();
      pip_unset_signal_handler( SIGCHLD,
				&pip_root->old_sigchld );
      pip_named_export_fin_all();

      PIP_REPORT( time_load_dso  );
      PIP_REPORT( time_load_prog );
      PIP_REPORT( time_dlmopen   );

      void pip_undo_patch_GOT( void );
      pip_undo_patch_GOT();

      pip_root = NULL;
      pip_task = NULL;

      memset( root, 0, sizeof(pip_root_t) );
      free( root );
    }
  }
  RETURN( err );
}

int pip_get_mode( int *mode ) {
  if( pip_root == NULL ) RETURN( EPERM  );
  if( mode != NULL ) {
    *mode = ( pip_root->opts & PIP_MODE_MASK );
  }
  RETURN( 0 );
}

int pip_get_system_id( int pipid, pip_id_t *idp ) {
  pip_task_t 	*task;
  intptr_t	id;
  int 		err;

  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );

  task = pip_get_task_( pipid );
  if( pip_is_threaded_() ) {
    /* Do not use gettid(). This is a very Linux-specific function */
    /* The reason of supporintg the thread PiP execution mode is   */
    /* some OSes other than Linux does not support clone()         */
    id = (intptr_t) task->thread;
  } else {
    id = (intptr_t) task->tid;
  }
  if( idp != NULL ) *idp = id;
  RETURN( 0 );
}

void pip_exit( int extval ) {
  DBGF( "extval:%d", extval );
  if( !pip_is_initialized() ) {
    exit( extval );
  } else if( pip_isa_root() ) {
    exit( extval );
  } else {
    pip_return_from_start_func( pip_task, extval );
  }
  NEVER_REACH_HERE;
}

int pip_get_pid_( int pipid, pid_t *pidp ) {
  pid_t	pid;
  int 	err = 0;

  if( pip_root->opts & PIP_MODE_PROCESS ) {
    /* only valid with the "process" execution mode */
    if( ( err = pip_check_pipid( &pipid ) ) == 0 ) {
      if( pipid == PIP_PIPID_ROOT ) {
	err = EPERM;
      } else {
	pid = pip_root->tasks[pipid].tid;
      }
    }
  } else {
    err = EPERM;
  }
  if( !err && pidp != NULL ) *pidp = pid;
  RETURN( err );
}

int pip_barrier_init( pip_barrier_t *barrp, int n ) {
  if( !pip_is_initialized() ) return EPERM;
  if( n <= 0 ) return EINVAL;
  barrp->count      = n;
  barrp->count_init = n;
  barrp->gsense     = 0;
  return 0;
}

int pip_barrier_wait( pip_barrier_t *barrp ) {
  if( barrp->count_init > 1 ) {
    int lsense = !barrp->gsense;
    if( __sync_sub_and_fetch( &barrp->count, 1 ) == 0 ) {
      barrp->count  = barrp->count_init;
      pip_memory_barrier();
      barrp->gsense = lsense;
    } else {
      while( barrp->gsense != lsense ) pip_pause();
    }
  }
  return 0;
}

int pip_barrier_fin( pip_barrier_t *barrp ) {
  if( !pip_is_initialized() ) return EPERM;
  if( barrp->count != 0     ) return EBUSY;
  return 0;
}

int pip_yield( int flag ) {
  if( !pip_is_initialized() ) RETURN( EPERM );
  if( pip_root != NULL && pip_is_threaded_() ) {
    int pthread_yield( void );
    (void) pthread_yield();
  } else {
    sched_yield();
  }
  return 0;
}

int pip_set_aux( void *aux ) {
  if( !pip_is_initialized() ) RETURN( EPERM );
  pip_task->aux = aux;
  RETURN( 0 );
}

int pip_get_aux( void **auxp ) {
  if( !pip_is_initialized() ) RETURN( EPERM );
  if( auxp != NULL ) {
    *auxp = pip_task->aux;
  }
  RETURN( 0 );
}

void pip_glibc_lock( void ) {
  if( pip_root != NULL ) {
    (void) pip_recursive_lock_lock( &pip_root->glibc_lock );
  }
}

void pip_glibc_unlock( void ) {
  if( pip_root != NULL ) {
    (void) pip_recursive_lock_unlock( &pip_root->glibc_lock );
  }
}

void pip_universal_lock( void ) {
  if( pip_root != NULL ) {
    pip_sem_wait( &pip_root->universal_lock );
  }
}

void pip_universal_unlock( void ) {
  if( pip_root != NULL ) {
    pip_sem_post( &pip_root->universal_lock );
  }
}

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
