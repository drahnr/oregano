#!/usr/bin/env bash

dnf install -y pyliblzma pxz
pwd
mkdir -p rpmbuild/{SOURCES,BUILD,RPMS,SPECS} || echo "Failed to create directories: $?"
cp oregano.spec rpmbuild/SPECS/ || echo "Failed to copy spec file $?"
# ./waf dist is too stupid to know xz ...
git archive --format=tar --prefix=oregano/ HEAD | xz > rpmbuild/SOURCES/oregano-0.83.3.tar.xz || echo "Failed to create dist file $?"
cd rpmbuild || echo "Failed to cd to rpmbuild $?"
rpmbuild \
--define "_topdir %(pwd)/rpmbuild" \
--define "_builddir %{_topdir}/BUILD" \
--define "_rpmdir %{_topdir}/RPMS" \
--define "_srcrpmdir %{_topdir}/RPMS" \
--define "_specdir %{_topdir}/SPECS" \
--define "_sourcedir  %{_topdir}/SOURCES" \
-ba SPECS/oregano.spec
