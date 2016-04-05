#!/usr/bin/env bash

distribution=$(cat /etc/os-release |grep "^NAME=" |cut -d '=' -f 2)

echo "Distribution detected is \"${distribution}\""

MODE="$1"

if [ ${MODE:="interactive"} != "ci" ]; then
	MODE="interactive"
fi

echo "Running $0 in $MODE mode"


if [ "${distribution}" = "\"Ubuntu\"" ]; then # idiots...
	PKGS="python libglib2.0-dev intltool libgtk-3-dev libxml2-dev libgoocanvas-2.0-dev libgtksourceview-3.0-dev gcc clang"
	if [ ${MODE} = "ci" ]; then
		apt-get update -y
		apt-get install -y $PKGS
	else
		apt-get install $PKGS
	fi
elif [ "${distribution}" = "Fedora" ]; then
	PKGS="python gtk3-devel libxml2-devel gtksourceview3-devel intltool glib2-devel goocanvas2-devel desktop-file-utils clang gcc"

	if [ ${MODE} = "ci" ]; then
		dnf update -y
		dnf install -y $PKGS
	else
		dnf install $PKGS
	fi
else
	echo "No idea what to do.. trying to run regular builddeps"
fi
