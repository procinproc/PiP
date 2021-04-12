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
 * $PIP_VERSION: Version 3.1.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#include <pip/pip_internal.h>
#include <pip/pip.h>

#include <sys/ptrace.h>

void pip_onstart( pip_task_internal_t *taski ) {
  extern char **environ;
  char *script = MA(taski)->onstart_script;
  pid_t tid, pid;

  ENTER;
  while( 1 ) {
    int status;
    DBGF( "pid:%d", AA(taski)->tid );
    tid = waitpid( AA(taski)->tid, &status, 
#ifdef __WALL
		   __WALL |
#endif
#ifdef __WCLONE
		   __WCLONE |
#endif
		   WUNTRACED );
    if( tid < 0 ) {
      if( errno == EINTR ) continue;
      DBGF( "waitpid():%d", errno );
      RETURNV;
    }
    DBGF( "status:0x%x", status );
    if( WIFSTOPPED(  status ) ) break;

    if( WIFEXITED( status ) ) {
      pip_set_exit_status( taski, status );
      if( WIFSIGNALED( status ) ) {
	pip_task_signaled( taski, status );
      }
      pip_annul_task( taski );
      RETURNV;
    }
    if( WIFSIGNALED( status ) ) {
      pip_set_exit_status( taski, status );
      if( WIFSIGNALED( status ) ) {
	pip_task_signaled( taski, status );
      }
      pip_annul_task( taski );
      RETURNV;
    }
  }
#define NUMARG_LEN	16
  if( ( pid = fork() ) == 0 ) {
    char *argv[5];
    char pid_str[NUMARG_LEN];
    char pipid_str[NUMARG_LEN];
    int argc = 0;
    
    snprintf( pid_str,   NUMARG_LEN, "%d", MA(taski)->pid   );
    snprintf( pipid_str, NUMARG_LEN, "%d", TA(taski)->pipid );
    argv[argc++] = script;
    argv[argc++] = pid_str;
    argv[argc++] = pipid_str;
    argv[argc++] = MA(taski)->args.prog;
    argv[argc++] = NULL;
    
    execve( argv[0], argv, environ );
    pip_warn_mesg( "Unable to exec: %s (%s)", 
		   script, PIP_ENV_STOP_ON_START );
  } else if( pid < 0 ) {
    pip_warn_mesg( "Unable to fork (%s)", 
		   PIP_ENV_STOP_ON_START );
  } else {
    MA(taski)->pid_onstart = pid;
  }
  RETURNV;
}
