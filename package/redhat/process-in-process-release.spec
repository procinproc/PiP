%define gpgcheck	1

Name: process-in-process-release
Version: 1
Release: 0

Summary: PiP (Process in Process) release yum repository settings
Group: System Environment/Libraries
License: BSD
URL: https://github.com/procinproc/PiP/
Vendor: PiP Development Team
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

Source0: process-in-process.repo
%if %{gpgcheck}
Source1: RPM-GPG-KEY-Process-in-Process
%endif

BuildArch: noarch

%description
Yum repository settings for PiP - Process in Process

%prep

%build

%install
rm -rf $RPM_BUILD_ROOT

# process-in-process.repo
install -dm 755 $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d
install -pm 644 %{SOURCE0} $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d/

# RPM-GPG-KEY-Process-in-Process
%if %{gpgcheck}
install -dm 755 $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg
install -pm 644 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg/
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config(noreplace) /etc/yum.repos.d/*
%if %{gpgcheck}
/etc/pki/rpm-gpg/*
%endif

%if %{gpgcheck}
%post
rpm --import /etc/pki/rpm-gpg/%{SOURCE1}
%endif
