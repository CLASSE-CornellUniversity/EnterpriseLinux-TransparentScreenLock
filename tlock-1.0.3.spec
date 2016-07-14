Name: tlock
Summary: Transparent Lock for Linux
Version: 1.0.2
Release: 1%{?dist}
License: GPLv2+
ExclusiveOS: linux

Group: X11 Utilities
URL: http://bitbucket.org/akaan/tlock
Source: tlock-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc

%description
The "Transparent Lock" program...

%prep
%setup

%build
./configure --prefix=/usr/local
make all 

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

install -m 755 -d $RPM_BUILD_ROOT/%{_prefix}/local/bin
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/X11/xinit/xinitrc.d
install -m 755 bin/60tlock.sh $RPM_BUILD_ROOT/%{_sysconfdir}/X11/xinit/xinitrc.d/

%files
%defattr(755,root,root,755)
%doc
# Don't own /srv/, but own directories:
%dir /usr/
%dir /usr/local/
%dir /usr/local/bin/
%dir /etc/
%dir /etc/X11/
%dir /etc/X11/xinit/
%dir /etc/X11/xinit/xinitrc.d/
/usr/local/bin
/etc/X11/xinit/xinitrc.d

%clean
rm -rf %{buildroot}


%post

%changelog
* Sun May 4 2014 Andre Kaan <visitbethel@gmail.com> - 1.0.1
- added distribution of the /etc/X11/xinit/xinitrc.d/60tlock.sh

* Mon Apr 28 2014 Andre Kaan <visitbethel@gmail.com> - 1.0.0
- Initial version of the package distribution by means of RPM.


