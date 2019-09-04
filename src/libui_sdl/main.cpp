/*
    Copyright 2016-2019 Arisotura

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

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include "libui/ui.h"

#include "../OpenGLSupport.h"
#include "main_shaders.h"

#include "../types.h"
#include "../version.h"
#include "PlatformConfig.h"

#include "DlgEmuSettings.h"
#include "DlgInputConfig.h"
#include "DlgVideoSettings.h"
#include "DlgAudioSettings.h"
#include "DlgWifiSettings.h"

#include "../NDS.h"
#include "../GPU.h"
#include "../SPU.h"
#include "../Wifi.h"
#include "../Platform.h"
#include "../Config.h"

#include "../Savestate.h"

#include "OSD.h"


// savestate slot mapping
// 1-8: regular slots (quick access)
// '9': load/save arbitrary file
const int kSavestateNum[9] = {1, 2, 3, 4, 5, 6, 7, 8, 0};

const int kScreenSize[4] = {1, 2, 3, 4};
const int kScreenRot[4] = {0, 1, 2, 3};
const int kScreenGap[6] = {0, 1, 8, 64, 90, 128};
const int kScreenLayout[3] = {0, 1, 2};
const int kScreenSizing[4] = {0, 1, 2, 3};


char* EmuDirectory;


uiWindow* MainWindow;
uiArea* MainDrawArea;
uiAreaHandler MainDrawAreaHandler;

const u32 kGLVersions[] = {uiGLVersion(3,2), uiGLVersion(3,1), 0};
uiGLContext* GLContext;

int WindowWidth, WindowHeight;

uiMenuItem* MenuItem_SaveState;
uiMenuItem* MenuItem_LoadState;
uiMenuItem* MenuItem_UndoStateLoad;

uiMenuItem* MenuItem_SaveStateSlot[9];
uiMenuItem* MenuItem_LoadStateSlot[9];

uiMenuItem* MenuItem_Pause;
uiMenuItem* MenuItem_Reset;
uiMenuItem* MenuItem_Stop;

uiMenuItem* MenuItem_SavestateSRAMReloc;

uiMenuItem* MenuItem_ScreenRot[4];
uiMenuItem* MenuItem_ScreenGap[6];
uiMenuItem* MenuItem_ScreenLayout[3];
uiMenuItem* MenuItem_ScreenSizing[4];

uiMenuItem* MenuItem_ScreenFilter;
uiMenuItem* MenuItem_LimitFPS;
uiMenuItem* MenuItem_AudioSync;
uiMenuItem* MenuItem_ShowOSD;

SDL_Thread* EmuThread;
int EmuRunning;
volatile int EmuStatus;

bool RunningSomething;
char ROMPath[1024];
char SRAMPath[1024];
char PrevSRAMPath[1024]; // for savestate 'undo load'

bool SavestateLoaded;

bool Screen_UseGL;

bool ScreenDrawInited = false;
uiDrawBitmap* ScreenBitmap[2] = {NULL,NULL};

GLuint GL_ScreenShader[3];
GLuint GL_ScreenShaderAccel[3];
GLuint GL_ScreenShaderOSD[3];
struct
{
    float uScreenSize[2];
    u32 u3DScale;
    u32 uFilterMode;

} GL_ShaderConfig;
GLuint GL_ShaderConfigUBO;
GLuint GL_ScreenVertexArrayID, GL_ScreenVertexBufferID;
float GL_ScreenVertices[2 * 3*2 * 4]; // position/texcoord
GLuint GL_ScreenTexture;
bool GL_ScreenSizeDirty;

int GL_3DScale;

bool GL_VSyncStatus;

int ScreenGap = 0;
int ScreenLayout = 0;
int ScreenSizing = 0;
int ScreenRotation = 0;

int MainScreenPos[3];
int AutoScreenSizing;

uiRect TopScreenRect;
uiRect BottomScreenRect;
uiDrawMatrix TopScreenTrans;
uiDrawMatrix BottomScreenTrans;

bool Touching = false;

u32 KeyInputMask, JoyInputMask;
u32 KeyHotkeyMask, JoyHotkeyMask;
u32 HotkeyMask, LastHotkeyMask;
u32 HotkeyPress, HotkeyRelease;

#define HotkeyDown(hk)     (HotkeyMask & (1<<(hk)))
#define HotkeyPressed(hk)  (HotkeyPress & (1<<(hk)))
#define HotkeyReleased(hk) (HotkeyRelease & (1<<(hk)))

bool LidStatus;

int JoystickID;
SDL_Joystick* Joystick;

int AudioFreq;
float AudioSampleFrac;
SDL_AudioDeviceID AudioDevice, MicDevice;

SDL_cond* AudioSync;
SDL_mutex* AudioSyncLock;

u32 MicBufferLength = 2048;
s16 MicBuffer[2048];
u32 MicBufferReadPos, MicBufferWritePos;

u32 MicWavLength;
s16* MicWavBuffer;

void SetupScreenRects(int width, int height);

void TogglePause(void* blarg);
void Reset(void* blarg);

void SetupSRAMPath();

void SaveState(int slot);
void LoadState(int slot);
void UndoStateLoad();
void GetSavestateName(int slot, char* filename, int len);

void CreateMainWindow(bool opengl);
void DestroyMainWindow();
void RecreateMainWindow(bool opengl);



bool GLScreen_InitShader(GLuint* shader, const char* fs)
{
    if (!OpenGL_BuildShaderProgram(kScreenVS, fs, shader, "ScreenShader"))
        return false;

    glBindAttribLocation(shader[2], 0, "vPosition");
    glBindAttribLocation(shader[2], 1, "vTexcoord");
    glBindFragDataLocation(shader[2], 0, "oColor");

    if (!OpenGL_LinkShaderProgram(shader))
        return false;

    GLuint uni_id;

    uni_id = glGetUniformBlockIndex(shader[2], "uConfig");
    glUniformBlockBinding(shader[2], uni_id, 16);

    glUseProgram(shader[2]);
    uni_id = glGetUniformLocation(shader[2], "ScreenTex");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(shader[2], "_3DTex");
    glUniform1i(uni_id, 1);

    return true;
}

bool GLScreen_InitOSDShader(GLuint* shader)
{
    if (!OpenGL_BuildShaderProgram(kScreenVS_OSD, kScreenFS_OSD, shader, "ScreenShaderOSD"))
        return false;

    glBindAttribLocation(shader[2], 0, "vPosition");
    glBindFragDataLocation(shader[2], 0, "oColor");

    if (!OpenGL_LinkShaderProgram(shader))
        return false;

    GLuint uni_id;

    uni_id = glGetUniformBlockIndex(shader[2], "uConfig");
    glUniformBlockBinding(shader[2], uni_id, 16);

    glUseProgram(shader[2]);
    uni_id = glGetUniformLocation(shader[2], "OSDTex");
    glUniform1i(uni_id, 0);

    return true;
}

bool GLScreen_Init()
{
    GL_VSyncStatus = Config::ScreenVSync;

    // TODO: consider using epoxy?
    if (!OpenGL_Init())
        return false;

    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION); // version as a string
    printf("OpenGL: renderer: %s\n", renderer);
    printf("OpenGL: version: %s\n", version);

    if (!GLScreen_InitShader(GL_ScreenShader, kScreenFS))
        return false;
    if (!GLScreen_InitShader(GL_ScreenShaderAccel, kScreenFS_Accel))
        return false;
    if (!GLScreen_InitOSDShader(GL_ScreenShaderOSD))
        return false;

    memset(&GL_ShaderConfig, 0, sizeof(GL_ShaderConfig));

    glGenBuffers(1, &GL_ShaderConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, GL_ShaderConfigUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GL_ShaderConfig), &GL_ShaderConfig, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 16, GL_ShaderConfigUBO);

    glGenBuffers(1, &GL_ScreenVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_ScreenVertices), NULL, GL_STATIC_DRAW);

    glGenVertexArrays(1, &GL_ScreenVertexArrayID);
    glBindVertexArray(GL_ScreenVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(0));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(2*4));

    glGenTextures(1, &GL_ScreenTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GL_ScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256*3 + 1, 192*2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);

    GL_ScreenSizeDirty = true;

    return true;
}

void GLScreen_DeInit()
{
    glDeleteTextures(1, &GL_ScreenTexture);

    glDeleteVertexArrays(1, &GL_ScreenVertexArrayID);
    glDeleteBuffers(1, &GL_ScreenVertexBufferID);

    OpenGL_DeleteShaderProgram(GL_ScreenShader);
    OpenGL_DeleteShaderProgram(GL_ScreenShaderAccel);
    OpenGL_DeleteShaderProgram(GL_ScreenShaderOSD);
}

void GLScreen_DrawScreen()
{
    bool vsync = Config::ScreenVSync && !HotkeyDown(HK_FastForward);
    if (vsync != GL_VSyncStatus)
    {
        GL_VSyncStatus = vsync;
        uiGLSetVSync(vsync);
    }

    float scale = uiGLGetFramebufferScale(GLContext);

    glBindFramebuffer(GL_FRAMEBUFFER, uiGLGetFramebuffer(GLContext));

    if (GL_ScreenSizeDirty)
    {
        GL_ScreenSizeDirty = false;

        GL_ShaderConfig.uScreenSize[0] = WindowWidth;
        GL_ShaderConfig.uScreenSize[1] = WindowHeight;
        GL_ShaderConfig.u3DScale = GL_3DScale;

        glBindBuffer(GL_UNIFORM_BUFFER, GL_ShaderConfigUBO);
        void* unibuf = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
        if (unibuf) memcpy(unibuf, &GL_ShaderConfig, sizeof(GL_ShaderConfig));
        glUnmapBuffer(GL_UNIFORM_BUFFER);

        float scwidth, scheight;

        float x0, y0, x1, y1;
        float s0, s1, s2, s3;
        float t0, t1, t2, t3;

#define SETVERTEX(i, x, y, s, t) \
    GL_ScreenVertices[4*(i) + 0] = x; \
    GL_ScreenVertices[4*(i) + 1] = y; \
    GL_ScreenVertices[4*(i) + 2] = s; \
    GL_ScreenVertices[4*(i) + 3] = t;

        x0 = TopScreenRect.X;
        y0 = TopScreenRect.Y;
        x1 = TopScreenRect.X + TopScreenRect.Width;
        y1 = TopScreenRect.Y + TopScreenRect.Height;

        scwidth = 256;
        scheight = 192;

        switch (ScreenRotation)
        {
        case 0:
            s0 = 0; t0 = 0;
            s1 = scwidth; t1 = 0;
            s2 = 0; t2 = scheight;
            s3 = scwidth; t3 = scheight;
            break;

        case 1:
            s0 = 0; t0 = scheight;
            s1 = 0; t1 = 0;
            s2 = scwidth; t2 = scheight;
            s3 = scwidth; t3 = 0;
            break;

        case 2:
            s0 = scwidth; t0 = scheight;
            s1 = 0; t1 = scheight;
            s2 = scwidth; t2 = 0;
            s3 = 0; t3 = 0;
            break;

        case 3:
            s0 = scwidth; t0 = 0;
            s1 = scwidth; t1 = scheight;
            s2 = 0; t2 = 0;
            s3 = 0; t3 = scheight;
            break;
        }

        SETVERTEX(0, x0, y0, s0, t0);
        SETVERTEX(1, x1, y1, s3, t3);
        SETVERTEX(2, x1, y0, s1, t1);
        SETVERTEX(3, x0, y0, s0, t0);
        SETVERTEX(4, x0, y1, s2, t2);
        SETVERTEX(5, x1, y1, s3, t3);

        x0 = BottomScreenRect.X;
        y0 = BottomScreenRect.Y;
        x1 = BottomScreenRect.X + BottomScreenRect.Width;
        y1 = BottomScreenRect.Y + BottomScreenRect.Height;

        scwidth = 256;
        scheight = 192;

        switch (ScreenRotation)
        {
        case 0:
            s0 = 0; t0 = 192;
            s1 = scwidth; t1 = 192;
            s2 = 0; t2 = 192+scheight;
            s3 = scwidth; t3 = 192+scheight;
            break;

        case 1:
            s0 = 0; t0 = 192+scheight;
            s1 = 0; t1 = 192;
            s2 = scwidth; t2 = 192+scheight;
            s3 = scwidth; t3 = 192;
            break;

        case 2:
            s0 = scwidth; t0 = 192+scheight;
            s1 = 0; t1 = 192+scheight;
            s2 = scwidth; t2 = 192;
            s3 = 0; t3 = 192;
            break;

        case 3:
            s0 = scwidth; t0 = 192;
            s1 = scwidth; t1 = 192+scheight;
            s2 = 0; t2 = 192;
            s3 = 0; t3 = 192+scheight;
            break;
        }

        SETVERTEX(6, x0, y0, s0, t0);
        SETVERTEX(7, x1, y1, s3, t3);
        SETVERTEX(8, x1, y0, s1, t1);
        SETVERTEX(9, x0, y0, s0, t0);
        SETVERTEX(10, x0, y1, s2, t2);
        SETVERTEX(11, x1, y1, s3, t3);

#undef SETVERTEX

        glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_ScreenVertices), GL_ScreenVertices);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, WindowWidth*scale, WindowHeight*scale);

    if (GPU3D::Renderer == 0)
        OpenGL_UseShaderProgram(GL_ScreenShader);
    else
        OpenGL_UseShaderProgram(GL_ScreenShaderAccel);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (RunningSomething)
    {
        int frontbuf = GPU::FrontBuffer;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GL_ScreenTexture);

        if (GPU::Framebuffer[frontbuf][0] && GPU::Framebuffer[frontbuf][1])
        {
            if (GPU3D::Renderer == 0)
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][0]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
            }
            else
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256*3 + 1, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][0]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256*3 + 1, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
            }
        }

        glActiveTexture(GL_TEXTURE1);
        if (GPU3D::Renderer != 0)
            GPU3D::GLRenderer::SetupAccelFrame();

        glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
        glBindVertexArray(GL_ScreenVertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, 4*3);
    }

    OpenGL_UseShaderProgram(GL_ScreenShaderOSD);
    OSD::Update(true, NULL);

    glFlush();
    uiGLSwapBuffers(GLContext);
}

void MicLoadWav(char* name)
{
    SDL_AudioSpec format;
    memset(&format, 0, sizeof(SDL_AudioSpec));

    if (MicWavBuffer) delete[] MicWavBuffer;
    MicWavBuffer = NULL;
    MicWavLength = 0;

    u8* buf;
    u32 len;
    if (!SDL_LoadWAV(name, &format, &buf, &len))
        return;

    const u64 dstfreq = 44100;

    if (format.format == AUDIO_S16 || format.format == AUDIO_U16)
    {
        int srcinc = format.channels;
        len /= (2 * srcinc);

        MicWavLength = (len * dstfreq) / format.freq;
        if (MicWavLength < 735) MicWavLength = 735;
        MicWavBuffer = new s16[MicWavLength];

        float res_incr = len / (float)MicWavLength;
        float res_timer = 0;
        int res_pos = 0;

        for (int i = 0; i < MicWavLength; i++)
        {
            u16 val = ((u16*)buf)[res_pos];
            if (SDL_AUDIO_ISUNSIGNED(format.format)) val ^= 0x8000;

            MicWavBuffer[i] = val;

            res_timer += res_incr;
            while (res_timer >= 1.0)
            {
                res_timer -= 1.0;
                res_pos += srcinc;
            }
        }
    }
    else if (format.format == AUDIO_S8 || format.format == AUDIO_U8)
    {
        int srcinc = format.channels;
        len /= srcinc;

        MicWavLength = (len * dstfreq) / format.freq;
        if (MicWavLength < 735) MicWavLength = 735;
        MicWavBuffer = new s16[MicWavLength];

        float res_incr = len / (float)MicWavLength;
        float res_timer = 0;
        int res_pos = 0;

        for (int i = 0; i < MicWavLength; i++)
        {
            u16 val = buf[res_pos] << 8;
            if (SDL_AUDIO_ISUNSIGNED(format.format)) val ^= 0x8000;

            MicWavBuffer[i] = val;

            res_timer += res_incr;
            while (res_timer >= 1.0)
            {
                res_timer -= 1.0;
                res_pos += srcinc;
            }
        }
    }
    else
        printf("bad WAV format %08X\n", format.format);

    SDL_FreeWAV(buf);
}

void AudioCallback(void* data, Uint8* stream, int len)
{
    len /= (sizeof(s16) * 2);

    // resample incoming audio to match the output sample rate

    float f_len_in = (len * 32823.6328125) / (float)AudioFreq;
    f_len_in += AudioSampleFrac;
    int len_in = (int)floor(f_len_in);
    AudioSampleFrac = f_len_in - len_in;

    s16 buf_in[1024*2];
    s16* buf_out = (s16*)stream;

    int num_in;
    int num_out = len;

    SDL_LockMutex(AudioSyncLock);
    num_in = SPU::ReadOutput(buf_in, len_in);
    SDL_CondSignal(AudioSync);
    SDL_UnlockMutex(AudioSyncLock);

    if (num_in < 1)
    {
        memset(stream, 0, len*sizeof(s16)*2);
        return;
    }

    int margin = 6;
    if (num_in < len_in-margin)
    {
        int last = num_in-1;
        if (last < 0) last = 0;

        for (int i = num_in; i < len_in-margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = len_in-margin;
    }

    float res_incr = num_in / (float)num_out;
    float res_timer = 0;
    int res_pos = 0;

    int volume = Config::AudioVolume;

    for (int i = 0; i < len; i++)
    {
        buf_out[i*2  ] = (buf_in[res_pos*2  ] * volume) >> 8;
        buf_out[i*2+1] = (buf_in[res_pos*2+1] * volume) >> 8;

        /*s16 s_l = buf_in[res_pos*2  ];
        s16 s_r = buf_in[res_pos*2+1];

        float a = res_timer;
        float b = 1.0 - a;
        s_l = (s_l * a) + (buf_in[(res_pos-1)*2  ] * b);
        s_r = (s_r * a) + (buf_in[(res_pos-1)*2+1] * b);

        buf_out[i*2  ] = (s_l * volume) >> 8;
        buf_out[i*2+1] = (s_r * volume) >> 8;*/

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}

