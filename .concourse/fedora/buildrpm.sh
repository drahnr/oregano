#!/usr/bin/env bash

set -x
set -e

pwd 2>&1
RPMBUILD_DIR="$(pwd)/${1}/rpmbuild"

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
-ba SPECS/oregano.spec || exit 1

mkdir -p $(pwd)/${1}/{,s}rpm/
rm -vf ${RPMBUILD_DIR}/RPMS/x86_64/oregano-*debug*.rpm
cp -vf ${RPMBUILD_DIR}/RPMS/x86_64/oregano-*.rpm $(pwd)/${1}/rpm/
cp -vf ${RPMBUILD_DIR}/SRPMS/oregano-*.src.rpm $(pwd)/${1}/srpm/
