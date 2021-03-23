#!/bin/sh

set -x

cd /host/PiP &&
./configure --prefix=$HOME/pip --with-glibc-libdir=/opt/process-in-process/pip-glibc/lib &&
time make install &&

cd /host/PiP-Testsuite &&
./configure --with-pip=$HOME/pip &&
time make test
