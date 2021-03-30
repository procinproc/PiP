#!/bin/sh

set -x
yum -y install rpmdevtools &&
rpmdev-setuptree &&
cp $RPM_SOURCE $HOME/rpmbuild/SOURCES/ &&
rpmbuild -bs $RPM_SPEC &&
mkdir -p $RPM_RESULTS &&
cp $HOME/rpmbuild/SRPMS/* $RPM_RESULTS/