void MicCallback(void* data, Uint8* stream, int len)
{
    if (Config::MicInputType != 1) return;

    s16* input = (s16*)stream;
    len /= sizeof(s16);

    if ((MicBufferWritePos + len) > MicBufferLength)
    {
        u32 len1 = MicBufferLength - MicBufferWritePos;
        memcpy(&MicBuffer[MicBufferWritePos], &input[0], len1*sizeof(s16));
        memcpy(&MicBuffer[0], &input[len1], (len - len1)*sizeof(s16));
        MicBufferWritePos = len - len1;
    }
    else
    {
        memcpy(&MicBuffer[MicBufferWritePos], input, len*sizeof(s16));
        MicBufferWritePos += len;
    }
}

void FeedMicInput()
{
    int type = Config::MicInputType;
    bool cmd = HotkeyDown(HK_Mic);

    if ((type != 1 && !cmd) ||
        (type == 1 && MicBufferLength == 0) ||
        (type == 3 && MicWavBuffer == NULL))
    {
        type = 0;
        MicBufferReadPos = 0;
    }

    switch (type)
    {
    case 0: // no mic
        NDS::MicInputFrame(NULL, 0);
        break;

    case 1: // host mic
        if ((MicBufferReadPos + 735) > MicBufferLength)
        {
            s16 tmp[735];
            u32 len1 = MicBufferLength - MicBufferReadPos;
            memcpy(&tmp[0], &MicBuffer[MicBufferReadPos], len1*sizeof(s16));
            memcpy(&tmp[len1], &MicBuffer[0], (735 - len1)*sizeof(s16));

            NDS::MicInputFrame(tmp, 735);
            MicBufferReadPos = 735 - len1;
        }
        else
        {
            NDS::MicInputFrame(&MicBuffer[MicBufferReadPos], 735);
            MicBufferReadPos += 735;
        }
        break;

    case 2: // white noise
        {
            s16 tmp[735];
            for (int i = 0; i < 735; i++) tmp[i] = rand() & 0xFFFF;
            NDS::MicInputFrame(tmp, 735);
        }
        break;

    case 3: // WAV
        if ((MicBufferReadPos + 735) > MicWavLength)
        {
            s16 tmp[735];
            u32 len1 = MicWavLength - MicBufferReadPos;
            memcpy(&tmp[0], &MicWavBuffer[MicBufferReadPos], len1*sizeof(s16));
            memcpy(&tmp[len1], &MicWavBuffer[0], (735 - len1)*sizeof(s16));

            NDS::MicInputFrame(tmp, 735);
            MicBufferReadPos = 735 - len1;
        }
        else
        {
            NDS::MicInputFrame(&MicWavBuffer[MicBufferReadPos], 735);
            MicBufferReadPos += 735;
        }
        break;
    }
}

