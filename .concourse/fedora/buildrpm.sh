#!/usr/bin/env bash

set -e
set -x

pwd 2>&1
RPMBUILD_DIR="$(pwd)/../rpmbuild/"
mkdir -p ${RPMBUILD_DIR}/{SOURCES,BUILD,RPMS,SRPMS,SPECS}

./waf configure rpmspec
cp -v build/rpmspec/oregano.spec ${RPMBUILD_DIR}/SPECS/
./waf dist
cp -v oregano*.tar.xz ${RPMBUILD_DIR}/SOURCES/

cd ${RPMBUILD_DIR}
rpmbuild \
--define "_topdir %(pwd)" \
--define "_builddir %{_topdir}/BUILD" \
--define "_rpmdir %{_topdir}/RPMS" \
--define "_srcrpmdir %{_topdir}/SRPMS" \
--define "_specdir %{_topdir}/SPECS" \
--define "_sourcedir  %{_topdir}/SOURCES" \
-ba SPECS/oregano.spec && echo "RPM was built"
pwd 2>&1
