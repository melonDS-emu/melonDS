// 26 may 2019
#include "uipriv_unix.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <EGL/egl.h>

extern GThread* gtkthread;
extern GMutex glmutex;

struct uiGLContext 
{
	GtkWidget* widget;
	GdkWindow* window;
	
	GdkGLContext *gctx;
	int vermaj, vermin;
	
	int width, height;
	int scale;
	GLuint renderbuffer[2][2];
	GLuint framebuffer[2];
	int backbuffer;
};

static void areaAllocRenderbuffer(uiGLContext* glctx);

static PFNGLGENRENDERBUFFERSPROC _glGenRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC _glDeleteRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC _glBindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC _glRenderbufferStorage;
static PFNGLGETRENDERBUFFERPARAMETERIVPROC _glGetRenderbufferParameteriv;

static PFNGLGENRENDERBUFFERSPROC _glGenFramebuffers;
static PFNGLDELETERENDERBUFFERSPROC _glDeleteFramebuffers;
static PFNGLBINDRENDERBUFFERPROC _glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTUREPROC _glFramebufferTexture;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC _glFramebufferRenderbuffer;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC _glCheckFramebufferStatus;

static int _procsLoaded = 0;

static void _loadGLProcs(GdkGLContext* glctx)
{
    if (_procsLoaded) return;
    
    _glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)uiGLGetProcAddress("glGenRenderbuffers");
    _glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)uiGLGetProcAddress("glDeleteRenderbuffers");
    _glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)uiGLGetProcAddress("glBindRenderbuffer");
    _glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)uiGLGetProcAddress("glRenderbufferStorage");
    _glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)uiGLGetProcAddress("glGetRenderbufferParameteriv");
    
    _glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)uiGLGetProcAddress("glGenFramebuffers");
    _glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)uiGLGetProcAddress("glDeleteFramebuffers");
    _glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)uiGLGetProcAddress("glBindFramebuffer");
    _glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)uiGLGetProcAddress("glFramebufferTexture");
    _glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)uiGLGetProcAddress("glFramebufferRenderbuffer");
    _glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)uiGLGetProcAddress("glCheckFramebufferStatus");
    
    _procsLoaded = 1;
}

uiGLContext *createGLContext(GtkWidget* widget, int maj, int min)
{
    GdkWindow* gdkwin = gtk_widget_get_window(widget);

	GError* err = NULL;
	GdkGLContext* gctx = gdk_window_create_gl_context(gdkwin, &err);
	if (err != NULL || gctx == NULL)
	{
	    return NULL;
	}
	
	// TODO: make the set_use_es call conditional (#ifdef or smth) for older versions of gdk?
	gdk_gl_context_set_use_es(gctx, FALSE);
	gdk_gl_context_set_required_version(gctx, maj, min);
	
	gboolean res = gdk_gl_context_realize(gctx, &err);
	if (err != NULL || res == FALSE)
	{
	    return NULL;
	}
	
	uiGLContext* ctx = uiNew(uiGLContext);
	
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);
	int window_scale = gdk_window_get_scale_factor(gdkwin);
	ctx->width = allocation.width;
	ctx->height = allocation.height;
	ctx->scale = window_scale;
	
	gdk_gl_context_make_current(gctx);
	_loadGLProcs(gctx);
	areaAllocRenderbuffer(ctx);
	ctx->backbuffer = 0;
	
	ctx->widget = widget;
	ctx->window = gdkwin;
	ctx->gctx = gctx;
	
    return ctx;
}

void freeGLContext(uiGLContext* glctx)
{
    if (glctx == NULL) return;
    
    gdk_gl_context_make_current(glctx->gctx);
    _glDeleteRenderbuffers(4, &glctx->renderbuffer[0][0]);
    _glDeleteFramebuffers(2, &glctx->framebuffer[0]);
    
    gdk_gl_context_clear_current();
    g_object_unref(glctx->gctx);
    uiFree(glctx);
}

static void areaAllocRenderbuffer(uiGLContext* glctx)
{
    // TODO: create textures as a fallback if GL_RGB renderbuffer isn't supported?
    // they say GL implementations aren't required to support a GL_RGB renderbuffer
    // however, a GL_RGBA one would cause gdk_cairo_draw_from_gl() to fall back to glReadPixels()
    
    _glGenRenderbuffers(4, &glctx->renderbuffer[0][0]);
    _glGenFramebuffers(2, &glctx->framebuffer[0]);
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    //_glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][1]);
    //_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
    
    _glBindFramebuffer(GL_FRAMEBUFFER, glctx->framebuffer[0]);
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glctx->renderbuffer[0][0]);
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glctx->renderbuffer[0][1]);
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    //_glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][1]);
    //_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
    
    _glBindFramebuffer(GL_FRAMEBUFFER, glctx->framebuffer[1]);
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glctx->renderbuffer[1][0]);
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glctx->renderbuffer[1][1]);
    
    //if (_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    //    printf("FRAMEBUFFER IS BAD!! %04X\n", _glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

static void areaReallocRenderbuffer(uiGLContext* glctx)
{
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    //_glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][1]);
    //_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    //_glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][1]);
    //_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
}

void areaDrawGL(GtkWidget* widget, uiAreaDrawParams* dp, cairo_t* cr, uiGLContext* glctx)
{
    int window_scale = gdk_window_get_scale_factor(glctx->window);
    
    if (glctx->width != dp->AreaWidth || glctx->height != dp->AreaHeight || glctx->scale != window_scale)
    {
        glctx->width = dp->AreaWidth;
        glctx->height = dp->AreaHeight;
        glctx->scale = window_scale;
        areaReallocRenderbuffer(glctx);
    }
    else
    {
        gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(widget), 
                               glctx->renderbuffer[glctx->backbuffer][0], GL_RENDERBUFFER, 
                               1, 0, 0, glctx->width*glctx->scale, glctx->height*glctx->scale);
    }
}

int uiGLGetFramebuffer(uiGLContext* ctx)
{
    return ctx->framebuffer[ctx->backbuffer];
}

float uiGLGetFramebufferScale(uiGLContext* ctx)
{
    return (float)ctx->scale;
}

void uiGLSwapBuffers(uiGLContext* ctx)
{
    ctx->backbuffer = ctx->backbuffer ? 0 : 1;
}

void uiGLMakeContextCurrent(uiGLContext* ctx)
{
	if (!ctx)
	{
	    gdk_gl_context_clear_current();
	    return;
	}

    if (ctx->gctx == gdk_gl_context_get_current()) return;
	gdk_gl_context_make_current(ctx->gctx);
}

void uiGLBegin(uiGLContext* ctx)
{
    if (g_thread_self() != gtkthread)
    {
        g_mutex_lock(&glmutex);
    }
}

void uiGLEnd(uiGLContext* ctx)
{
    if (g_thread_self() != gtkthread)
    {
        g_mutex_unlock(&glmutex);
    }
}

void *uiGLGetProcAddress(const char* proc)
{
	// TODO: consider using epoxy or something funny
	
	void* ptr;

	ptr = glXGetProcAddressARB((const GLubyte*)proc);
	if (ptr) return ptr;
	
	ptr = eglGetProcAddress(proc);
	if (ptr) return ptr;
	
	ptr = dlsym(NULL /* RTLD_DEFAULT */, proc);
	if (ptr) return ptr;
	
	return NULL;
}
unsigned int uiGLGetVersion(uiGLContext* ctx)
{
	if (!ctx) return 0;
	return uiGLVersion(ctx->vermaj, ctx->vermin);
}

