// 26 may 2019
#include "uipriv_unix.h"

#include <GL/gl.h>

void* glXGetProcAddressARB(const GLubyte* name);

extern GThread* gtkthread;

struct uiGLContext 
{
	GtkWidget* widget;
	GdkWindow* window;
	
	GdkGLContext *gctx;
	int vermaj, vermin;
	
	GMutex mutex;
	
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
	
	g_mutex_init(&ctx->mutex);
	
	ctx->widget = widget;
	ctx->window = gdkwin;
	ctx->gctx = gctx;
	
    return ctx;
}

void freeGLContext(uiGLContext* glctx)
{
    if (glctx == NULL) return;
    gdk_gl_context_clear_current();
    g_object_unref(glctx->gctx);
    uiFree(glctx);
}

static void areaAllocRenderbuffer(uiGLContext* glctx)
{
    _glGenRenderbuffers(4, &glctx->renderbuffer[0][0]);printf("ylarg0 %04X, %d %d\n", glGetError(), glctx->width, glctx->height);
    //glGenTextures(2, &glctx->renderbuffer[0]);
    _glGenFramebuffers(2, &glctx->framebuffer[0]);printf("ylarg1 %04X\n", glGetError());
    printf("FB %08X created under ctx %p (%p %p)\n", glctx?glctx->framebuffer[0]:0, gdk_gl_context_get_current(), glctx?glctx->gctx:NULL, glctx);
    /*glBindTexture(GL_TEXTURE_2D, glctx->renderbuffer[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBindTexture(GL_TEXTURE_2D, glctx->renderbuffer[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);*/
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][0]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][1]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    
    _glBindFramebuffer(GL_FRAMEBUFFER, glctx->framebuffer[0]);printf("ylarg2 %04X\n", glGetError());
    /*_glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glctx->renderbuffer[0], 0);
    _glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, glctx->renderbuffer[1], 0);*/
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glctx->renderbuffer[0][0]);printf("ylarg3 %04X\n", glGetError());
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glctx->renderbuffer[0][1]);printf("ylarg4 %04X\n", glGetError());
    //printf("ylarg: %08X, %04X, %08X %08X\n", glctx->framebuffer, glGetError(), glctx->renderbuffer[0], glctx->renderbuffer[1]);
    
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][0]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][1]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    
    _glBindFramebuffer(GL_FRAMEBUFFER, glctx->framebuffer[1]);printf("ylarg2 %04X\n", glGetError());
    /*_glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glctx->renderbuffer[0], 0);
    _glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, glctx->renderbuffer[1], 0);*/
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glctx->renderbuffer[1][0]);printf("ylarg3 %04X\n", glGetError());
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glctx->renderbuffer[1][1]);printf("ylarg4 %04X\n", glGetError());
    //printf("ylarg: %08X, %04X, %08X %08X\n", glctx->framebuffer, glGetError(), glctx->renderbuffer[0], glctx->renderbuffer[1]);
    
    if(_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    printf("FRAMEBUFFER IS BAD!! %04X\n", _glCheckFramebufferStatus(GL_FRAMEBUFFER));
    
    int alpha_size;
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][0]);
    _glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &alpha_size);
    printf("FRAMEBUFFER GOOD. ALPHA SIZE IS %d\n", alpha_size);
}

static void areaRellocRenderbuffer(uiGLContext* glctx)
{
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0][1]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1][1]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
}

void areaPreRedrawGL(uiGLContext* glctx)
{
    g_mutex_lock(&glctx->mutex);
}

void areaPostRedrawGL(uiGLContext* glctx)
{
    g_mutex_unlock(&glctx->mutex);
}

void areaDrawGL(GtkWidget* widget, uiAreaDrawParams* dp, cairo_t* cr, uiGLContext* glctx)
{
    int window_scale = gdk_window_get_scale_factor(glctx->window);
    
    if (glctx->width != dp->AreaWidth || glctx->height != dp->AreaHeight || glctx->scale != window_scale)
    {
        glctx->width = dp->AreaWidth;
        glctx->height = dp->AreaHeight;
        glctx->scale = window_scale;
        areaRellocRenderbuffer(glctx);
    }
    
    gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(widget), 
                           glctx->renderbuffer[glctx->backbuffer][0], GL_RENDERBUFFER, 
                           1, 0, 0, glctx->width*glctx->scale, glctx->height*glctx->scale);
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
        g_mutex_lock(&ctx->mutex);
    }
}

void uiGLEnd(uiGLContext* ctx)
{
    if (g_thread_self() != gtkthread)
    {
        g_mutex_unlock(&ctx->mutex);
    }
}

void *uiGLGetProcAddress(const char* proc)
{
	// TODO: consider using epoxy or something funny
	
	void* ptr = dlsym(NULL /* RTLD_DEFAULT */, proc);
	if (ptr) return ptr;
	
	ptr = glXGetProcAddressARB((const GLubyte*)proc);
	if (ptr) return ptr;
	
	return NULL;
}
unsigned int uiGLGetVersion(uiGLContext* ctx)
{
	if (!ctx) return 0;
	return uiGLVersion(ctx->vermaj, ctx->vermin);
}

