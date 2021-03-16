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

#define _GNU_SOURCE 
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static char *prog;

char *sigtab[] = {
  "HUP",
  "INT",
  "QUIT",
  "ILL",
  "TRAP",
  "ABRT",
  "BUS",
  "FPE",
  "KILL",
  "USR1",
  "SEGV",
  "USR2",
  "PIPE",
  "ALRM",
  "TERM",
  "STKFLT",
  "CHLD",
  "CONT",
  "STOP",
  "TSTP",
  "TTIN",
  "TTOU",
  "URG",
  "XCPU",
  "XFSZ",
  "VTALRM",
  "PROF",
  "WINCH",
  "IO",
  "PWR",
  "SYS",
  NULL };
  
void print_usage( void ) {
  fprintf( stderr, "%s <SIGNAL> <PID> <TID>\n", prog );
  exit( 2 );
}

static int tgkill( int tgid, int tid, int sig ) {
  return (int) syscall( (long int) SYS_tgkill, tgid, tid, sig );
}

int main( int argc, char **argv ) {
  char *signam = argv[1];
  int signo, pid, tid, err;

  prog = basename( argv[0] );
  if( argc != 4 ) print_usage();

  if( isalpha( *signam ) ) {
    int i;
    if( strncasecmp( signam, "SIG", 3 ) == 0 ) {
      signam = signam + 3;
    }
    for( i=0; sigtab[i]!=NULL; i++ ) {
      if( strcasecmp( signam, sigtab[i] ) == 0 ) {
	signo = i + 1;
	goto found;
      }
    }
    print_usage();
  } else {
    signo = strtol( argv[1], NULL, 10 );
  }
  if( signo < 2 || signo > 32 ) print_usage();
 found:

  pid = strtol( argv[2], NULL, 10 );
  if( pid < 2 ) print_usage();

  tid = strtol( argv[3], NULL, 10 );
  if( tid < 2 ) print_usage();

  err = 0;
  if( tgkill( pid, tid, signo ) != 0 ) {
    err = errno;
    fprintf( stderr, "%s %d %d %d: %s\n", prog, signo, pid, tid, strerror( err ) );
  }
  return err;
}