void OpenJoystick()
{
    if (Joystick) SDL_JoystickClose(Joystick);

    int num = SDL_NumJoysticks();
    if (num < 1)
    {
        Joystick = NULL;
        return;
    }

    if (JoystickID >= num)
        JoystickID = 0;

    Joystick = SDL_JoystickOpen(JoystickID);
}

bool JoystickButtonDown(int val)
{
    if (val == -1) return false;

    bool hasbtn = ((val & 0xFFFF) != 0xFFFF);

    if (hasbtn)
    {
        if (val & 0x100)
        {
            int hatnum = (val >> 4) & 0xF;
            int hatdir = val & 0xF;
            Uint8 hatval = SDL_JoystickGetHat(Joystick, hatnum);

            bool pressed = false;
            if      (hatdir == 0x1) pressed = (hatval & SDL_HAT_UP);
            else if (hatdir == 0x4) pressed = (hatval & SDL_HAT_DOWN);
            else if (hatdir == 0x2) pressed = (hatval & SDL_HAT_RIGHT);
            else if (hatdir == 0x8) pressed = (hatval & SDL_HAT_LEFT);

            if (pressed) return true;
        }
        else
        {
            int btnnum = val & 0xFFFF;
            Uint8 btnval = SDL_JoystickGetButton(Joystick, btnnum);

            if (btnval) return true;
        }
    }

    if (val & 0x10000)
    {
        int axisnum = (val >> 24) & 0xF;
        int axisdir = (val >> 20) & 0xF;
        Sint16 axisval = SDL_JoystickGetAxis(Joystick, axisnum);

        switch (axisdir)
        {
        case 0: // positive
            if (axisval > 16384) return true;
            break;

        case 1: // negative
            if (axisval < -16384) return true;
            break;

        case 2: // trigger
            if (axisval > 0) return true;
            break;
        }
    }

    return false;
}

void ProcessInput()
{
    SDL_JoystickUpdate();

    if (Joystick)
    {
        if (!SDL_JoystickGetAttached(Joystick))
        {
            SDL_JoystickClose(Joystick);
            Joystick = NULL;
        }
    }
    if (!Joystick && (SDL_NumJoysticks() > 0))
    {
        JoystickID = Config::JoystickID;
        OpenJoystick();
    }

    JoyInputMask = 0xFFF;
    for (int i = 0; i < 12; i++)
        if (JoystickButtonDown(Config::JoyMapping[i]))
            JoyInputMask &= ~(1<<i);

    JoyHotkeyMask = 0;
    for (int i = 0; i < HK_MAX; i++)
        if (JoystickButtonDown(Config::HKJoyMapping[i]))
            JoyHotkeyMask |= (1<<i);

    HotkeyMask = KeyHotkeyMask | JoyHotkeyMask;
    HotkeyPress = HotkeyMask & ~LastHotkeyMask;
    HotkeyRelease = LastHotkeyMask & ~HotkeyMask;
    LastHotkeyMask = HotkeyMask;
}

bool JoyButtonPressed(int btnid, int njoybuttons, Uint8* joybuttons, Uint32 hat)
{
    if (btnid < 0) return false;

    hat &= ~(hat >> 4);

    bool pressed = false;
    if (btnid == 0x101) // up
        pressed = (hat & SDL_HAT_UP);
    else if (btnid == 0x104) // down
        pressed = (hat & SDL_HAT_DOWN);
    else if (btnid == 0x102) // right
        pressed = (hat & SDL_HAT_RIGHT);
    else if (btnid == 0x108) // left
        pressed = (hat & SDL_HAT_LEFT);
    else if (btnid < njoybuttons)
        pressed = (joybuttons[btnid] & ~(joybuttons[btnid] >> 1)) & 0x01;

    return pressed;
}

bool JoyButtonHeld(int btnid, int njoybuttons, Uint8* joybuttons, Uint32 hat)
{
    if (btnid < 0) return false;

    bool pressed = false;
    if (btnid == 0x101) // up
        pressed = (hat & SDL_HAT_UP);
    else if (btnid == 0x104) // down
        pressed = (hat & SDL_HAT_DOWN);
    else if (btnid == 0x102) // right
        pressed = (hat & SDL_HAT_RIGHT);
    else if (btnid == 0x108) // left
        pressed = (hat & SDL_HAT_LEFT);
    else if (btnid < njoybuttons)
        pressed = joybuttons[btnid] & 0x01;

    return pressed;
}

void UpdateWindowTitle(void* data)
{
    if (EmuStatus == 0) return;
    uiWindowSetTitle(MainWindow, (const char*)data);
}

void UpdateFPSLimit(void* data)
{
    uiMenuItemSetChecked(MenuItem_LimitFPS, Config::LimitFPS==1);
}

int EmuThreadFunc(void* burp)
{
    NDS::Init();

    MainScreenPos[0] = 0;
    MainScreenPos[1] = 0;
    MainScreenPos[2] = 0;
    AutoScreenSizing = 0;

    if (Screen_UseGL)
    {
        uiGLMakeContextCurrent(GLContext);
        GPU3D::InitRenderer(true);
        uiGLMakeContextCurrent(NULL);
    }
    else
    {
        GPU3D::InitRenderer(false);
    }

    Touching = false;
    KeyInputMask = 0xFFF;
    JoyInputMask = 0xFFF;
    KeyHotkeyMask = 0;
    JoyHotkeyMask = 0;
    HotkeyMask = 0;
    LastHotkeyMask = 0;
    LidStatus = false;

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;
    u64 perfcount = SDL_GetPerformanceCounter();
    u64 perffreq = SDL_GetPerformanceFrequency();
    float samplesleft = 0;
    u32 nsamples = 0;
    char melontitle[100];

    while (EmuRunning != 0)
    {
        ProcessInput();

        if (HotkeyPressed(HK_FastForwardToggle))
        {
            Config::LimitFPS = !Config::LimitFPS;
            uiQueueMain(UpdateFPSLimit, NULL);
        }
        // TODO: similar hotkeys for video/audio sync?

        if (HotkeyPressed(HK_Pause)) uiQueueMain(TogglePause, NULL);
        if (HotkeyPressed(HK_Reset)) uiQueueMain(Reset, NULL);

        if (EmuRunning == 1)
        {
            EmuStatus = 1;

            // process input and hotkeys
            NDS::SetKeyMask(KeyInputMask & JoyInputMask);

            if (HotkeyPressed(HK_Lid))
            {
                LidStatus = !LidStatus;
                NDS::SetLidClosed(LidStatus);
                OSD::AddMessage(0, LidStatus ? "Lid closed" : "Lid opened");
            }

            // microphone input
            FeedMicInput();

            if (Screen_UseGL)
            {
                uiGLBegin(GLContext);
                uiGLMakeContextCurrent(GLContext);
            }

            // auto screen layout
            {
                MainScreenPos[2] = MainScreenPos[1];
                MainScreenPos[1] = MainScreenPos[0];
                MainScreenPos[0] = NDS::PowerControl9 >> 15;

                int guess;
                if (MainScreenPos[0] == MainScreenPos[2] &&
                    MainScreenPos[0] != MainScreenPos[1])
                {
                    // constant flickering, likely displaying 3D on both screens
                    // TODO: when both screens are used for 2D only...???
                    guess = 0;
                }
                else
                {
                    if (MainScreenPos[0] == 1)
                        guess = 1;
                    else
                        guess = 2;
                }

                if (guess != AutoScreenSizing)
                {
                    AutoScreenSizing = guess;
                    SetupScreenRects(WindowWidth, WindowHeight);
                }
            }

            // emulate
            u32 nlines = NDS::RunFrame();

            if (EmuRunning == 0) break;

            if (Screen_UseGL)
            {
                GLScreen_DrawScreen();
                uiGLEnd(GLContext);
            }
            uiAreaQueueRedrawAll(MainDrawArea);

            bool fastforward = HotkeyDown(HK_FastForward);

            if (Config::AudioSync && !fastforward)
            {
                SDL_LockMutex(AudioSyncLock);
                while (SPU::GetOutputSize() > 1024)
                {
                    int ret = SDL_CondWaitTimeout(AudioSync, AudioSyncLock, 500);
                    if (ret == SDL_MUTEX_TIMEDOUT) break;
                }
                SDL_UnlockMutex(AudioSyncLock);
            }
            else
            {
                // ensure the audio FIFO doesn't overflow
                //SPU::TrimOutput();
            }

            float framerate = (1000.0f * nlines) / (60.0f * 263.0f);

            {
                u32 curtick = SDL_GetTicks();
                u32 delay = curtick - lasttick;

                bool limitfps = Config::LimitFPS && !fastforward;
                if (limitfps)
                {
                    float wantedtickF = starttick + (framerate * (fpslimitcount+1));
                    u32 wantedtick = (u32)ceil(wantedtickF);
                    if (curtick < wantedtick) SDL_Delay(wantedtick - curtick);

                    lasttick = SDL_GetTicks();
                    fpslimitcount++;
                    if ((abs(wantedtickF - (float)wantedtick) < 0.001312) || (fpslimitcount > 60))
                    {
                        fpslimitcount = 0;
                        nsamples = 0;
                        starttick = lasttick;
                    }
                }
            }

            nframes++;
            if (nframes >= 30)
            {
                u32 tick = SDL_GetTicks();
                u32 diff = tick - lastmeasuretick;
                lastmeasuretick = tick;

                u32 fps;
                if (diff < 1) fps = 77777;
                else fps = (nframes * 1000) / diff;
                nframes = 0;

                float fpstarget;
                if (framerate < 1) fpstarget = 999;
                else fpstarget = 1000.0f/framerate;

                sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, fpstarget);
                uiQueueMain(UpdateWindowTitle, melontitle);
            }
        }
        else
        {
            // paused
            nframes = 0;
            lasttick = SDL_GetTicks();
            starttick = lasttick;
            lastmeasuretick = lasttick;
            fpslimitcount = 0;

            if (EmuRunning == 2)
            {
                if (Screen_UseGL)
                {
                    uiGLBegin(GLContext);
                    uiGLMakeContextCurrent(GLContext);
                    GLScreen_DrawScreen();
                    uiGLEnd(GLContext);
                }
                uiAreaQueueRedrawAll(MainDrawArea);
            }

            if (Screen_UseGL) uiGLMakeContextCurrent(NULL);

            EmuStatus = EmuRunning;

            SDL_Delay(100);
        }
    }

    EmuStatus = 0;

    if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);

    NDS::DeInit();
    Platform::LAN_DeInit();

    if (Screen_UseGL)
    {
        OSD::DeInit(true);
        GLScreen_DeInit();
    }
    else
        OSD::DeInit(false);

    if (Screen_UseGL) uiGLMakeContextCurrent(NULL);

    return 44203;
}

