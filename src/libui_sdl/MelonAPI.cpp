#define DLL extern "C" __declspec(dllexport)

#include <stdio.h>
#include <SDL2/SDL.h>
#include "../NDS.h"
#include "../GPU3D.h"
#include "../GPU.h"
#include "../Platform.h"
#include "../OpenGLSupport.h"
#include "../Savestate.h"
#include "../SPU.h"
#include "../NDSCart.h"
#include "../SPI.h"
#include "../Config.h"

// Because Platform.cpp calls main.cpp's Stop method. I doubt we will ever need to do anything here.
void Stop(bool internal) {};

DLL void ResetCounters();

char* EmuDirectory;
bool inited = false;
bool LidStatus;

bool directBoot = true;

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

	LidStatus = false;
    if (!NDS::Init())
    {
        printf("failed to init NDS\n");
        return false;
    }

    Config::Load();

    bool hasGL = OpenGL_Init();
    if (!hasGL)
        printf("failed to init OpenGL\n");
    GPU3D::InitRenderer(hasGL);

	ResetCounters();

    // redirect console output so BizHawk can see it
    freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

    inited = true;
    return true;
}

DLL void LoadROM(u8* file, s32 fileSize)
{
    NDS::LoadROM(file, fileSize, directBoot);
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

    const int screenKey = 16+6; // got this number from main.cpp
    if (buttons & 0x2000)
    {
        NDS::PressKey(screenKey);
        NDS::TouchScreen(touchX, touchY);
    }
    else
    {
        NDS::ReleaseKey(screenKey);
        NDS::ReleaseScreen();
    }
        
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

DLL bool UseSavestate(u8* data, s32 len)
{
	Savestate* state = new Savestate(data, len);
    if (!state->Error)
    	NDS::DoSavestate(state);
    bool error = state->Error;
	delete state;
    return !error;
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

s32 sampleCount = -1;
DLL int GetSampleCount()
{
    // OutputSize is double sample count, because stereo sound
    return sampleCount = SPU::GetOutputSize() / 2;
}
DLL void GetSamples(s16* data, s32 count)
{
    if (count != sampleCount)
        throw "sample count mismatch; call GetSampleCount first";
    if (SPU::ReadOutput(data, count) != count)
        throw "sample count was less than expected";
    sampleCount = -1;
}
DLL void DiscardSamples()
{
    SPU::DrainOutput();
}

DLL s32 GetSRAMLength()
{
    return (s32)NDSCart_SRAM::SRAMLength;
}
DLL void GetSRAM(u8* dst, s32 size)
{
    if (!inited) return;

    if (size != NDSCart_SRAM::SRAMLength)
        throw "SRAM size mismatch; call GetSRAMLength first";
    memcpy(dst, NDSCart_SRAM::SRAM, size);
    NDSCart_SRAM::SRAMModified = false;
}
DLL void SetSRAM(u8* src, s32 size)
{
    if (!inited) return;

    if (size != NDSCart_SRAM::SRAMLength)
        throw "SRAM size mismatch";
    memcpy(NDSCart_SRAM::SRAM, src, size);
}
DLL bool IsSRAMModified()
{
    return NDSCart_SRAM::SRAMModified;
}

const s32 userSettingsLength = SPI_Firmware::userSettingsLength;
DLL s32 getUserSettingsLength() { return userSettingsLength; }
// Gets the currently loaded user settings. Returns false if no settings are loaded.
DLL bool GetUserSettings(u8* dst)
{
    u8* src = SPI_Firmware::GetUserSettings();
    if (src)
        memcpy(dst, src, userSettingsLength);
    return src != NULL;
}
// Overwrites the currently loaded user settings. If none are loaded, the settings are put in a buffer to load during the next boot.
// If src is NULL, default settings are used.
DLL void SetUserSettings(u8* src) { SPI_Firmware::SetUserSettings(src); }

DLL bool GetDirectBoot() { return directBoot; }
DLL void SetDirectBoot(bool value) { directBoot = value; }

DLL u32 GetTimeAtBoot() { return Config::TimeAtBoot; }
DLL void SetTimeAtBoot(u32 value) { Config::TimeAtBoot = value; }

DLL u8* GetMainMemory() { return NDS::MainRAM; }
DLL s32 GetMainMemorySize() { return MAIN_RAM_SIZE; }
