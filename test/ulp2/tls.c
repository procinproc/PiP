/*
  * $RIKEN_copyright: 2018 Riken Center for Computational Sceience,
  * 	  System Software Devlopment Team. All rights researved$
  * $PIP_VERSION: Version 1.0$
  * $PIP_license: <Simplified BSD License>
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions are
  * met:
  *
  * 1. Redistributions of source code must retain the above copyright
  *    notice, this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright
  *    notice, this list of conditions and the following disclaimer in the
  *    documentation and/or other materials provided with the distribution.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  * The views and conclusions contained in the software and documentation
  * are those of the authors and should not be interpreted as representing
  * official policies, either expressed or implied, of the PiP project.$
*/
/*
 * Written by Atsushi HORI <ahori@riken.jp>, 2016
 */

#define PIP_INTERNAL_FUNCS

#define NULPS		(NTASKS-10)
//#define NULPS	(10)
//#define NULPS	(3)

//#define DEBUG

#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>

#define PIP_PRINT_FSREG
#include <test.h>
#include <pip_ulp.h>

pip_locked_queue_t	queue;

int 		var_static;
__thread int 	var_tls;

void print_tls( int pipid ) {
  char *type = pip_type_str();
  intptr_t fsreg;
  int var_stack;

  TESTINT( pip_get_fsreg( &fsreg ) );
  fprintf( stderr,
	   "<%4s:%3d> Hello  var_static@%p  var_stack@%p  var_tls@%p  "
	   "fsreg:%lx\n",
	   type, pipid, &var_static, &var_stack, &var_tls, fsreg );
}

int main( int argc, char **argv ) {
  pip_locked_queue_t *qp = &queue;
  int ntasks;
  int i, pipid;

  set_sigsegv_watcher();

  if( argc   > 1 ) {
    ntasks = atoi( argv[1] );
  } else {
    ntasks = NTASKS;
  }
  if( ntasks < 2 ) {
    fprintf( stderr,
	     "Too small number of PiP tasks (must be latrger than 1)\n" );
    exit( 1 );
  }

  TESTINT( pip_init( &pipid, NULL, (void*) &qp, 0 ) );
  if( pipid == PIP_PIPID_ROOT ) {
    pip_spawn_program_t prog;
    pip_spawn_from_main( &prog, argv[0], argv, NULL );
    TESTINT( pip_locked_queue_init( &queue ) );
    for( i=0; i<ntasks; i++ ) {
      pipid = i;
      TESTINT( pip_task_spawn( &prog, PIP_CPUCORE_ASIS, &pipid, NULL ) );
    }
    for( i=0; i<ntasks; i++ ) {
      TESTINT( pip_wait( i, NULL ) );
    }
  } else {
    if( pipid < ntasks - 1 ) {
      TESTINT( pip_sleep_and_enqueue( qp, NULL, 0 ) );
    } else {
      for( i=0; i<ntasks-1; i++ ) {
	while( 1 ) {
	  int err = pip_ulp_dequeue_and_involve( qp, NULL, 0 );
	  if( err != ENOENT ) break;
	}
      }
      pip_ulp_yield();
    }
    print_tls( pipid );
  }
  TESTINT( pip_fin() );
  return 0;
}