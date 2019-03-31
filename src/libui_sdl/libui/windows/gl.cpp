// 31 march 2019
#include "uipriv_windows.hpp"

struct uiGLContext
{
    uiControl* c;

    HWND hwnd;
    HDC dc;
    HGLRC rc;
};


uiGLContext* uiGLNewContext(uiControl* c)
{
    uiGLContext* ctx;

    ctx = uiNew(uiGLContext);

    ctx->c = c;
    if (c)
    {
        ctx->hwnd = (HWND)c->Handle(c); // welp
    }
    else
    {
        // windowless context
        ctx->hwnd = GetDesktopWindow();
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    ctx->dc = GetDC(ctx->hwnd);

    int pixelformat = ChoosePixelFormat(ctx->dc, &pfd);
    SetPixelFormat(ctx->dc, pixelformat, &pfd);

    ctx->rc = wglCreateContext(ctx->dc);
}

void uiGLFreeContext(uiGLContext* ctx)
{
    wglDeleteContext(ctx->rc);
    ReleaseDC(ctx->hwnd, ctx->dc);
    uiFree(ctx);
}

void uiGLMakeContextCurrent(uiGLContext* ctx)
{
    wglMakeCurrent(ctx->dc, ctx->rc);
}

void *uiGLGetProcAddress(const char* proc)
{
    return (void*)wglGetProcAddress(proc);
}
