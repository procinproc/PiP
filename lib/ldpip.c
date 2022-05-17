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

#define CLONE_SYSCALL	"__clone"

#define LDPIP_ELF_GLOBAL		(1)
#define LDPIP_ELF_LOCAL			(2)
#define LDPIP_ELF_MULTI_LOCAL		(-1)
#define LDPIP_ELF_ERROR			(-2)

static pip_root_t	*ldpip_root;
static pip_task_t	*ldpip_task;
static char		*ldpip_err_mesg     = NULL;
static char		*ldpip_warn_mesg    = NULL;
static pip_clone_syscall_t ldpip_clone_orig = NULL;
static int		flag_wrap_clone  = 0;

static int ldpip_iterate_phdr( struct dl_phdr_info *info, 
			       size_t size, 
			       void *unused ) {
  fprintf( stderr, "PHDR: '%s'\t%p\n", info->dlpi_name, (void*)info->dlpi_addr );
  return 0;
}

static void ldpip_print_maps( char *title, void *loaded ) {
  //#define DEBUG_AHAH
#ifdef DEBUG_AHAH
#define LDPIP_BUFSZ		(4096)
  int fd = open( "/proc/self/maps", O_RDONLY );
  char buf[LDPIP_BUFSZ];
  char *begin = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
  char *end   = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

  if( fd > 0 ) {
    fprintf( stderr, "%s\n%s\n", begin, title );

    dl_iterate_phdr( ldpip_iterate_phdr, NULL );
    {
      struct link_map *lm = (struct link_map*) loaded;
      while( lm != NULL ) {
	fprintf( stderr, "LINK: '%s'\t%p\n", lm->l_name, (void*)lm->l_addr );
	lm = lm->l_next;
      }
    }
    while( 1 ) {
      ssize_t rc;
      if( ( rc = read( fd, buf, LDPIP_BUFSZ-1 ) ) <= 0 ) break;
      buf[rc] = '\0';
      fprintf( stderr, "%s", buf );
    }
    close( fd );
    fprintf( stderr, "%s\n", end );
  }
#endif
}

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
  ldpip_libc_ftab.pthread_create	= pthread_create;
  ldpip_libc_ftab.pthread_exit 		= pthread_exit;
  ldpip_libc_ftab.dlsym 		= dlsym;
  ldpip_libc_ftab.dlopen 		= dlopen;
  ldpip_libc_ftab.dlinfo 		= dlinfo;
  ldpip_libc_ftab.dlerror 		= dlerror;
  ldpip_libc_ftab.dladdr 		= dladdr;
  ldpip_libc_ftab.dlvsym 		= dlvsym;

  task->libc_ftabp = &ldpip_libc_ftab;
}

static pid_t ldpip_gettid( void ) {
  return (pid_t) syscall( (long int) SYS_gettid );
}

static void ldpip_libc_lock( void ) {
  pip_recursive_lock_lock( ldpip_gettid(), 
			   &ldpip_root->libc_lock );
}

static void ldpip_libc_unlock( void ) {
  pip_recursive_lock_unlock( ldpip_gettid(),
			     &ldpip_root->libc_lock );
}

static void *ldpip_dlopen( const char *filename, int flag ) {
  void *handle;
  ldpip_libc_lock();
  handle = dlopen( filename, flag );
  ldpip_libc_unlock();
  return handle;
}

static void *ldpip_dlsym( void *handle, const char *symbol ) {
  ldpip_libc_lock();
  void *addr = dlsym( handle, symbol );
  ldpip_libc_unlock();
  return addr;
}

static void ldpip_set_name( pip_root_t *root, pip_task_t *task ) {
  SET_NAME_BODY(root,task);
}

