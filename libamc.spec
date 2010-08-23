Summary: AMC Drive interface library
Name: libamc
Version: 0.1.0
Release: 1
License: LGPL V3+
Packager: Jim George
URL: http://www.chill.colostate.edu
Group: Applications/System 
Provides: libamc=0.1.0
Requires: ,/bin/sh

Source0: libamc-0.1.0.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: autoconf, automake

%description
AMC DPR Drive interface library

%prep
%setup -q

autoreconf

%build
%configure 

make


%install
rm -rf $RPM_BUILD_ROOT
mkdir -p -m755 $RPM_BUILD_ROOT/
make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p -m755 $RPM_BUILD_ROOT/usr/share/libamc/
ls -lRh $RPM_BUILD_ROOT/


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%attr(0755,root,root) %dir %{_libdir}
%attr(0755,root,root) %dir %{_libdir}/pkgconfig
%attr(0755,root,root) %dir %{_includedir}
%attr(0755,root,root) %dir %{_includedir}/amc/
%dir %{_libdir}/libamc.so.1
%dir %{_libdir}/libamc.so
%attr(0755,root,root) %{_libdir}/libamc.so.0.1.0
%attr(0755,root,root) %{_libdir}/libamc.la
%attr(0644,root,root) %{_libdir}/pkgconfig/amc.pc
%attr(0644,root,root) %{_includedir}/amc/amc.h
%doc AUTHORS ChangeLog INSTALL NEWS COPYING* README


%changelog
* Mon Aug 23 2010 Jim George <jgeorge@engr.colostate.edu> - 0.1.0
- Initial creation of library
