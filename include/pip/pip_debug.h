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

#ifndef _pip_debug_h_
#define _pip_debug_h_

#ifdef DOXYGEN_SHOULD_SKIP_THIS
#ifndef DOXYGEN_INPROGRESS
#define DOXYGEN_INPROGRESS
#endif
#endif

#ifndef DOXYGEN_INPROGRESS

/**** debug macros ****/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pip/pip_util.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

INLINE int pip_debug_env( void ) {
  static int flag = 0;
  if( !flag ) {
    if( getenv( "PIP_NODEBUG" ) ) {
      flag = -1;
    } else {
      flag = 1;
    }
  }
  return flag > 0;
}
#define DBGSW		pip_debug_env()

#ifndef LDPIP
extern size_t pip_idstr( char *buf, size_t sz );
#define IDSTR(BUF,SZ)	pip_idstr((BUF),(SZ))
#define PIP_DEBUG_INFO	pip_debug_info()
#define PIP_ABORT	pip_abort()
#else
static size_t ldpip_idstr(char*,size_t);
#define IDSTR(BUF,SZ)	ldpip_idstr((BUF),(SZ))
#define PIP_DEBUG_INFO	
#define PIP_ABORT	abort()
#endif

#define DBGBUFLEN	(512)
#define DBGTAGLEN	(128)

#define DBG_PRTBUF	\
  char *_dbuf = alloca(DBGBUFLEN);				\
  memset(_dbuf,0,DBGBUFLEN);					\
  int _nn=DBGBUFLEN
#define DBG_PRNT(...)	do {				\
    snprintf(_dbuf+strlen(_dbuf),_nn,__VA_ARGS__);	\
    _nn=DBGBUFLEN-strlen(_dbuf); } while(0)

#define DBG_OUTPUT	\
  do { int _dbuf_len = strlen( _dbuf ), _dbuf_rv;		\
    _dbuf[ _dbuf_len ] = '\n';					\
    _dbuf_rv = write( 2, _dbuf, _dbuf_len + 1 );		\
    (void)_dbuf_rv;						\
    _dbuf[0]='\0';_nn=DBGBUFLEN;} while(0)

#define DBG_OUTPUT_NNL	\
  do { int _dbuf_len = strlen( _dbuf ), _dbuf_rv;		\
    _dbuf_rv = write( 2, _dbuf, _dbuf_len );			\
    (void)_dbuf_rv;						\
    memset(_dbuf,0,DBGBUFLEN);_nn=DBGBUFLEN;} while(0)

#define DBG_NN

#define DBG_TAG							   \
  do { char *__tag=alloca(DBGTAGLEN);  memset(__tag,0,DBGTAGLEN);  \
    IDSTR(__tag,DBGTAGLEN);					   \
    DBG_PRNT("%s %s:%d %s()",__tag,				   \
	     basename(__FILE__), __LINE__, __func__ );	} while(0)

#define EMSG(...)							\
  do { DBG_PRTBUF; DBG_TAG; DBG_PRNT(": ");				\
    DBG_PRNT(__VA_ARGS__); DBG_OUTPUT; } while(0)

#define EMSG_NNL(...)							\
  do { DBG_PRTBUF; DBG_TAG; DBG_PRNT(": ");				\
    DBG_PRNT(__VA_ARGS__); DBG_OUTPUT_NNL; } while(0)

#define NL_EMSG(...)							\
  do { DBG_PRTBUF; DBG_PRNT("\n"); DBG_TAG; DBG_PRNT(": ");		\
    DBG_PRNT(__VA_ARGS__); DBG_OUTPUT; } while(0)

#ifndef LDPIP
#define DISABLE_WRAP_MALLOC		\
  int __wrap_malloc = pip_dont_wrap_malloc; pip_dont_wrap_malloc=1
#define ENABLE_WRAP_MALLOC		\
  pip_dont_wrap_malloc = __wrap_malloc;
#else
#define DISABLE_WRAP_MALLOC
#define ENABLE_WRAP_MALLOC
#endif

