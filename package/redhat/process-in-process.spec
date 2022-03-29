# How to build this RPM:
#
#	env QA_RPATHS=3 rpmbuild -bb process-in-process.spec
#

%define base_prefix		/opt/process-in-process
%define _prefix			%{base_prefix}/pip-2

%define glibc_libdir		%{base_prefix}/pip-glibc/lib
%define libpip_version		0
%define libxpmem_version	0
%define pip_tarname		PiP

# workaround for "error: Empty %files file ..../debugsourcefiles.list"
# on wallaby2 (this doesn't happen on jupiter05 for some reason)
%global debug_package		%{nil}

Name: process-in-process
Version: 2.4.0
Release: 0%{?dist}
Epoch: 1
Source: %{pip_tarname}-%{version}.tar.gz

Summary: PiP - Process in Process
Group: System Environment/Libraries
License: BSD
URL: https://github.com/procinproc/PiP/
Vendor: PiP Development Team
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
BuildRequires: pip-glibc

%description
PiP is a user-level library to have the best of the both worlds of
multi-process and multi-thread parallel execution models.
PiP allows a process to create sub-processes into the same virtual
address space where the parent process runs.
The parent process and sub-processes share the same address space,
however, each process has its own variable set.  So, each process runs
independently from the other process. If some or all processes agree,
then data own by a process can be accessed by the other processes.
Those processes share the same address space, just like pthreads, and
each process has its own variables like a process. The parent process
is called PiP process and a sub-process are called a PiP task.

%prep

%setup -n %{pip_tarname}-%{version}

%build
%configure --with-glibc-libdir=%{glibc_libdir}
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make DESTDIR="$RPM_BUILD_ROOT" install
make DESTDIR="$RPM_BUILD_ROOT" doc

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(0755,root,root) %{_bindir}/pipcc
%attr(0755,root,root) %{_bindir}/pipfc
%attr(0755,root,root) %{_bindir}/pip-check
%attr(0755,root,root) %{_bindir}/pip-exec
%attr(0755,root,root) %{_bindir}/pip-man
%attr(0755,root,root) %{_bindir}/pip-mode
%attr(0755,root,root) %{_bindir}/pip-tgkill
%attr(0755,root,root) %{_bindir}/pips
%attr(0755,root,root) %{_bindir}/pip-unpie
%attr(0755,root,root) %{_bindir}/printpipmode
# libs
%defattr(-,root,root)
%attr(0755,root,root) %{_libdir}/libpip.so.%{libpip_version}
%attr(0755,root,root) %{_libdir}/ldpip.so
%attr(0755,root,root) %{_libdir}/libxpmem.so.%{libxpmem_version}
# devel
%{_prefix}/include
%{_libdir}/ldpip.so
%{_libdir}/libpip.so
%{_libdir}/libxpmem.so
#doc
%{_mandir}
%{_docdir}
