#!/bin/bash

if [[ ! -x khDaysMM.exe ]]; then
	echo "Run this script from the directory you built khDaysMM."
	exit 1
fi

mkdir -p dist

for lib in $(ldd khDaysMM.exe | grep mingw | sed "s/.*=> //" | sed "s/(.*)//"); do
	cp "${lib}" dist
done

cp khDaysMM.exe dist
windeployqt dist
