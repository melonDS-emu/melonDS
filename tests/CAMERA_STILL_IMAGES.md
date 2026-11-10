# Camera still-image input format

The Qt image reader converts a loaded image to
RGB32, but previously passed the original image's pixels into routines that
consume 32-bit RGB. A 16-bit-per-channel PNG can remain RGBA64 after loading;
interpreting that storage as RGB32 corrupts the emulated camera colors.

Both RGB and YUV camera outputs now read the converted RGB32 image. The source
image and its ownership remain unchanged. This is a frontend input-format fix,
not a sensor protocol, warm-reset, cache timing or game-specific workaround.

The original generated-image probe needs a completed Linux Unix Makefiles Qt6 build:

```sh
python3 tests/run_camera_still_image.py /path/to/build
```

It compiles the selected camera source and generated-image probe with AddressSanitizer
and UndefinedBehaviorSanitizer, replacing just the completed build's camera
object and wrapping main to reuse its platform services. It uses temporary
32-bit and 64-bit PNGs and compares RGB32/YUV outputs against the same RGB32
reference. No BIOS, NAND, Nintendo software or camera device is required.
The original camera source passes 2/4 controls; this candidate passes 4/4.
`--source /path/to/CameraManager.cpp` selects an explicit comparison source.
Configuration is prepared in memory; the probe does not load user settings.

The test does not establish native DSi Camera live-view operation, physical
camera quality, or warm-launch compatibility. The separate `CameraFrameFormats.cpp` probe covers live-frame
buffer conversion.
