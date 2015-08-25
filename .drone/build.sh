#!/usr/bin/env bash
set -ev
git archive -v --prefix=oregano -o oregano.tar.gz HEAD

./waf configure --prefix="/usr"

rpmdev-setuptree
set +ev
tag=$(git describe --exact-match --tags HEAD) > /dev/null 2>&1
skip=$?
set -ev
if [[ $skip -ne 0 ]]; then
	./waf debug install
elif [[ $tag =~ ^v?[0-9]+\.[0-9]+(\.[0-9]+(\.[0-9]+)?)?$ ]]; then
	./waf release install
else
	./waf debug install
fi

cp oregano.spec ~/rpmbuild/SPECS/oregano.spec
cp oregano.tar.gz ~/rpmbuild/SOURCES/oregano.tar.gz
cd ~/rpmbuild/
rpmbuild -ba SPECS/oregano.spec
rpmlint RPMS/x86_64/oregano*.rpm
