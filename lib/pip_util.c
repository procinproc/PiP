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
#include <pip/pip_util.h>

#include <dlfcn.h>
#include <elf.h>

extern int pip_is_coefd( int );
extern int pip_get_dso( int, void** );
extern int pip_root_p_( void );

/* the following function(s) are for debugging */

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
      if( ( wc = write( 1, p, rc ) ) < 0 ) { /* STDOUT */
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
  struct dirent *de;
  char idstr[64];
  char fdpath[FDPATH_LEN];
  char fdname[RDLINK_BUF_SP];
  ssize_t sz;

  pip_idstr( idstr, 64 );
  if( dir != NULL ) {
    int fd_dir = dirfd( dir );
    int fd;

    while( ( de = readdir( dir ) ) != NULL ) {
      sprintf( fdpath, "/proc/self/fd/%s", de->d_name );
      if( ( sz = readlink( fdpath, fdname, RDLINK_BUF ) ) > 0 ) {
	fdname[sz] = '\0';
	if( ( fd = atoi( de->d_name ) ) != fd_dir ) {
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

void pip_print_loaded_solibs( FILE *file ) {
  void *handle = NULL;
  char idstr[PIPIDLEN];
  int err;

  /* pip_init() must be called in advance */
  (void) pip_idstr( idstr, PIPIDLEN );
  if( file == NULL ) file = stderr;

  if( ( err = pip_get_dlmopen_info( PIP_PIPID_MYSELF, &handle, NULL ) ) != 0 ) {
    fprintf( file, "%s (no solibs found: %d)\n", idstr, err );
  } else {
    struct link_map *map = (struct link_map*) handle;
    for( ; map!=NULL; map=map->l_next ) {
      char *fname;
      if( *map->l_name == '\0' ) {
	fname = "(noname)";
      } else {
	fname = map->l_name;
      }
      fprintf( file, "%s %s at %p\n", idstr, fname, (void*)map->l_addr );
    }
  }
  if( pip_root_p_() && handle != NULL ) dlclose( handle );
}