void StopEmuThread()
{
    EmuRunning = 0;
    SDL_WaitThread(EmuThread, NULL);
}


void OnAreaDraw(uiAreaHandler* handler, uiArea* area, uiAreaDrawParams* params)
{
    if (!ScreenDrawInited)
    {
        if (ScreenBitmap[0]) uiDrawFreeBitmap(ScreenBitmap[0]);
        if (ScreenBitmap[1]) uiDrawFreeBitmap(ScreenBitmap[1]);

        ScreenDrawInited = true;
        ScreenBitmap[0] = uiDrawNewBitmap(params->Context, 256, 192, 0);
        ScreenBitmap[1] = uiDrawNewBitmap(params->Context, 256, 192, 0);
    }

    int frontbuf = GPU::FrontBuffer;
    if (!ScreenBitmap[0] || !ScreenBitmap[1]) return;
    if (!GPU::Framebuffer[frontbuf][0] || !GPU::Framebuffer[frontbuf][1]) return;

    uiRect top = {0, 0, 256, 192};
    uiRect bot = {0, 0, 256, 192};

    uiDrawBitmapUpdate(ScreenBitmap[0], GPU::Framebuffer[frontbuf][0]);
    uiDrawBitmapUpdate(ScreenBitmap[1], GPU::Framebuffer[frontbuf][1]);

    uiDrawSave(params->Context);
    uiDrawTransform(params->Context, &TopScreenTrans);
    uiDrawBitmapDraw(params->Context, ScreenBitmap[0], &top, &TopScreenRect, Config::ScreenFilter==1);
    uiDrawRestore(params->Context);

    uiDrawSave(params->Context);
    uiDrawTransform(params->Context, &BottomScreenTrans);
    uiDrawBitmapDraw(params->Context, ScreenBitmap[1], &bot, &BottomScreenRect, Config::ScreenFilter==1);
    uiDrawRestore(params->Context);

    OSD::Update(false, params);
}

void OnAreaMouseEvent(uiAreaHandler* handler, uiArea* area, uiAreaMouseEvent* evt)
{
    int x = (int)evt->X;
    int y = (int)evt->Y;

    if (Touching && (evt->Up == 1))
    {
        Touching = false;
        NDS::ReleaseKey(16+6);
        NDS::ReleaseScreen();
    }
    else if (!Touching && (evt->Down == 1) &&
             (x >= BottomScreenRect.X) && (y >= BottomScreenRect.Y) &&
             (x < (BottomScreenRect.X+BottomScreenRect.Width)) && (y < (BottomScreenRect.Y+BottomScreenRect.Height)))
    {
        Touching = true;
        NDS::PressKey(16+6);
    }

    if (Touching)
    {
        x -= BottomScreenRect.X;
        y -= BottomScreenRect.Y;

        if (ScreenRotation == 0 || ScreenRotation == 2)
        {
            if (BottomScreenRect.Width != 256)
                x = (x * 256) / BottomScreenRect.Width;
            if (BottomScreenRect.Height != 192)
                y = (y * 192) / BottomScreenRect.Height;

            if (ScreenRotation == 2)
            {
                x = 255 - x;
                y = 191 - y;
            }
        }
        else
        {
            if (BottomScreenRect.Width != 192)
                x = (x * 192) / BottomScreenRect.Width;
            if (BottomScreenRect.Height != 256)
                y = (y * 256) / BottomScreenRect.Height;

            if (ScreenRotation == 1)
            {
                int tmp = x;
                x = y;
                y = 191 - tmp;
            }
            else
            {
                int tmp = x;
                x = 255 - y;
                y = tmp;
            }
        }

        // clamp
        if (x < 0) x = 0;
        else if (x > 255) x = 255;
        if (y < 0) y = 0;
        else if (y > 191) y = 191;

        // TODO: take advantage of possible extra precision when possible? (scaled window for example)
        NDS::TouchScreen(x, y);
    }
}

void OnAreaMouseCrossed(uiAreaHandler* handler, uiArea* area, int left)
{
}

void OnAreaDragBroken(uiAreaHandler* handler, uiArea* area)
{
}

bool EventMatchesKey(uiAreaKeyEvent* evt, int val, bool checkmod)
{
    if (val == -1) return false;

    int key = val & 0xFFFF;
    int mod = val >> 16;
    return evt->Scancode == key && (!checkmod || evt->Modifiers == mod);
}

int OnAreaKeyEvent(uiAreaHandler* handler, uiArea* area, uiAreaKeyEvent* evt)
{
    // TODO: release all keys if the window loses focus? or somehow global key input?
    if (evt->Scancode == 0x38) // ALT
        return 0;
    if (evt->Modifiers == 0x2) // ALT+key
        return 0;

    if (evt->Up)
    {
        for (int i = 0; i < 12; i++)
            if (EventMatchesKey(evt, Config::KeyMapping[i], false))
                KeyInputMask |= (1<<i);

        for (int i = 0; i < HK_MAX; i++)
            if (EventMatchesKey(evt, Config::HKKeyMapping[i], true))
                KeyHotkeyMask &= ~(1<<i);
    }
    else if (!evt->Repeat)
    {
        // TODO, eventually: make savestate keys configurable?
        // F keys: 3B-44, 57-58 | SHIFT: mod. 0x4
        if (evt->Scancode >= 0x3B && evt->Scancode <= 0x42) // F1-F8, quick savestate
        {
            if      (evt->Modifiers == 0x4) SaveState(1 + (evt->Scancode - 0x3B));
            else if (evt->Modifiers == 0x0) LoadState(1 + (evt->Scancode - 0x3B));
        }
        else if (evt->Scancode == 0x43) // F9, savestate from/to file
        {
            if      (evt->Modifiers == 0x4) SaveState(0);
            else if (evt->Modifiers == 0x0) LoadState(0);
        }
        else if (evt->Scancode == 0x58) // F12, undo savestate
        {
            if (evt->Modifiers == 0x0) UndoStateLoad();
        }

        for (int i = 0; i < 12; i++)
            if (EventMatchesKey(evt, Config::KeyMapping[i], false))
                KeyInputMask &= ~(1<<i);

        for (int i = 0; i < HK_MAX; i++)
            if (EventMatchesKey(evt, Config::HKKeyMapping[i], true))
                KeyHotkeyMask |= (1<<i);

        // REMOVE ME
        //if (evt->Scancode == 0x57) // F11
        //    NDS::debug(0);
    }

    return 1;
}

void SetupScreenRects(int width, int height)
{
    bool horizontal = false;
    bool sideways = false;

    if (ScreenRotation == 1 || ScreenRotation == 3)
        sideways = true;

    if (ScreenLayout == 2) horizontal = true;
    else if (ScreenLayout == 0)
    {
        if (sideways)
            horizontal = true;
    }

    int sizemode;
    if (ScreenSizing == 3)
        sizemode = AutoScreenSizing;
    else
        sizemode = ScreenSizing;

    int screenW, screenH, gap;
    if (sideways)
    {
        screenW = 192;
        screenH = 256;
    }
    else
    {
        screenW = 256;
        screenH = 192;
    }

    gap = ScreenGap;

    uiRect *topscreen, *bottomscreen;
    if (ScreenRotation == 1 || ScreenRotation == 2)
    {
        topscreen = &BottomScreenRect;
        bottomscreen = &TopScreenRect;
    }
    else
    {
        topscreen = &TopScreenRect;
        bottomscreen = &BottomScreenRect;
    }

    if (horizontal)
    {
        // side-by-side

        int heightreq;
        int startX = 0;

        width -= gap;

        if (sizemode == 0) // even
        {
            heightreq = (width * screenH) / (screenW*2);
            if (heightreq > height)
            {
                int newwidth = (height * width) / heightreq;
                startX = (width - newwidth) / 2;
                heightreq = height;
                width = newwidth;
            }
        }
        else // emph. top/bottom
        {
            heightreq = ((width - screenW) * screenH) / screenW;
            if (heightreq > height)
            {
                int newwidth = ((height * (width - screenW)) / heightreq) + screenW;
                startX = (width - newwidth) / 2;
                heightreq = height;
                width = newwidth;
            }
        }

        if (sizemode == 2)
        {
            topscreen->Width = screenW;
            topscreen->Height = screenH;
        }
        else
        {
            topscreen->Width = (sizemode==0) ? (width / 2) : (width - screenW);
            topscreen->Height = heightreq;
        }
        topscreen->X = startX;
        topscreen->Y = ((height - heightreq) / 2) + (heightreq - topscreen->Height);

        bottomscreen->X = topscreen->X + topscreen->Width + gap;

        if (sizemode == 1)
        {
            bottomscreen->Width = screenW;
            bottomscreen->Height = screenH;
        }
        else
        {
            bottomscreen->Width = width - topscreen->Width;
            bottomscreen->Height = heightreq;
        }
        bottomscreen->Y = ((height - heightreq) / 2) + (heightreq - bottomscreen->Height);
    }
    else
    {
        // top then bottom

        int widthreq;
        int startY = 0;

        height -= gap;

        if (sizemode == 0) // even
        {
            widthreq = (height * screenW) / (screenH*2);
            if (widthreq > width)
            {
                int newheight = (width * height) / widthreq;
                startY = (height - newheight) / 2;
                widthreq = width;
                height = newheight;
            }
        }
        else // emph. top/bottom
        {
            widthreq = ((height - screenH) * screenW) / screenH;
            if (widthreq > width)
            {
                int newheight = ((width * (height - screenH)) / widthreq) + screenH;
                startY = (height - newheight) / 2;
                widthreq = width;
                height = newheight;
            }
        }

        if (sizemode == 2)
        {
            topscreen->Width = screenW;
            topscreen->Height = screenH;
        }
        else
        {
            topscreen->Width = widthreq;
            topscreen->Height = (sizemode==0) ? (height / 2) : (height - screenH);
        }
        topscreen->Y = startY;
        topscreen->X = (width - topscreen->Width) / 2;

        bottomscreen->Y = topscreen->Y + topscreen->Height + gap;

        if (sizemode == 1)
        {
            bottomscreen->Width = screenW;
            bottomscreen->Height = screenH;
        }
        else
        {
            bottomscreen->Width = widthreq;
            bottomscreen->Height = height - topscreen->Height;
        }
        bottomscreen->X = (width - bottomscreen->Width) / 2;
    }

    // setup matrices for potential rotation

    uiDrawMatrixSetIdentity(&TopScreenTrans);
    uiDrawMatrixSetIdentity(&BottomScreenTrans);

    switch (ScreenRotation)
    {
    case 1: // 90°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, M_PI/2.0f);
            uiDrawMatrixScale(&TopScreenTrans, 0, 0,
                              TopScreenRect.Width/(double)TopScreenRect.Height,
                              TopScreenRect.Height/(double)TopScreenRect.Width);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X+TopScreenRect.Width, TopScreenRect.Y);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, M_PI/2.0f);
            uiDrawMatrixScale(&BottomScreenTrans, 0, 0,
                              BottomScreenRect.Width/(double)BottomScreenRect.Height,
                              BottomScreenRect.Height/(double)BottomScreenRect.Width);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X+BottomScreenRect.Width, BottomScreenRect.Y);
        }
        break;

    case 2: // 180°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, M_PI);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X+TopScreenRect.Width, TopScreenRect.Y+TopScreenRect.Height);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, M_PI);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X+BottomScreenRect.Width, BottomScreenRect.Y+BottomScreenRect.Height);
        }
        break;

    case 3: // 270°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, -M_PI/2.0f);
            uiDrawMatrixScale(&TopScreenTrans, 0, 0,
                              TopScreenRect.Width/(double)TopScreenRect.Height,
                              TopScreenRect.Height/(double)TopScreenRect.Width);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X, TopScreenRect.Y+TopScreenRect.Height);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, -M_PI/2.0f);
            uiDrawMatrixScale(&BottomScreenTrans, 0, 0,
                              BottomScreenRect.Width/(double)BottomScreenRect.Height,
                              BottomScreenRect.Height/(double)BottomScreenRect.Width);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X, BottomScreenRect.Y+BottomScreenRect.Height);
        }
        break;
    }

    GL_ScreenSizeDirty = true;
}

