#!/bin/sh

set -x

cd /host/PiP &&
./configure --prefix=$HOME/pip --with-glibc-libdir=/opt/process-in-process/pip-glibc/lib &&
time make install &&

cd /host/PiP-Testsuite &&
./configure --with-pip=$HOME/pip &&
if true # <- change this to false to disable "make test"
then
	PIP_TEST_THRESHOLD=10; export PIP_TEST_THRESHOLD
	case `uname -p` in
	aarch64)	PIP_TEST_THRESHOLD=50;; # emulated, 10 times slower
	*)		PIP_TEST_THRESHOLD=5;;
	esac
	export PIP_TEST_THRESHOLD
	time make test
fi