#define ASSERTD(X)		   				\
  do { if(!(X)) { DISABLE_WRAP_MALLOC;				\
      NL_EMSG("{%s} Assertion FAILED !!!!!!\n",#X);		\
      PIP_DEBUG_INFO; PIP_ABORT; ENABLE_WRAP_MALLOC; } } while(0)

#ifdef DEBUG

#define DBG_TAG_ENTER							\
  do { char __tag[DBGTAGLEN]; IDSTR(__tag,DBGTAGLEN);		\
    DBG_PRNT("%s %s:%d >> %s()",__tag,					\
	     basename(__FILE__), __LINE__, __func__ );	} while(0)
#define DBG_TAG_LEAVE							\
  do { char __tag[DBGTAGLEN]; IDSTR(__tag,DBGTAGLEN);		\
    DBG_PRNT("%s %s:%d << %s()",__tag,					\
	     basename(__FILE__), __LINE__, __func__ );	} while(0)

#define DBG						\
  if(DBGSW) { DISABLE_WRAP_MALLOC;				\
    DBG_PRTBUF; DBG_TAG; DBG_OUTPUT;			\
    ENABLE_WRAP_MALLOC;}

#define DBG_NL						\
  if(DBGSW) { DBG_PRTBUF; DBG_PRNT("\n"); DBG_OUTPUT; }

#define DBGF(...)						\
  if(DBGSW) { DISABLE_WRAP_MALLOC;				\
    EMSG(__VA_ARGS__);						\
    ENABLE_WRAP_MALLOC; }

#define DBGF_NNL(...)						\
  if(DBGSW) { EMSG_NNL(__VA_ARGS__); }

#define ENTER							\
    if(DBGSW) { DISABLE_WRAP_MALLOC;				\
    DBG_PRTBUF; DBG_TAG_ENTER; DBG_OUTPUT;			\
    ENABLE_WRAP_MALLOC; }

#define ENTERF(...)							\
    if(DBGSW) do { DISABLE_WRAP_MALLOC;					\
      DBG_PRTBUF; DBG_TAG_ENTER; DBG_PRNT(": ");			\
      DBG_PRNT(__VA_ARGS__); DBG_OUTPUT;				\
      ENABLE_WRAP_MALLOC; } while(0)

#define LEAVE							\
    if(DBGSW) { DISABLE_WRAP_MALLOC;				\
      DBG_PRTBUF; DBG_TAG_LEAVE; DBG_OUTPUT;			\
      ENABLE_WRAP_MALLOC; }

#define LEAVEF(...)							\
    if(DBGSW) do { DISABLE_WRAP_MALLOC;					\
      DBG_PRTBUF; DBG_TAG_LEAVE; DBG_PRNT(": ");			\
      DBG_PRNT(__VA_ARGS__); DBG_OUTPUT;				\
      ENABLE_WRAP_MALLOC; } while(0)

#define RETURN(X)							\
  do { int __xxx=(X);							\
    if(DBGSW) { DISABLE_WRAP_MALLOC;					\
      DBG_PRTBUF; DBG_TAG_LEAVE;					\
      if(__xxx) {							\
	DBG_PRNT(": ERROR RETURN %d:'%s'",__xxx, pip_errname(__xxx)); } \
      else {								\
	DBG_PRNT(": returns %d",__xxx); }				\
      DBG_OUTPUT; ENABLE_WRAP_MALLOC; }					\
    return (__xxx); } while(0)

#define RETURN_NE(X)							\
  do { int __xxx=(X);							\
    if(DBGSW) { DISABLE_WRAP_MALLOC;					\
      DBG_PRTBUF; DBG_TAG_LEAVE;					\
      DBG_PRNT(": returns %d",__xxx);					\
      DBG_OUTPUT; ENABLE_WRAP_MALLOC; } return (__xxx); } while(0)

#define RETURNV								\
  do { if(DBGSW) { DISABLE_WRAP_MALLOC;					\
      DBG_PRTBUF; DBG_TAG_LEAVE; DBG_OUTPUT;				\
      ENABLE_WRAP_MALLOC; } return; } while(0)

#define DPAUSE	\
  do { struct timespec __ts; __ts.tv_sec=0; __ts.tv_nsec=1*1000*1000;	\
    nanosleep( &__ts, NULL ); } while(0)

#define ASSERT(X)		   				\
  do { DISABLE_WRAP_MALLOC; if(!(X)) {				\
      NL_EMSG("{%s} Assertion FAILED !!!!!!\n",#X);		\
      PIP_DEBUG_INFO; PIP_ABORT;				\
    } else if(DBGSW) {						\
      EMSG("{%s} Assertion SUCCEEDED",#X);			\
    } ENABLE_WRAP_MALLOC; } while(0)

#else  /* DEBUG */

#define DBG
#define DBGF(...)
#define DBGF_NNL(...)
#define ENTER
#define ENTERF(...)
#define LEAVE
#define LEAVEF(...)
#define RETURN(X)		return(X)
#define RETURN_NE(X)		return(X)
#define RETURNV			return
#define DPAUSE
#define ASSERT(X)		ASSERTD(X)

#endif	/* !DEBUG */

#define NEVER_REACH_HERE					\
  do { NL_EMSG( "Should never reach here !!!!!!\n" ); for(;;); } while(0)

#define ERRJ		{ DBG;                goto error; }
#define ERRJ_ERRNO	{ DBG; err=errno;     goto error; }
#define ERRJ_ERR(ENO)	{ DBG; err=(ENO);     goto error; }
#define ERRJ_CHK(FUNC)	{ if( (FUNC) ) { DBG; goto error; } }

#endif
#endif