void SetMinSize(int w, int h)
{
    int cw, ch;
    uiWindowContentSize(MainWindow, &cw, &ch);

    uiControlSetMinSize(uiControl(MainDrawArea), w, h);
    if ((cw < w) || (ch < h))
    {
        if (cw < w) cw = w;
        if (ch < h) ch = h;
        uiWindowSetContentSize(MainWindow, cw, ch);
    }
}

void OnAreaResize(uiAreaHandler* handler, uiArea* area, int width, int height)
{
    SetupScreenRects(width, height);

    // TODO:
    // should those be the size of the uiArea, or the size of the window client area?
    // for now the uiArea fills the whole window anyway
    // but... we never know, I guess
    WindowWidth = width;
    WindowHeight = height;

    int ismax = uiWindowMaximized(MainWindow);
    int ismin = uiWindowMinimized(MainWindow);

    Config::WindowMaximized = ismax;
    if (!ismax && !ismin)
    {
        Config::WindowWidth = width;
        Config::WindowHeight = height;
    }

    OSD::WindowResized(Screen_UseGL);
}


void Run()
{
    EmuRunning = 1;
    RunningSomething = true;

    SPU::InitOutput();
    AudioSampleFrac = 0;
    SDL_PauseAudioDevice(AudioDevice, 0);
    SDL_PauseAudioDevice(MicDevice, 0);

    uiMenuItemEnable(MenuItem_SaveState);
    uiMenuItemEnable(MenuItem_LoadState);

    if (SavestateLoaded)
        uiMenuItemEnable(MenuItem_UndoStateLoad);
    else
        uiMenuItemDisable(MenuItem_UndoStateLoad);

    for (int i = 0; i < 8; i++)
    {
        char ssfile[1024];
        GetSavestateName(i+1, ssfile, 1024);
        if (Platform::FileExists(ssfile)) uiMenuItemEnable(MenuItem_LoadStateSlot[i]);
        else                              uiMenuItemDisable(MenuItem_LoadStateSlot[i]);
    }

    for (int i = 0; i < 9; i++) uiMenuItemEnable(MenuItem_SaveStateSlot[i]);
    uiMenuItemEnable(MenuItem_LoadStateSlot[8]);

    uiMenuItemEnable(MenuItem_Pause);
    uiMenuItemEnable(MenuItem_Reset);
    uiMenuItemEnable(MenuItem_Stop);
    uiMenuItemSetChecked(MenuItem_Pause, 0);
}

void TogglePause(void* blarg)
{
    if (!RunningSomething) return;

    if (EmuRunning == 1)
    {
        // enable pause
        EmuRunning = 2;
        uiMenuItemSetChecked(MenuItem_Pause, 1);

        SPU::DrainOutput();
        SDL_PauseAudioDevice(AudioDevice, 1);
        SDL_PauseAudioDevice(MicDevice, 1);

        OSD::AddMessage(0, "Paused");
    }
    else
    {
        // disable pause
        EmuRunning = 1;
        uiMenuItemSetChecked(MenuItem_Pause, 0);

        SPU::InitOutput();
        AudioSampleFrac = 0;
        SDL_PauseAudioDevice(AudioDevice, 0);
        SDL_PauseAudioDevice(MicDevice, 0);

        OSD::AddMessage(0, "Resumed");
    }
}

void Reset(void* blarg)
{
    if (!RunningSomething) return;

    EmuRunning = 2;
    while (EmuStatus != 2);

    SavestateLoaded = false;
    uiMenuItemDisable(MenuItem_UndoStateLoad);

    if (ROMPath[0] == '\0')
        NDS::LoadBIOS();
    else
    {
        SetupSRAMPath();
        NDS::LoadROM(ROMPath, SRAMPath, Config::DirectBoot);
    }

    Run();

    OSD::AddMessage(0, "Reset");
}

void Stop(bool internal)
{
    EmuRunning = 2;
    if (!internal) // if shutting down from the UI thread, wait till the emu thread has stopped
        while (EmuStatus != 2);
    RunningSomething = false;

    uiWindowSetTitle(MainWindow, "melonDS " MELONDS_VERSION);

    for (int i = 0; i < 9; i++) uiMenuItemDisable(MenuItem_SaveStateSlot[i]);
    for (int i = 0; i < 9; i++) uiMenuItemDisable(MenuItem_LoadStateSlot[i]);
    uiMenuItemDisable(MenuItem_UndoStateLoad);

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);
    uiMenuItemSetChecked(MenuItem_Pause, 0);

    uiAreaQueueRedrawAll(MainDrawArea);

    SPU::DrainOutput();
    SDL_PauseAudioDevice(AudioDevice, 1);
    SDL_PauseAudioDevice(MicDevice, 1);

    OSD::AddMessage(0xFFC040, "Shutdown");
}

void SetupSRAMPath()
{
    strncpy(SRAMPath, ROMPath, 1023);
    SRAMPath[1023] = '\0';
    strncpy(SRAMPath + strlen(ROMPath) - 3, "sav", 3);
}

void TryLoadROM(char* file, int prevstatus)
{
    char oldpath[1024];
    char oldsram[1024];
    strncpy(oldpath, ROMPath, 1024);
    strncpy(oldsram, SRAMPath, 1024);

    strncpy(ROMPath, file, 1023);
    ROMPath[1023] = '\0';

    SetupSRAMPath();

    if (NDS::LoadROM(ROMPath, SRAMPath, Config::DirectBoot))
    {
        SavestateLoaded = false;
        uiMenuItemDisable(MenuItem_UndoStateLoad);

        strncpy(PrevSRAMPath, SRAMPath, 1024); // safety
        Run();
    }
    else
    {
        uiMsgBoxError(MainWindow,
                      "Failed to load the ROM",
                      "Make sure the file can be accessed and isn't opened in another application.");

        strncpy(ROMPath, oldpath, 1024);
        strncpy(SRAMPath, oldsram, 1024);
        EmuRunning = prevstatus;
    }
}


// SAVESTATE TODO
// * configurable paths. not everyone wants their ROM directory to be polluted, I guess.

void GetSavestateName(int slot, char* filename, int len)
{
    int pos;

    if (ROMPath[0] == '\0') // running firmware, no ROM
    {
        strcpy(filename, "firmware");
        pos = 8;
    }
    else
    {
        int l = strlen(ROMPath);
        pos = l;
        while (ROMPath[pos] != '.' && pos > 0) pos--;
        if (pos == 0) pos = l;

        // avoid buffer overflow. shoddy
        if (pos > len-5) pos = len-5;

        strncpy(&filename[0], ROMPath, pos);
    }
    strcpy(&filename[pos], ".ml");
    filename[pos+3] = '0'+slot;
    filename[pos+4] = '\0';
}

void LoadState(int slot)
{
    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    char filename[1024];

    if (slot > 0)
    {
        GetSavestateName(slot, filename, 1024);
    }
    else
    {
        char* file = uiOpenFile(MainWindow, "melonDS savestate (any)|*.ml1;*.ml2;*.ml3;*.ml4;*.ml5;*.ml6;*.ml7;*.ml8;*.mln", Config::LastROMFolder);
        if (!file)
        {
            EmuRunning = prevstatus;
            return;
        }

        strncpy(filename, file, 1023);
        filename[1023] = '\0';
        uiFreeText(file);
    }

    if (!Platform::FileExists(filename))
    {
        char msg[64];
        if (slot > 0) sprintf(msg, "State slot %d is empty", slot);
        else          sprintf(msg, "State file does not exist");
        OSD::AddMessage(0xFFA0A0, msg);

        EmuRunning = prevstatus;
        return;
    }

    // backup
    Savestate* backup = new Savestate("timewarp.mln", true);
    NDS::DoSavestate(backup);
    delete backup;

    bool failed = false;

    Savestate* state = new Savestate(filename, false);
    if (state->Error)
    {
        delete state;

        uiMsgBoxError(MainWindow, "Error", "Could not load savestate file.");

        // current state might be crapoed, so restore from sane backup
        state = new Savestate("timewarp.mln", false);
        failed = true;
    }

    NDS::DoSavestate(state);
    delete state;

    if (!failed)
    {
        if (Config::SavestateRelocSRAM && ROMPath[0]!='\0')
        {
            strncpy(PrevSRAMPath, SRAMPath, 1024);

            strncpy(SRAMPath, filename, 1019);
            int len = strlen(SRAMPath);
            strcpy(&SRAMPath[len], ".sav");
            SRAMPath[len+4] = '\0';

            NDS::RelocateSave(SRAMPath, false);
        }

        char msg[64];
        if (slot > 0) sprintf(msg, "State loaded from slot %d", slot);
        else          sprintf(msg, "State loaded from file");
        OSD::AddMessage(0, msg);

        SavestateLoaded = true;
        uiMenuItemEnable(MenuItem_UndoStateLoad);
    }

    EmuRunning = prevstatus;
}

