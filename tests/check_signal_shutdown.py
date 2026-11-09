#!/usr/bin/env python3
"""Stress real Qt signal shutdown during painting without ROM/BIOS files."""
import argparse
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
parser.add_argument('--iterations', type=int, default=20)
args = parser.parse_args()
if args.iterations < 1:
    parser.error('iterations must be positive')
with tempfile.TemporaryDirectory(prefix='melonds-signal-') as folder:
    root = Path(folder)
    shutil.copy2(args.executable.resolve(), root / 'melonDS')
    portable = root / 'portable'
    portable.mkdir()
    config = '[Instance0]\nConsoleType = 0\n[Instance0.DS]\nExternalBIOSEnable = false\n[Instance0.Window0]\nScreenUseGL = false\n'
    for index in range(args.iterations):
        (portable / 'melonDS.toml').write_text(config)
        with (root / 'runtime.log').open('w') as log:
            child = subprocess.Popen([str(root / 'melonDS'), '--boot', 'never'], cwd=root,
                                     stdout=log, stderr=log, env={**os.environ, 'QT_QPA_PLATFORM': 'xcb'})
            painter = None
            try:
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    result = subprocess.run(['xdotool', 'search', '--pid', str(child.pid),
                                             '--name', 'melonDS 1\\.1'], capture_output=True, text=True)
                    if result.returncode == 0:
                        window = result.stdout.splitlines()[-1]
                        break
                    if child.poll() is not None:
                        raise RuntimeError('GUI exited before the window appeared')
                    time.sleep(.02)
                else:
                    raise RuntimeError('No emulator window')
                painter = subprocess.Popen(['xdotool', 'windowsize', window, '540', '800',
                                            'windowsize', window, '560', '830',
                                            'windowsize', window, '541', '801'],
                                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                time.sleep((index % 5) * .001)
                child.send_signal(signal.SIGTERM if index % 2 else signal.SIGINT)
                if child.wait(timeout=5) != 0:
                    raise RuntimeError('Signal did not shut down normally')
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
                if painter is not None:
                    painter.wait(timeout=5)
        print(f'{index + 1}/{args.iterations}: graceful signal shutdown', flush=True)
