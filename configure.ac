#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Process-in-Processs],
	[2.4.0],
	[procinproc-info@googlegroups.com],
	[PiP],
	[https://github.com/procinproc/PiP/])
AC_CONFIG_SRCDIR([RELEASE_NOTE])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADERS([include/pip/pip_config.h])

# Specialized system macros
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

case "$target_os" in
     linux*) true;;
     *) AC_MSG_ERROR([Unsupported OS]);;
esac

case "$target" in
      x86_64-*)  true;;
      aarch64-*) true;;
      *) AC_MSG_ERROR([Unsupported target CPU architecture]);;
esac;

AC_SUBST(PACKAGE_VERSION)

# srcdir_top

cwd=`dirname $0`;
srcdir_top=`realpath $cwd`;
AC_SUBST(srcdir_top)

# Checks for command line options

AC_MSG_CHECKING([for glibc libdir])
AC_ARG_WITH([glibc-libdir],
  [AS_HELP_STRING([--with-glibc-libdir=DIR],
                  [glibc lib directory @<:@default=/usr/lib64@:>@])])
case "${with_glibc_libdir}" in
""|yes|no) have_pipglibc=false; with_glibc_libdir="/usr/lib64";;
*) have_pipglibc=true;;
esac
dynamic_linker=`ls -d "${with_glibc_libdir}"/ld-@<:@0-9@:>@*.so | sed -n '$p'`
glibc_lib=`ls -d "${with_glibc_libdir}"/libc-@<:@0-9@:>@*.so | sed -n '$p'`
if test -d "${with_glibc_libdir}" -a -x "${dynamic_linker}" -a -f "${glibc_lib}"; then
  AC_MSG_RESULT([${with_glibc_libdir}])
  glibc_libdir="${with_glibc_libdir}"
  glibc_incdir="`realpath ${with_glibc_libdir}/../include`"
else
  AC_MSG_ERROR([can't find ld.so.])
fi
if ! test -d "${glibc_incdir}" ; then
  glibc_incdir="/usr/include"
fi
AC_MSG_RESULT([${glibc_incdir}])

AC_ARG_ENABLE([pipwrapper],
  [AS_HELP_STRING([--enable-pipwrapper],
                  [allow PiP lib. to wrap some glibc functions (default)])])
case "${enable_pipwrapper}" in
""|yes) pip_wrapper=1;;
*) pip_wrapper=0;;
esac

AC_MSG_CHECKING([malloc library])
AC_ARG_WITH([malloc],
  [AS_HELP_STRING([--with-malloc=PATH_TO_MALLOC.SO],
                  [malloc library path (optional)])])
case "${with_malloc}" in
""|yes|no) have_malloc=false;;
*) if test -x ${with_malloc}; then
      real_malloc_so="`readlink -f ${with_malloc} 2> /dev/null`"
      if nm ${real_malloc_so} | grep "T malloc" > /dev/null 2>&1;
      then have_malloc=true; else have_malloc=false; fi;
   else
      AC_MSG_ERROR([can't find ${with_malloc}])
   fi;;
esac
if ${have_malloc}; then
   AC_MSG_RESULT([${with_malloc}])
   malloc_libdir="`dirname ${with_malloc}`"
   malloc_libname="`basename ${with_malloc} | sed 's/lib\(.*\)\.so/\1/'`"
   malloc_ldflags="-L${malloc_libdir} -Wl,-rpath=${malloc_libdir} -l${malloc_libname}"
else
  malloc_ldflags=""
fi

AC_SUBST(prefix)
AC_SUBST(exec_prefix)
AC_SUBST(includedir)
AC_SUBST(libdir)
AC_SUBST(bindir)
AC_SUBST(have_pipglibc)
AC_SUBST(glibc_incdir)
AC_SUBST(glibc_libdir)
AC_SUBST(glibc_lib)
AC_SUBST(dynamic_linker)
AC_SUBST(malloc_ldflags)
AC_SUBST(pip_wrapper)

getconf=`type -P getconf`
if test -x $getconf; then
   cache_line_size=`$getconf LEVEL1_DCACHE_LINESIZE`
   AC_DEFINE_UNQUOTED([CACHE_LINE_SIZE], [${cache_line_size}], ["Cache Line Size"])
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

if test "$GCC" = yes; then
  CFLAGS="$CFLAGS -Wall"
fi
AC_SUBST(CC)
AC_SUBST(CFLAGS)

AC_ARG_VAR([FC], [Fortran compiler command])
AC_CHECK_PROGS([FC],[gfortran g95 g77])
AC_SUBST(FC)

AC_CHECK_PROGS([PYTHON],[python3 python36 python38 python2 python],[false])
if test x"${PYTHON}" == x"false" ; then
  AC_MSG_ERROR([can't find python.])
fi
python=`which ${PYTHON}`
AC_SUBST(python)

AC_ARG_VAR([CFLAG_PIC], [PIC compiler flag (default -fPIC)])
if test x"${CFLAG_PIC}" = "xno" ; then
   CFLAG_PIC=""
elif test x"${CFLAG_PIC}" = x ; then
   CFLAG_PIC="-fPIC"
else
   CFLAG_PIC="${CFLAG_PIC}"
fi
AC_SUBST(CFLAG_PIC)

AC_ARG_VAR([CFLAG_PIE], [PIE compiler flag (default -fpie)])
if test x"${CFLAG_PIE}" = "xno" ; then
   CFLAG_PIE=""
elif test x"${CFLAG_PIE}" = x ; then
   CFLAG_PIE="-fpie"
else
   CFLAG_PIE="${CFLAG_PIE}"
fi
AC_SUBST(CFLAG_PIE)

AC_ARG_VAR([LDFLAG_PIE], [PIE linker flag (default -pie)])
if test x"${LDFLAG_PIE}" = "xno" ; then
   LDFLAG_PIE=""
elif test x"${LDFLAG_PIE}" = x ; then
   LDFLAG_PIE="-pie"
else
   LDFLAG_PIE="${LDFLAG_PIE}"
fi
AC_SUBST(LDFLAG_PIE)

CFLAGS_SAVE=${CFLAGS}
LDFLAGS=${LDFLAGS_SAVE}

CFLAGS="${CFLAG_PIC} ${CFLAGS}"

AC_MSG_CHECKING([PIC compile flag (${CFLAGS})])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE([[main(){return 0;}]])],
[AC_MSG_RESULT([OK])],
[AC_MSG_ERROR([${CFLAG_PIC} is invalid])])

CFLAGS="${CFLAG_PIE} ${CFLAGS}"

AC_MSG_CHECKING([PIE compile flag (${CFLAGS})])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE([[main(){return 0;}]])],
[AC_MSG_RESULT([OK])],
[AC_MSG_ERROR([${CFLAG_PIE} is invalid])])

LDFLAGS="${LDFLAG_PIE} ${LDFLAGS}"

AC_MSG_CHECKING([PIE linker flag (${LDFLAGS})])
AC_LINK_IFELSE(
[AC_LANG_SOURCE([[main(){return 0;}]])],
[AC_MSG_RESULT([OK])],
[AC_MSG_ERROR([${LDFLAG_PIE} is invalid])])

CFLAGS="${CFLAGS_SAVE}"
LDFLAGS="${LDFLAGS_SAVE}"

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
	build/config.mk
	release/version.conf
	package/redhat/process-in-process.spec
	bin/pipcc.in
	bin/pips
	bin/pip-mode
	bin/pip-check
	bin/pip-man
	doc/latex-inuse/libpip-manpages.tex
])
AC_OUTPUT
