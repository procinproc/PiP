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

#include <errno.h>

const char *pip_errname( int err ) {
  switch( err ) {
  case 0:		return( "(Success)" );
#ifdef	EPERM
  case EPERM:		return( "EPERM" );
#endif
#ifdef	ENOENT
  case ENOENT:		return( "ENOENT" );
#endif
#ifdef	ESRCH
  case ESRCH:		return( "ESRCH" );
#endif
#ifdef	EINTR
  case EINTR:		return( "EINTR" );
#endif
#ifdef	EIO
  case EIO:		return( "EIO" );
#endif
#ifdef	ENXIO
  case ENXIO:		return( "ENXIO" );
#endif
#ifdef	E2BIG
  case E2BIG:		return( "E2BIG" );
#endif
#ifdef	ENOEXEC
  case ENOEXEC:		return( "ENOEXEC" );
#endif
#ifdef	EBADF
  case EBADF:		return( "EBADF" );
#endif
#ifdef	ECHILD
  case ECHILD:		return( "ECHILD" );
#endif
#ifdef	EAGAIN
  case EAGAIN:		return( "EAGAIN" );
#endif
#ifdef	ENOMEM
  case ENOMEM:		return( "ENOMEM" );
#endif
#ifdef	EACCES
  case EACCES:		return( "EACCES" );
#endif
#ifdef	EFAULT
  case EFAULT:		return( "EFAULT" );
#endif
#ifdef	ENOTBLK
  case ENOTBLK:		return( "ENOTBLK" );
#endif
#ifdef	EBUSY
  case EBUSY:		return( "EBUSY" );
#endif
#ifdef	EEXIST
  case EEXIST:		return( "EEXIST" );
#endif
#ifdef	EXDEV
  case EXDEV:		return( "EXDEV" );
#endif
#ifdef	ENODEV
  case ENODEV:		return( "ENODEV" );
#endif
#ifdef	ENOTDIR
  case ENOTDIR:		return( "ENOTDIR" );
#endif
#ifdef	EISDIR
  case EISDIR:		return( "EISDIR" );
#endif
#ifdef	EINVAL
  case EINVAL:		return( "EINVAL" );
#endif
#ifdef	ENFILE
  case ENFILE:		return( "ENFILE" );
#endif
#ifdef	EMFILE
  case EMFILE:		return( "EMFILE" );
#endif
#ifdef	ENOTTY
  case ENOTTY:		return( "ENOTTY" );
#endif
#ifdef	ETXTBSY
  case ETXTBSY:		return( "ETXTBSY" );
#endif
#ifdef	EFBIG
  case EFBIG:		return( "EFBIG" );
#endif
#ifdef	ENOSPC
  case ENOSPC:		return( "ENOSPC" );
#endif
#ifdef	ESPIPE
  case ESPIPE:		return( "ESPIPE" );
#endif
#ifdef	EROFS
  case EROFS:		return( "EROFS" );
#endif
#ifdef	EMLINK
  case EMLINK:		return( "EMLINK" );
#endif
#ifdef	EPIPE
  case EPIPE:		return( "EPIPE" );
#endif
#ifdef	EDOM
  case EDOM:		return( "EDOM" );
#endif
#ifdef	ERANGE
  case ERANGE:		return( "ERANGE" );
#endif
#ifdef	EDEADLK
  case EDEADLK:		return( "EDEADLK" );
#endif
#ifdef	ENAMETOOLONG
  case ENAMETOOLONG:	return( "ENAMETOOLONG" );
#endif
#ifdef	ENOLCK
  case ENOLCK:		return( "ENOLCK" );
#endif
#ifdef	ENOSYS
  case ENOSYS:		return( "ENOSYS" );
#endif
#ifdef	ENOTEMPTY
  case ENOTEMPTY:	return( "ENOTEMPTY" );
#endif
#ifdef	ELOOP
  case ELOOP:		return( "ELOOP" );
#endif
#if defined(EWOULDBLOCK) && EWOULDBLOCK!= EAGAIN
  case EWOULDBLOCK:	return( "EWOULDBLOCK" );
#endif
#ifdef	ENOMSG
  case ENOMSG:		return( "ENOMSG" );
#endif
#ifdef	EIDRM
  case EIDRM:		return( "EIDRM" );
#endif
#ifdef	ECHRNG
  case ECHRNG:		return( "ECHRNG" );
#endif
#ifdef	EL2NSYNC
  case EL2NSYNC:	return( "EL2NSYNC" );
#endif
#ifdef	EL3HLT
  case EL3HLT:		return( "EL3HLT" );
#endif
#ifdef	EL3RST
  case EL3RST:		return( "EL3RST" );
#endif
#ifdef	ELNRNG
  case ELNRNG:		return( "ELNRNG" );
#endif
#ifdef	EUNATCH
  case EUNATCH:		return( "EUNATCH" );
#endif
#ifdef	ENOCSI
  case ENOCSI:		return( "ENOCSI" );
#endif
#ifdef	EL2HLT
  case EL2HLT:		return( "EL2HLT" );
#endif
#ifdef	EBADE
  case EBADE:		return( "EBADE" );
#endif
#ifdef	EBADR
  case EBADR:		return( "EBADR" );
#endif
#ifdef	EXFULL
  case EXFULL:		return( "EXFULL" );
#endif
#ifdef	ENOANO
  case ENOANO:		return( "ENOANO" );
#endif
#ifdef	EBADRQC
  case EBADRQC:		return( "EBADRQC" );
#endif
#ifdef	EBADSLT
  case EBADSLT:		return( "EBADSLT" );
#endif
#if defined(EDEADLOCK) && EDEADLOCK!=EDEADLK
  case EDEADLOCK:	return( "EDEADLOCK" );
#endif
#ifdef	EBFONT
  case EBFONT:		return( "EBFONT" );
#endif
#ifdef	ENOSTR
  case ENOSTR:		return( "ENOSTR" );
#endif
#ifdef	ENODATA
  case ENODATA:		return( "ENODATA" );
#endif
#ifdef	ETIME
  case ETIME:		return( "ETIME" );
#endif
#ifdef	ENOSR
  case ENOSR:		return( "ENOSR" );
#endif
#ifdef	ENONET
  case ENONET:		return( "ENONET" );
#endif
#ifdef	ENOPKG
  case ENOPKG:		return( "ENOPKG" );
#endif
#ifdef	EREMOTE
  case EREMOTE:		return( "EREMOTE" );
#endif
#ifdef	ENOLINK
  case ENOLINK:		return( "ENOLINK" );
#endif
#ifdef	EADV
  case EADV:		return( "EADV" );
#endif
#ifdef	ESRMNT
  case ESRMNT:		return( "ESRMNT" );
#endif
#ifdef	ECOMM
  case ECOMM:		return( "ECOMM" );
#endif
#ifdef	EPROTO
  case EPROTO:		return( "EPROTO" );
#endif
#ifdef	EMULTIHOP
  case EMULTIHOP:	return( "EMULTIHOP" );
#endif
#ifdef	EDOTDOT
  case EDOTDOT:		return( "EDOTDOT" );
#endif
#ifdef	EBADMSG
  case EBADMSG:		return( "EBADMSG" );
#endif
#ifdef	EOVERFLOW
  case EOVERFLOW:	return( "EOVERFLOW" );
#endif
#ifdef	ENOTUNIQ
  case ENOTUNIQ:	return( "ENOTUNIQ" );
#endif
#ifdef	EBADFD
  case EBADFD:		return( "EBADFD" );
#endif
#ifdef	EREMCHG
  case EREMCHG:		return( "EREMCHG" );
#endif
#ifdef	ELIBACC
  case ELIBACC:		return( "ELIBACC" );
#endif
#ifdef	ELIBBAD
  case ELIBBAD:		return( "ELIBBAD" );
#endif
#ifdef	ELIBSCN
  case ELIBSCN:		return( "ELIBSCN" );
#endif
#ifdef	ELIBMAX
  case ELIBMAX:		return( "ELIBMAX" );
#endif
#ifdef	ELIBEXEC
  case ELIBEXEC:	return( "ELIBEXEC" );
#endif
#ifdef	EILSEQ
  case EILSEQ:		return( "EILSEQ" );
#endif
#ifdef	ERESTART
  case ERESTART:	return( "ERESTART" );
#endif
#ifdef	ESTRPIPE
  case ESTRPIPE:	return( "ESTRPIPE" );
#endif
#ifdef	EUSERS
  case EUSERS:		return( "EUSERS" );
#endif
#ifdef	ENOTSOCK
  case ENOTSOCK:	return( "ENOTSOCK" );
#endif
#ifdef	EDESTADDRREQ
  case EDESTADDRREQ:	return( "EDESTADDRREQ" );
#endif
#ifdef	EMSGSIZE
  case EMSGSIZE:	return( "EMSGSIZE" );
#endif
#ifdef	EPROTOTYPE
  case EPROTOTYPE:	return( "EPROTOTYPE" );
#endif
#ifdef	ENOPROTOOPT
  case ENOPROTOOPT:	return( "ENOPROTOOPT" );
#endif
#ifdef	EPROTONOSUPPORT
  case EPROTONOSUPPORT:	return( "EPROTONOSUPPORT" );
#endif
#ifdef	ESOCKTNOSUPPORT
  case ESOCKTNOSUPPORT:	return( "ESOCKTNOSUPPORT" );
#endif
#ifdef	EOPNOTSUPP
  case EOPNOTSUPP:	return( "EOPNOTSUPP" );
#endif
#ifdef	EPFNOSUPPORT
  case EPFNOSUPPORT:	return( "EPFNOSUPPORT" );
#endif
#ifdef	EAFNOSUPPORT
  case EAFNOSUPPORT:	return( "EAFNOSUPPORT" );
#endif
#ifdef	EADDRINUSE
  case EADDRINUSE:	return( "EADDRINUSE" );
#endif
#ifdef	EADDRNOTAVAIL
  case EADDRNOTAVAIL:	return( "EADDRNOTAVAIL" );
#endif
#ifdef	ENETDOWN
  case ENETDOWN:	return( "ENETDOWN" );
#endif
#ifdef	ENETUNREACH
  case ENETUNREACH:	return( "ENETUNREACH" );
#endif
#ifdef	ENETRESET
  case ENETRESET:	return( "ENETRESET" );
#endif
#ifdef	ECONNABORTED
  case ECONNABORTED:	return( "ECONNABORTED" );
#endif
#ifdef	ECONNRESET
  case ECONNRESET:	return( "ECONNRESET" );
#endif
#ifdef	ENOBUFS
  case ENOBUFS:		return( "ENOBUFS" );
#endif
#ifdef	EISCONN
  case EISCONN:		return( "EISCONN" );
#endif
#ifdef	ENOTCONN
  case ENOTCONN:	return( "ENOTCONN" );
#endif
#ifdef	ESHUTDOWN
  case ESHUTDOWN:	return( "ESHUTDOWN" );
#endif
#ifdef	ETOOMANYREFS
  case ETOOMANYREFS:	return( "ETOOMANYREFS" );
#endif
#ifdef	ETIMEDOUT
  case ETIMEDOUT:	return( "ETIMEDOUT" );
#endif
#ifdef	ECONNREFUSED
  case ECONNREFUSED:	return( "ECONNREFUSED" );
#endif
#ifdef	EHOSTDOWN
  case EHOSTDOWN:	return( "EHOSTDOWN" );
#endif
#ifdef	EHOSTUNREACH
  case EHOSTUNREACH:	return( "EHOSTUNREACH" );
#endif
#ifdef	EALREADY
  case EALREADY:	return( "EALREADY" );
#endif
#ifdef	EINPROGRESS
  case EINPROGRESS:	return( "EINPROGRESS" );
#endif
#ifdef	ESTALE
  case ESTALE:		return( "ESTALE" );
#endif
#ifdef	EUCLEAN
  case EUCLEAN:		return( "EUCLEAN" );
#endif
#ifdef	ENOTNAM
  case ENOTNAM:		return( "ENOTNAM" );
#endif
#ifdef	ENAVAIL
  case ENAVAIL:		return( "ENAVAIL" );
#endif
#ifdef	EISNAM
  case EISNAM:		return( "EISNAM" );
#endif
#ifdef	EREMOTEIO
  case EREMOTEIO:	return( "EREMOTEIO" );
#endif
#ifdef	EDQUOT
  case EDQUOT:		return( "EDQUOT" );
#endif
#ifdef	ENOMEDIUM
  case ENOMEDIUM:	return( "ENOMEDIUM" );
#endif
#ifdef	EMEDIUMTYPE
  case EMEDIUMTYPE:	return( "EMEDIUMTYPE" );
#endif
#ifdef	ECANCELED
  case ECANCELED:	return( "ECANCELED" );
#endif
#ifdef	ENOKEY
  case ENOKEY:		return( "ENOKEY" );
#endif
#ifdef	EKEYEXPIRED
  case EKEYEXPIRED:	return( "EKEYEXPIRED" );
#endif
#ifdef	EKEYREVOKED
  case EKEYREVOKED:	return( "EKEYREVOKED" );
#endif
#ifdef	EKEYREJECTED
  case EKEYREJECTED:	return( "EKEYREJECTED" );
#endif
#ifdef	EOWNERDEAD
  case EOWNERDEAD:	return( "EOWNERDEAD" );
#endif
#ifdef	ENOTRECOVERABLE
  case ENOTRECOVERABLE:	return( "ENOTRECOVERABLE" );
#endif
#ifdef ERFKILL
  case ERFKILL:		return( "ERFKILL" );
#endif
#ifdef EHWPOISON
  case EHWPOISON: 	return( "EHWPOISON" );
#endif
  }
  return( "(No such errno)" );
}
