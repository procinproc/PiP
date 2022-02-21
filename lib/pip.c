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
#include <pip/pip_common.h>
#include <pip/pip_util.h>
#include <pip/pip_gdbif.h>

int 			pip_dont_wrap_malloc PIP_PRIVATE = 1;
int 			pip_initialized      PIP_PRIVATE = 0;
int 			pip_finalized        PIP_PRIVATE = 1;
pip_sem_t		*pip_universal_lockp PIP_PRIVATE;
pip_root_t		*pip_root            PIP_PRIVATE;
pip_task_t		*pip_task            PIP_PRIVATE;
struct pip_gdbif_root	*pip_gdbif_root      PIP_PRIVATE;

static pip_spinlock_t 	*pip_clone_lock;
static pip_clone_t*	pip_cloneinfo   = NULL;
struct pip_gdbif_root	*pip_gdbif_root;

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

static int pip_count_vec( char **vecsrc ) {
  int n;
  for( n=0; vecsrc[n]!= NULL; n++ );
  return( n );
}

static void pip_set_magic( pip_root_t *root ) {
  memcpy( root->magic, PIP_MAGIC_WORD, PIP_MAGIC_LEN );
}

void pip_reset_task_struct( pip_task_t *task ) {
  pip_root_t	*root = task->task_root;
  void		*namexp = task->named_exptab;
  memset( (void*) task, 0, sizeof(pip_task_t) );
  task->pipid        = PIP_PIPID_NULL;
  task->type         = PIP_TYPE_NULL;
  task->task_root    = root;
  task->named_exptab = namexp;
}

