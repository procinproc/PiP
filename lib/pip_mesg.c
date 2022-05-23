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

#define PIP_MESGLEN		(512)

static void
pip_message( FILE *fp, char *tag, int dnl, const char *format, va_list ap ) {
  char mesg[PIP_MESGLEN];
  char idstr[PIP_MIDLEN];
  int len, nnl, fd;

  if( pip_root == NULL || !pip_root->flag_quiet ) {
    if( !dnl ) {
      nnl = 2;
    } else {
      nnl = 3;
    }
    pip_idstr( idstr, PIP_MIDLEN );
    len = 0;
    if( dnl ) {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len-nnl, "\n" );
    }
    len += snprintf(    &mesg[len], PIP_MESGLEN-len-nnl, "%s%s ", tag, idstr );
    len += vsnprintf(   &mesg[len], PIP_MESGLEN-len-nnl, format, ap );
    if( dnl ) {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len, "\n\n" );
    } else {
      len += snprintf(  &mesg[len], PIP_MESGLEN-len, "\n" );
    }
    fflush( fp );
    /* !!!! DON'T USE FPRINTF HERE !!!! */
    fd = fileno( fp );
    (void) write( fd, mesg, len );
  }
}

void pip_info_fmesg( FILE *fp, const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  if( fp == NULL ) fp = stderr;
  pip_message( fp, "PiP-INFO", 0, format, ap );
  va_end( ap );
}

void pip_info_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr, "PiP-INFO", 0, format, ap );
  va_end( ap );
}

void pip_warn_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr, "PiP-WARN", 0, format, ap );
  va_end( ap );
}

void pip_err_mesg( const char *format, ... ) {
  va_list ap;
  va_start( ap, format );
  pip_message( stderr,
	       "PiP-ERR", 
#ifdef DEBUG
	       1,
#else
	       0,
#endif
	       format, ap );
  va_end( ap );
}
