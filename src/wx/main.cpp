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

#include "InputConfig.h"


bool Touching;

int WindowX, WindowY;
int WindowW, WindowH;


wxIMPLEMENT_APP_NO_MAIN(wxApp_melonDS);


int main(int argc, char** argv)
{
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


bool wxApp_melonDS::OnInit()
{
    printf("melonDS " MELONDS_VERSION "\n" MELONDS_URL "\n");

    Config::Load();

    MainFrame* melon = new MainFrame();
    melon->Show(true);

    return true;
}


wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_CLOSE(MainFrame::OnClose)

    EVT_MENU(ID_OPENROM, MainFrame::OnOpenROM)
    EVT_MENU(ID_EXIT, MainFrame::OnCloseFromMenu)

    EVT_MENU(ID_RUN, MainFrame::OnRun)
    EVT_MENU(ID_PAUSE, MainFrame::OnPause)
    EVT_MENU(ID_RESET, MainFrame::OnReset)

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
    settingsmenu->Append(ID_INPUTCONFIG, "Input");

    wxMenuBar* melonbar = new wxMenuBar();
    melonbar->Append(filemenu, "File");
    melonbar->Append(systemmenu, "System");
    melonbar->Append(settingsmenu, "Settings");

    SetMenuBar(melonbar);

    SetClientSize(256, 256);
    SetMinSize(GetSize());

    emuthread = new EmuThread(this);
    if (emuthread->Run() != wxTHREAD_NO_ERROR)
    {
        printf("thread shat itself :( giving up now\n");
        delete emuthread;
        return;
    }

    NDS::Init();
    rompath = wxEmptyString;
    GetMenuBar()->Enable(ID_PAUSE, false);
    GetMenuBar()->Enable(ID_RESET, false);

    Touching = false;

    joy = NULL;
    joyid = -1;
}

void MainFrame::CloseFromOutside()
{
    emuthread = NULL;
    Close();
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    if (emuthread)
    {
        emuthread->EmuExit();

        emuthread->Wait();
        delete emuthread;
        emuthread = NULL;
    }

    NDS::DeInit();

    if (joy)
    {
        SDL_JoystickClose(joy);
        joy = NULL;
        joyid = -1;
    }

    SDL_Quit();

    Destroy();
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
    NDS::LoadROM(rompath.mb_str(), true);

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
        NDS::LoadROM(rompath.mb_str(), true);
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


EmuThread::EmuThread(MainFrame* parent)
    : wxThread(wxTHREAD_JOINABLE)
{
    this->parent = parent;
}

EmuThread::~EmuThread()
{
}

wxThread::ExitCode EmuThread::Entry()
{
    emustatus = 3;
    emupaused = false;

    sdlwin = SDL_CreateWindow("melonDS " MELONDS_VERSION,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              256, 384,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    sdlrend = SDL_CreateRenderer(sdlwin, -1, SDL_RENDERER_ACCELERATED);// | SDL_RENDERER_PRESENTVSYNC);
    sdltex = SDL_CreateTexture(sdlrend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 384);

    axismask = 0;

    u32 nframes = 0;
    u32 lasttick = SDL_GetTicks();
    u32 fpslimitcount = 0;

    for (;;)
    {
        if (emustatus == 0) break;

        ProcessEvents();

        if (emustatus == 1)
        {
            u32 starttick = SDL_GetTicks();

            NDS::RunFrame();

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

            //SDL_RenderClear(sdlrend);
            SDL_RenderCopy(sdlrend, sdltex, NULL, NULL);
            SDL_RenderPresent(sdlrend);

            fpslimitcount++;
            if (fpslimitcount >= 3) fpslimitcount = 0;
            u32 frametime = (fpslimitcount == 0) ? 16 : 17;

            u32 endtick = SDL_GetTicks();
            u32 diff = endtick - starttick;
            if (diff < frametime)
                Sleep(frametime - diff);

            nframes++;
            if (nframes >= 30)
            {
                u32 tick = SDL_GetTicks();
                u32 diff = tick - lasttick;
                lasttick = tick;

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
            fpslimitcount = 0;

            emupaused = true;
            Sleep(50);

            if (emustatus == 2)
            {
                char* melontitle = "Paused - melonDS " MELONDS_VERSION;
                SDL_SetWindowTitle(sdlwin, melontitle);
            }
        }
    }

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
                wxThread* thread = parent->emuthread;
                parent->CloseFromOutside();
                EmuExit();
                delete thread;
                return;
            }
            if (evt.window.event != SDL_WINDOWEVENT_EXPOSED)
            {
                SDL_GetWindowPosition(sdlwin, &WindowX, &WindowY);
                SDL_GetWindowSize(sdlwin, &WindowW, &WindowH);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (!running) return;
            if (evt.button.y >= 192 && evt.button.button == SDL_BUTTON_LEFT)
            {
                Touching = true;
                NDS::PressKey(16+6);
            }
            break;

        case SDL_KEYDOWN:
            if (!running) return;
            for (int i = 0; i < 10; i++)
                if (evt.key.keysym.scancode == Config::KeyMapping[i]) NDS::PressKey(i);
            if (evt.key.keysym.scancode == Config::KeyMapping[10]) NDS::PressKey(16);
            if (evt.key.keysym.scancode == Config::KeyMapping[11]) NDS::PressKey(17);
            break;

        case SDL_KEYUP:
            if (!running) return;
            for (int i = 0; i < 10; i++)
                if (evt.key.keysym.scancode == Config::KeyMapping[i]) NDS::ReleaseKey(i);
            if (evt.key.keysym.scancode == Config::KeyMapping[10]) NDS::ReleaseKey(16);
            if (evt.key.keysym.scancode == Config::KeyMapping[11]) NDS::ReleaseKey(17);
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
            mx -= WindowX;
            my -= (WindowY + 192);

            if (mx < 0)        mx = 0;
            else if (mx > 255) mx = 255;

            if (my < 0)        my = 0;
            else if (my > 191) my = 191;

            NDS::TouchScreen(mx, my);
        }
    }
}
