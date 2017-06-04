/*
    Copyright 2016-2017 StapleButter

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "../types.h"
#include "main.h"
#include "../version.h"
#include "../Config.h"
#include "../NDS.h"
#include "../GPU.h"
#include "../GPU3D.h"
#include "../SPU.h"

#include "InputConfig.h"
#include "EmuConfig.h"


wxIMPLEMENT_APP_NO_MAIN(wxApp_melonDS);


int main(int argc, char** argv)
{
    srand(time(NULL));

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL shat itself :(\n");
        return 1;
    }

    int ret = wxEntry(argc, argv);

    SDL_Quit();
    return ret;
}

#ifdef __WXMSW__

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    char cmdargs[16][256];
    int arg = 0;
    int j = 0;
    bool inquote = false;
    int len = strlen(cmdline);
    for (int i = 0; i < len; i++)
    {
        char c = cmdline[i];
        if (c == '\0') break;
        if (c == '"') inquote = !inquote;
        if (!inquote && c==' ')
        {
            if (arg < 16) cmdargs[arg][j] = '\0';
            arg++;
            j = 0;
        }
        else
        {
            if (arg < 16 && j < 255) cmdargs[arg][j] = c;
            j++;
        }
    }
    if (arg < 16) cmdargs[arg][j] = '\0';

    return main(arg, (char**)cmdargs);
}

#endif // __WXMSW__


bool _fileexists(char* name)
{
    FILE* f = fopen(name, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}


bool wxApp_melonDS::OnInit()
{
    printf("melonDS " MELONDS_VERSION "\n" MELONDS_URL "\n");

    Config::Load();
    
    if (!_fileexists("bios7.bin") || !_fileexists("bios9.bin") || !_fileexists("firmware.bin"))
    {
        wxMessageBox(
            "One or more of the following required files don't exist or couldn't be accessed:\n\n"
            "bios7.bin -- ARM7 BIOS\n"
            "bios9.bin -- ARM9 BIOS\n"
            "firmware.bin -- firmware image\n\n"
            "Place the following files in the directory you run melonDS from.\n"
            "Make sure that the files can be accessed.",
            "melonDS",
            wxICON_ERROR);
            
        return false;
    }

    emuthread = new EmuThread();
    if (emuthread->Run() != wxTHREAD_NO_ERROR)
    {
        printf("thread shat itself :( giving up now\n");
        delete emuthread;
        return false;
    }

    MainFrame* melon = new MainFrame();
    melon->Show(true);

    melon->emuthread = emuthread;
    emuthread->parent = melon;

    return true;
}

int wxApp_melonDS::OnExit()
{
    emuthread->Wait();
    delete emuthread;

    return wxApp::OnExit();
}


wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_CLOSE(MainFrame::OnClose)

    EVT_MENU(ID_OPENROM, MainFrame::OnOpenROM)
    EVT_MENU(ID_EXIT, MainFrame::OnCloseFromMenu)

    EVT_MENU(ID_RUN, MainFrame::OnRun)
    EVT_MENU(ID_PAUSE, MainFrame::OnPause)
    EVT_MENU(ID_RESET, MainFrame::OnReset)

    EVT_MENU(ID_EMUCONFIG, MainFrame::OnEmuConfig)
    EVT_MENU(ID_INPUTCONFIG, MainFrame::OnInputConfig)
wxEND_EVENT_TABLE()


MainFrame::MainFrame()
    : wxFrame(NULL, wxID_ANY, "melonDS " MELONDS_VERSION)
{
    wxMenu* filemenu = new wxMenu();
    filemenu->Append(ID_OPENROM, "Open ROM...");
    filemenu->AppendSeparator();
    filemenu->Append(ID_EXIT, "Quit");

    wxMenu* systemmenu = new wxMenu();
    systemmenu->Append(ID_RUN, "Run");
    systemmenu->AppendCheckItem(ID_PAUSE, "Pause");
    systemmenu->AppendSeparator();
    systemmenu->Append(ID_RESET, "Reset");

    wxMenu* settingsmenu = new wxMenu();
    settingsmenu->Append(ID_EMUCONFIG, "Emulation");
    settingsmenu->Append(ID_INPUTCONFIG, "Input");

    wxMenuBar* melonbar = new wxMenuBar();
    melonbar->Append(filemenu, "File");
    melonbar->Append(systemmenu, "System");
    melonbar->Append(settingsmenu, "Settings");

    SetMenuBar(melonbar);

    SetClientSize(256, 256);
    SetMinSize(GetSize());

    NDS::Init();
    rompath = wxEmptyString;
    GetMenuBar()->Enable(ID_PAUSE, false);
    GetMenuBar()->Enable(ID_RESET, false);

    joy = NULL;
    joyid = -1;
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    emuthread->EmuPause();
    emuthread->EmuExit();

    NDS::DeInit();

    if (joy)
    {
        SDL_JoystickClose(joy);
        joy = NULL;
        joyid = -1;
    }

    Destroy();
    emuthread->parent = NULL;

    Config::Save();
}

void MainFrame::OnCloseFromMenu(wxCommandEvent& event)
{
    Close();
}

void MainFrame::OnOpenROM(wxCommandEvent& event)
{
    wxFileDialog opener(this, _("Open ROM"), "", "", "DS ROM (*.nds)|*.nds;*.srl|Any file|*.*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (opener.ShowModal() == wxID_CANCEL)
        return;

    emuthread->EmuPause();

    rompath = opener.GetPath();
    NDS::LoadROM(rompath.mb_str(), Config::DirectBoot);

    emuthread->EmuRun();
    GetMenuBar()->Enable(ID_PAUSE, true);
    GetMenuBar()->Check(ID_PAUSE, false);
    GetMenuBar()->Enable(ID_RESET, true);

    if (!joy)
    {
        if (SDL_NumJoysticks() > 0)
        {
            joy = SDL_JoystickOpen(0);
            joyid = SDL_JoystickInstanceID(joy);
        }
    }
}

void MainFrame::OnRun(wxCommandEvent& event)
{
    // TODO: reduce duplicate code

    if (!emuthread->EmuIsRunning())
    {
        NDS::LoadBIOS();
    }

    emuthread->EmuRun();
    GetMenuBar()->Enable(ID_PAUSE, true);
    GetMenuBar()->Check(ID_PAUSE, false);
    GetMenuBar()->Enable(ID_RESET, true);

    if (!joy)
    {
        if (SDL_NumJoysticks() > 0)
        {
            joy = SDL_JoystickOpen(0);
            joyid = SDL_JoystickInstanceID(joy);
        }
    }
}

void MainFrame::OnPause(wxCommandEvent& event)
{
    if (!emuthread->EmuIsPaused())
    {
        emuthread->EmuPause();
        GetMenuBar()->Check(ID_PAUSE, true);
    }
    else
    {
        emuthread->EmuRun();
        GetMenuBar()->Check(ID_PAUSE, false);
    }
}

void MainFrame::OnReset(wxCommandEvent& event)
{
    emuthread->EmuPause();

    if (!rompath.IsEmpty())
        NDS::LoadROM(rompath.mb_str(), Config::DirectBoot);
    else
        NDS::LoadBIOS();

    emuthread->EmuRun();
    GetMenuBar()->Enable(ID_PAUSE, true);
    GetMenuBar()->Check(ID_PAUSE, false);
    GetMenuBar()->Enable(ID_RESET, true);

    if (!joy)
    {
        if (SDL_NumJoysticks() > 0)
        {
            joy = SDL_JoystickOpen(0);
            joyid = SDL_JoystickInstanceID(joy);
        }
    }
}

void MainFrame::OnEmuConfig(wxCommandEvent& event)
{
    bool oldpause = emuthread->EmuIsPaused();
    if (!oldpause) emuthread->EmuPause();

    EmuConfigDialog dlg(this);
    dlg.ShowModal();

    // apply threaded 3D setting
    GPU3D::SoftRenderer::SetupRenderThread();

    if (!oldpause) emuthread->EmuRun();
}

void MainFrame::OnInputConfig(wxCommandEvent& event)
{
    if (joy)
    {
        SDL_JoystickClose(joy);
        joy = NULL;
        joyid = -1;
    }

    InputConfigDialog dlg(this);
    dlg.ShowModal();

    if (SDL_NumJoysticks() > 0)
    {
        joy = SDL_JoystickOpen(0);
        joyid = SDL_JoystickInstanceID(joy);
    }
}


EmuThread::EmuThread()
    : wxThread(wxTHREAD_JOINABLE)
{
}

EmuThread::~EmuThread()
{
}

static void AudioCallback(void* data, Uint8* stream, int len)
{
    SPU::ReadOutput((s16*)stream, len>>2);
}

wxThread::ExitCode EmuThread::Entry()
{
    emustatus = 3;
    emupaused = false;

    limitfps = true;

    sdlwin = SDL_CreateWindow("melonDS " MELONDS_VERSION,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              Config::WindowWidth, Config::WindowHeight,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_SetWindowMinimumSize(sdlwin, 256, 384);

    sdlrend = SDL_CreateRenderer(sdlwin, -1, SDL_RENDERER_ACCELERATED);// | SDL_RENDERER_PRESENTVSYNC);
    sdltex = SDL_CreateTexture(sdlrend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 384);

    SDL_SetRenderDrawColor(sdlrend, 0, 0, 0, 255);

    SDL_LockTexture(sdltex, NULL, &texpixels, &texstride);
    memset(texpixels, 0, texstride*384);
    SDL_UnlockTexture(sdltex);

    topsrc.x = 0; topsrc.y = 0;
    topsrc.w = 256; topsrc.h = 192;
    botsrc.x = 0; botsrc.y = 192;
    botsrc.w = 256; botsrc.h = 192;

    topdst.x = 0; topdst.y = 0;
    topdst.w = 256; topdst.h = 192;
    botdst.x = 0; botdst.y = 192;
    botdst.w = 256; botdst.h = 192;

    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 32824; // 32823.6328125
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = AudioCallback;
    audio = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, 0);
    if (!audio)
    {
        printf("Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        SDL_PauseAudioDevice(audio, 0);
    }

    Touching = false;
    axismask = 0;

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;

    for (;;)
    {
        if (emustatus == 0) break;

        ProcessEvents();

        if (emustatus == 0) break;

        if (emustatus == 1)
        {
            u32 nlines = NDS::RunFrame();

            SDL_LockTexture(sdltex, NULL, &texpixels, &texstride);
            if (texstride == 256*4)
            {
                memcpy(texpixels, GPU::Framebuffer, 256*384*4);
            }
            else
            {
                int dsty = 0;
                for (int y = 0; y < 256*384; y+=256)
                {
                    memcpy(&((u8*)texpixels)[dsty], &GPU::Framebuffer[y], 256*4);
                    dsty += texstride;
                }
            }
            SDL_UnlockTexture(sdltex);

            SDL_RenderClear(sdlrend);
            SDL_RenderCopy(sdlrend, sdltex, &topsrc, &topdst);
            SDL_RenderCopy(sdlrend, sdltex, &botsrc, &botdst);
            SDL_RenderPresent(sdlrend);

            // framerate limiter based off SDL2_gfx
            float framerate;
            if (nlines == 263) framerate = 1000.0f / 60.0f;
            else               framerate = ((1000.0f * nlines) / 263.0f) / 60.0f;

            fpslimitcount++;
            u32 curtick = SDL_GetTicks();
            u32 delay = curtick - lasttick;
            lasttick = curtick;

            u32 wantedtick = starttick + (u32)((float)fpslimitcount * framerate);
            if (curtick < wantedtick && limitfps)
            {
                Sleep(wantedtick - curtick);
            }
            else
            {
                fpslimitcount = 0;
                starttick = curtick;
            }

            nframes++;
            if (nframes >= 30)
            {
                u32 tick = SDL_GetTicks();
                u32 diff = tick - lastmeasuretick;
                lastmeasuretick = tick;

                u32 fps = (nframes * 1000) / diff;
                nframes = 0;

                char melontitle[100];
                sprintf(melontitle, "%d FPS - melonDS " MELONDS_VERSION, fps);
                SDL_SetWindowTitle(sdlwin, melontitle);
            }
        }
        else
        {
            nframes = 0;
            lasttick = SDL_GetTicks();
            starttick = lasttick;
            lastmeasuretick = lasttick;
            fpslimitcount = 0;

            Sleep(50);

            SDL_RenderCopy(sdlrend, sdltex, NULL, NULL);
            SDL_RenderPresent(sdlrend);

            if (emustatus == 2)
            {
                char* melontitle = "Paused - melonDS " MELONDS_VERSION;
                SDL_SetWindowTitle(sdlwin, melontitle);
            }

            emupaused = true;
        }
    }

    emupaused = true;

    if (audio) SDL_CloseAudioDevice(audio);

    SDL_DestroyTexture(sdltex);
    SDL_DestroyRenderer(sdlrend);
    SDL_DestroyWindow(sdlwin);

    return (wxThread::ExitCode)0;
}

void EmuThread::ProcessEvents()
{
    bool running = (emustatus == 1);
    SDL_Event evt;

    while (SDL_PollEvent(&evt))
    {
        switch (evt.type)
        {
        case SDL_WINDOWEVENT:
            if (evt.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                if (parent) parent->Close();
                EmuExit();
                return;
            }
            if (evt.window.event != SDL_WINDOWEVENT_EXPOSED)
            {
                if (evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int w = evt.window.data1;
                    int h = evt.window.data2;

                    // SDL_SetWindowMinimumSize() doesn't seem to work on Linux. oh well
                    if ((w < 256) || (h < 384))
                    {
                        if (w < 256) w = 256;
                        if (h < 384) h = 384;
                        SDL_SetWindowSize(sdlwin, w, h);
                    }

                    int ratio = (w * 384) / h;
                    if (ratio > 256)
                    {
                        // borders on the sides

                        int screenw = (4 * (h/2)) / 3;
                        int gap = (w - screenw) / 2;

                        topdst.x = gap; topdst.y = 0;
                        topdst.w = screenw; topdst.h = h/2;

                        botdst.x = gap; botdst.y = h/2;
                        botdst.w = screenw; botdst.h = h/2;
                    }
                    else
                    {
                        // separator

                        int screenh = (3 * w) / 4;
                        int gap = h - (screenh*2);

                        topdst.x = 0; topdst.y = 0;
                        topdst.w = w; topdst.h = screenh;

                        botdst.x = 0; botdst.y = screenh + gap;
                        botdst.w = w; botdst.h = screenh;
                    }

                    Config::WindowWidth = w;
                    Config::WindowHeight = h;
                }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (!running) return;
            if (evt.button.button == SDL_BUTTON_LEFT)
            {
                if (evt.button.x >= botdst.x && evt.button.x < (botdst.x+botdst.w) &&
                    evt.button.y >= botdst.y && evt.button.y < (botdst.y+botdst.h))
                {
                    Touching = true;
                    NDS::PressKey(16+6);

                    int mx, my;
                    SDL_GetGlobalMouseState(&mx, &my);
                    txoffset = mx - evt.button.x;
                    tyoffset = my - evt.button.y;
                }
            }
            break;

        case SDL_KEYDOWN:
            if (!running) return;
            for (int i = 0; i < 10; i++)
                if (evt.key.keysym.scancode == Config::KeyMapping[i]) NDS::PressKey(i);
            if (evt.key.keysym.scancode == Config::KeyMapping[10]) NDS::PressKey(16);
            if (evt.key.keysym.scancode == Config::KeyMapping[11]) NDS::PressKey(17);
            if (evt.key.keysym.scancode == SDL_SCANCODE_F12) NDS::debug(0);
            if (evt.key.keysym.scancode == SDL_SCANCODE_TAB) limitfps = !limitfps;
            break;

        case SDL_KEYUP:
            if (!running) return;
            for (int i = 0; i < 10; i++)
                if (evt.key.keysym.scancode == Config::KeyMapping[i]) NDS::ReleaseKey(i);
            if (evt.key.keysym.scancode == Config::KeyMapping[10]) NDS::ReleaseKey(16);
            if (evt.key.keysym.scancode == Config::KeyMapping[11]) NDS::ReleaseKey(17);
            //if (evt.key.keysym.scancode == SDL_SCANCODE_TAB) limitfps = true;
            break;

        case SDL_JOYBUTTONDOWN:
            if (!running) return;
            if (evt.jbutton.which != parent->joyid) return;
            for (int i = 0; i < 10; i++)
                if (evt.jbutton.button == Config::JoyMapping[i]) NDS::PressKey(i);
            if (evt.jbutton.button == Config::JoyMapping[10]) NDS::PressKey(16);
            if (evt.jbutton.button == Config::JoyMapping[11]) NDS::PressKey(17);
            break;

        case SDL_JOYBUTTONUP:
            if (!running) return;
            if (evt.jbutton.which != parent->joyid) return;
            for (int i = 0; i < 10; i++)
                if (evt.jbutton.button == Config::JoyMapping[i]) NDS::ReleaseKey(i);
            if (evt.jbutton.button == Config::JoyMapping[10]) NDS::ReleaseKey(16);
            if (evt.jbutton.button == Config::JoyMapping[11]) NDS::ReleaseKey(17);
            break;

        case SDL_JOYHATMOTION:
            if (!running) return;
            if (evt.jhat.which != parent->joyid) return;
            if (evt.jhat.hat != 0) return;
            for (int i = 0; i < 12; i++)
            {
                int j = (i >= 10) ? (i+6) : i;
                if (Config::JoyMapping[i] == 0x101)
                    (evt.jhat.value & SDL_HAT_UP) ? NDS::PressKey(j) : NDS::ReleaseKey(j);
                else if (Config::JoyMapping[i] == 0x102)
                    (evt.jhat.value & SDL_HAT_RIGHT) ? NDS::PressKey(j) : NDS::ReleaseKey(j);
                else if (Config::JoyMapping[i] == 0x104)
                    (evt.jhat.value & SDL_HAT_DOWN) ? NDS::PressKey(j) : NDS::ReleaseKey(j);
                else if (Config::JoyMapping[i] == 0x108)
                    (evt.jhat.value & SDL_HAT_LEFT) ? NDS::PressKey(j) : NDS::ReleaseKey(j);
            }
            break;

        case SDL_JOYAXISMOTION:
            if (!running) return;
            if (evt.jaxis.which != parent->joyid) return;
            if (evt.jaxis.axis == 0)
            {
                if (evt.jaxis.value >= 16384) { NDS::PressKey(4); axismask |= 0x1; }
                else if (axismask & 0x1)      { NDS::ReleaseKey(4); axismask &= ~0x1; }
                if (evt.jaxis.value <= -16384) { NDS::PressKey(5); axismask |= 0x2; }
                else if (axismask & 0x2)       { NDS::ReleaseKey(5); axismask &= ~0x2; }
            }
            else if (evt.jaxis.axis == 1)
            {
                if (evt.jaxis.value >= 16384) { NDS::PressKey(7); axismask |= 0x4; }
                else if (axismask & 0x4)      { NDS::ReleaseKey(7); axismask &= ~0x4; }
                if (evt.jaxis.value <= -16384) { NDS::PressKey(6); axismask |= 0x8; }
                else if (axismask & 0x8)       { NDS::ReleaseKey(6); axismask &= ~0x8; }
            }
            break;
        }
    }

    if (Touching && running)
    {
        int mx, my;
        u32 btn = SDL_GetGlobalMouseState(&mx, &my);
        if (!(btn & SDL_BUTTON(SDL_BUTTON_LEFT)))
        {
            Touching = false;
            NDS::ReleaseKey(16+6);
            NDS::ReleaseScreen();
        }
        else
        {
            mx -= (txoffset + botdst.x);
            my -= (tyoffset + botdst.y);

            if (botdst.w != 256) mx = (mx * 256) / botdst.w;
            if (botdst.h != 192) my = (my * 192) / botdst.h;

            if (mx < 0)        mx = 0;
            else if (mx > 255) mx = 255;

            if (my < 0)        my = 0;
            else if (my > 191) my = 191;

            NDS::TouchScreen(mx, my);
        }
    }
}
