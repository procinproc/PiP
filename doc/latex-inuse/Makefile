
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

LIBPIP = libpip-manpages

pdf: all

all :
	cp ../latex/*.sty .
	./man-pages.sh
	-pdflatex $(LIBPIP)
	makeindex $(LIBPIP).idx
	-pdflatex $(LIBPIP)
	rerun_count=10 ; \
	while egrep -s 'Rerun (LaTeX|to get cross-references right)' $(LIBPIP).log; \
	    do \
	      if [ $${rerun_count} -eq 0 ]; then echo 'Something goes wrong'; exit 1; fi; \
	      echo "Rerunning latex .... ($${rerun_count})" ;\
	      rm -f $(LIBPIP).log; \
	      pdflatex $(LIBPIP) > /dev/null 2>&1 ;\
	      rerun_count=`expr $${rerun_count} - 1` ;\
	    done
	makeindex $(LIBPIP).idx > /dev/null 2>&1
	-pdflatex $(LIBPIP) > /dev/null 2>&1
	echo "Succeeded"

clean:
	$(RM) -f *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out *.brf *.blg *.bbl
	$(RM) -f *-inputs.tex *.sty
	$(RM) -f \#*\# *~

veryclean: clean
	$(RM) libpip-manpages.tex