void SaveState(int slot)
{
    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    char filename[1024];

    if (slot > 0)
    {
        GetSavestateName(slot, filename, 1024);
    }
    else
    {
        char* file = uiSaveFile(MainWindow, "melonDS savestate (*.mln)|*.mln", Config::LastROMFolder);
        if (!file)
        {
            EmuRunning = prevstatus;
            return;
        }

        strncpy(filename, file, 1023);
        filename[1023] = '\0';
        uiFreeText(file);
    }

    Savestate* state = new Savestate(filename, true);
    if (state->Error)
    {
        delete state;

        uiMsgBoxError(MainWindow, "Error", "Could not save state.");
    }
    else
    {
        NDS::DoSavestate(state);
        delete state;

        if (slot > 0)
            uiMenuItemEnable(MenuItem_LoadStateSlot[slot-1]);

        if (Config::SavestateRelocSRAM && ROMPath[0]!='\0')
        {
            strncpy(SRAMPath, filename, 1019);
            int len = strlen(SRAMPath);
            strcpy(&SRAMPath[len], ".sav");
            SRAMPath[len+4] = '\0';

            NDS::RelocateSave(SRAMPath, true);
        }
    }

    char msg[64];
    if (slot > 0) sprintf(msg, "State saved to slot %d", slot);
    else          sprintf(msg, "State saved to file");
    OSD::AddMessage(0, msg);

    EmuRunning = prevstatus;
}

void UndoStateLoad()
{
    if (!SavestateLoaded) return;

    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    // pray that this works
    // what do we do if it doesn't???
    // but it should work.
    Savestate* backup = new Savestate("timewarp.mln", false);
    NDS::DoSavestate(backup);
    delete backup;

    if (ROMPath[0]!='\0')
    {
        strncpy(SRAMPath, PrevSRAMPath, 1024);
        NDS::RelocateSave(SRAMPath, false);
    }

    OSD::AddMessage(0, "State load undone");

    EmuRunning = prevstatus;
}


void CloseAllDialogs()
{
    DlgAudioSettings::Close();
    DlgEmuSettings::Close();
    DlgInputConfig::Close(0);
    DlgInputConfig::Close(1);
    DlgVideoSettings::Close();
    DlgWifiSettings::Close();
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    EmuRunning = 3;
    while (EmuStatus != 3);

    CloseAllDialogs();
    StopEmuThread();
    uiQuit();
    return 1;
}

void OnDropFile(uiWindow* window, char* file, void* blarg)
{
    char* ext = &file[strlen(file)-3];
    int prevstatus = EmuRunning;

    if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl"))
    {
        if (RunningSomething)
        {
            EmuRunning = 2;
            while (EmuStatus != 2);
        }

        TryLoadROM(file, prevstatus);
    }
}

void OnGetFocus(uiWindow* window, void* blarg)
{
    uiControlSetFocus(uiControl(MainDrawArea));
}

void OnLoseFocus(uiWindow* window, void* blarg)
{
    // TODO: shit here?
}

void OnCloseByMenu(uiMenuItem* item, uiWindow* window, void* blarg)
{
    EmuRunning = 3;
    while (EmuStatus != 3);

    CloseAllDialogs();
    StopEmuThread();
    DestroyMainWindow();
    uiQuit();
}

void OnOpenFile(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    char* file = uiOpenFile(window, "DS ROM (*.nds)|*.nds;*.srl|Any file|*.*", Config::LastROMFolder);
    if (!file)
    {
        EmuRunning = prevstatus;
        return;
    }

    int pos = strlen(file)-1;
    while (file[pos] != '/' && file[pos] != '\\' && pos > 0) pos--;
    strncpy(Config::LastROMFolder, file, pos);
    Config::LastROMFolder[pos] = '\0';

    TryLoadROM(file, prevstatus);
    uiFreeText(file);
}

void OnSaveState(uiMenuItem* item, uiWindow* window, void* param)
{
    int slot = *(int*)param;
    SaveState(slot);
}

void OnLoadState(uiMenuItem* item, uiWindow* window, void* param)
{
    int slot = *(int*)param;
    LoadState(slot);
}

void OnUndoStateLoad(uiMenuItem* item, uiWindow* window, void* param)
{
    UndoStateLoad();
}

void OnRun(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething)
    {
        ROMPath[0] = '\0';
        NDS::LoadBIOS();
    }

    Run();
}

void OnPause(uiMenuItem* item, uiWindow* window, void* blarg)
{
    TogglePause(NULL);
}

void OnReset(uiMenuItem* item, uiWindow* window, void* blarg)
{
    Reset(NULL);
}

void OnStop(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething) return;

    Stop(false);
}

void OnOpenEmuSettings(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgEmuSettings::Open();
}

void OnOpenInputConfig(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgInputConfig::Open(0);
}

void OnOpenHotkeyConfig(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgInputConfig::Open(1);
}

void OnOpenVideoSettings(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgVideoSettings::Open();
}

void OnOpenAudioSettings(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgAudioSettings::Open();
}

void OnOpenWifiSettings(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgWifiSettings::Open();
}


void OnSetSavestateSRAMReloc(uiMenuItem* item, uiWindow* window, void* param)
{
    Config::SavestateRelocSRAM = uiMenuItemChecked(item) ? 1:0;
}


void EnsureProperMinSize()
{
    bool isHori = (ScreenRotation == 1 || ScreenRotation == 3);

    int w0 = 256;
    int h0 = 192;
    int w1 = 256;
    int h1 = 192;

    if (ScreenLayout == 0) // natural
    {
        if (isHori)
            SetMinSize(h0+ScreenGap+h1, std::max(w0,w1));
        else
            SetMinSize(std::max(w0,w1), h0+ScreenGap+h1);
    }
    else if (ScreenLayout == 1) // vertical
    {
        if (isHori)
            SetMinSize(std::max(h0,h1), w0+ScreenGap+w1);
        else
            SetMinSize(std::max(w0,w1), h0+ScreenGap+h1);
    }
    else // horizontal
    {
        if (isHori)
            SetMinSize(h0+ScreenGap+h1, std::max(w0,w1));
        else
            SetMinSize(w0+ScreenGap+w1, std::max(h0,h1));
    }
}

void OnSetScreenSize(uiMenuItem* item, uiWindow* window, void* param)
{
    int factor = *(int*)param;
    bool isHori = (ScreenRotation == 1 || ScreenRotation == 3);

    int w = 256*factor;
    int h = 192*factor;

    // FIXME

    if (ScreenLayout == 0) // natural
    {
        if (isHori)
            uiWindowSetContentSize(window, (h*2)+ScreenGap, w);
        else
            uiWindowSetContentSize(window, w, (h*2)+ScreenGap);
    }
    else if (ScreenLayout == 1) // vertical
    {
        if (isHori)
            uiWindowSetContentSize(window, h, (w*2)+ScreenGap);
        else
            uiWindowSetContentSize(window, w, (h*2)+ScreenGap);
    }
    else // horizontal
    {
        if (isHori)
            uiWindowSetContentSize(window, (h*2)+ScreenGap, w);
        else
            uiWindowSetContentSize(window, (w*2)+ScreenGap, h);
    }
}

void OnSetScreenRotation(uiMenuItem* item, uiWindow* window, void* param)
{
    int rot = *(int*)param;

    int oldrot = ScreenRotation;
    ScreenRotation = rot;

    int w, h;
    uiWindowContentSize(window, &w, &h);

    bool isHori = (rot == 1 || rot == 3);
    bool wasHori = (oldrot == 1 || oldrot == 3);

    EnsureProperMinSize();

    if (ScreenLayout == 0) // natural
    {
        if (isHori ^ wasHori)
        {
            int blarg = h;
            h = w;
            w = blarg;

            uiWindowSetContentSize(window, w, h);
        }
    }

    SetupScreenRects(w, h);

    for (int i = 0; i < 4; i++)
        uiMenuItemSetChecked(MenuItem_ScreenRot[i], i==ScreenRotation);
}

void OnSetScreenGap(uiMenuItem* item, uiWindow* window, void* param)
{
    int gap = *(int*)param;

    //int oldgap = ScreenGap;
    ScreenGap = gap;

    EnsureProperMinSize();
    SetupScreenRects(WindowWidth, WindowHeight);

    for (int i = 0; i < 6; i++)
        uiMenuItemSetChecked(MenuItem_ScreenGap[i], kScreenGap[i]==ScreenGap);
}

void OnSetScreenLayout(uiMenuItem* item, uiWindow* window, void* param)
{
    int layout = *(int*)param;
    ScreenLayout = layout;

    EnsureProperMinSize();
    SetupScreenRects(WindowWidth, WindowHeight);

    for (int i = 0; i < 3; i++)
        uiMenuItemSetChecked(MenuItem_ScreenLayout[i], i==ScreenLayout);
}

void OnSetScreenSizing(uiMenuItem* item, uiWindow* window, void* param)
{
    int sizing = *(int*)param;
    ScreenSizing = sizing;

    SetupScreenRects(WindowWidth, WindowHeight);

    for (int i = 0; i < 4; i++)
        uiMenuItemSetChecked(MenuItem_ScreenSizing[i], i==ScreenSizing);
}

void OnSetScreenFiltering(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int chk = uiMenuItemChecked(item);
    if (chk != 0) Config::ScreenFilter = 1;
    else          Config::ScreenFilter = 0;
}

void OnSetLimitFPS(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int chk = uiMenuItemChecked(item);
    if (chk != 0) Config::LimitFPS = true;
    else          Config::LimitFPS = false;
}

void OnSetAudioSync(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int chk = uiMenuItemChecked(item);
    if (chk != 0) Config::AudioSync = true;
    else          Config::AudioSync = false;
}

void OnSetShowOSD(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int chk = uiMenuItemChecked(item);
    if (chk != 0) Config::ShowOSD = true;
    else          Config::ShowOSD = false;
}

