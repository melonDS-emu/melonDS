#define DLL extern "C" __declspec(dllexport)

#include <stdio.h>
#include <SDL2/SDL.h>
#include "../NDS.h"
#include "../GPU3D.h"
#include "../Platform.h"
#include "../OpenGLSupport.h"

// Because Platform.cpp calls main.cpp's Stop method. I doubt we will ever need to do anything here.
void Stop(bool internal) {};

char* EmuDirectory;
bool inited = false;

DLL void Deinit()
{
	NDS::DeInit();
    inited = false;
}
DLL bool Init()
{
	if (inited)
	{
		printf("MelonDS is already inited. De-initing before init.");
		Deinit();
	}

	if (SDL_Init(0) < 0)
	{
		printf("MelonDS failed to init SDL.");
		return false;
	}
    
	EmuDirectory = new char[6];
    strcpy(EmuDirectory, "melon");

	// TODO: Load settings

    if (!Platform::LocalFileExists("bios7.bin") ||
        !Platform::LocalFileExists("bios9.bin") ||
        !Platform::LocalFileExists("firmware.bin"))
	{
		printf("MelonDS could not find bios7.bin, bios9.bin, and/or firmware.bin.");
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
                              "You should use the latest version of romlist.bin (provided in melonDS release packages).");
            }
        }
        else
        {
        	printf("romlist.bin not found.\nSave memory type detection will not work correctly.\n\n"
				         "You should use the latest version of romlist.bin (provided in melonDS release packages).");
        }
    }

    if (!NDS::Init())
    {
        printf("failed to init NDS");
        return false;
    }

    if (!OpenGL_Init())
        printf("failed to init OpenGL");
    GPU3D::InitRenderer(true);

    inited = true;
    return true;
}
