#!/bin/sh

set -x

cd /host/PiP &&
./configure --prefix=$HOME/pip --with-glibc-libdir=/opt/process-in-process/pip-glibc/lib &&
time make install &&

cd /host/PiP-Testsuite &&
./configure --with-pip=$HOME/pip &&
PIP_TEST_THRESHOLD=10 make test
#:
### FIXME:
### the above line should be "time make test" instead of ":", but it hangs