void ApplyNewSettings(int type)
{
    if (!RunningSomething)
    {
        if (type == 1) return;
    }

    int prevstatus = EmuRunning;
    EmuRunning = 3;
    while (EmuStatus != 3);

    if (type == 0) // 3D renderer settings
    {
        if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);
        GPU3D::UpdateRendererConfig();
        if (Screen_UseGL) uiGLMakeContextCurrent(NULL);

        GL_3DScale = Config::GL_ScaleFactor; // dorp
        GL_ScreenSizeDirty = true;
    }
    else if (type == 1) // wifi settings
    {
        if (Wifi::MPInited)
        {
            Platform::MP_DeInit();
            Platform::MP_Init();
        }

        Platform::LAN_DeInit();
        Platform::LAN_Init();
    }
    else if (type == 2) // video output method
    {
        bool usegl = Config::ScreenUseGL || (Config::_3DRenderer != 0);
        if (usegl != Screen_UseGL)
        {
            if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);
            GPU3D::DeInitRenderer();
            OSD::DeInit(Screen_UseGL);
            if (Screen_UseGL) uiGLMakeContextCurrent(NULL);

            Screen_UseGL = usegl;
            RecreateMainWindow(usegl);

            if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);
            GPU3D::InitRenderer(Screen_UseGL);
            if (Screen_UseGL) uiGLMakeContextCurrent(NULL);
        }
    }
    else if (type == 3) // 3D renderer
    {
        if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);
        GPU3D::DeInitRenderer();
        GPU3D::InitRenderer(Screen_UseGL);
        if (Screen_UseGL) uiGLMakeContextCurrent(NULL);
    }
    /*else if (type == 4) // vsync
    {
        if (Screen_UseGL)
        {
            uiGLMakeContextCurrent(GLContext);
            uiGLSetVSync(Config::ScreenVSync);
            uiGLMakeContextCurrent(NULL);
        }
        else
        {
            // TODO eventually: VSync for non-GL screen?
        }
    }*/

    EmuRunning = prevstatus;
}


void CreateMainWindowMenu()
{
    uiMenu* menu;
    uiMenuItem* menuitem;

    menu = uiNewMenu("File");
    menuitem = uiMenuAppendItem(menu, "Open ROM...");
    uiMenuItemOnClicked(menuitem, OnOpenFile, NULL);
    uiMenuAppendSeparator(menu);
    {
        uiMenu* submenu = uiNewMenu("Save state");

        for (int i = 0; i < 9; i++)
        {
            char name[32];
            if (i < 8)
                sprintf(name, "%d\tShift+F%d", kSavestateNum[i], kSavestateNum[i]);
            else
                strcpy(name, "File...\tShift+F9");

            uiMenuItem* ssitem = uiMenuAppendItem(submenu, name);
            uiMenuItemOnClicked(ssitem, OnSaveState, (void*)&kSavestateNum[i]);

            MenuItem_SaveStateSlot[i] = ssitem;
        }

        MenuItem_SaveState = uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Load state");

        for (int i = 0; i < 9; i++)
        {
            char name[32];
            if (i < 8)
                sprintf(name, "%d\tF%d", kSavestateNum[i], kSavestateNum[i]);
            else
                strcpy(name, "File...\tF9");

            uiMenuItem* ssitem = uiMenuAppendItem(submenu, name);
            uiMenuItemOnClicked(ssitem, OnLoadState, (void*)&kSavestateNum[i]);

            MenuItem_LoadStateSlot[i] = ssitem;
        }

        MenuItem_LoadState = uiMenuAppendSubmenu(menu, submenu);
    }
    menuitem = uiMenuAppendItem(menu, "Undo state load\tF12");
    uiMenuItemOnClicked(menuitem, OnUndoStateLoad, NULL);
    MenuItem_UndoStateLoad = menuitem;
    uiMenuAppendSeparator(menu);
    menuitem = uiMenuAppendItem(menu, "Quit");
    uiMenuItemOnClicked(menuitem, OnCloseByMenu, NULL);

    menu = uiNewMenu("System");
    menuitem = uiMenuAppendItem(menu, "Run");
    uiMenuItemOnClicked(menuitem, OnRun, NULL);
    menuitem = uiMenuAppendCheckItem(menu, "Pause");
    uiMenuItemOnClicked(menuitem, OnPause, NULL);
    MenuItem_Pause = menuitem;
    uiMenuAppendSeparator(menu);
    menuitem = uiMenuAppendItem(menu, "Reset");
    uiMenuItemOnClicked(menuitem, OnReset, NULL);
    MenuItem_Reset = menuitem;
    menuitem = uiMenuAppendItem(menu, "Stop");
    uiMenuItemOnClicked(menuitem, OnStop, NULL);
    MenuItem_Stop = menuitem;

    menu = uiNewMenu("Config");
    {
        menuitem = uiMenuAppendItem(menu, "Emu settings");
        uiMenuItemOnClicked(menuitem, OnOpenEmuSettings, NULL);
        menuitem = uiMenuAppendItem(menu, "Input config");
        uiMenuItemOnClicked(menuitem, OnOpenInputConfig, NULL);
        menuitem = uiMenuAppendItem(menu, "Hotkey config");
        uiMenuItemOnClicked(menuitem, OnOpenHotkeyConfig, NULL);
        menuitem = uiMenuAppendItem(menu, "Video settings");
        uiMenuItemOnClicked(menuitem, OnOpenVideoSettings, NULL);
        menuitem = uiMenuAppendItem(menu, "Audio settings");
        uiMenuItemOnClicked(menuitem, OnOpenAudioSettings, NULL);
        menuitem = uiMenuAppendItem(menu, "Wifi settings");
        uiMenuItemOnClicked(menuitem, OnOpenWifiSettings, NULL);
    }
    uiMenuAppendSeparator(menu);
    {
        uiMenu* submenu = uiNewMenu("Savestate settings");

        MenuItem_SavestateSRAMReloc = uiMenuAppendCheckItem(submenu, "Separate savefiles");
        uiMenuItemOnClicked(MenuItem_SavestateSRAMReloc, OnSetSavestateSRAMReloc, NULL);

        uiMenuAppendSubmenu(menu, submenu);
    }
    uiMenuAppendSeparator(menu);
    {
        uiMenu* submenu = uiNewMenu("Screen size");

        for (int i = 0; i < 4; i++)
        {
            char name[32];
            sprintf(name, "%dx", kScreenSize[i]);
            uiMenuItem* item = uiMenuAppendItem(submenu, name);
            uiMenuItemOnClicked(item, OnSetScreenSize, (void*)&kScreenSize[i]);
        }

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Screen rotation");

        for (int i = 0; i < 4; i++)
        {
            char name[32];
            sprintf(name, "%d", kScreenRot[i]*90);
            MenuItem_ScreenRot[i] = uiMenuAppendCheckItem(submenu, name);
            uiMenuItemOnClicked(MenuItem_ScreenRot[i], OnSetScreenRotation, (void*)&kScreenRot[i]);
        }

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Mid-screen gap");

        //for (int i = 0; kScreenGap[i] != -1; i++)
        for (int i = 0; i < 6; i++)
        {
            char name[32];
            sprintf(name, "%d pixels", kScreenGap[i]);
            MenuItem_ScreenGap[i] = uiMenuAppendCheckItem(submenu, name);
            uiMenuItemOnClicked(MenuItem_ScreenGap[i], OnSetScreenGap, (void*)&kScreenGap[i]);
        }

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Screen layout");

        MenuItem_ScreenLayout[0] = uiMenuAppendCheckItem(submenu, "Natural");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[0], OnSetScreenLayout, (void*)&kScreenLayout[0]);
        MenuItem_ScreenLayout[1] = uiMenuAppendCheckItem(submenu, "Vertical");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[1], OnSetScreenLayout, (void*)&kScreenLayout[1]);
        MenuItem_ScreenLayout[2] = uiMenuAppendCheckItem(submenu, "Horizontal");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[2], OnSetScreenLayout, (void*)&kScreenLayout[2]);

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Screen sizing");

        MenuItem_ScreenSizing[0] = uiMenuAppendCheckItem(submenu, "Even");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[0], OnSetScreenSizing, (void*)&kScreenSizing[0]);
        MenuItem_ScreenSizing[1] = uiMenuAppendCheckItem(submenu, "Emphasize top");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[1], OnSetScreenSizing, (void*)&kScreenSizing[1]);
        MenuItem_ScreenSizing[2] = uiMenuAppendCheckItem(submenu, "Emphasize bottom");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[2], OnSetScreenSizing, (void*)&kScreenSizing[2]);
        MenuItem_ScreenSizing[3] = uiMenuAppendCheckItem(submenu, "Auto");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[3], OnSetScreenSizing, (void*)&kScreenSizing[3]);

        uiMenuAppendSubmenu(menu, submenu);
    }

    MenuItem_ScreenFilter = uiMenuAppendCheckItem(menu, "Screen filtering");
    uiMenuItemOnClicked(MenuItem_ScreenFilter, OnSetScreenFiltering, NULL);

    MenuItem_ShowOSD = uiMenuAppendCheckItem(menu, "Show OSD");
    uiMenuItemOnClicked(MenuItem_ShowOSD, OnSetShowOSD, NULL);

    uiMenuAppendSeparator(menu);

    MenuItem_LimitFPS = uiMenuAppendCheckItem(menu, "Limit framerate");
    uiMenuItemOnClicked(MenuItem_LimitFPS, OnSetLimitFPS, NULL);

    MenuItem_AudioSync = uiMenuAppendCheckItem(menu, "Audio sync");
    uiMenuItemOnClicked(MenuItem_AudioSync, OnSetAudioSync, NULL);
}

void CreateMainWindow(bool opengl)
{
    MainWindow = uiNewWindow("melonDS " MELONDS_VERSION,
                             WindowWidth, WindowHeight,
                             Config::WindowMaximized, 1, 1);
    uiWindowOnClosing(MainWindow, OnCloseWindow, NULL);

    uiWindowSetDropTarget(MainWindow, 1);
    uiWindowOnDropFile(MainWindow, OnDropFile, NULL);

    uiWindowOnGetFocus(MainWindow, OnGetFocus, NULL);
    uiWindowOnLoseFocus(MainWindow, OnLoseFocus, NULL);

    ScreenDrawInited = false;
    bool opengl_good = opengl;

    if (!opengl) MainDrawArea = uiNewArea(&MainDrawAreaHandler);
    else         MainDrawArea = uiNewGLArea(&MainDrawAreaHandler, kGLVersions);

    uiWindowSetChild(MainWindow, uiControl(MainDrawArea));
    uiControlSetMinSize(uiControl(MainDrawArea), 256, 384);
    uiAreaSetBackgroundColor(MainDrawArea, 0, 0, 0);

    uiControlShow(uiControl(MainWindow));
    uiControlSetFocus(uiControl(MainDrawArea));

    if (opengl_good)
    {
        GLContext = uiAreaGetGLContext(MainDrawArea);
        if (!GLContext) opengl_good = false;
    }
    if (opengl_good)
    {
        uiGLMakeContextCurrent(GLContext);
        uiGLSetVSync(Config::ScreenVSync);
        if (!GLScreen_Init()) opengl_good = false;
        if (opengl_good)
        {
            OpenGL_UseShaderProgram(GL_ScreenShaderOSD);
            OSD::Init(true);
        }
        uiGLMakeContextCurrent(NULL);
    }

    if (opengl && !opengl_good)
    {
        printf("OpenGL: initialization failed\n");
        RecreateMainWindow(false);
        Screen_UseGL = false;
    }

    if (!opengl) OSD::Init(false);
}

