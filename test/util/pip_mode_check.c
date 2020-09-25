/*
 * $RIKEN_copyright: Riken Center for Computational Sceience,
 * System Software Development Team, 2016, 2017, 2018, 2019$
 * $PIP_VERSION: Version 2.0.0$
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

#include <test.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
  int c, rv = 0;
  int option_pip_mode = 0;
  int pipid, ntasks;

  while ((c = getopt( argc, argv, "CLGPT" )) != -1) {
    switch (c) {
    case 'C':
      option_pip_mode = PIP_MODE_PROCESS_PIPCLONE;
      break;
    case 'L':
      option_pip_mode = PIP_MODE_PROCESS_PRELOAD;
      break;
    case 'G':
      option_pip_mode = PIP_MODE_PROCESS_GOT;
      break;
    case 'P':
      option_pip_mode = PIP_MODE_PROCESS;
      break;
    case 'T':
      option_pip_mode = PIP_MODE_PTHREAD;
      break;
    default:
      fprintf(stderr, "Usage: pip_mode [-CLGPT]\n");
      exit(2);
    }
  }

  ntasks = 1;
  rv = pip_init( &pipid, &ntasks, NULL, option_pip_mode );
  if ( rv != 0 ) {
    fprintf( stderr, "pip_init: %s\n", strerror( rv ));
    return EXIT_FAILURE;
  }
  if( pipid != PIP_PIPID_ROOT ) return EXIT_UNTESTED;
  printf( "%s\n", pip_get_mode_str() );

  return EXIT_SUCCESS;
}
