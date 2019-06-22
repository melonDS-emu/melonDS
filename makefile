#NMAKE

CC_FLAGS = /c /Ox /EHsc /GL /Dstrncasecmp=_strnicmp /Dstrcasecmp=_stricmp /DWIN32 /D__WIN32__
LNK_FLAGS = /LTCG /SUBSYSTEM:WINDOWS
LNK_LIBS = gdi32.lib user32.lib shell32.lib ole32.lib dwrite.lib d2d1.lib \
	usp10.lib ws2_32.lib iphlpapi.lib SDL2.lib uxtheme.lib opengl32.lib

SOURCES1 = \
	src/libui_sdl/main.cpp                                   \
	src/libui_sdl/Platform.cpp                               \
	src/libui_sdl/DlgEmuSettings.cpp                         \
	src/libui_sdl/DlgInputConfig.cpp                         \
	src/libui_sdl/DlgAudioSettings.cpp                       \
	src/libui_sdl/DlgVideoSettings.cpp                       \
	src/libui_sdl/DlgWifiSettings.cpp                        \
	src/libui_sdl/LAN_PCap.cpp                               \
	src/libui_sdl/LAN_Socket.cpp                             \
	src/libui_sdl/OSD.cpp                                    \
	src/libui_sdl/PlatformConfig.cpp

SOURCES2 = \
	src/ARM.cpp                                              \
	src/ARMInterpreter.cpp                                   \
	src/ARMInterpreter_ALU.cpp                               \
	src/ARMInterpreter_Branch.cpp                            \
	src/ARMInterpreter_LoadStore.cpp                         \
	src/Config.cpp                                           \
	src/CP15.cpp                                             \
	src/CRC32.cpp                                            \
	src/DMA.cpp                                              \
	src/GPU.cpp                                              \
	src/GPU2D.cpp                                            \
	src/GPU3D.cpp                                            \
	src/GPU3D_Soft.cpp                                       \
	src/GPU3D_OpenGL.cpp                                     \
	src/NDS.cpp                                              \
	src/NDSCart.cpp                                          \
	src/OpenGLSupport.cpp                                    \
	src/RTC.cpp                                              \
	src/Savestate.cpp                                        \
	src/SPI.cpp                                              \
	src/SPU.cpp                                              \
	src/Wifi.cpp                                             \
	src/WifiAP.cpp

SOURCES3 = \
	src/libui_sdl/libui/common/areaevents.c                  \
	src/libui_sdl/libui/common/control.c                     \
	src/libui_sdl/libui/common/debug.c                       \
	src/libui_sdl/libui/common/matrix.c                      \
	src/libui_sdl/libui/common/shouldquit.c                  \
	src/libui_sdl/libui/common/userbugs.c

