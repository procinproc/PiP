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
#include <pip/pip.h>

typedef int(*main_t)(int,char**,char**);

int main( int argc, char **argv, char **env ) {
  char *prel = argv[1];
  char *func = argv[2];
  char *arg2 = argv[3];
  char *prog = argv[4];
  char **new_argv = &argv[4];

  void *loaded;
  void *start, *argp;
  int  extval;

  if( *prel != '\0' ) {
    load_preload( prel );
  }
  loaded = dlopen( prog, RTLD_NOW );
  if( loaded == NULL ) {
    printf( "dlopen(%s): %s\n", prog, dlerror() );
    return( 1 );
  }
  start = dlsym( loaded, func );
  if( start == NULL ) {
    printf( "dlsym(%s): %s\n", prog, dlerror() );
    return( 1 );
  }
  if( strcmp( func, "main" ) == 0 ) {
    extval = start( argc - 4, new_argv, env );
  } else {
    argp = (void*) strtol( args, NULL, 16 );
    extval = start( argp );
  }
  return( extval );
}
