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

#define LDPIP

#include <pip/pip_internal.h>
#include <pip/pip_common.h>

static pip_root_t	*ldpip_root;
static pip_task_t	*ldpip_task;
static char		*ldpip_err_mesg  = NULL;
static char		*ldpip_warn_mesg = NULL;

static pip_libc_ftab_t ldpip_libc_ftab;

static void ldpip_set_libc_ftab( pip_task_t *task ) {
  ldpip_libc_ftab.fflush 		= fflush;
  ldpip_libc_ftab.exit 			= exit;
  ldpip_libc_ftab.sbrk 			= sbrk;
  ldpip_libc_ftab.malloc_usable_size 	= malloc_usable_size;
  ldpip_libc_ftab.posix_memalign 	= posix_memalign;
  ldpip_libc_ftab.getaddrinfo	 	= getaddrinfo;
  ldpip_libc_ftab.freeaddrinfo 		= freeaddrinfo;
  ldpip_libc_ftab.gai_strerror 		= gai_strerror;
  ldpip_libc_ftab.pthread_exit 		= pthread_exit;
  ldpip_libc_ftab.dlsym 		= dlsym;
  ldpip_libc_ftab.dlopen 		= dlopen;
  ldpip_libc_ftab.dlinfo 		= dlinfo;
  ldpip_libc_ftab.dlerror 		= dlerror;
  ldpip_libc_ftab.dladdr 		= dladdr;
  ldpip_libc_ftab.dlvsym 		= dlvsym;

  task->libc_ftabp = &ldpip_libc_ftab;
}

static void ldpip_set_name( pip_root_t *root, pip_task_t *task ) {
  SET_NAME_BODY(root,task);
}

static pid_t ldpip_gettid( void ) {
  return (pid_t) syscall( (long int) SYS_gettid );
}

static size_t ldpip_idstr( char *p, size_t s ) {
  pid_t		tid  = ldpip_gettid();
  char 		*opn = "(", *cls = ")";
  int		n;

  n = snprintf( p, s, "%s", opn ); 	s -= n; p += n;
  n = snprintf( p, s, "%d", tid ); 	s -= n; p += n;
  n = snprintf( p, s, "%s", cls ); 	s -= n; p += n;
  return s;
}

static void ldpip_warning( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  if( ldpip_warn_mesg == NULL ) {
    vasprintf( &ldpip_warn_mesg, format, ap );
  }
  va_end( ap );
}

static void ldpip_error( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  if( ldpip_err_mesg == NULL ) {
    vasprintf( &ldpip_err_mesg, format, ap );
  }
  va_end( ap );
}

static int ldpip_is_coefd( int fd ) {
  int flags = fcntl( fd, F_GETFD );
  return( flags > 0 && ( flags & FD_CLOEXEC ) );
}

static void ldpip_close_on_exec( void ) {
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
	  ldpip_is_coefd( fd ) ) {
	(void) close( fd );
	DBGF( "FD:%d is closed (CLOEXEC)", fd );
      }
    }
    (void) closedir( dir );
    (void) close( fd_dir );
  }
}