SOURCES4 = \
	src/libui_sdl/libui/windows/alloc.cpp                    \
	src/libui_sdl/libui/windows/area.cpp                     \
	src/libui_sdl/libui/windows/areadraw.cpp                 \
	src/libui_sdl/libui/windows/areaevents.cpp               \
	src/libui_sdl/libui/windows/areascroll.cpp               \
	src/libui_sdl/libui/windows/areautil.cpp                 \
	src/libui_sdl/libui/windows/box.cpp                      \
	src/libui_sdl/libui/windows/button.cpp                   \
	src/libui_sdl/libui/windows/checkbox.cpp                 \
	src/libui_sdl/libui/windows/colorbutton.cpp              \
	src/libui_sdl/libui/windows/colordialog.cpp              \
	src/libui_sdl/libui/windows/combobox.cpp                 \
	src/libui_sdl/libui/windows/container.cpp                \
	src/libui_sdl/libui/windows/control.cpp                  \
	src/libui_sdl/libui/windows/d2dscratch.cpp               \
	src/libui_sdl/libui/windows/datetimepicker.cpp           \
	src/libui_sdl/libui/windows/debug.cpp                    \
	src/libui_sdl/libui/windows/draw.cpp                     \
	src/libui_sdl/libui/windows/drawmatrix.cpp               \
	src/libui_sdl/libui/windows/drawpath.cpp                 \
	src/libui_sdl/libui/windows/drawtext.cpp                 \
	src/libui_sdl/libui/windows/dwrite.cpp                   \
	src/libui_sdl/libui/windows/editablecombo.cpp            \
	src/libui_sdl/libui/windows/entry.cpp                    \
	src/libui_sdl/libui/windows/events.cpp                   \
	src/libui_sdl/libui/windows/fontbutton.cpp               \
	src/libui_sdl/libui/windows/fontdialog.cpp               \
	src/libui_sdl/libui/windows/form.cpp                     \
	src/libui_sdl/libui/windows/gl.cpp                       \
	src/libui_sdl/libui/windows/graphemes.cpp                \
	src/libui_sdl/libui/windows/grid.cpp                     \
	src/libui_sdl/libui/windows/group.cpp                    \
	src/libui_sdl/libui/windows/init.cpp                     \
	src/libui_sdl/libui/windows/label.cpp                    \
	src/libui_sdl/libui/windows/main.cpp                     \
	src/libui_sdl/libui/windows/menu.cpp                     \
	src/libui_sdl/libui/windows/multilineentry.cpp           \
	src/libui_sdl/libui/windows/parent.cpp                   \
	src/libui_sdl/libui/windows/progressbar.cpp              \
	src/libui_sdl/libui/windows/radiobuttons.cpp             \
	src/libui_sdl/libui/windows/separator.cpp                \
	src/libui_sdl/libui/windows/sizing.cpp                   \
	src/libui_sdl/libui/windows/slider.cpp                   \
	src/libui_sdl/libui/windows/spinbox.cpp                  \
	src/libui_sdl/libui/windows/stddialogs.cpp               \
#	src/libui_sdl/libui/windows/tab.cpp                      \
#	src/libui_sdl/libui/windows/tabpage.cpp                  \
	src/libui_sdl/libui/windows/text.cpp                     \
	src/libui_sdl/libui/windows/utf16.cpp                    \
	src/libui_sdl/libui/windows/utilwin.cpp                  \
	src/libui_sdl/libui/windows/window.cpp                   \
	src/libui_sdl/libui/windows/winpublic.cpp                \
	src/libui_sdl/libui/windows/winutil.cpp                  
	
SOURCES5 = \
	src/libui_sdl/libui/windows/resources.rc                 \
	melon.rc

all:
	@if NOT EXIST obj1 mkdir obj1
	@if NOT EXIST obj2 mkdir obj2
	@if NOT EXIST obj3 mkdir obj3
	@if NOT EXIST obj4 mkdir obj4
	@if NOT EXIST obj5 mkdir obj5
	@cl $(CC_FLAGS) $(SOURCES1) /Foobj1/
	@cl $(CC_FLAGS) $(SOURCES2) /Foobj2/
	@cl $(CC_FLAGS) $(SOURCES3) /Foobj3/
	@cl $(CC_FLAGS) $(SOURCES4) /Foobj4/
	@rc /Foobj5/libui.res src/libui_sdl/libui/windows/resources.rc
	@rc /Foobj5/melon.res melon.rc
	@link $(LNK_FLAGS) obj1/*.obj obj2/*.obj obj3/*.obj obj4/*.obj obj5/*.res $(LNK_LIBS) /OUT:melonDS.exe
	@if EXIST obj1 rmdir obj1  /s /q
	@if EXIST obj2 rmdir obj2  /s /q
	@if EXIST obj3 rmdir obj3  /s /q
	@if EXIST obj4 rmdir obj4  /s /q
	@if EXIST obj5 rmdir obj5  /s /q

clean:
	@if EXIST obj1 rmdir obj1  /s /q
	@if EXIST obj2 rmdir obj2  /s /q
	@if EXIST obj3 rmdir obj3  /s /q
	@if EXIST obj4 rmdir obj4  /s /q
	@if EXIST obj5 rmdir obj5  /s /q