#!/usr/bin/env bash

set -e

mkdir -p rpmbuild/{SOURCES,BUILD,RPMS,SPECS}
./waf configure rpmspec
cp -v build/rpmspec/oregano.spec rpmbuild/SPECS/
./waf dist
cp -v oregano*.tar.xz rpmbuild/SOURCES/
cd rpmbuild
rpmbuild \
--define "_topdir %(pwd)" \
--define "_builddir %{_topdir}/BUILD" \
--define "_rpmdir %{_topdir}/RPMS" \
--define "_srcrpmdir %{_topdir}/RPMS" \
--define "_specdir %{_topdir}/SPECS" \
--define "_sourcedir  %{_topdir}/SOURCES" \
-ba SPECS/oregano.spec && echo "RPM was built"