static int pip_check_prog( const char *path, int verbose ) {
  struct stat stbuf;
  Elf64_Ehdr elfh;
  int fd;
  int err = 0;

  if( ( fd = open( (path), O_RDONLY ) ) < 0 ) {
    err = errno;
  } else {
    if( fstat( fd, &stbuf ) < 0 ) {
      err = errno;
      if( verbose ) {
	pip_err_mesg( "'%s': stat() fails (%s)", path, strerror( errno ) );
      }
    } else {
      /* check file attributes */
      if( !( stbuf.st_mode & S_IFREG ) ) {
	err = EACCES;
	if( verbose ) {
	  pip_err_mesg( "'%s' is not a regular file", path );
	}
      } else if( ( stbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH) ) == 0 ) {
	err = EACCES;
	if( verbose ) {
	  pip_err_mesg( "'%s' is not executable", path );
	}
      } else {
	/* check ELF header */
	if( read( (fd), &elfh, sizeof( elfh ) ) != sizeof( elfh ) ) {
	  err = EUNATCH;
	  if( verbose ) {
	    pip_err_mesg( "Unable to read ELF header (%s)", path );
	  }
	} else if( elfh.e_ident[EI_MAG0] != ELFMAG0 ||
		   elfh.e_ident[EI_MAG1] != ELFMAG1 ||
		   elfh.e_ident[EI_MAG2] != ELFMAG2 ||
		   elfh.e_ident[EI_MAG3] != ELFMAG3 ) {
	  err = ELIBBAD;
	  if( verbose ) {
	    pip_err_mesg( "'%s' is not ELF", path );
	  }
	} else if( elfh.e_type != ET_DYN ) {
	  err = ELIBEXEC;
	  if( verbose ) {
	    pip_err_mesg( "'%s' is not PIE", path );
	  }
	}
      }
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
    if ( pip_root->pip_pthread_create == NULL )
      pip_root->pip_pthread_create = 
	pip_dlsym( RTLD_DEFAULT, "pip_clone_mostly_pthread" );
    if ( pip_root->pip_pthread_create != NULL ) {
      newmod = PIP_MODE_PROCESS_PIPCLONE;
      goto done;
    } else if( !( desired & PIP_MODE_PTHREAD_BIT) ) {
      pip_err_mesg( "%s mode is requested but pip_clone_mostly_pthread() "
		    "cannot not be found in PiP-glibc",
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

char *pip_prefix_dir( void ) {
  FILE	 *fp_maps = NULL;
  size_t  sz = 0;
  ssize_t l;
  char	 *line = NULL;
  char	 *libpip, *prefix, *p;
  char	 *updir = "/..";
  void	 *sta, *end;
  void	 *faddr = (void*) pip_prefix_dir;

  ASSERTD( ( fp_maps = fopen( PIP_MAPS_PATH, "r" ) ) != NULL );
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
      ASSERTD( ( p = rindex( libpip, '/' ) ) != NULL );
      *p = '\0';
      ASSERTD( ( prefix = (char*) pip_malloc( strlen( libpip ) + 
					      strlen( updir ) + 1 ) )
	      != NULL );
      p = prefix;
      p = stpcpy( p, libpip );
      p = stpcpy( p, updir  );
      ASSERTD( ( p = realpath( prefix, NULL ) ) != NULL );
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

static void pip_set_name( pip_root_t *root, pip_task_t *task ) {
  SET_NAME_BODY(root,task);
}

static void pip_get_cpuset( pip_root_t *root ) {
  cpu_set_t *maxp = &root->maxset;
  cpu_set_t cpuset;
  pid_t pid = getpid();
  int coreno_max, i;

  coreno_max = ( CPU_SETSIZE < PIP_CPUCORE_CORENO_MAX ) ? 
    CPU_SETSIZE : PIP_CPUCORE_CORENO_MAX;
  ASSERT( sched_getaffinity( pid, sizeof(cpuset), &root->cpuset ) == 0 );
  CPU_ZERO( maxp );
  for( i=0; i<coreno_max; i++ ) {
    CPU_ZERO( &cpuset );
    CPU_SET( i, &cpuset );
    if( sched_setaffinity( pid, sizeof(cpuset), &cpuset ) == 0 ) {
      CPU_SET( i, maxp );
    }
  }
  ASSERT( CPU_COUNT( maxp ) > 0 );
  ASSERT( sched_setaffinity( pid, sizeof(cpuset), &root->cpuset ) == 0 );
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

  if( pip_root == NULL && pip_task == NULL ) {
    if(  pip_initialized || !pip_finalized ) RETURN( EBUSY );
    DBGF( "ROOT ROOT ROOT" );
    /* root process */
    if( ntasksp == NULL ) {
      ntasks = PIP_NTASKS_MAX;
    } else {
      ntasks = *ntasksp;
    }
    if( ntasks <= 0             ) RETURN( EINVAL );
    if( ntasks > PIP_NTASKS_MAX ) RETURN( EOVERFLOW );
    {
      sigset_t sigset;
      (void) sigprocmask( SIG_BLOCK, NULL, &sigset );
      if( sigismember( &sigset, SIGCHLD ) ) {
	pip_warn_mesg( "SICHLD is being masked (blocked)" );
	RETURN( EINVAL );
      }
    }
    if( ( err = pip_check_opt_and_env( &opts ) ) != 0 ) RETURN( err );

    sz = sizeof( pip_root_t ) + sizeof( pip_task_t ) * ( ntasks + 1 );
    pip_page_alloc( sz, (void**) &root );
    (void) memset( root, 0, sz );
    pip_set_magic( root );
    root->size_whole = sz;
    root->size_root  = sizeof( pip_root_t );
    root->size_task  = sizeof( pip_task_t );

    pip_spin_init( &root->lock_tasks   );
    pip_recursive_lock_init( &root->glibc_lock );
    pip_sem_init( &root->lock_clone );
    pip_sem_post( &root->lock_clone );
    pip_sem_init( &root->sync_spawn );
    pip_sem_init( &root->universal_lock );
    pip_sem_post( &root->universal_lock );

    pip_get_cpuset( root );
    root->prefixdir    = pip_prefix_dir();
    root->flag_quiet   = ( getenv( PIP_ENV_QUIET ) != NULL );
    root->version      = PIP_API_VERSION;
    root->ntasks       = ntasks;
    root->ntasks_count = 1; /* root is also a PiP task */
    root->cloneinfo    = pip_cloneinfo;
    root->opts         = opts;
    root->page_size    = sysconf( _SC_PAGESIZE );
    root->task_root    = &root->tasks[ntasks];
    for( i=0; i<ntasks+1; i++ ) {
      pip_reset_task_struct( &root->tasks[i] );
      pip_named_export_init( &root->tasks[i] );
    }
    if( rt_expp != NULL ) {
      root->export_root = *rt_expp;
    }
    pipid = PIP_PIPID_ROOT;
    root->task_root->pipid      = pipid;
    root->task_root->type       = PIP_TYPE_ROOT;
    root->task_root->loaded     = pip_dlopen_unsafe( NULL, RTLD_NOW );
    root->task_root->thread     = pthread_self();
    root->task_root->pid        = getpid();
    root->task_root->tid        = pip_gettid();
    root->task_root->task_root  = root;
    root->task_root->libc_ftabp = pip_libc_ftab( NULL );
    pip_task = root->task_root;
    pip_root = root;

    pip_set_name( pip_root, pip_task );
    pip_dont_wrap_malloc = 0;

    if( opts & PIP_MODE_PTHREAD ) {
      pip_initialized = PIP_MODE_PTHREAD;
    } else {
      pip_initialized = PIP_MODE_PROCESS;
    }
    pip_finalized = 0;

    pip_gdbif_initialize_root( ntasks );
    pip_gdbif_task_commit( pip_task );
    
    pip_set_signal_handlers();
    ASSERTD( pthread_atfork( NULL, NULL, pip_after_fork ) == 0 );

    DBGF( "PiP Execution Mode: %s", pip_get_mode_str() );

  } else if( PIP_ISA_ROOT( pip_task ) ) {
    RETURN( EBUSY );

  } else {
    DBGF( "TASK TASK TASK" );
    /* child task */
    DBGF( "pip_root: %p @ %p  piptask : %p @ %p", 
	  pip_root, &pip_root, pip_task, &pip_task );
    if( ntasksp != NULL ) *ntasksp = pip_root->ntasks;
    if( rt_expp != NULL ) *rt_expp = pip_task->import_root;
  }
  /* root and child task */
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
  return( pip_task != NULL         && 
	  PIP_IS_ALIVE( pip_task ) &&
	  !PIP_ISNTA_TASK( pip_task ) );
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

  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) RETURN( err );
  task = pip_get_task_( pipid );
  if( task == NULL || !PIP_IS_ALIVE( task ) ) RETURN( ESRCH );
  if( handle != NULL ) *handle = task->loaded;
  if( lmidp  != NULL ) *lmidp  = task->lmid;
  RETURN( 0 );
}

int pip_get_pipid_( void ) {
  int pipid;
  if( pip_root == NULL ) {
    pipid = PIP_PIPID_ANY;
  } else if( PIP_ISA_ROOT( pip_task ) ) {
    pipid = PIP_PIPID_ROOT;
  } else {
    pipid = pip_task->pipid;
  }
  DBGF( "pipid:%d", pipid );
  return pipid;
}

int pip_get_pipid( int *pipidp ) {
  if( !pip_is_effective() ) RETURN( EPERM );
  if( pipidp != NULL ) *pipidp = pip_get_pipid_();
  RETURN( 0 );
}

int pip_get_ntasks( int *ntasksp ) {
  if( !pip_is_effective() ) RETURN( EPERM );
  if( ntasksp != NULL ) *ntasksp = pip_root->ntasks;
  RETURN( 0 );
}

static pip_task_t *pip_get_myself( void ) {
  return pip_task;
}

int pip_export( void *export ) {
  pip_task_t *task;

  if( !pip_is_effective() )  RETURN( EPERM );
  if( export == NULL )       RETURN( EINVAL );
  task = pip_get_myself();
  if( task->export != NULL ) RETURN( EBUSY );
  task->export = export;
  RETURN( 0 );
}

int pip_import( int pipid, void **exportp ) {
  pip_task_t *task;
  int err;

  ENTER;
  if( ( err = pip_check_pipid( &pipid ) ) == 0 ) {
    task = pip_get_task_( pipid );
    if( task == NULL || !PIP_IS_ALIVE( task ) ) RETURN( ESRCH );
    if( exportp != NULL ) *exportp = (void*) task->export;
    pip_memory_barrier();
  }
  RETURN( 0 );
}

#define PIP_GET_ADDR
#ifdef PIP_GET_ADDR
int pip_get_addr( int pipid, const char *name, void **addrp ) {
  pip_task_t *task;
  void *addr;
  int err;

  if( name == NULL ) return EINVAL;
  if( ( err = pip_check_pipid( &pipid ) ) != 0 ) return err;
  task = pip_get_task_( pipid );
  if( task == NULL || !PIP_IS_ALIVE( task ) ) return ESRCH;
  addr = pip_dlsym( task->loaded, name );
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

void pip_finalize_root( pip_root_t *root ) {
  if( root != NULL ) {
    pip_named_export_fin_all( root );
  }
  pip_unset_signal_handlers();
  pip_undo_patch_GOT();

  memset( root, 0, sizeof(pip_root_t) );
  pip_libc_free( root );
  pip_root = NULL;
  pip_task = NULL;
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

static int pip_find_a_free_task( int *pipidp ) {
  int pipid = *pipidp;
  int err = 0;

  if( pipid < PIP_PIPID_ANY || pipid >= pip_root->ntasks ) {
    DBGF( "pipid=%d", pipid );
    RETURN( EINVAL );
  }

  pip_spin_lock( &pip_root->lock_tasks );
  /*** begin lock region ***/
  do {
    if( pipid != PIP_PIPID_ANY ) {
      if( pip_root->tasks[pipid].type != PIP_TYPE_NULL ) {
	err = EAGAIN;
	goto unlock;
      }
    } else {
      int i;

      for( i=pip_root->pipid_curr; i<pip_root->ntasks; i++ ) {
	if( pip_root->tasks[i].type == PIP_TYPE_NULL ) {
	  pipid = i;
	  goto found;
	}
      }
      for( i=0; i<pip_root->pipid_curr; i++ ) {
	if( pip_root->tasks[i].type == PIP_TYPE_NULL ) {
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

static int 
pip_foreach_path( char *path, void **argp, int flag, int *errp ) {
  char **progp = (char**)argp;
  char *fullpath, *p;
  size_t len;

  len = strlen( path ) + strlen( *progp ) + 1;
  fullpath = alloca( len );
  p = fullpath;
  p = stpcpy( p, path );
  p = stpcpy( p, "/" );
  p = stpcpy( p, *progp );

  if( pip_check_prog( fullpath, flag ) == 0 ) {
    *((char**)argp) = strdup( fullpath );
    ASSERTD( *progp != NULL );
    *errp = 0;
    return 1;			/* stop the foreeach loop */
  }
  return 0;
}

static void pip_colon_sep_path( char *colon_sep_path,
				int(*for_each_path)(char*,void**,int,int*),
				void **argp,
				int flag,
				int *errp ) {
  *errp = ENOENT;
  EXPAND_PATH_LIST_BODY(colon_sep_path,for_each_path,argp,flag,errp);
}

static int pip_check_user_prog( char **progp ) {
  char *paths = getenv( "PATH" );
  int err = 0;

  if( index( *progp, '/' ) == NULL &&
      paths  != NULL &&
      *paths != '\0' ) {
    pip_colon_sep_path( paths, pip_foreach_path, (void**)progp, 0, &err );

  } else {
    err = pip_check_prog( *progp, 1 );
  }
  return err;
}

static int pip_do_task_spawn( pip_spawn_program_t *progp,
			      int pipid,
			      int coreno,
			      uint32_t opts,
			      pip_task_t **tskp,
			      pip_spawn_hook_t *hookp ) {
  pip_spawn_args_t	*args = NULL;
  pip_task_t		*task = NULL;
  char			*userprog, *env_stop;
  int 			err = 0;

  ENTER;
  if( !pip_is_effective() )                  RETURN( EPERM   );
  if( pip_root == NULL )                     RETURN( EPERM );
  if( progp == NULL || progp->prog == NULL ) RETURN( EINVAL );
  /* starting from main */
  if( progp->funcname == NULL &&
      ( progp->argv == NULL || progp->argv[0] == NULL ) ) {
    RETURN( EINVAL );
  }
  /* starting from an arbitrary func */
  if( progp->funcname == NULL && progp->prog == NULL ) {
    progp->prog = progp->argv[0];
  }
  /* checking user program */
  userprog = progp->prog;
  if( ( err = pip_check_user_prog( &userprog ) ) ) RETURN( err );
  progp->prog = userprog;
  /* checking pipid */
  if( pipid == PIP_PIPID_MYSELF ||
      pipid == PIP_PIPID_NULL ) {
    RETURN( EINVAL );
  }
  if( pipid != PIP_PIPID_ANY ) {
    if( pipid < 0 || pipid > pip_root->ntasks ) RETURN( EINVAL );
  }
  /* checking coreno */
  if( coreno != PIP_CPUCORE_ASIS ) {
    int flags = coreno & PIP_CPUCORE_FLAG_MASK;
    int value = coreno & PIP_CPUCORE_CORENO_MASK;
    if( ( flags & PIP_CPUCORE_ABS ) != flags ) RETURN( EINVAL );
    if( value >= PIP_CPUCORE_CORENO_MAX      ) RETURN( EINVAL );
  }
  /* checking prog */

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
      args->prog = pip_strdup( prog );
    } else {
      args->prog = pip_strdup( p + 1 );
    }
    args->prog_full = pip_strdup( prog );

    DBGF( "prog:%s full:%s", args->prog, args->prog_full );
    ASSERTD( args->prog != NULL && args->prog_full != NULL );
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
  if( ( env_stop = getenv( PIP_ENV_STOP_ON_START ) ) != NULL &&
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

  pip_gdbif_task_new( task );
  {
    char *libdir = "/lib/";
    int l = strlen( pip_root->prefixdir ) + strlen( libdir ) +
      strlen( LDPIP_NAME ) + 1;
    char *p = alloca( l );
    char *q;
    Lmid_t lmid;
    int(*ldpip_load)( pip_root_t*, 
		      pip_task_t*, 
		      pip_spawn_args_t *arg,
		      pip_spinlock_t *lock );
    q = p;
    q = stpcpy( q, pip_root->prefixdir );
    q = stpcpy( q, "/lib/" );
    q = stpcpy( q, LDPIP_NAME );

    void *loaded = pip_dlmopen( LM_ID_NEWLM, p, DLOPEN_FLAGS );
    if( loaded == NULL ) {
      pip_err_mesg( "Unable to load %s - %s", LDPIP_NAME, pip_dlerror() );
      err = ENOEXEC;
      goto error;
    }
    ASSERT( ( ldpip_load = pip_dlsym( loaded, "ldpip_load_prog_" ) ) != NULL );
    if( ( err = ldpip_load( pip_root, task, args, pip_clone_lock ) ) != 0 ) goto error;
    ASSERT( pip_dlinfo( loaded, RTLD_DI_LMID, &lmid ) == 0 );

    DBGF( "lmid:%d", (int) lmid );

    task->loaded = loaded;
    task->lmid   = lmid;
  }
  if( err == 0 ) {
    pip_root->ntasks_count ++;
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
      pip_gdbif_finalize_task( task );
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
  if( progp == NULL       ) RETURN( EINVAL );
  if( !pip_is_effective() ) RETURN( EPERM );
  if( pip_task != NULL && !PIP_ISA_ROOT( pip_task ) ) RETURN( EPERM );
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
  if( prog == NULL        ) RETURN( EINVAL );
  if( !pip_is_effective() ) RETURN( EPERM );
  if( pip_task != NULL && !PIP_ISA_ROOT( pip_task ) ) RETURN( EPERM );
  pip_spawn_from_main( &program, prog, argv, envv, NULL, NULL );
  pip_spawn_hook( &hook, before, after, hookarg );
  RETURN( pip_task_spawn( &program, coreno, 0, pipidp, &hook ) );
}

int pip_fin( void ) {
  int i, err = 0;

  ENTER;
  pip_free_all();

  if( pip_task == NULL ) {
    RETURN( EPERM );
  } else if( PIP_ISA_ROOT( pip_task ) ) {
    if( pip_is_finalized()  ) RETURN( EBUSY );
    if( !pip_is_effective() ) RETURN( EPERM );

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
      pip_finalize_root( pip_root );
      pip_finalized = pip_initialized;
      pip_initialized = 0;
    }
  } else { 			/* child task */
    pip_unset_signal_handlers();
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
  if( task == NULL || !PIP_IS_ALIVE( task ) ) RETURN( ESRCH );
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
  pip_do_exit( pip_task, PIP_EXIT_EXIT, extval );
  NEVER_REACH_HERE;
}

int pip_get_pid_( int pipid, pid_t *pidp ) {
  pip_task_t 	*task;
  pid_t	pid;
  int 	err = 0;

  if( !pip_is_threaded_() ) {
    /* only valid with the "process" execution mode */
    if( ( err = pip_check_pipid( &pipid ) ) == 0 ) {
      if( pipid == PIP_PIPID_ROOT ) {
	err = EPERM;
      } else {
	task = pip_get_task_( pipid );
	if( task == NULL || 
	    !PIP_IS_ALIVE( task ) ) {
	  err = ESRCH;
	} else {
	  pid = task->tid;
	}
      }
    }
  } else {
    err = EPERM;
  }
  if( !err && pidp != NULL ) *pidp = pid;
  RETURN( err );
}

int pip_barrier_init( pip_barrier_t *barrp, int n ) {
  if( !pip_is_effective() ) return EPERM;
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
  if( !pip_is_effective()               ) return EPERM;
  if( barrp->count != barrp->count_init ) return EBUSY;
  return 0;
}

int pip_yield( int flag ) {
  if( !pip_is_effective() ) RETURN( EPERM );
  if( pip_root != NULL && pip_is_threaded_() ) {
    int pthread_yield( void );
    (void) pthread_yield();
  } else {
    sched_yield();
  }
  return 0;
}

int pip_set_aux( void *aux ) {
  if( !pip_is_effective() ) RETURN( EPERM );
  pip_task->aux = aux;
  RETURN( 0 );
}

int pip_get_aux( void **auxp ) {
  if( !pip_is_effective() ) RETURN( EPERM );
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
