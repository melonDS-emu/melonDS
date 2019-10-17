#define DLL extern "C" __declspec(dllexport)

#include <stdio.h>
#include <SDL2/SDL.h>
#include "../NDS.h"
#include "../GPU3D.h"
#include "../GPU.h"
#include "../Platform.h"
#include "../OpenGLSupport.h"
#include "../Savestate.h"

// Because Platform.cpp calls main.cpp's Stop method. I doubt we will ever need to do anything here.
void Stop(bool internal) {};

DLL void ResetCounters();

char* EmuDirectory;
bool inited = false;
bool LidStatus;

DLL void Deinit()
{
	NDS::DeInit();
    inited = false;
}
DLL bool Init()
{
	if (inited)
	{
		printf("MelonDS is already inited. De-initing before init.\n");
		Deinit();
	}

	if (SDL_Init(0) < 0)
	{
		printf("MelonDS failed to init SDL.\n");
		return false;
	}
    
	EmuDirectory = new char[6];
    strcpy(EmuDirectory, "melon");

	// TODO: Load settings

    if (!Platform::LocalFileExists("bios7.bin") ||
        !Platform::LocalFileExists("bios9.bin") ||
        !Platform::LocalFileExists("firmware.bin"))
	{
		printf("MelonDS could not find bios7.bin, bios9.bin, and/or firmware.bin.\n");
		SDL_Quit();
		return false;
	}

	{ // TODO: Would probably be nice to include the romlist in the DLL, rather than use a separate file.
        FILE* f = Platform::OpenLocalFile("romlist.bin", "rb");
        if (f)
        {
            u32 data;
            fread(&data, 4, 1, f);
            fclose(f);

            if ((data >> 24) == 0) // old CRC-based list
            {
                printf("Your version of romlist.bin is outdated.\nSave memory type detection will not work correctly.\n\n"
                              "You should use the latest version of romlist.bin (provided in melonDS release packages)\n.");
            }
        }
        else
        {
        	printf("romlist.bin not found.\nSave memory type detection will not work correctly.\n\n"
				         "You should use the latest version of romlist.bin (provided in melonDS release packages)\n.");
        }
    }

	LidStatus = false;
    if (!NDS::Init())
    {
        printf("failed to init NDS\n");
        return false;
    }

    if (!OpenGL_Init())
        printf("failed to init OpenGL\n");
    GPU3D::InitRenderer(true);

	ResetCounters();

    inited = true;
    return true;
}

DLL void LoadROM(u8* file, s32 fileSize)
{
    NDS::LoadROM(file, fileSize, true);
}

DLL void ResetCounters()
{
	NDS::NumFrames = 0;
	NDS::NumLagFrames = 0;
}
DLL int GetFrameCount()
{
	return NDS::NumFrames;
}
DLL bool IsLagFrame()
{
	return NDS::LagFrameFlag;
}
DLL int GetLagFrameCount()
{
	return NDS::NumLagFrames;
}

DLL void FrameAdvance(u16 buttons, u8 touchX, u8 touchY)
{
    if (!inited) return;

    if (buttons & 0x2000)
        NDS::TouchScreen(touchX, touchY);
    else
        NDS::ReleaseScreen();    
        
    NDS::SetKeyMask(~buttons & 0xFFF); // 12 buttons
    if (buttons & 0x4000)
    {
        LidStatus = !LidStatus;
        NDS::SetLidClosed(LidStatus);
    }

    NDS::MicInputFrame(NULL, 0);

    NDS::RunFrame();
}

DLL void VideoBuffer32bit(s32* dst)
{
    // 2D
    u32* src = GPU::Framebuffer[GPU::FrontBuffer][0];
    memcpy(dst, src, 4 * 256 * 192);
    dst += 256 * 192;
    src = GPU::Framebuffer[GPU::FrontBuffer][1];
    memcpy(dst, src, 4 * 256 * 192);
}

DLL void UseSavestate(u8* data, s32 len)
{
	Savestate* state = new Savestate(data, len);
	NDS::DoSavestate(state);
	delete state;
}
Savestate* _loadedState;
u8* stateData;
s32 stateLength = -1;
DLL int GetSavestateSize()
{
	if (_loadedState) delete _loadedState;

	_loadedState = new Savestate("", true);
	NDS::DoSavestate(_loadedState);
	stateData = _loadedState->GetData();
	stateLength = _loadedState->GetDataLength();

	return stateLength;
}
DLL void GetSavestateData(u8* data, s32 size)
{
	if (size != stateLength)
		throw "size mismatch; call GetSavestateSize first";
	if (stateData)
	{
		memcpy(data, stateData, stateLength);
		delete _loadedState;
		_loadedState = NULL;
		stateData = NULL;
		stateLength = -1;
	}
}
