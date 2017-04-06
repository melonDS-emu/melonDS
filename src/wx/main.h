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

#ifndef WX_MAIN_H
#define WX_MAIN_H

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <SDL2/SDL.h>

enum
{
    ID_OPENROM = 1,
    ID_EXIT,

    ID_RUN,
    ID_PAUSE,
    ID_RESET,

    ID_EMUCONFIG,
    ID_INPUTCONFIG,
};

class EmuThread;

class wxApp_melonDS : public wxApp
{
public:
    virtual bool OnInit();
    virtual int OnExit();

    EmuThread* emuthread;
};

class MainFrame : public wxFrame
{
public:
    MainFrame();

    SDL_Joystick* joy;
    SDL_JoystickID joyid;

    EmuThread* emuthread;

    wxString rompath;

private:
    wxDECLARE_EVENT_TABLE();

    void OnClose(wxCloseEvent& event);
    void OnCloseFromMenu(wxCommandEvent& event);
    void OnOpenROM(wxCommandEvent& event);

    void OnRun(wxCommandEvent& event);
    void OnPause(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);

    void OnEmuConfig(wxCommandEvent& event);
    void OnInputConfig(wxCommandEvent& event);

    void ProcessSDLEvents();
};

class EmuThread : public wxThread
{
public:
    EmuThread();
    ~EmuThread();

    void EmuRun() { emustatus = 1; emupaused = false; SDL_RaiseWindow(sdlwin); }
    void EmuPause() { emustatus = 2; while (!emupaused); }
    void EmuExit() { emustatus = 0; }

    bool EmuIsRunning() { return (emustatus == 1) || (emustatus == 2); }
    bool EmuIsPaused() { return (emustatus == 2) && emupaused; }

    MainFrame* parent;

protected:
    virtual ExitCode Entry();
    void ProcessEvents();

    SDL_Window* sdlwin;
    SDL_Renderer* sdlrend;
    SDL_Texture* sdltex;

    SDL_Rect topsrc, topdst;
    SDL_Rect botsrc, botdst;

    SDL_AudioDeviceID audio;

    bool Touching;
    int txoffset, tyoffset;

    void* texpixels;
    int texstride;

    u32 axismask;

    int emustatus;
    volatile bool emupaused;
};

#endif // WX_MAIN_H
