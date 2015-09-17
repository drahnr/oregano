Name:			oregano
Summary:		Schematic capture and simulation of electrical circuits
Version:		0.83.3
Release:		1%{?dist}
License:		GPLv2+
Group:			Applications/Engineering
Source0:		https://github.com/drahnr/oregano/archive/oregano-%{version}.tar.xz
Url:			https://ahoi.io/project/oregano

BuildRequires: gtk3-devel
BuildRequires: libxml2-devel
BuildRequires: gtksourceview3-devel
BuildRequires: intltool
BuildRequires: glib2-devel
BuildRequires: goocanvas2-devel
Requires: gtksourceview3
Requires: gtk3
Requires: glib2 >= 2.24
Requires: goocanvas2
Provides: oregano = %{version}-%{release}
Conflicts: oregano-master

%description
Schematic capture and simulation of electrical circuits utilizing gtk3
as front-end and ng-spice/gnu-cap as back-end

%prep
%setup -q -n oregano

%build
./waf distclean || true
#seems we always need to be able to do debug builds to allow fedora stripping them into a pkg
echo "Building with rpm CFLAGS=\"%{optflags}\""
CFLAGS="%{optflags}" ./waf configure --prefix="%{_prefix}" debug


%install
./waf install --destdir="$RPM_BUILD_ROOT" --prefix="%{_prefix}" --sysconfdir="%{_sysconfdir}"
rm -f "$RPM_BUILD_ROOT/%{_bindir}/microtests"
%find_lang oregano

%post
/usr/bin/update-mime-database %{_datadir}/mime &> /dev/null || :
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database &> /dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi
/usr/bin/update-mime-database %{_datadir}/mime &> /dev/null || :
/usr/bin/update-desktop-database &> /dev/null || :


%posttrans
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :



%clean
rm -rf "$RPM_BUILD_ROOT"





%files -f oregano.lang
%defattr(-, root, root)

%doc HACKING.md README.md AUTHORS COPYING ARCHITECTURE.md docs/oregano-inheritance.dia docs/Library-HOWTO.txt

%{_bindir}/*
#%{_datadir}/gnome/help/oregano/*/*
%{_datadir}/applications/oregano.desktop
%{_datadir}/mime-info/*
%{_datadir}/icons/hicolor/*/*
%{_datadir}/oregano/*/*
%{_datadir}/mime/packages/oregano-mimetypes.xml
%{_datadir}/glib-2.0/schemas/io.ahoi.oregano.gschema.xml

%changelog
* Wed Jul 15 2015 Bernhard Schuster <bernhard@ahoi.io> 0.83.3-1
- Version bump
- Ignore distclean exit codes not equal 0

* Thu Apr 16 2015 Bernhard Schuster <bernhard@ahoi.io> 0.83.2-4
- Minor spec updates

* Thu Dec 25 2014  Bernhard Schuster  <bernhard@ahoi.io> 0.83.2-1
- Bump version to 0.83.2

* Sun Dec 07 2014  Bernhard Schuster  <bernhard@ahoi.io> 0.83.1-3
- Initial RPM release with changelog