static int ldpip_corebind( pip_task_t *task, 
			   uint32_t coreno, 
			   cpu_set_t *oldsetp ) {
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

  tid = ldpip_gettid();
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

static void ldpip_colon_sep_path( char *colon_sep_path,
				  int(*for_each_path)(char*,void*,int*),
				  void *arg,
				  int *errp ) {
  char *paths, *p, *q;

  if( colon_sep_path != NULL ) {
    int len = strlen( colon_sep_path );
    paths = alloca( len + 1 );
    strcpy( paths, colon_sep_path );
    ASSERTD( paths != NULL );
    for( p=paths; (q=index(p,':'))!=NULL; p=q+1) {
      *q = '\0';
      if( for_each_path( p, arg, errp ) ) break;
    }
  }
}

static int ldpip_foreach_preload( char *path, void *unused_arg, int *errp ) {
  struct stat st;

  if( stat( path, &st ) != 0 ) {
    ldpip_warning( "Unable to find preload object (%s)", path );
  } else if( !S_ISREG(st.st_mode) ) {
    ldpip_warning( "Specified preload object (%s) is not a file", path );
  } else if( !( st.st_mode & S_IRUSR ) &&
	     !( st.st_mode & S_IRGRP ) &&
	     !( st.st_mode & S_IROTH ) ) {
    ldpip_warning( "Specified preload object (%s) is not readable", path );
  } else if( dlopen( path, DLMOPEN_FLAGS ) == NULL ) {
    ldpip_warning( "Specified preload object (%s) is not loadable", path );
  }
  return 0;
}

static void ldpip_preload( char *env ) {
  ldpip_colon_sep_path( env, ldpip_foreach_preload, NULL, NULL );
}

static void ldpip_libc_init( pip_spawn_args_t *args ) {
  extern void __ctype_init( void );
  void	*loaded = ldpip_task->loaded;
  char	**progname;
  char	**progfull;

  if( ( progname = dlsym( loaded, "__progname"      ) ) != NULL ) {
    *progname = args->prog;
  }
  if( ( progfull = dlsym( loaded, "__progname_full" ) ) != NULL ) {
    *progfull = args->prog_full;
  }
  __ctype_init();
 }

static void ldpip_load_libpip( void **start_task ) {
  int 	l = strlen( ldpip_root->prefixdir ) + strlen( PIPLIB_NAME ) + 2;
  char 	*path = alloca( l );
  char 	*q;
  void	*loaded;
  
  q = path;
  q = stpcpy( q, ldpip_root->prefixdir );
  q = stpcpy( q, "/" );
  q = stpcpy( q, PIPLIB_NAME );

  *start_task = NULL;
  if( ( loaded = dlopen( path, DLMOPEN_FLAGS ) ) != NULL ) {
    *start_task = dlsym( loaded, "pip_start_task_" );
  }
  ASSERT( *start_task != NULL );
}

static int ldpip_check_pie( const char *path ) {
  Elf64_Ehdr elfh;
  int fd;
  int err = 0;

  ASSERT( ( fd = open( path, O_RDONLY ) ) >= 0 );
  if( read( fd, &elfh, sizeof( elfh ) ) != sizeof( elfh ) ) {
    ldpip_error( "Unable to obtain ELF header (%s)", path );
    err = EUNATCH;
  } else if( elfh.e_ident[EI_MAG0] != ELFMAG0 ||
	     elfh.e_ident[EI_MAG1] != ELFMAG1 ||
	     elfh.e_ident[EI_MAG2] != ELFMAG2 ||
	     elfh.e_ident[EI_MAG3] != ELFMAG3 ) {
    ldpip_error( "'%s' is not ELF", path );
    err = ELIBBAD;
  } else if( elfh.e_type != ET_DYN ) {
    ldpip_error( "'%s' is not PIE", path );
    err = ELIBEXEC;
  }
  (void) close( fd );
  return err;
}

static int ldpip_foreach_path( char *path, void *arg, int *errp ) {
  char *prog = arg;
  char *fullpath, *p;
  size_t len;
  struct stat st;

  len = strlen( path ) + strlen( prog ) + 1;
  fullpath = alloca( len );
  p = fullpath;
  p = stpcpy( p, path );
  p = stpcpy( p, "/" );
  p = stpcpy( p, prog );

  if( stat( fullpath, &st ) != 0 ) {
    /* Unable to find task program */
    return 0;
  } else if( !S_ISREG(st.st_mode) ) {
    /* Specified task program is not a file */
    return 0;
  } else if( !( st.st_mode & S_IRUSR ) &&
	     !( st.st_mode & S_IRGRP ) &&
	     !( st.st_mode & S_IROTH ) ) {
    /* Specified task program is not readable */
    return 0;
  } else if( dlopen( fullpath, DLMOPEN_FLAGS ) == NULL ) {
    /* Specified task program is not loadable */
    *errp = ldpip_check_pie( fullpath );
  }
  return 1;
}

static int ldpip_load_user_prog( char *prog ) {
  char *paths = getenv( "PATH" );
  int err = 0;

  if(  index( prog, '/' ) == NULL) {
    ldpip_colon_sep_path( paths, ldpip_foreach_path, prog, &err );
  } else {
    struct stat st;
    
    if( stat( prog, &st ) != 0 ) {
      err = errno;
      ldpip_warning( "Unable to find PiP task program (%s)", prog );
    } else if( !S_ISREG(st.st_mode) ) {
      err = EACCES;
      ldpip_warning( "Specified PiP task program (%s) is not a file", prog );
    } else if( !( st.st_mode & S_IXUSR ) &&
	       !( st.st_mode & S_IXGRP ) &&
	       !( st.st_mode & S_IXOTH ) ) {
      err = EACCES;
      ldpip_warning( "Specified PiP task program (%s) is not executable", prog );
    } else if( dlopen( prog, DLMOPEN_FLAGS ) == NULL ) {
      err = ldpip_check_pie( prog );
    }
  }
  return err;
}

static void *ldpip_load( void *vargs ) {
  pip_spawn_args_t *args = vargs;
  pip_start_task_t start_task;
  int	coreno = args->coreno;
  int	err = 0;

  ENTER;
  ldpip_task->thread = pthread_self();
  ldpip_task->pid    = getpid();
  ldpip_task->tid    = ldpip_gettid();
  /* resume root after setting above variables */
  ldpip_set_name( ldpip_root, ldpip_task );
  pip_sem_post( &ldpip_root->sync_spawn );

  if( !( ldpip_root->opts & PIP_MODE_PTHREAD ) ) ldpip_close_on_exec();
  if( ldpip_corebind( ldpip_task, coreno, NULL ) != 0 ) {
    ldpip_warning( "Failed to bound CPU core:%d (%d)", coreno, err );
  }

  ldpip_libc_init( args );
  ldpip_set_libc_ftab( ldpip_task );
  ldpip_preload( getenv( PIP_ENV_PRELOAD ) );
  ldpip_load_libpip( (void**) &start_task );
  err = ldpip_load_user_prog( args->prog_full );

  return start_task( ldpip_root, 
		     ldpip_task, 
		     args, 
		     err, 
		     ldpip_err_mesg, 
		     ldpip_warn_mesg );
}

static size_t ldpip_stack_size( void ) {
  char 		*env, *endptr;
  ssize_t 	s, sz, scale, smax;
  struct rlimit rlimit;

  if( ( env = getenv( PIP_ENV_STACKSZ ) ) == NULL &&
      ( env = getenv( "KMP_STACKSIZE" ) ) == NULL &&
      ( env = getenv( "OMP_STACKSIZE" ) ) == NULL ) {
    sz = PIP_STACK_SIZE;	/* default */
  } else {
    if( ( sz = (ssize_t) strtoll( env, &endptr, 10 ) ) <= 0 ) {
      ldpip_warning( "stacksize: '%s' is illegal and "
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
	ldpip_warning( "stacksize: '%s' is illegal and "
		       "default size (%ldB) is used instead",
		       env, sz );
	break;
      }
      sz = ( sz < PIP_STACK_SIZE_MIN ) ? PIP_STACK_SIZE_MIN : sz;
      sz = ( sz > smax               ) ? smax               : sz;
    }
  }
  return sz;
}

int ldpip_load_prog_( pip_root_t *root, 
		      pip_task_t *task, 
		      pip_spawn_args_t *args,
		      pip_spinlock_t *lockp ) {
  extern char **environ;
  char 	**envv = args->envvec.vec;
  size_t stack_size;
  pid_t pid;
  int i, err = 0;

  ldpip_root = root;
  ldpip_task = task;

  environ = NULL;
  for( i=0; envv[i]!=NULL; i++ ) putenv( envv[i] );
  /* from now on, getenv() can be called */

  stack_size = ldpip_stack_size();

  if( ( root->opts & PIP_MODE_PROCESS_PIPCLONE ) ==
      PIP_MODE_PROCESS_PIPCLONE ) {
    int flags = pip_clone_flags( CLONE_PARENT_SETTID |
				 CLONE_CHILD_CLEARTID );
    err = root->pip_pthread_create( &task->thread,
				    flags,
				    -1,
				    stack_size,
				    (void*(*)(void*)) ldpip_load,
				    args,
				    &pid );
    DBGF( "pip_clone_mostly_pthread()=%d", err );

  } else {
    pthread_attr_t 	attr;
    pid_t 		tid = ldpip_gettid();

    ASSERTD( pthread_attr_init( &attr )                             == 0 );
    ASSERTD( pthread_attr_setdetachstate( &attr, 
					  PTHREAD_CREATE_JOINABLE ) == 0 );
    ASSERTD( pthread_attr_setstacksize( &attr, stack_size )         == 0 );

    DBGF( "tid=%d  cloneinfo@%p", tid, root->cloneinfo );
    
    if( lockp != NULL ) {
      /* another lock is needed, because the pip_clone()
	 might also be called from outside of PiP lib. */
      pip_spin_lock_wv( lockp, tid );
      /* unlock is done in the wrapper function */
    }
    err = pthread_create( &task->thread,
			  &attr,
			  (void*(*)(void*)) ldpip_load,
			  (void*) args );
    DBGF( "pthread_create()=%d", err );
  }
  /* for synching */
  pip_sem_wait( &ldpip_root->sync_spawn );
  return err;
}
