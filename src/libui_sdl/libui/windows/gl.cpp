// 31 march 2019
#include "uipriv_windows.hpp"
#include "area.hpp"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

struct uiGLContext
{
    uiArea* a;

    HWND hwnd;
    HDC dc;
    HGLRC rc;

    unsigned int version;
};


uiGLContext* createGLContext(uiArea* a, int vermajor, int verminor)
{
    uiGLContext* ctx;
    BOOL res;

    ctx = uiNew(uiGLContext);

    ctx->a = a;
    ctx->hwnd = a->hwnd;

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

        ctx->version = uiGLVersion(vermajor, verminor);
        ctx->rc = rc_better;
        wglMakeCurrent(ctx->dc, ctx->rc);
    }

    return ctx;
}

void freeGLContext(uiGLContext* ctx)
{
    if (ctx == NULL) return;
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

unsigned int uiGLGetVersion(uiGLContext* ctx)
{
    if (ctx == NULL) return 0;
    return ctx->version;
}

void *uiGLGetProcAddress(const char* proc)
{
    return (void*)wglGetProcAddress(proc);
}

void uiGLBegin(uiGLContext* ctx)
{
}

void uiGLEnd(uiGLContext* ctx)
{
}

void uiGLSwapBuffers(uiGLContext* ctx)
{
    if (ctx == NULL) return;
    SwapBuffers(ctx->dc);
}

int uiGLGetFramebuffer(uiGLContext* ctx)
{
    return 0;
}

float uiGLGetFramebufferScale(uiGLContext* ctx)
{
    // TODO
    return 1;
}

void uiGLSetVSync(int sync)
{
	static PFNWGLSWAPINTERVALEXTPROC _wglSwapIntervalEXT = NULL;
	static bool symloaded = false;

	if (!symloaded)
    {
        PFNGLGETSTRINGIPROC _glGetStringi = (PFNGLGETSTRINGIPROC)wglGetProcAddress("glGetStringi");
        if (_glGetStringi == NULL) return;

        GLint numext;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numext);

        bool hasswapctrl = false;
        for (GLint i = 0; i < numext; i++)
        {
            const char* ext = (const char*)_glGetStringi(GL_EXTENSIONS, i);
            if (!stricmp(ext, "WGL_EXT_swap_control"))
            {
                hasswapctrl = true;
                break;
            }
        }

        if (hasswapctrl)
            _wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

        symloaded = true;
    }

    if (_wglSwapIntervalEXT)
        _wglSwapIntervalEXT(sync);
}
