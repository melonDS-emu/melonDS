# Camera input conversion direction

Internal follow-up to the still-image format fix. `CameraManager::feedFrame`
previously chose conversion using the input's YUV flag in the opposite
direction: RGB input for a YUV framebuffer entered YUV-to-RGB conversion,
and YUV input for an RGB framebuffer entered RGB-to-YUV conversion.

Select the conversion by destination framebuffer format instead. This preserves
the same-format copy/scale paths and the existing pixel conversion algorithms.
It applies to live camera frame input; it does not change emulated camera sensor
registers, IRQ timing or native-title boot behavior.

The original BIOS-free RGB/YUV probe uses owned pixel arrays and two 4x2
framebuffers. It compares the input adapter with the existing capture conversion
path. Run against a completed Unix Makefiles Qt6 build:

```sh
python3 tests/run_camera_still_image.py /path/to/build --probe tests/camera_frame_formats.cpp
python3 tests/run_camera_still_image.py /path/to/build
```

Both units compile with AddressSanitizer and UndefinedBehaviorSanitizer. Before
the fix, RGB-to-YUV input produces an ASan heap-buffer-overflow: an RGB-sized
output is written into a YUV-sized allocation. After the fix, both conversion
controls pass (2/2); the still-image controls remain 4/4. No user config, BIOS,
Nintendo software or physical camera is used. Physical capture devices, wider
formats and native Camera live-view operation remain separate validations.
