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

plugindir="$app/Contents/PlugIns"
if [[ ! -d "$plugindir" ]]; then
	qt_plugins="$qtdir/plugins"
	if [[ ! -d "$qt_plugins" ]]; then
		qt_plugins="$qtdir/share/qt/plugins"
	fi

    mkdir -p "$plugindir/styles" "$plugindir/platforms"
    cp "$qt_plugins/styles/libqmacstyle.dylib" "$plugindir/styles/"
    cp "$qt_plugins/platforms/libqcocoa.dylib" "$plugindir/platforms/"
fi

mkdir "$app/Contents/Frameworks"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$app/Contents/MacOS/melonDS"

fixup_libs() {
	local libs=($(otool -L "$1" | grep -vE "/System|/usr/lib|:$" | sed -E 's/\t(.*) \(.*$/\1/'))
	
	for lib in "${libs[@]}"; do
		# Dereference symlinks to get the actual .dylib as binaries' load
		# commands can contain paths to symlinked libraries.
		local abslib="$(abspath "$lib")"

		if [[ "$abslib" == *".framework/"* ]]; then
			local fwpath="$(echo "$abslib" | sed -E 's@(.*\.framework)/.*@\1/@')"
			local fwlib="$(echo "$abslib" | sed -E 's/.*\.framework//')"
			local fwname="$(basename "$fwpath")"
			local install_path="$app/Contents/Frameworks/$fwname"

			install_name_tool -change "$lib" "@rpath/$fwname/$fwlib" "$1"

			if [[ ! -d "$install_path" ]]; then
				cp -a "$fwpath" "$install_path"
				chown -R $UID "$install_path"
				chmod -R u+w "$install_path"
				strip -SNTx "$install_path/$fwlib"
				fixup_libs "$install_path/$fwlib"
			fi
		else
			local base="$(basename "$abslib")"
			local install_path="$app/Contents/Frameworks/$base"

			install_name_tool -change "$lib" "@rpath/$base" "$1"

			if [[ ! -f "$install_path" ]]; then
				install -m644 "$abslib" "$install_path"
				strip -SNTx "$install_path"
				fixup_libs "$install_path"
			fi
		fi
	done
}


fixup_libs "$app/Contents/MacOS/melonDS"
find "$app/Contents/PlugIns" -name '*.dylib' | while read lib; do
	fixup_libs "$lib"
done

bad_rpaths=($(otool -l "$app/Contents/MacOS/melonDS" | grep -E "^ *path (/usr/local|/opt)" | sed -E 's/^ *path (.*) \(.*/\1/'))

for path in "${bad_rpaths[@]}"; do
	install_name_tool -delete_rpath "$path" "$app/Contents/MacOS/melonDS"
done

codesign -s - --deep "$app"

if [[ $build_dmg == 1 ]]; then
	mkdir dmg
	cp -r "$app" dmg/
	ln -s /Applications dmg/Applications
	hdiutil create -volname melonDS -srcfolder dmg -ov -format UDZO melonDS.dmg
	rm -r dmg
fi
