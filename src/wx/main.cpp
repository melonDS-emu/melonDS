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
#include "../NDS.h"
#include "../GPU.h"


const int DefaultKeyMapping[18] =
{
    15, 54,
    44, 40,
    22, 4, 26, 29,
    19, 20,
    0, 0, 0, 0, 0, 0,
    18, 14
};

int KeyMapping[18];
int JoyMapping[18];

bool Touching;

int WindowX, WindowY;


wxIMPLEMENT_APP(wxApp_melonDS);


bool wxApp_melonDS::OnInit()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL shat itself :(\n");
        return 1;
    }

    printf("melonDS " MELONDS_VERSION "\n" MELONDS_URL "\n");

    MainFrame* melon = new MainFrame();
    melon->Show(true);

    return true;
}


wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_OPENROM, MainFrame::OnOpenROM)
    EVT_PAINT(MainFrame::OnPaint)
wxEND_EVENT_TABLE()


MainFrame::MainFrame()
    : wxFrame(NULL, wxID_ANY, "melonDS")
{
    wxMenu* filemenu = new wxMenu();
    filemenu->Append(ID_OPENROM, "Open ROM...");
    filemenu->AppendSeparator();
    filemenu->Append(ID_EXIT, "Quit");

    wxMenu* systemmenu = new wxMenu();
    systemmenu->Append(ID_RUN, "Run");
    systemmenu->Append(ID_PAUSE, "Pause");
    systemmenu->AppendSeparator();
    systemmenu->Append(ID_RESET, "Reset");

    wxMenuBar* melonbar = new wxMenuBar();
    melonbar->Append(filemenu, "File");
    melonbar->Append(systemmenu, "System");

    SetMenuBar(melonbar);

    SetClientSize(256, 384);

    emumutex = new wxMutex();
    emucond = new wxCondition(*emumutex);

    emuthread = new EmuThread(this, emumutex, emucond);
    if (emuthread->Run() != wxTHREAD_NO_ERROR)
    {
        printf("thread shat itself :( giving up now\n");
        delete emuthread;
        return;
    }

    sdlwin = SDL_CreateWindowFrom(GetHandle());

    sdlrend = SDL_CreateRenderer(sdlwin, -1, SDL_RENDERER_ACCELERATED); // SDL_RENDERER_PRESENTVSYNC
    sdltex = SDL_CreateTexture(sdlrend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 384);

    NDS::Init();

    memcpy(KeyMapping, DefaultKeyMapping, 18*sizeof(int));
    memset(JoyMapping, 0, 18*sizeof(int));

    Touching = false;

    SDL_GetWindowPosition(sdlwin, &WindowX, &WindowY);
}

void MainFrame::OnOpenROM(wxCommandEvent& event)
{
    wxFileDialog opener(this, _("Open ROM"), "", "", "DS ROM (*.nds)|*.nds;*.srl|Any file|*.*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (opener.ShowModal() == wxID_CANCEL)
        return;

    wxString filename = opener.GetPath();
    NDS::LoadROM(filename.mb_str(), true);

    emuthread->EmuStatus = 1;
    emumutex->Lock();
    emucond->Signal();
    emumutex->Unlock();
}

void MainFrame::OnPaint(wxPaintEvent& event)
{
    /*wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    //

    delete gc;*/
}


EmuThread::EmuThread(MainFrame* parent, wxMutex* mutex, wxCondition* cond)
    : wxThread(wxTHREAD_JOINABLE)
{
    this->parent = parent;
    this->mutex = mutex;
    this->cond = cond;

    EmuStatus = 2;
}

EmuThread::~EmuThread()
{
}

wxThread::ExitCode EmuThread::Entry()
{
    mutex->Lock();

    for (;;)
    {
        cond->Wait();

        if (EmuStatus == 0)
            break;

        while (EmuStatus == 1)
        {
            NDS::RunFrame();

            SDL_Event evt;
            while (SDL_PollEvent(&evt))
            {
                switch (evt.type)
                {
                case SDL_WINDOWEVENT:
                    SDL_GetWindowPosition(parent->sdlwin, &WindowX, &WindowY);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (evt.button.y >= 192 && evt.button.button == SDL_BUTTON_LEFT)
                    {
                        Touching = true;
                        NDS::PressKey(16+6);
                    }
                    break;

                case SDL_KEYDOWN:
                    for (int i = 0; i < 10; i++)
                        if (evt.key.keysym.scancode == KeyMapping[i]) NDS::PressKey(i);
                    if (evt.key.keysym.scancode == KeyMapping[16]) NDS::PressKey(16);
                    if (evt.key.keysym.scancode == KeyMapping[17]) NDS::PressKey(17);
                    break;

                case SDL_KEYUP:
                    for (int i = 0; i < 10; i++)
                        if (evt.key.keysym.scancode == KeyMapping[i]) NDS::ReleaseKey(i);
                    if (evt.key.keysym.scancode == KeyMapping[16]) NDS::ReleaseKey(16);
                    if (evt.key.keysym.scancode == KeyMapping[17]) NDS::ReleaseKey(17);
                    break;
                }
            }

            if (Touching)
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

            void* pixels; int zorp;
            SDL_LockTexture(parent->sdltex, NULL, &pixels, &zorp);
            memcpy(pixels, GPU::Framebuffer, 256*384*4);
            SDL_UnlockTexture(parent->sdltex);

            SDL_SetRenderTarget(parent->sdlrend, parent->sdltex);
            SDL_RenderClear(parent->sdlrend);
            SDL_RenderCopy(parent->sdlrend, parent->sdltex, NULL, NULL);
            SDL_RenderPresent(parent->sdlrend);
        }
    }

    return (wxThread::ExitCode)0;
}
