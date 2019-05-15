// 31 march 2019
#include "uipriv_windows.hpp"

#include <GL/gl.h>
#include <GL/wglext.h>

struct uiGLContext
{
    uiControl* c;

    HWND hwnd;
    HDC dc;
    HGLRC rc;
};


uiGLContext* uiGLNewContext(uiControl* c, int vermajor, int verminor)
{
    uiGLContext* ctx;
    BOOL res;

    ctx = uiNew(uiGLContext);

    ctx->c = c;
    if (c)
    {
        ctx->hwnd = (HWND)c->Handle(c); // welp
    }
    else
    {
        // windowless context
        //ctx->hwnd = GetDesktopWindow();
        // nope.
        uiFree(ctx);
        return NULL;
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    ctx->dc = GetDC(ctx->hwnd);
    if (!ctx->dc)
    {
        uiFree(ctx);
        return NULL;
    }

    int pixelformat = ChoosePixelFormat(ctx->dc, &pfd);
    res = SetPixelFormat(ctx->dc, pixelformat, &pfd);
    if (!res)
    {
        ReleaseDC(ctx->hwnd, ctx->dc);
        uiFree(ctx);
        return NULL;
    }

    ctx->rc = wglCreateContext(ctx->dc);
    if (!ctx->rc)
    {
        ReleaseDC(ctx->hwnd, ctx->dc);
        uiFree(ctx);
        return NULL;
    }

    wglMakeCurrent(ctx->dc, ctx->rc);

    if (vermajor >= 3)
    {
        HGLRC (*wglCreateContextAttribsARB)(HDC,HGLRC,const int*);
        HGLRC rc_better = NULL;

        wglCreateContextAttribsARB = (HGLRC(*)(HDC,HGLRC,const int*))wglGetProcAddress("wglCreateContextAttribsARB");
        if (wglCreateContextAttribsARB)
        {
            int attribs[15];
            int i = 0;

            attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
            attribs[i++] = vermajor;
            attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
            attribs[i++] = verminor;

            attribs[i] = 0;
            rc_better = wglCreateContextAttribsARB(ctx->dc, NULL, attribs);
        }

        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(ctx->rc);

        if (!rc_better)
        {
            ReleaseDC(ctx->hwnd, ctx->dc);
            uiFree(ctx);
            return NULL;
        }

        ctx->rc = rc_better;
        wglMakeCurrent(ctx->dc, ctx->rc);
    }

    return ctx;
}

void uiGLFreeContext(uiGLContext* ctx)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(ctx->rc);
    ReleaseDC(ctx->hwnd, ctx->dc);
    uiFree(ctx);
}

void uiGLMakeContextCurrent(uiGLContext* ctx)
{
    if (ctx == NULL)
    {
        wglMakeCurrent(NULL, NULL);
        return;
    }

    if (wglGetCurrentContext() == ctx->rc) return;
    int res = wglMakeCurrent(ctx->dc, ctx->rc);
}

void *uiGLGetProcAddress(const char* proc)
{
    return (void*)wglGetProcAddress(proc);
}

void uiGLSwapBuffers(uiGLContext* ctx)
{
    SwapBuffers(ctx->dc);
}
