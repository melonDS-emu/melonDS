blip_buf $vers: Band-Limited Audio Buffer
-----------------------------------------
Blip_buf is a small waveform synthesis library meant for use in classic
video game sound chip emulation. It greatly simplifies sound chip
emulation code by handling all the details of resampling. The emulator
merely sets the input clock rate and output sample rate, adds waveforms
by specifying the clock times where their amplitude changes, then reads
the resulting output samples. For a more full-features synthesis
library, get Blip_Buffer.

* Simple C interface and usage.
* Several code examples, including simple sound chip emulator.
* Uses fast, high-quality band-limited resampling algorithm (BLEP).
* Output is low-pass and high-pass filtered and clamped to 16-bit range.
* Supports mono, stereo, and multi-channel synthesis.

Author  : Shay Green <gblargg@gmail.com>
Website : http://www.slack.net/~ant/
License : GNU Lesser General Public License (LGPL)
Language: C


Getting Started
---------------
Build the demos by typing "make" at the command-line. If that doesn't
work, manually build a program from demo.c, blip_buf.c, and
wave_writer.c. Run "demo", which should record a square wave sweep to
"out.wav". The others demo advanced topics, and their source code can be
used as a starting point for experimentation (after making any changes,
just type "make" to do any necessary recompilation).

To build the SDL demo, type "make demo_sdl" at the command-line, or
manually build a program from demo_sdl.c, blip_buf.c, and the SDL
multimedia library.

See blip_buf.h for reference and blip_buf.txt for documentation.


Files
-----
readme.txt            Essential information
blip_buf.txt          Library documentation
license.txt           GNU Lesser General Public License

makefile              Builds demos (type "make" at command-line)

demo.c                Generates square wave sweep
demo_stereo.c         Generates stereo sound using two blip buffers
demo_fixed.c          Works in fixed-point time rather than clocks
demo_sdl.c            Plays sound live using SDL multimedia library
demo_chip.c           Emulates sound hardware and plays back log.txt
demo_log.txt          Log of sound hardware writes for demo_chip.c
wave_writer.h         Simple wave sound file writer used by demos
wave_writer.c

blip_buf.h            Library header and reference (Doxygen-enabled)
blip_buf.c            source code

tests/                Unit tests (build with "make test")

-- 
Shay Green <gblargg@gmail.com>
