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
 * System Software Development Team, 2016-2022
 * $
 * $PIP_VERSION: Version 2.4.1$
 *
 * $Author: Atsushi Hori 
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#include <pip/pip_internal.h>
#include <pip/build.h>

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

#define VALTAB_PREFIX		(5)

struct value_table valtab[] =
  { { "Package",     PACKAGE_NAME },			      /* 0 */
    { "Version",     PACKAGE_VERSION },			      /* 1 */
    { "License",     "the 2-clause simplified BSD License" }, /* 2 */
    { "Build OS",    BUILD_OS },			      /* 3 */
    { "Build CC",    BUILD_CC },			      /* 4 */
    { "Prefix dir",  NULL },				      /* 5 */
    { "PiP-glibc",   PIP_INSTALL_GLIBCDIR },		      /* 6 */
    { "ld-linux",    LDLINUX },				      /* 7 */
    { "Commit Hash", COMMIT_HASH },			      /* 8 */
    { "Debug build",					      /* 9 */
#ifdef DEBUG
      "yes"
#else
      "no"
#endif
    },
    { NULL, NULL },
  };

static void print_item( int item ) {
  if( item == ALL ) {
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
  char *argstr = NULL, **argvec = NULL;
  size_t szs = ARGSTR_SZ, sz;
  int fd = -1, argc, i, c;

  if( ( argstr = (char*) malloc( szs ) ) == NULL ) return -1;
  if( ( fd = open( procfs, O_RDONLY ) ) < 0 ) goto fail;
  while( 1 ) {
    memset( argstr, 0, szs );
    if( ( sz = read( fd, argstr, szs ) ) < szs ) break;
    szs *= 2;
    if( ( argstr = (char*) realloc( argstr, szs ) ) == NULL ) {
      goto fail;
    }
    if( lseek( fd, 0, SEEK_SET ) < 0 ) goto fail;
  }
  close( fd );
  fd = -1;

  argc = 0;
  for( i=0; i<sz; i++ ) {
    if( argstr[i] == '\0' ) argc++;
  }
  argvec = (char**) malloc( sizeof(char*) * ( argc + 1 ) );
  if( argvec == NULL ) goto fail;

  for( c=0, i=0; c<argc; c++ ) {
    argvec[c] = &argstr[i];
    for( ; argstr[i]!='\0'; i++ );
    i++;			/* skip '\0' */
  }
  argvec[c] = NULL;
  *argvp = argvec;
  return argc;

 fail:
  if( fd >= 0 ) close( fd );
  free( argstr );
  return -1;
}

int pip_main( void ) {
  extern void __ctype_init( void );
  extern char *pip_prefix_dir( void );
  char **argv;
  char *argv0;
  int   argc, extval = 0;

  __ctype_init();
  if( ( argc = parse_cmdline( &argv ) ) < 0 ) {
    fprintf( stderr, "Error: Unable to get parameters\n" );
    print_item( ALL );
  } else {
    argv[0] = argv0 = basename( argv[0] );
    valtab[VALTAB_PREFIX].value = pip_prefix_dir();
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
      print_item( ALL );
    }
    free( argv );
  }
  fflush( NULL );
  /* we cannot return from this */
  _exit( extval );
  NEVER_REACH_HERE;
  return 0;			/* dummy */
}
