#!/usr/bin/env python3
"""Test the camera image adapter using a completed Unix Makefiles Qt build."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--source', type=Path, default=ROOT / 'src/frontend/qt_sdl/CameraManager.cpp')
    args = parser.parse_args()
    target = args.build.resolve() / 'src/frontend/qt_sdl'
    build_files = target / 'CMakeFiles/melonDS.dir'
    flags = {}
    for line in (build_files / 'flags.make').read_text().splitlines():
        key, separator, value = line.partition(' = ')
        if separator:
            flags[key] = shlex.split(value)
    link = shlex.split((build_files / 'link.txt').read_text())
    with tempfile.TemporaryDirectory(prefix='melonds-camera-image-') as directory:
        directory = Path(directory)
        objects = []
        for index, source in enumerate([Path(__file__).with_name('camera_still_image.cpp'), args.source]):
            obj = directory / f'unit{index}.o'
            subprocess.run([link[0], '-I' + str(ROOT / 'src/frontend/qt_sdl'),
                            *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
        executable = directory / 'probe'
        link[link.index('-o') + 1] = str(executable)
        link = [item for item in link if not item.endswith('/CameraManager.cpp.o')]
        subprocess.run([*link, *objects, '-fsanitize=address,undefined', '-Wl,--wrap=main'], cwd=target, check=True)
        subprocess.run([str(executable)], cwd=directory, check=True)


if __name__ == '__main__':
    main()
