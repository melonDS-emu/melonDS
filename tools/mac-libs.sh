#!/bin/bash


set -o errexit
set -o pipefail

build_dmg=0
app=melonDS.app

if [[ "$1" == "--dmg" ]]; then
	build_dmg=1
	shift
fi

if [[ ! -d "$1" ]]; then
	echo "Usage: $0 [--dmg] <build-dir>"
	exit 1
fi

cd "$1"

# macOS does not have the -f flag for readlink
abspath() {
	perl -MCwd -le 'print Cwd::abs_path shift' "$1"
}

cmake_qtdir=$(grep -E "Qt._DIR" CMakeCache.txt | cut -d= -f2)
qtdir="$(abspath "$cmake_qtdir"/../../..)"

if [[ ! -d "$app/Contents/Frameworks" ]]; then
	"${qtdir}/bin/macdeployqt" "$app"
fi

# We'll have to copy the Qt plugins we need on our own if macdeployqt forgets
# Qt6 bug?
plugindir="$app/Contents/PlugIns"
if [[ ! -d "$plugindir" ]]; then
    mkdir -p "$plugindir/styles" "$plugindir/platforms"
    cp "$qtdir/share/qt/plugins/styles/libqmacstyle.dylib" "$plugindir/styles/"
    cp "$qtdir/share/qt/plugins/platforms/libqcocoa.dylib" "$plugindir/platforms/"

    install_name_tool -add_rpath "@executable_path/../Frameworks" "$app/Contents/MacOS/melonDS"
fi

# Fix library load paths that macdeployqt forgot about
fixup_libs() {
	local libs=($(otool -L "$1" | sed -E 's/\t(.*) \(.*$/\1/' | grep -vE '/System|/usr/lib|\.framework|^\@|:$'))
	
	for lib in "${libs[@]}"; do
		local base="$(basename "$lib")"
		install_name_tool -change "$lib" "@executable_path/../Frameworks/$base" "$1"
	done
}

find "$app/Contents/Frameworks" -maxdepth 1 -name '*.dylib' | while read lib; do
	fixup_libs "$lib"
done

fixup_libs "$app/Contents/MacOS/melonDS"

if [[ $build_dmg == 1 ]]; then
	mkdir dmg
	cp -r "$app" dmg/
	ln -s /Applications dmg/Applications
	hdiutil create -volname melonDS -srcfolder dmg -ov -format UDZO melonDS.dmg
	rm -r dmg
fi
