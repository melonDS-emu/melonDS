#!/bin/bash

if [[ ! -x melonDS.exe ]]; then
	echo "Run this script from the directory you built melonDS."
	exit 1
fi

mkdir -p dist
tool=$(gcc -v 2>&1 | head -1 | awk '{print $1}')
for lib in $(ldd melonDS.exe | grep $tool | sed "s/.*=> //" | sed "s/(.*)//"); do
	cp "${lib}" dist
done

cp melonDS.exe dist
windeployqt dist
