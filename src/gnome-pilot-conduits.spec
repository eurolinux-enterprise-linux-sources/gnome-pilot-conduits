#
# Note that this is NOT a relocatable package
# $Id: gnome-pilot-conduits.spec.in,v 1.10 2001/09/17 04:36:40 eskil Exp $
#
%define ver      2.0.17
%define rel      1
%define prefix   /usr/local
%define name	 gnome-pilot-conduits
%define epoch	 0

Summary: gnome-pilot conduits
Name: %name
Version: %ver
Release: %rel
Epoch: %epoch
Copyright: LGPL
Group: Applications/Communications
Source: http://eskil.org/gnome-pilot/download/%name-%ver.tar.gz
BuildRoot: /var/tmp/gnome-pilot-conduits
Packager: Eskil Heyn Olsen <eskil@eskil.dk>
URL: http://eskil.org/gnome-pilot
Prereq: /sbin/install-info
Prefix: %{prefix}
Docdir: %{prefix}/doc
Requires: gnome-pilot >= 0.1.62

%description
gnome-pilot is a collection of programs and daemon for integrating
GNOME and the PalmPilot<tm>.

This is a collection of additional conduits for gnome-pilot, it
currently features
 - MAL conduit

%changelog

* Sun Mar 5 2000 Eskil Heyn Olsen <deity@eskil.dk>

- redid the package from mal-conduit to gnome-pilot-conduits

* Sun Dec 5 1999 Eskil Heyn Olsen <deity@eskil.dk>

- Created the .spec file

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT
make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/share/gnome-pilot/conduits/*.conduit
%{prefix}/lib/gnome-pilot/conduits/*.so

