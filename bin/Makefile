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
# System Software Development Team, 2016-2022
# $
# $PIP_VERSION: Version 2.4.1$
#
# $Author: Atsushi Hori 
# Query:   procinproc-info@googlegroups.com
# User ML: procinproc-users@googlegroups.com
# $

top_builddir = ..
top_srcdir = $(top_builddir)
srcdir = .

include $(top_srcdir)/build/var.mk

PROGRAM = pip-exec
PROGRAMS = $(PROGRAM) printpipmode pip-unpie pipcc pipfc pips pip-tgkill
PROGRAMS_TO_INSTALL = $(PROGRAMS) pips pip-mode pip-check pip-man 
EXTRA_PROGRAMS = pip-exec.to-install printpipmode.to-install

MAN1_CHEAT_DOXYGEN = pipcc.py pipfc.py pip-mode.py pip-check.py pip-man.py
MAN1C_SRCS = pip-exec.c printpipmode.c pip-tgkill.c
MAN1P_SRCS = pips.py $(MAN1_CHEAT_DOXYGEN)

CPPFLAGS += $(PIP_CPPFLAGS) $(DBGFLAG)

all: $(EXTRA_PROGRAMS) FORCE

debug:
	DBGFLAG=-DDEBUG $(MAKE) all

include $(top_srcdir)/build/rule.mk

FORCE:
	chmod +x pip-check pip-mode pip-man
.PHONY: FORCE

pip-exec: pip-exec.c $(DEPS) Makefile
	$(CC) -g -O2 $(CPPFLAGS) $(PIP_CFLAGS_ROOT) $(PIP_LDFLAGS_ROOT) $< -o $@

pip-exec.to-install: pip-exec.c $(DEPS) Makefile
	$(CC) -g -O2 $(CPPFLAGS) $(PIP_CFLAGS_ROOT) $(PIP_LDFLAGS_ROOT_TO_INSTALL) $< -o $@

printpipmode: printpipmode.c $(DEPS) Makefile
	$(CC) -g -O2 $(CPPFLAGS) $(PIP_CFLAGS_BOTH) $(PIP_LDFLAGS_BOTH) $< -o $@

printpipmode.to-install: printpipmode.c $(DEPS) Makefile
	$(CC) -g -O2 $(CPPFLAGS) $(PIP_CFLAGS_BOTH) $(PIP_LDFLAGS_BOTH_TO_INSTALL) $< -o $@

install: $(PROGRAMS_TO_INSTALL)
	$(MKDIR_P) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) pip-exec.to-install $(DESTDIR)$(bindir)/pip-exec
	$(INSTALL_PROGRAM) printpipmode.to-install $(DESTDIR)$(bindir)/printpipmode

pipcc: pipcc.in
	sed -e s/\=LANG\=/C/g -e s/\=PROG\=/pipcc/g -e s/\=CC\=/CC/g < pipcc.in > pipcc
	chmod +x pipcc

pipfc: pipcc.in
	sed -e s/\=LANG\=/Fortran/g -e s/\=PROG\=/pipfc/g -e s/\=CC\=/FC/g < pipcc.in > pipfc
	chmod +x pipfc

pips: pips.py
	$(CP) $< $@
	chmod +x $@

pip-unpie: pip-unpie.c
	$(CC) -g -O2 -Wall $< -o $@

pip-tgkill: pip-tgkill.c
	$(CC) -g -O2 -Wall $< -o $@

## Cheating DOXYGEN

pipcc.py: pipcc
	if ! [ -f $@ ]; then ln -s $< $@; fi

pipfc.py: pipfc
	if ! [ -f $@ ]; then ln -s $< $@; fi

pip-mode.py: pip-mode
	if ! [ -f $@ ]; then ln -s $< $@; fi

pip-check.py: pip-check
	if ! [ -f $@ ]; then ln -s $< $@; fi

pip-man.py: pip-man
	if ! [ -f $@ ]; then ln -s $< $@; fi

## Clean

post-clean-hook:
	$(RM) *.E *.log
	for sl in $(MAN1_CHEAT_DOXYGEN); do unlink $$sl; done

post-veryclean-hook: post-clean-hook
	$(RM) pipcc pipfc pip-mode pip-check pip-man pips $(EXTRA_PROGRAMS)
	$(RM) .doxygen_*

post-distclean-hook: post-veryclean-hook