static int
ldpip_pipid_str( char *p, size_t sz, int pipid, int upper ) {
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
    if( ldpip_root != NULL && ldpip_root->ntasks > 0 ) {
      if( 0 <= pipid && pipid < ldpip_root->ntasks ) {
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

static int ldpip_task_str( char *p, size_t sz, pip_task_t *task ) {
  int 	n = 0;

  if( task == NULL ) {
    n = snprintf( p, sz, "-" );
  } else if( task->type == PIP_TYPE_NULL ) {
    n = snprintf( p, sz, "*" );
  } else if( PIP_IS_ALIVE( task ) ) {
    n = ldpip_pipid_str( p, sz, task->pipid, 1 );
  } else {
    n = snprintf( p, sz, "x" );
  }
  return n;
}

size_t ldpip_idstr( char *p, size_t s ) {
  pid_t		tid  = ldpip_gettid();
  pip_task_t	*ctx = ldpip_task;
  char 		*opn = "<", *cls = ">", *delim = ":";
  int		n;

  n = snprintf( p, s, "%s", opn ); 	s -= n; p += n;
  {
    n = snprintf( p, s, "%d", tid ); 	s -= n; p += n;
    n = snprintf( p, s, "%s", delim ); 	s -= n; p += n;
    n = ldpip_task_str( p, s, ctx ); 	s -= n; p += n;
  }
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

static pip_clone_syscall_t	pip_clone_orig;
static pip_spinlock_t 		pip_lock_clone;

static pip_spinlock_t ldpip_lock_clone_lock( pid_t tid ) {
  pip_spinlock_t oldval;

  while( 1 ) {
    oldval = pip_spin_trylock_wv( &pip_lock_clone, 
				  PIP_CLONE_LOCK_OTHERWISE );
    if( oldval == tid ) {
      /* called and locked by PiP lib */
      break;
    }
    if( oldval == PIP_CLONE_LOCK_UNLOCKED ) { /* lock succeeds */
      /* not called by PiP lib */
      break;
    }
  }
  return oldval;
}

int __clone( int(*fn)(void*), void *child_stack, int flags, void *args, ... ) {
  va_list ap;
  va_start( ap, args );

  pid_t		*ptid = va_arg( ap, pid_t* );
  void		*tls  = va_arg( ap, void*  );
  pid_t 	*ctid = va_arg( ap, pid_t* );
  pid_t		tid = ldpip_gettid();
  int 		retval = -1;

  ENTER;
  if( ldpip_lock_clone_lock( tid ) == tid && flag_wrap_clone ) {
    /* __clone is called from ldpip */
    flags = pip_clone_flags( flags );
  } else {
    /* __clone is called outside of ldpip (and pip) */
  }
  retval = ldpip_clone_orig( fn, child_stack, flags, args, ptid, tls, ctid );
  pip_spin_unlock( &pip_lock_clone );

  va_end( ap );
  RETURN_NE( retval );
}

static void 
ldpip_colon_sep_path( char *colon_sep_path,
		      int(*for_each_path)(char*,void**,int,char**,int*),
		      void **argp,
		      int flag,
		      char **pathp,
		      int *errp ) {
  EXPAND_PATH_LIST_BODY(colon_sep_path,for_each_path,argp,flag,pathp,errp);
}

static int ldpip_foreach_preload( char *path, 
				  void **unused_argp, 
				  int unused_flag, 
				  char **pathp,
				  int *errp ) {
  struct stat st;

  if( stat( path, &st ) != 0 ) {
    ldpip_warning( "Unable to find preload object (%s)", path );
  } else if( !S_ISREG(st.st_mode) ) {
    ldpip_warning( "Specified preload object (%s) is not a file", path );
  } else if( !( st.st_mode & S_IRUSR ) &&
	     !( st.st_mode & S_IRGRP ) &&
	     !( st.st_mode & S_IROTH ) ) {
    ldpip_warning( "Specified preload object (%s) is not readable", path );
  } else if( ldpip_dlopen( path, RTLD_LAZY ) == NULL ) {
    ldpip_warning( "Specified preload object (%s) is not loadable", path );
  } else {
    if( ldpip_dlopen( path, RTLD_LAZY ) == NULL ) {
      ldpip_warning( "Unable to load preload object (%s): ", path, dlerror() );
    }
  }
  return 0;
}

static void ldpip_preload( char *env ) {
  if( env != NULL && *env != '\0' ) {
    ldpip_colon_sep_path( env, ldpip_foreach_preload, NULL, 1, NULL, NULL );
  }
}

static void ldpip_libc_init( void ) {
  extern void __ctype_init( void );
  __ctype_init();
}

static void *ldpip_morecore( ptrdiff_t size ) {
  return mmap( NULL,
	       size,
	       PROT_READ | PROT_WRITE,
	       MAP_ANONYMOUS | MAP_PRIVATE,
	       -1,
	       0 );
}

__attribute__((constructor))
static void ldpip_init_section( void ) {
  /* setting to use mmap in malloc, instead of sbrk */
  __morecore = ldpip_morecore;
}

static void ldpip_libc_setup( pip_spawn_args_t *args ) {
  /* extern void *(*__morecore)(ptrdiff_t __size) in malloc.h */
  char	**progname;
  char	**progfull;
  
  if( ( progname = ldpip_dlsym( RTLD_DEFAULT, "__progname"      ) ) != NULL ) {
    *progname = args->prog;
  }
  if( ( progfull = ldpip_dlsym( RTLD_DEFAULT, "__progname_full" ) ) != NULL ) {
    *progfull = args->prog_full;
  }
}

static int ldpip_determin_narena( void ) {
  /* workaround for glibc */
  long nproc_conf, nproc_online;
  long narena = ldpip_root->ntasks;

  nproc_conf   = (int) sysconf( _SC_NPROCESSORS_CONF );
  nproc_online = (int) sysconf( _SC_NPROCESSORS_ONLN );
  narena = ( 8            > narena ) ? 8            : narena;
  narena = ( nproc_conf   > narena ) ? nproc_conf   : narena;
  narena = ( nproc_online > narena ) ? nproc_online : narena; 
  narena ++;
  return narena;
}

static void *ldpip_load_libpip( void ) {
  char 	*path;
  char  *libdir = "/lib/";
  char 	*q;
  void	*loaded;
  int 	l;
  
  l = strlen( ldpip_root->prefixdir ) + 
      strlen( libdir ) + 
      strlen( PIPLIB_NAME ) + 1;
  q = path = alloca( l );
  q = stpcpy( q, ldpip_root->prefixdir );
  q = stpcpy( q, libdir );
  q = stpcpy( q, PIPLIB_NAME );

  DBGF( "%s", path );
  ASSERT( ( loaded = ldpip_dlopen( path, DLOPEN_FLAGS ) ) != NULL );
  ldpip_print_maps( "libpip", loaded );
  return loaded;
}

static int ldpip_search_libpip_itr( struct dl_phdr_info *info, 
				    size_t size, 
				    void *arg ) {
  int *flag = (int*) arg;
  char *fullname = (char*) info->dlpi_name;
  char *basename;

  if( fullname != NULL ) {
    basename = strrchr( fullname, '/' );
    if( basename != NULL ) {
      basename ++;
    } else {
      basename = fullname;
    }
    if( strncmp( basename, PIPLIB_NAME, strlen(PIPLIB_NAME) ) == 0 ) {
      *flag = 1;
      return 1;
    }
  }
  return 0;
}

static void *ldpip_load( void *vargs ) {
  pip_spawn_args_t *args = vargs;
  pip_start_task_t start_task;
  void	*retv;
  char  **envv = args->envvec.vec;
  int	coreno = args->coreno;
  int	i, err = 0;

  ENTER;
  ldpip_libc_init();
  ldpip_task->thread = pthread_self();
  ldpip_task->pid    = getpid();
  ldpip_task->tid    = ldpip_gettid();
  ldpip_set_name( ldpip_root, ldpip_task );
  /* resume root after setting above variables */
  pip_sem_post( &ldpip_root->sync_spawn );
  {
    /* here, do not call pthread_setaffinity(). This MAY fail */
    /* because pd->tid is NOT set yet.  I do not know why.    */
    /* But it is OK to call sched_setaffinity() with tid.     */
    if( sched_setaffinity( ldpip_gettid(), 
			   sizeof(cpu_set_t), 
			   &ldpip_task->cpuset ) != 0 ) {
      ldpip_warning( "Unable to bind CPU core:%d (%d)", coreno, err );
    }
  }
  if( !( ldpip_root->opts & PIP_MODE_PTHREAD ) ) ldpip_close_on_exec();

  ASSERT( ( start_task = (pip_start_task_t) 
	    ldpip_dlsym( ldpip_task->loaded, "__pip_start_task" ) ) != NULL );

  retv = start_task( ldpip_root, 
		     ldpip_task, 
		     args, 
		     err, 
		     ldpip_err_mesg, 
		     ldpip_warn_mesg );
  return retv;
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

static int ldpip_search_symbol( char*, void** );

int __ldpip_load_prog( pip_root_t *root, 
		       pip_task_t *task, 
		       pip_spawn_args_t *args,
		       char **warn_mesg,
		       char **err_mesg ) {
  extern char **environ;
  pip_clone_mostly_pthread_t libc_clone;
  char **envv = args->envvec.vec;
  char *start_func;
  void *loaded;
  void *start;
  size_t stack_size;
  pid_t pid;
  int narena, i, rv, err = 0;

  ldpip_root = root;
  ldpip_task = task;

  environ = NULL;
  for( i=0; envv[i]!=NULL; i++ ) putenv( envv[i] );
  /* from now on, getenv() can be called */
  SETUP_MALLOC_ARENA_ENV( root->ntasks );
  /* from now on, malloc() can be called */
  
  ldpip_libc_setup( args );
  ldpip_print_maps( "LDPIP", ldpip_task->loaded );
  ldpip_set_libc_ftab( ldpip_task );
  ldpip_preload( getenv( PIP_ENV_PRELOAD ) );
  ASSERT( ( loaded = ldpip_load_libpip() ) != NULL );
  ldpip_task->loaded = loaded;
  if( ( loaded =
	ldpip_dlopen( args->prog_full, DLOPEN_FLAGS ) ) == NULL ) {
    err = ELIBEXEC;
    goto error;
  }
  ldpip_print_maps( "dlopen", loaded );
  /* in the following code, we cannot call dlsym() because the */
  /* dlsym() call in some glibc versions aborts, not returning */
  /* an error, when a symbol is not found. this happens when   */
  /* calling dlsym() from dlmopen()ed new symbol space         */ 
  if( args->funcname == NULL ) {
    start_func = "main";
  } else {
    start_func = args->funcname;
  }
  rv = ldpip_search_symbol( start_func, &start );
  switch( rv ) {
  case LDPIP_ELF_GLOBAL:
    if( args->funcname == NULL ) {
      args->func_main = start;
    } else {
      args->func_user = start;
    }
    break;
  case LDPIP_ELF_LOCAL:
    if( args->funcname == NULL ) {
      args->func_main = start;
      ldpip_warning( "%s: main function is a local symbol. "
		   "(possibly, not compiled with '-rdynamic' option)",
		   args->prog );
    } else {
      args->func_user = start;
      ldpip_warning( "%s: function '%s' is a local symbol",
		     args->prog, args->funcname );
    }
    *warn_mesg = ldpip_warn_mesg;
    break;
  case LDPIP_ELF_MULTI_LOCAL:
    ldpip_error( "%s: symbol '%s' has multiple definitions",
		 args->prog, start_func );
    *err_mesg = ldpip_err_mesg;
    err = ENOEXEC;
    break;
  default:
    if( args->funcname == NULL ) {
      ldpip_error( "%s: Unable to find main", args->prog );
    } else {
      ldpip_error( "%s: Unable to find start function (%s)",
		   args->prog, args->funcname );
    }
    *err_mesg = ldpip_err_mesg;
    err = ENOEXEC;
    break;
  }
  if( err ) goto error;

  ldpip_clone_orig = ldpip_dlsym( RTLD_NEXT, CLONE_SYSCALL );
  ASSERT( ldpip_clone_orig != NULL );

  stack_size = ldpip_stack_size();
  if( ( root->opts & PIP_MODE_MASK ) == PIP_MODE_PROCESS_PIPCLONE ) {
    int flags = pip_clone_flags( CLONE_PARENT_SETTID |
				 CLONE_CHILD_CLEARTID );
    libc_clone = ldpip_dlsym( RTLD_DEFAULT, "pip_clone_mostly_pthread" );
    ASSERT( libc_clone != NULL );
    err = libc_clone( &task->thread,
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
      
    DBGF( "tid=%d", tid );
      
    if( root->opts & PIP_MODE_PROCESS_PRELOAD ) {
      flag_wrap_clone = 1;
      /* another lock is needed, because the pip_clone()
	 might also be called from outside of PiP lib. */
    }
    pip_spin_lock_wv( &pip_lock_clone, tid );
    /* unlock is done in the wrapper function of __clone */
    err = pthread_create( &task->thread,
			  &attr,
			  (void*(*)(void*)) ldpip_load,
			  (void*) args );
    DBGF( "pthread_create()=%d", err );
  }
  /* for synching */
  pip_sem_wait( &ldpip_root->sync_spawn );

 error:
  return err;
}

/* ---------------------------------------------------- */

static int ldpip_read_elf64_header( int fd, Elf64_Ehdr *ehdr ) {
  if( read( fd, ehdr, sizeof(Elf64_Ehdr) ) != sizeof(Elf64_Ehdr) ) {
    return -1;
  } else if( ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	     ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	     ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	     ehdr->e_ident[EI_MAG3] != ELFMAG3 ) {
    return -1;
  } else if( ehdr->e_ident[EI_CLASS] != ELFCLASS64 ) {
    return -1;
  }
  return 0;
}

static int ldpip_read_elf64_section_header( int fd,
					    int nth,
					    Elf64_Ehdr *ehdr,
					    Elf64_Shdr *shdr ) {
  off_t off = ehdr->e_shoff + ( ehdr->e_shentsize * nth );
  if( pread( fd, shdr, sizeof(Elf64_Shdr), off ) != sizeof(Elf64_Shdr) ) {
    return -1;
  }
  return 0;
}

static int ldpip_read_elf64_dynamic_section( int fd,
					     off_t offset,
					     size_t size,
					     Elf64_Dyn *dyns ) {
  if( pread( fd, dyns, size, offset ) != size ) return -1;
  return 0;
}

static int ldpip_read_elf64_symbol_value( int fd,
					  Elf64_Ehdr *ehdr,
					  Elf64_Sym *sym,
					  intptr_t *valp ) {
  Elf64_Shdr 	shdr;
  intptr_t 	val;
  int 		err = 0;
  
  ldpip_read_elf64_section_header( fd, sym->st_shndx, ehdr, &shdr );
  if( pread( fd, &val, sym->st_size, shdr.sh_offset+sym->st_value ) !=
      sym->st_size ) {
    err = -1;
  } else {
    *valp = val;
  }
  return err;
}

typedef struct {
  char	*symbol;
  void	*addr; 
  int 	status;
} ldpip_search_symbol_arg_t;

static int ldpip_read_elf64( struct dl_phdr_info *info, size_t size, void *varg ) {
  ldpip_search_symbol_arg_t *arg = (ldpip_search_symbol_arg_t*) varg;
  Elf64_Ehdr 	ehdr;
  Elf64_Shdr	shdr;
  Elf64_Sym	*symtab, *sym;
  const char 	*fname = info->dlpi_name;
  char		*p, *strtab;
  intptr_t	val;
  void		*addr;
  int		fd, i, j, nsyms, ith_sec, retv;

  ENTERF( "%s", fname );
  p = strrchr( fname, '/' );
  if( p != NULL ) {
    /* ldpip itself has also main. so ignore this */
    if( strcmp( p+1, LDPIP_NAME  ) == 0 ) RETURN_NE( 0 );
    if( strcmp( p+1, PIPLIB_NAME ) == 0 ) RETURN_NE( 0 );
  }
  if( ( fd = open( fname, O_RDONLY ) )     <  0 ) RETURN_NE( 0 );
  if( ldpip_read_elf64_header( fd, &ehdr ) != 0 ) RETURN_NE( 0 );

  symtab = NULL;
  strtab = NULL;
  retv   = 0;
  nsyms  = 0;
  for( i=0; i<ehdr.e_shnum; i++ ) {
    if( ldpip_read_elf64_section_header( fd, i, &ehdr, &shdr ) != 0 ) {
      goto try_next;
    }
    if( shdr.sh_type != SHT_SYMTAB ) continue;
    
    nsyms  = shdr.sh_size / sizeof(Elf64_Sym);
    symtab = (Elf64_Sym*) malloc( shdr.sh_size );
    if( pread( fd, symtab, shdr.sh_size, shdr.sh_offset ) !=
	shdr.sh_size ) {
      goto try_next;
    }
    if( ldpip_read_elf64_section_header( fd,
					 shdr.sh_link,
					 &ehdr,
					 &shdr ) != 0 ) {
      goto try_next;
    }
    strtab = (char*) malloc( shdr.sh_size );
    if( pread( fd, strtab, shdr.sh_size, shdr.sh_offset ) != shdr.sh_size ) {
      goto try_next;
    }
    break;
  }
  ASSERTD( symtab != NULL && strtab != NULL );

  sym = symtab;
  for( j=0; j<nsyms; j++,sym++ ) {
    if( ELF64_ST_TYPE(sym->st_info) != STT_FUNC ) continue;
    char *name = strtab + sym->st_name;
    if( *name == '\0' ) continue;
    //DBGF( "%s : '%s' : %d", fname, name, ELF64_ST_TYPE(sym->st_info) );
    if( strcmp( name, arg->symbol ) != 0 ) continue;
    val  = (intptr_t) sym->st_value;
    addr = (void*) info->dlpi_addr + val;
      
    switch( ELF64_ST_BIND(sym->st_info) ) {
    case STB_GLOBAL:
      //DBGF( "%s : '%s' : GLOBAL (%p)", fname, name, (void*)val );
      arg->addr   = addr;
      arg->status = LDPIP_ELF_GLOBAL;
      retv = 1;		/* discontinue */
      break;
    case STB_LOCAL:
      //DBGF( "%s : '%s' : LOCAL", fname, name );
      if( arg->status == LDPIP_ELF_LOCAL ) {
	/* another symbol found */
	arg->addr   = NULL;
	arg->status = LDPIP_ELF_MULTI_LOCAL;
	retv = 1;		/* discontinue */
      } else {
	arg->addr   = addr;
	arg->status = LDPIP_ELF_LOCAL;
	/* continue to check if multiple defined */
      }
      break;
    default:
      DBGF( "%s : '%s' : (something else)", fname, name );
    }
    if( retv ) break;;
  }
 try_next:
  free( symtab );
  free( strtab );
  (void) close( fd );
  RETURN_NE( retv );
}

static int ldpip_search_symbol( char *symbol, void **addrp ) {
  ldpip_search_symbol_arg_t arg;
  arg.symbol = symbol;
  arg.addr   = NULL;
  arg.status = 0;
  dl_iterate_phdr( ldpip_read_elf64, (void*) &arg );
  if( arg.status > 0 ) *addrp = arg.addr;
  return arg.status;
}

int main() {
  return 1;
}
