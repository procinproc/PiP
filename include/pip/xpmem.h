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
 * $PIP_VERSION: Version 2.3.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#ifndef _XPMEM_H
#define _XPMEM_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <sys/types.h>
#include <stdint.h>

#define XPMEM_MAXADDR_SIZE	(size_t)(-1L)

/*
 * The following are the possible XPMEM related errors.
 */
/** Unknown thread due to fork() */
#define XPMEM_ERRNO_NOPROC	2004

/*
 * Valid permit_type values for xpmem_make().
 */
enum {
  /** Permit value are unix-style permissions with mask 0777. Any bit
   * set outside this range is an error. This is the only valid permit
   * mode at this time. */
  XPMEM_PERMIT_MODE = 0x1,
};

/*
 * flags for segment permissions
 */
#define XPMEM_RDONLY			(0x1)
#define XPMEM_RDWR			(0x2)
#define XPMEM_CURRENT_VERSION		(0x00022000)
#define XPMEM_CURRENT_VERSION_STRING	"2.2"

typedef intptr_t xpmem_segid_t;	/* segid returned from xpmem_make() */
typedef intptr_t xpmem_apid_t;	/* apid returned from xpmem_get() */

struct xpmem_addr {
  xpmem_apid_t	apid;		/* apid that represents memory */
  off_t 	offset;		/* offset into apid's memory */
};

static inline int xpmem_version(void) {
  return XPMEM_CURRENT_VERSION;
}

xpmem_segid_t xpmem_make( void*, size_t, int, void* );

static inline
int xpmem_remove( xpmem_segid_t segid ) {
  return 0;
}

static inline
xpmem_apid_t xpmem_get( xpmem_segid_t segid,
			int flags,
			int permit_type,
			void *permit_value ) {
  return segid;
}

static inline
int xpmem_release( xpmem_apid_t apid ) {
  return 0;
}

static inline
void *xpmem_attach( struct xpmem_addr addr, size_t size, void *vaddr ) {
  return (void*) ( addr.apid + addr.offset );
}

static inline
int xpmem_detach( void *vaddr ) {
  return 0;
}

#endif

#endif
