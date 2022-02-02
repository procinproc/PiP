
# $PIP_license: <Simplified BSD License>
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
# 
#     Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
# $
# $RIKEN_copyright: Riken Center for Computational Sceience (R-CCS),
# System Software Development Team, 2016-2021
# $
# $PIP_VERSION: Version 2.3.0$
#
# $Author: Atsushi Hori (R-CCS)
# Query:   procinproc-info@googlegroups.com
# User ML: procinproc-users@googlegroups.com
# $

top_builddir = ..
top_srcdir = $(top_builddir)
srcdir = .

include $(top_srcdir)/build/var.mk

SO_MAJOR = 0
LIBNAME  = libpip.so
LIBRARY  = $(LIBNAME).$(SO_MAJOR)
LDFLAGS_PIP = -shared -Wl,-soname -Wl,$(LIBRARY) $(PTHREAD_FLAG)

LIBRARY_PRELOAD = pip_preload.so
LDFLAGS_PRELOAD = -shared -ldl

SO_MAJOR_XPMEM = 0
LIBNAME_XPMEM = libxpmem.so
LIBRARY_XPMEM = $(LIBNAME_XPMEM).$(SO_MAJOR_XPMEM)

LIBRARIES = $(LIBRARY) $(LIBRARY_PRELOAD) $(LIBRARY_XPMEM)

CPPFLAGS += $(PIP_CPPFLAGS) \
	-DPIP_INSTALL_GLIBCDIR=\"$(glibc_libdir)\" \
	-DLDLINUX=\"$(dynamic_linker)\" \
	$(CPPFLAGS_DEBUG)
CFLAGS   += $(PIC_CFLAG) $(PTHREAD_FLAG)

LIBMAIN = -Wl,-e,pip_main -ldl

SRCS  = pip_init.c pip_main.c pip.c pip_2_backport.c pip_wait.c \
	pip_namexp.c pip_clone.c pip_signal.c pip_util.c \
	pip_preload.c pip_elf.c pip_wrapper.c pip_malloc.c \
	pip_onstart.c pip_gdbif.c \
	xpmem.c

OBJS  = pip_init.o pip_main.o pip.o pip_2_backport.o pip_wait.o \
	pip_namexp.o pip_clone.o pip_signal.o pip_util.o \
	pip_elf.o pip_onstart.o pip_gdbif.o pip_wrapper.o pip_malloc.o

OBJS_PRELOAD = pip_preload.o
OBJS_XPMEM   = xpmem.o

include $(top_srcdir)/build/rule.mk

BUILD_H = $(top_srcdir)/include/pip/build.h

$(BUILD_H): $(top_srcdir)/include/pip/pip_config.h
	$(MAKE) -C ../include/pip header

$(OBJS): $(BUILD_H) $(DEPINCS) 

$(LIBRARY): $(OBJS) Makefile
	$(CC) $(LDFLAGS_PIP) $(LIBMAIN) $(OBJS) -o $@
	$(RM) $(LIBNAME)
	ln -s $(LIBRARY) $(LIBNAME)

$(LIBRARY_PRELOAD): $(OBJS_PRELOAD) $(DEPINCS) Makefile
	$(CC) $(LDFLAGS_PRELOAD) $(OBJS_PRELOAD) -o $@

$(LIBRARY_XPMEM): $(OBJS_XPMEM) $(DEPINCS) Makefile
	$(CC) $(OBJS_XPMEM) -shared -o $@
	$(RM) $(LIBNAME_XPMEM)
	ln -s $(LIBRARY_XPMEM) $(LIBNAME_XPMEM)

# chmod is to make rpmbuild happy
install:
	chmod 0755 $(DESTDIR)$(libdir)/$(LIBRARY) \
		$(DESTDIR)$(libdir)/$(LIBRARY_PRELOAD) \
		$(DESTDIR)$(libdir)/$(LIBRARY_XPMEM)
	$(RM) $(DESTDIR)$(libdir)/$(LIBNAME) \
		$(DESTDIR)$(libdir)/$(LIBNAME_XPMEM)
	ln -s $(LIBRARY) $(DESTDIR)$(libdir)/$(LIBNAME)
	ln -s $(LIBRARY_XPMEM) $(DESTDIR)$(libdir)/$(LIBNAME_XPMEM)

debug:
	CPPFLAGS="-DDEBUG" $(MAKE) all;

cdebug: clean
	CPPFLAGS="-DDEBUG" $(MAKE) all;

TAGS:
	ctags -Re

post-clean-hook:
	$(RM) $(LIBRARIES) $(LIBNAME) $(LIBNAME_XPMEM) $(OBJS_PIPINIT)
