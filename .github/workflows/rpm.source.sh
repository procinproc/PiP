#!/bin/sh

set -x
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-Linux-*
yum -y install rpmdevtools &&
rpmdev-setuptree &&
cp $RPM_SOURCE $HOME/rpmbuild/SOURCES/ &&
rpmbuild -bs $RPM_SPEC &&
mkdir -p $RPM_RESULTS &&
cp $HOME/rpmbuild/SRPMS/* $RPM_RESULTS/
