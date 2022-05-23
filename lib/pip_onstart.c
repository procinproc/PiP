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
#include <pip/pip.h>

#include <sys/ptrace.h>

void pip_onstart( pip_task_t *task ) {
  extern char **environ;
  char *script = task->onstart_script;
  pid_t pid;
  siginfo_t info;
  int rv, status;

  ENTER;
  while( 1 ) {
    DBGF( "pid:%d", task->tid );
    rv = waitid( P_PID, task->tid, &info, WSTOPPED | WEXITED | WNOWAIT );
    if( rv < 0 ) {
      if( errno == EINTR ) continue;
      ASSERT( 0 );
      RETURNV;
    }
    status = info.si_status;
    switch( info.si_code ) {
    case CLD_STOPPED:
      goto invoke_onstart_script;
      break;
    case CLD_EXITED:
    case CLD_KILLED:
    case CLD_DUMPED:
    case CLD_TRAPPED:
    case CLD_CONTINUED:
    default:
      DBG;
      pip_set_exit_status( task, status, 0 );
      pip_annul_task( task );
    }
    RETURNV;
  }
 invoke_onstart_script:
  if( script != NULL ) {
    if( *script == '\0' ) {
      pip_info_mesg( "PiP task[%d] (PID=%d) is SIGSTOPed",
		     task->pipid, task->pid );
    } else {
      pip_info_mesg( "PiP task[%d] (PID=%d) is SIGSTOPed and "
		     "start execution %s script",
		     task->pipid, task->pid, script );

      if( ( pid = fork() ) == 0 ) {
	char *argv[5];
#define NUMARG_LEN	16
	char pid_str[NUMARG_LEN];
	char pipid_str[NUMARG_LEN];
	int argc = 0;
	
	snprintf( pid_str,   NUMARG_LEN, "%d", task->pid );
	snprintf( pipid_str, NUMARG_LEN, "%d", task->pipid );
	argv[argc++] = script;
	argv[argc++] = pid_str;
	argv[argc++] = pipid_str;
	argv[argc++] = task->args.prog;
	argv[argc++] = NULL;
	
	execve( argv[0], argv, environ );
	pip_warn_mesg( "Unable to exec: %s (%s)", 
		       script, PIP_ENV_STOP_ON_START );
      } else if( pid < 0 ) {
	pip_warn_mesg( "Unable to fork (%s)", 
		       PIP_ENV_STOP_ON_START );
      } else {
	task->pid_onstart = pid;
      }
    }
  }
  RETURNV;
}
