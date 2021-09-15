VPATH=$(srcdir)

include $(top_builddir)/build/config.mk

sbindir = $(default_sbindir)
bindir = $(default_bindir)
libdir = $(default_libdir)
libexecdir = $(default_libexecdir)
includedir = $(default_includedir)
exec_includedir = $(default_exec_includedir)
datadir = $(default_datadir)
datarootdir = $(default_datarootdir)
mandir = $(default_mandir)
docdir = $(default_docdir)
htmldir = $(default_htmldir)
pdfdir = $(default_pdfdir)
sysconfdir = $(default_sysconfdir)
localedir = $(default_localedir)

CC       = $(DEFAULT_CC)
FC       = $(DEFAULT_FC)
CFLAGS   = $(DEFAULT_CFLAGS)
CXXFLAGS = $(DEFAULT_CFLAGS)
FFLAGS   = $(DEFAULT_FFLAGS)
LDFLAGS  = $(malloc_ldflags) $(DEFAULT_LDFLAGS)
LIBS     = $(DEFAULT_LIBS)

INCLUDE_BUILDDIR = $(top_builddir)/include
INCLUDE_SRCDIR   = $(top_srcdir)/include

INSTALL = $(DEFAULT_INSTALL)
INSTALL_PROGRAM = $(DEFAULT_INSTALL_PROGRAM)
INSTALL_SCRIPT  = $(DEFAULT_INSTALL_SCRIPT)
INSTALL_DATA    = $(DEFAULT_INSTALL_DATA)
INSTALL_DATTEST = $(DEFAULT_INSTALL_TEST)

MKDIR_P = mkdir -p
RM = rm -f
CP = cp -f
MV = mv
FC = gfortran

PROGRAMS = $(PROGRAM)
PROGRAMS_TO_INSTALL = $(PROGRAMS)
LIBRARIES = $(LIBRARY)

PIC_CFLAG     = $(CFLAG_PIC)
PIE_CFLAG     = $(CFLAG_PIE)
PIE_LDFLAG    = $(LDFLAG_PIE)
RDYNAMIC_FLAG = -rdynamic
PTHREAD_FLAG  = -pthread
DYLINKER_FLAG = -Wl,--dynamic-linker=$(dynamic_linker)

GLIBC_INCDIR  = $(glibc_incdir)
GLIBC_LIBDIR  = $(glibc_libdir)
GLIBC_LDFLAGS = -L$(glibc_libdir) -Wl,-rpath=$(GLIBC_LIBDIR)

PIP_INCDIR    = $(srcdir_top)/include
PIP_LIBDIR    = $(srcdir_top)/lib
PIP_LIBS      = $(srcdir_top)/lib/libpip.so
PIP_BINDIR    = $(srcdir_top)/bin

PIP_CPPFLAGS  = -I$(PIP_INCDIR) -I$(GLIBC_INCDIR)
PIP_LDFLAGS   = $(malloc_ldflags) -L$(PIP_LIBDIR) -Wl,-rpath=$(PIP_LIBDIR) $(GLIBC_LDFLAGS) \
		-lpip -ldl $(PTHREAD_FLAG)
PIP_LDFLAGS_TO_INSTALL = \
		$(malloc_ldflags) -L$(top_builddir)/lib -Wl,-rpath=$(libdir) $(GLIBC_LDFLAGS) \
		-lpip -ldl $(PTHREAD_FLAG)

PIP_CFLAGS_ROOT  = $(PIP_CPPFLAGS) $(PTHREAD_FLAG)
PIP_LDFLAGS_ROOT = $(PIP_LDFLAGS) $(DYLINKER_FLAG) $(PTHREAD_FLAG)
PIP_LDFLAGS_ROOT_TO_INSTALL = $(PIP_LDFLAGS_TO_INSTALL) $(DYLINKER_FLAG) $(PTHREAD_FLAG)

PIP_CFLAGS_TASK  = $(PIP_CPPFLAGS) $(PIC_CFLAG)
PIP_LDFLAGS_TASK = $(PIP_LDFLAGS) $(RDYNAMIC_FLAG) $(PTHREAD_FLAG) $(PIE_LDFLAG)
PIP_LDFLAGS_TASK_TO_INSTALL = $(PIP_LDFLAGS_TO_INSTALL) $(RDYNAMIC_FLAG) $(PTHREAD_FLAG) $(PIE_LDFLAG)

PIP_CFLAGS_BOTH  = $(PIP_CPPFLAGS) $(PIC_CFLAG) $(PTHREAD_FLAG)
PIP_LDFLAGS_BOTH = $(PIP_LDFLAGS) $(RDYNAMIC_FLAG) $(DYLINKER_FLAG) $(PTHREAD_FLAG) $(PIE_LDFLAG)
PIP_LDFLAGS_BOTH_TO_INSTALL = $(PIP_LDFLAGS_TO_INSTALL) $(RDYNAMIC_FLAG) $(DYLINKER_FLAG) \
	$(PTHREAD_FLAG) $(PIE_LDFLAG)

PIPCC = $(PIP_BINDIR)/pipcc

DEPINCS = $(PIP_INCDIR)/pip/pip_config.h		\
	  $(PIP_INCDIR)/pip/pip.h			\
	  $(PIP_INCDIR)/pip/pip_internal.h		\
	  $(PIP_INCDIR)/pip/pip_clone.h			\
	  $(PIP_INCDIR)/pip/pip_util.h			\
	  $(PIP_INCDIR)/pip/pip_list.h 			\
	  $(PIP_INCDIR)/pip/pip_debug.h			\
	  $(PIP_INCDIR)/pip/pip_machdep.h 		\
	  $(PIP_INCDIR)/pip/pip_machdep_aarch64.h 	\
	  $(PIP_INCDIR)/pip/pip_machdep_x86_64.h 	\
	  $(PIP_INCDIR)/pip/pip_gdbif.h			\
	  $(PIP_INCDIR)/pip/pip_gdbif_func.h		\
	  $(PIP_INCDIR)/pip/pip_gdbif_queue.h		\
	  $(PIP_INCDIR)/pip/pip_gdbif_enums.h		\
	  $(PIP_INCDIR)/pip/xpmem.h

DEPLIBS = $(PIP_LIBDIR)/libpip.so

DEPS = $(DEPINCS) $(DEPLIBS)
