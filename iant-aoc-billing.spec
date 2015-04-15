Name: iant-aoc-billing
Version: 14.10
Release: 14117.0001

Summary: iant-aoc-billing
License: Commercial
Group: Telcommunications
Vendor: IANT GmbH
Packager: IANT GmbH <info@iant.de>

Requires: sipxproxy >= %version

BuildRequires: automake
BuildRequires: libtool
BuildRequires: gcc-c++
BuildRequires: sipxconfig >= %version
BuildRequires: sipxproxy-devel >= %version
BuildRequires: libxml2
BuildRequires: java-1.7.0-openjdk-devel

Requires: boost
Requires: java-1.7.0-openjdk
Requires: sipxconfig >= %version
Requires: sipxproxy >= %version

Source: %name-%version.tar.gz

Prefix: %_prefix
BuildRoot: %{_tmppath}/%name-%version-root

%description
Collects Billing Information (AOC) SIP Messages send from Patton

%package config
Requires : sipxconfig >= %version
Group: Telecommunications
Vendor: IANT GmbH
Summary: TDB

%description config
Configuration UI for managing customer by called number

%package devel
Requires : %name
Group: Development/Libraries
Vendor:  IANT GmbH
Summary: Development libraries for iant-aoc-billing

%description devel
This is just sample text for the development portion of your project. Change this
text to describe the development portion of your project in more detail.

%prep
%setup -q

%build
%configure --enable-rpmbuild SIPXPBXUSER=sipx
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%{_libdir}/transactionplugins/libsipxiantaocbilling.so*
%{_datadir}/java/sipXecs/sipXconfig/plugins/iantaocbilling-config.jar
%{_sysconfdir}/sipxpbx/iantaocbilling/iantaocbilling.xml
%{_sysconfdir}/sipxpbx/iantaocbilling/iantaocbilling.properties

%files devel
%{_libdir}/transactionplugins/libsipxiantaocbilling.la
