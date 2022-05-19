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
#include <pip/pip_util.h>

extern int pip_get_dso( int, void** );
extern int pip_root_p_( void );

/* the following function(s) are for debugging */

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

#define PIP_DEBUG_BUFSZ		(4096)

void pip_print_maps( void ) {
  int fd = open( "/proc/self/maps", O_RDONLY );
  char buf[PIP_DEBUG_BUFSZ];

  while( 1 ) {
    ssize_t rc, wc;
    char *p;

    if( ( rc = read( fd, buf, PIP_DEBUG_BUFSZ ) ) <= 0 ) break;
    p = buf;
    do {
      if( ( wc = write( 2, p, rc ) ) < 0 ) { /* STDOUT */
	fprintf( stderr, "write error\n" );
	goto error;
      }
      p  += wc;
      rc -= wc;
    } while( rc > 0 );
  }
 error:
  close( fd );
}

#define FDPATH_LEN	(512)
#define RDLINK_BUF	(256)
#define RDLINK_BUF_SP	(RDLINK_BUF+8)

static int pip_is_coefd( int fd ) {
  int flags = fcntl( fd, F_GETFD );
  return( flags > 0 && ( flags & FD_CLOEXEC ) );
}

void pip_print_fd( FILE *fp, int fd ) {
  char fdpath[FDPATH_LEN];
  char fdname[RDLINK_BUF_SP];
  ssize_t sz;

  sprintf( fdpath, "/proc/self/fd/%d", fd );
  if( ( sz = readlink( fdpath, fdname, RDLINK_BUF ) ) > 0 ) {
    fdname[sz] = '\0';
    char idstr[64];
    pip_idstr( idstr, 64 );
    if( pip_is_coefd ( fd ) ) {
      fprintf( fp, "%s %s -> %s [COE]\n", idstr, fdpath, fdname );
    } else {
      fprintf( fp, "%s %s -> %s\n", idstr, fdpath, fdname );
    }
  }
}

void pip_print_fds( FILE *fp ) {
  DIR *dir = opendir( "/proc/self/fd" );
  struct dirent *dep;
  char idstr[64];
  char fdpath[FDPATH_LEN];
  char fdname[RDLINK_BUF_SP];
  ssize_t sz;

  pip_idstr( idstr, 64 );
  if( dir != NULL ) {
    int fd_dir = dirfd( dir );
    int fd;

    while( ( dep = readdir( dir ) ) == 0 ) {
      sprintf( fdpath, "/proc/self/fd/%s", dep->d_name );
      if( ( sz = readlink( fdpath, fdname, RDLINK_BUF ) ) > 0 ) {
	fdname[sz] = '\0';
	if( ( fd = atoi( dep->d_name ) ) != fd_dir ) {
	  if( pip_is_coefd ( fd ) ) {
	    fprintf( fp, "%s %s -> %s [COE]\n", idstr, fdpath, fdname );
	  } else {
	    fprintf( fp, "%s %s -> %s\n", idstr, fdpath, fdname );
	  }
	} else {
	  fprintf( fp, "%s %s -> %s  opendir(\"/proc/self/fd\")\n",
		   idstr, fdpath, fdname );
	}
      }
    }
    closedir( dir );
  }
}

void pip_check_addr( char *tag, void *addr ) {
  FILE *maps = fopen( "/proc/self/maps", "r" );
  char idstr[64];
  char *line = NULL;
  size_t sz  = 0;
  int retval;

  if( tag == NULL ) {
    pip_idstr( idstr, 64 );
    tag = &idstr[0];
  }
  while( maps != NULL ) {
    void *start, *end;

    if( ( retval = getline( &line, &sz, maps ) ) < 0 ) {
      fprintf( stderr, "getline()=%d\n", errno );
      break;
    } else if( retval == 0 ) {
      continue;
    }
    line[retval] = '\0';
    if( sscanf( line, "%p-%p", &start, &end ) == 2 ) {
      if( (intptr_t) addr >= (intptr_t) start &&
	  (intptr_t) addr <  (intptr_t) end ) {
	fprintf( stderr, "%s %p: %s", tag, addr, line );
	goto found;
      }
    }
  }
  fprintf( stderr, "%s %p (not found)\n", tag, addr );
 found:
  if( line != NULL ) free( line );
  fclose( maps );
}

double pip_gettime( void ) {
  struct timeval tv;
  gettimeofday( &tv, NULL );
  return ((double)tv.tv_sec + (((double)tv.tv_usec) * 1.0e-6));
}