void DestroyMainWindow()
{
    uiControlDestroy(uiControl(MainWindow));

    if (ScreenBitmap[0]) uiDrawFreeBitmap(ScreenBitmap[0]);
    if (ScreenBitmap[1]) uiDrawFreeBitmap(ScreenBitmap[1]);

    ScreenBitmap[0] = NULL;
    ScreenBitmap[1] = NULL;
}

void RecreateMainWindow(bool opengl)
{
    int winX, winY, maxi;
    uiWindowPosition(MainWindow, &winX, &winY);
    maxi = uiWindowMaximized(MainWindow);
    DestroyMainWindow();
    CreateMainWindow(opengl);
    uiWindowSetPosition(MainWindow, winX, winY);
    uiWindowSetMaximized(MainWindow, maxi);
}


int main(int argc, char** argv)
{
    srand(time(NULL));

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    if (argc > 0 && strlen(argv[0]) > 0)
    {
        int len = strlen(argv[0]);
        while (len > 0)
        {
            if (argv[0][len] == '/') break;
            if (argv[0][len] == '\\') break;
            len--;
        }
        if (len > 0)
        {
            EmuDirectory = new char[len+1];
            strncpy(EmuDirectory, argv[0], len);
            EmuDirectory[len] = '\0';
        }
        else
        {
            EmuDirectory = new char[2];
            strcpy(EmuDirectory, ".");
        }
    }
    else
    {
        EmuDirectory = new char[2];
        strcpy(EmuDirectory, ".");
    }

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_HAPTIC) < 0)
    {
        printf("SDL couldn't init rumble\n");
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        printf("SDL shat itself :(\n");
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    uiInitOptions ui_opt;
    memset(&ui_opt, 0, sizeof(uiInitOptions));
    const char* ui_err = uiInit(&ui_opt);
    if (ui_err != NULL)
    {
        printf("libui shat itself :( %s\n", ui_err);
        uiFreeInitError(ui_err);
        return 1;
    }

    Config::Load();

    if      (Config::AudioVolume < 0)   Config::AudioVolume = 0;
    else if (Config::AudioVolume > 256) Config::AudioVolume = 256;

    if (!Platform::LocalFileExists("bios7.bin") ||
        !Platform::LocalFileExists("bios9.bin") ||
        !Platform::LocalFileExists("firmware.bin"))
    {
        uiMsgBoxError(
            NULL,
            "BIOS/Firmware not found",
            "One or more of the following required files don't exist or couldn't be accessed:\n\n"
            "bios7.bin -- ARM7 BIOS\n"
            "bios9.bin -- ARM9 BIOS\n"
            "firmware.bin -- firmware image\n\n"
            "Dump the files from your DS and place them in the directory you run melonDS from.\n"
            "Make sure that the files can be accessed.");

        uiUninit();
        SDL_Quit();
        return 0;
    }
    {
        FILE* f = Platform::OpenLocalFile("romlist.bin", "rb");
        if (f)
        {
            u32 data;
            fread(&data, 4, 1, f);
            fclose(f);

            if ((data >> 24) == 0) // old CRC-based list
            {
                uiMsgBoxError(NULL,
                              "Your version of romlist.bin is outdated.",
                              "Save memory type detection will not work correctly.\n\n"
                              "You should use the latest version of romlist.bin (provided in melonDS release packages).");
            }
        }
        else
        {
        	uiMsgBoxError(NULL,
        			     "romlist.bin not found.",
        			     "Save memory type detection will not work correctly.\n\n"
				         "You should use the latest version of romlist.bin (provided in melonDS release packages).");
        }
    }

    CreateMainWindowMenu();

    MainDrawAreaHandler.Draw = OnAreaDraw;
    MainDrawAreaHandler.MouseEvent = OnAreaMouseEvent;
    MainDrawAreaHandler.MouseCrossed = OnAreaMouseCrossed;
    MainDrawAreaHandler.DragBroken = OnAreaDragBroken;
    MainDrawAreaHandler.KeyEvent = OnAreaKeyEvent;
    MainDrawAreaHandler.Resize = OnAreaResize;

    WindowWidth = Config::WindowWidth;
    WindowHeight = Config::WindowHeight;

    Screen_UseGL = Config::ScreenUseGL || (Config::_3DRenderer != 0);

    GL_3DScale = Config::GL_ScaleFactor;
    if      (GL_3DScale < 1) GL_3DScale = 1;
    else if (GL_3DScale > 8) GL_3DScale = 8;

    CreateMainWindow(Screen_UseGL);

    ScreenRotation = Config::ScreenRotation;
    ScreenGap = Config::ScreenGap;
    ScreenLayout = Config::ScreenLayout;
    ScreenSizing = Config::ScreenSizing;

#define SANITIZE(var, min, max)  if ((var < min) || (var > max)) var = 0;
    SANITIZE(ScreenRotation, 0, 3);
    SANITIZE(ScreenLayout, 0, 2);
    SANITIZE(ScreenSizing, 0, 3);
#undef SANITIZE

    for (int i = 0; i < 9; i++) uiMenuItemDisable(MenuItem_SaveStateSlot[i]);
    for (int i = 0; i < 9; i++) uiMenuItemDisable(MenuItem_LoadStateSlot[i]);
    uiMenuItemDisable(MenuItem_UndoStateLoad);

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);

    uiMenuItemSetChecked(MenuItem_SavestateSRAMReloc, Config::SavestateRelocSRAM?1:0);

    uiMenuItemSetChecked(MenuItem_ScreenRot[ScreenRotation], 1);
    uiMenuItemSetChecked(MenuItem_ScreenLayout[ScreenLayout], 1);
    uiMenuItemSetChecked(MenuItem_ScreenSizing[ScreenSizing], 1);

    for (int i = 0; i < 6; i++)
    {
        if (ScreenGap == kScreenGap[i])
            uiMenuItemSetChecked(MenuItem_ScreenGap[i], 1);
    }

    OnSetScreenRotation(MenuItem_ScreenRot[ScreenRotation], MainWindow, (void*)&kScreenRot[ScreenRotation]);

    uiMenuItemSetChecked(MenuItem_ScreenFilter, Config::ScreenFilter==1);
    uiMenuItemSetChecked(MenuItem_LimitFPS, Config::LimitFPS==1);
    uiMenuItemSetChecked(MenuItem_AudioSync, Config::AudioSync==1);
    uiMenuItemSetChecked(MenuItem_ShowOSD, Config::ShowOSD==1);

    AudioSync = SDL_CreateCond();
    AudioSyncLock = SDL_CreateMutex();

    AudioFreq = 48000; // TODO: make configurable?
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = AudioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = AudioCallback;
    AudioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!AudioDevice)
    {
        printf("Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        AudioFreq = whatIget.freq;
        printf("Audio output frequency: %d Hz\n", AudioFreq);
        SDL_PauseAudioDevice(AudioDevice, 1);
    }

    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 44100;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = 1024;
    whatIwant.callback = MicCallback;
    MicDevice = SDL_OpenAudioDevice(NULL, 1, &whatIwant, &whatIget, 0);
    if (!MicDevice)
    {
        printf("Mic init failed: %s\n", SDL_GetError());
        MicBufferLength = 0;
    }
    else
    {
        SDL_PauseAudioDevice(MicDevice, 1);
    }

    memset(MicBuffer, 0, sizeof(MicBuffer));
    MicBufferReadPos = 0;
    MicBufferWritePos = 0;

    MicWavBuffer = NULL;
    if (Config::MicInputType == 3) MicLoadWav(Config::MicWavPath);

    JoystickID = Config::JoystickID;
    Joystick = NULL;
    OpenJoystick();

    EmuRunning = 2;
    RunningSomething = false;
    EmuThread = SDL_CreateThread(EmuThreadFunc, "melonDS magic", NULL);

    if (argc > 1)
    {
        char* file = argv[1];
        char* ext = &file[strlen(file)-3];

        if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl"))
        {
            strncpy(ROMPath, file, 1023);
            ROMPath[1023] = '\0';

            SetupSRAMPath();

            if (NDS::LoadROM(ROMPath, SRAMPath, Config::DirectBoot))
                Run();
        }
    }

    uiMain();

    if (Joystick) SDL_JoystickClose(Joystick);
    if (AudioDevice) SDL_CloseAudioDevice(AudioDevice);
    if (MicDevice)   SDL_CloseAudioDevice(MicDevice);

    SDL_DestroyCond(AudioSync);
    SDL_DestroyMutex(AudioSyncLock);

    if (MicWavBuffer) delete[] MicWavBuffer;

    if (ScreenBitmap[0]) uiDrawFreeBitmap(ScreenBitmap[0]);
    if (ScreenBitmap[1]) uiDrawFreeBitmap(ScreenBitmap[1]);

    Config::ScreenRotation = ScreenRotation;
    Config::ScreenGap = ScreenGap;
    Config::ScreenLayout = ScreenLayout;
    Config::ScreenSizing = ScreenSizing;

    Config::Save();

    uiUninit();
    SDL_Quit();
    delete[] EmuDirectory;
    return 0;
}

#ifdef __WIN32__

#include <windows.h>

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    int argc = 0;
    wchar_t** argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    char* nullarg = "";

    char** argv = new char*[argc];
    for (int i = 0; i < argc; i++)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, NULL, 0, NULL, NULL);
        if (len < 1) return NULL;
        argv[i] = new char[len];
        int res = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], len, NULL, NULL);
        if (res != len) { delete[] argv[i]; argv[i] = nullarg; }
    }

    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        printf("\n");
    }

    int ret = main(argc, argv);

    printf("\n\n>");

    for (int i = 0; i < argc; i++) if (argv[i] != nullarg) delete[] argv[i];
    delete[] argv;

    return ret;
}

#endif
