# melonDS
DS emulator, sorta


The goal is to do things right and fast, akin to blargSNES (but hopefully better). But also to, you know, have a fun challenge :)


The source code is provided under the GPLv3 license.


How to use:

melonDS requires BIOS/firmware copies from a DS. Files required:
 * bios7.bin, 16KB: ARM7 BIOS
 * bios9.bin, 4KB: ARM9 BIOS
 * firmware.bin, 256KB: firmware
 
Note: the DS-mode firmware in the 3DS isn't bootable, it only contains the bare minimum to run games.

As for the rest, the interface should be pretty straightforward. If you have a question, don't hesitate to ask, though!


How to build:

Linux:
 * mkdir -p build
 * cd build
 * cmake ..
 * make

Windows:
 * use CodeBlocks

Build system is not set in stone.


TODO LIST

 * better 3D engine
 * wifi
 * other non-core shit (debugger, graphics viewers, cheat crapo, etc)

 
TODO LIST FOR LATER

 * more 3D engine features
 * hardware renderer for 3D
 * wifi
 * maybe emulate flashcarts or other fancy hardware
 * big-endian compatibility (Wii, etc)
 * LCD refresh time (used by some games for blending effects)
 * any feature you can eventually ask for that isn't outright stupid
