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

//#define DEBUG
#include <test.h>

int count_sigchld = 0;;

void sigchld_handler( int sig ) {
#ifdef DEBUG
  fprintf( stderr, "SIGCHLD\n" );
#endif
  count_sigchld ++;
}

int main( int argc, char **argv ) {
  int pipid, ntasks;
  int i;

  set_signal_watcher( SIGSEGV );

  ntasks = NTASKS;
  if( argc > 1 ) {
    ntasks = strtol( argv[1], NULL, 10 );
  }
  ntasks = ( ntasks == 0 ) ? NTASKS : ntasks;

  CHECK( pip_init( &pipid, &ntasks, NULL, 0 ), RV, return(EXIT_FAIL) );
  if( pipid == PIP_PIPID_ROOT ) {
    struct sigaction 	sigact;

    memset( &sigact, 0, sizeof( sigact ) );
    CHECK( sigemptyset( &sigact.sa_mask ),        RV, return(EXIT_FAIL) );
    CHECK( sigaddset( &sigact.sa_mask, SIGCHLD ), RV, return(EXIT_FAIL) );
    sigact.sa_sigaction = (void(*)()) sigchld_handler;
    CHECK( sigaction( SIGCHLD, &sigact, NULL ),   RV, return(EXIT_FAIL) );

    for( i=0; i<ntasks; i++ ) {
      pipid = i;
      CHECK( pip_spawn( argv[0], argv, NULL, PIP_CPUCORE_ASIS, &pipid,
			NULL, NULL, NULL ),
	     RV,
	     return(EXIT_FAIL) );
      CHECK( pip_wait_any( NULL, NULL ), RV, return(EXIT_FAIL) );
    }
    CHECK( (count_sigchld != ntasks), RV, return(EXIT_FAIL) );
  } else {
    CHECK( pip_fin(),    RV, return(EXIT_FAIL) );
    pip_exit( 0 );
    CHECK( "pip_exit() returns", RV, return(EXIT_FAIL) );
    /* never reach here */
  }
  CHECK( pip_fin(), RV, return(EXIT_FAIL) );
  return 0;
}