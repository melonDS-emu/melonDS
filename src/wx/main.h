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
};

class EmuThread;

class wxApp_melonDS : public wxApp
{
public:
    virtual bool OnInit();
};

class MainFrame : public wxFrame
{
public:
    MainFrame();

    SDL_Window* sdlwin;
    SDL_Renderer* sdlrend;
    SDL_Texture* sdltex;

private:
    wxDECLARE_EVENT_TABLE();

    void OnOpenROM(wxCommandEvent& event);

    void OnPaint(wxPaintEvent& event);

    EmuThread* emuthread;
    wxMutex* emumutex;
    wxCondition* emucond;
};

class EmuThread : public wxThread
{
public:
    EmuThread(MainFrame* parent, wxMutex* mutex, wxCondition* cond);
    ~EmuThread();

    u32 EmuStatus;

protected:
    virtual ExitCode Entry();

    MainFrame* parent;
    wxMutex* mutex;
    wxCondition* cond;
};

#endif // WX_MAIN_H
