#!/usr/bin/env bash

mkdir -p rpmbuild/{SOURCES,BUILD,RPMS,SPECS}
cp build/oregano.spec rpmbuild/SPECS/
./waf dist
mv oregano*.tar.xz rpmbuild/SOURCES/
cd rpmbuild
rpmbuild \
--define "_topdir %(pwd)" \
--define "_builddir %{_topdir}/BUILD" \
--define "_rpmdir %{_topdir}/RPMS" \
--define "_srcrpmdir %{_topdir}/RPMS" \
--define "_specdir %{_topdir}/SPECS" \
--define "_sourcedir  %{_topdir}/SOURCES" \
-ba SPECS/oregano.spec
