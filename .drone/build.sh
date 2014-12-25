#!/usr/bin/env bash
set -e
./waf configure --prefix="/usr" debug install
rpmdev-setuptree
cp oregano.spec.in ~/rpmbuild/SPECS/oregano.spec
cd ~/rpmbuild/
rpmbuild -ba SPECS/oregano.spec
rpmlint RPMS/x86_64/oregano*.rpm

