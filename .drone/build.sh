#!/usr/bin/env bash
set -ev

./waf configure --prefix="/usr"

version=$(cat ./build/VERSION)
git archive -v --prefix=oregano -o oregano-$version.tar.xz HEAD

rpmdev-setuptree
set +ev
tag=$(git describe --exact-match --tags HEAD) > /dev/null 2>&1
skip=$?
set -ev
extra="master"
if [[ $skip -ne 0 ]]; then
	./waf debug install
elif [[ $tag =~ ^v?[0-9]+\.[0-9]+(\.[0-9]+(\.[0-9]+)?)?$ ]]; then
	./waf release install
	extra=""
else
	./waf debug install
fi

cp oregano.spec ~/rpmbuild/SPECS/oregano.spec
cp oregano-$version.tar.xz ~/rpmbuild/SOURCES/oregano-$version.tar.xz
cd ~/rpmbuild/
rpmbuild -ba SPECS/oregano.spec
rpmlint RPMS/x86_64/oregano*.rpm
