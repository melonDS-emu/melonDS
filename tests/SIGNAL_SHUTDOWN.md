# Unix signal shutdown

The previous SIGINT/SIGTERM handler called `qApp->quit()` directly. A captured
native test deadlock showed Qt's close-event handling reentering XCB while the
interrupted `xcb_put_image` held the same connection mutex. This is a host
frontend issue, not emulated hardware behavior.

The handler now writes only a compile-time checked lock-free atomic request. A 50 ms Qt timer
dispatches the quit request from the ordinary event loop. Relaxed atomic access
is sufficient for this single flag and remains safe when delivery occurs on a
worker thread. Windows handling is
unchanged; this does not change the CPU, camera, storage or wireless model.

With a completed Qt build and an available X display:

```sh
python3 tests/check_signal_shutdown.py /path/to/build/melonDS --iterations 20
```

The test creates a disposable empty portable profile, starts the actual GUI
without ROM/BIOS files, provokes painting and alternates SIGINT/SIGTERM. It
requires xdotool. A timeout stops only the child created by that iteration.
This exercises shutdown while painting; it does not certify signal behavior
under every platform/plugin or failure mode.

For a disposable Xvfb display, use `-noreset` or retain a client connection
throughout the loop. Otherwise server resets between iterations can reject the
next GUI connection; that is an infrastructure startup failure, not a signal
shutdown result. Startup failures include the child log for that distinction.
