#!/bin/sh

set -x
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-Linux-*
yum -y install rpmdevtools &&
rpmdev-setuptree &&
rpm -Uvh $RPM_SRPM &&
time env QA_RPATHS=3 rpmbuild -bb $HOME/rpmbuild/SPECS/$RPM_SPEC &&
mkdir -p $RPM_RESULTS &&
cp -R $HOME/rpmbuild/RPMS/* $RPM_RESULTS/
