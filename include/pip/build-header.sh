#!/bin/sh

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

ofile=$1
cc=$2
tfile="${ofile}.tmp"

commit_hash=`git rev-parse HEAD`
build_os=`uname -s -r -v`

if [ "x$cc" == "x" ]; then
    cc=`type -P gcc 2> /dev/null`
else
    cc=`type -P ${cc} 2> /dev/null`
fi
if [ $? != 0 ]; then
    echo >&2 "Unable to find a C compiler"
    exit 1
fi
build_cc=`${cc} --version 2>&1 | head -n 1`
if [ x"${build_cc}" == x ]; then
    build_cc=${cc}
fi

cat > $tfile << EOF
/*** DO NOT EDIT THIS FILE ***/

#define COMMIT_HASH "${commit_hash}"
#define BUILD_OS "${build_os}"
#define BUILD_CC "${build_cc}"
EOF

if [ -f $ofile ]; then
    if diff $ofile $tfile > /dev/null 2>&1; then
	rm -f $tfile
    else
	mv -f $tfile $ofile
    fi
else
    mv -f $tfile $ofile
fi
