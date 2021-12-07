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
#include <pip/build.h>
#include <getopt.h>

const char interp[] __attribute__((section(".interp"))) = LDLINUX;

#define OPTION(name,val)	{ #name, optional_argument, NULL, (val) }

#define USAGE		(100)
#define ALL		(1000)

struct option opttab[] =
  { OPTION( name,      0 ),
    OPTION( version,   1 ),
    OPTION( license,   2 ),
    OPTION( os,        3 ),
    OPTION( cc,        4 ),
    OPTION( prefix,    5 ),
    OPTION( glibc,     6 ),
    OPTION( ldlinux,   7 ),
    OPTION( commit,    8 ),
    OPTION( debug,     9 ),
    OPTION( usage, USAGE ),
    OPTION( help,  USAGE ),
    OPTION( all,     ALL ),
    { NULL, 0, NULL, 0 } };

struct value_table {
  char *name;
  char *value;
};

struct value_table valtab[] =
  { { "Package",     PACKAGE_NAME },
    { "Version",     PACKAGE_VERSION },
    { "License",     "the 2-clause simplified BSD License" },
    { "Build OS",    BUILD_OS },
    { "Build CC",    BUILD_CC },
    { "Prefix dir",  NULL },
    { "PiP-glibc",   PIP_INSTALL_GLIBCDIR },
    { "ld-linux",    LDLINUX },
    { "Commit Hash", COMMIT_HASH },
    { "Debug build",
#ifdef DEBUG
      "yes"
#else
      "no"
#endif
    },
    { NULL, NULL },
  };

static void print_item( int item ) {
  if( item < 0 || item == ALL ) {
    int i;
    for( i=0; valtab[i].name!=NULL; i++ ) {
      printf( "%s:\t%s\n", valtab[i].name, valtab[i].value );
    }
    printf( "URL:\t\t%s\n",    PACKAGE_URL       );
    printf( "mailto:\t\t%s\n", PACKAGE_BUGREPORT );
  } else {
    printf( "%s\n", valtab[item].value );
  }
}

static void print_usage( char *argv0 ) {
  int i;
  fprintf( stderr, "%s ", argv0 );
  for( i=0; opttab[i].name != NULL; i++ ) {
    fprintf( stderr, "[--%s]", opttab[i].name );
  }
  fprintf( stderr, "\n" );
}

static int parse_cmdline( char ***argvp ) {
#define ARGSTR_SZ	(512)
  char *procfs = "/proc/self/cmdline";
  char *argstr, **argvec;
  size_t szs = ARGSTR_SZ, sz;
  int fd, argc, i, c;

  if( ( argstr = (char*) malloc( szs ) ) == NULL ) return -1;
  if( ( fd = open( procfs, O_RDONLY ) ) < 0 ) {
    free( argstr );
    return -1;
  }
  memset( argstr, 0, szs );
  while( 1 ) {
    if( ( sz = read( fd, argstr, szs ) ) < szs ) break;
    if( lseek( fd, 0, SEEK_SET ) < 0 ) {
      free( argstr );
      close( fd );
      return -1;
    }
    szs *= 2;
    if( ( argstr = (char*) realloc( argstr, szs ) ) == NULL ) {
      free( argstr );
      close( fd );
      return -1;
    }
    memset( argstr, 0, szs );
  }
  close( fd );
  argc = 0;
  for( i=0; i<sz; i++ ) {
    if( argstr[i] == '\0' ) argc++;
  }
  argvec = (char**) malloc( sizeof(char*) * ( argc + 1 ) );
  if( argvec == NULL ) {
    free( argstr );
    return -1;
  }
  for( c=0, i=0; c<argc; c++ ) {
    argvec[c] = &argstr[i];
    for( ; argstr[i]!='\0'; i++ );
    i++;			/* skip '\0' */
  }
  argvec[c] = NULL;
  *argvp = argvec;
  return argc;
}

int pip_main( void ) {
  extern char *pip_prefix_dir(void);
  char **argv;
  char *argv0;
  int argc, extval = 0;

  if( ( argc = parse_cmdline( &argv ) ) > 0 ) {
    argv[0] = argv0 = basename( argv[0] );
  }
  valtab[5].value = pip_prefix_dir();
  if( argc > 1 ) {
    int v;
    while( ( v = getopt_long_only( argc, argv, "", opttab, NULL ) ) >= 0 ) {
      if( v == USAGE || v == '?' || v ==':' ) {
	print_usage( argv0 );
	extval = 1;
	break;
      }
      print_item( v );
    }
  } else {
    print_item( -1 );
  }
  fflush( NULL );
  _exit( extval );
  NEVER_REACH_HERE;
  return 0;			/* dummy */
}
