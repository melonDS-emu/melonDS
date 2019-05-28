// 26 may 2019
#include "uipriv_unix.h"

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

extern GThread* gtkthread;

/*
 *(melonDS:17013): Gtk-CRITICAL **: 00:28:09.095: gtk_gl_area_set_required_version: assertion 'GTK_IS_GL_AREA (area)' failed

(melonDS:17013): GLib-GObject-WARNING **: 00:28:09.096: invalid cast from 'GtkGLArea' to 'areaWidget'
 */

struct uiGLContext {
	GtkGLArea *gla;
	GtkWidget* widget;
	GdkWindow* window;
	GdkGLContext *gctx;
	
	Display* xdisp;
	GLXPixmap glxpm;
	GLXContext glxctx;
	int vermaj, vermin;
	
	int width, height;
	int scale;
	GLuint renderbuffer[2];
	GLuint framebuffer;
};

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

static void _loadGLProcs()
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

Display* derp;

uiGLContext *createGLContext(GtkGLArea* gla, int maj, int min)
{
    printf("barp\n");
	uiGLContext *ret = uiNew(uiGLContext);//uiAlloc(sizeof(uiGLContext), "uiGLContext");
	
	// herp
	ret->gla = gla;
	ret->gctx = NULL;
	//while (!ret->gctx)
	/*{
	gtk_widget_realize(GTK_WIDGET(gla));
	printf("is area realized: %d\n", gtk_widget_get_realized(GTK_WIDGET(gla)));
	ret->gla = gla;
	ret->gctx = gtk_gl_area_get_context(gla);
	printf("context: %p\n", ret->gctx);
	}*/
	
	
	
	/*
	GtkAllocation allocation;
    GdkWindow *window;
    Display *display;
    int id;

    window = gtk_widget_get_window(widget);
    display = gdk_x11_display_get_xdisplay(gdk_window_get_display(window));
    id = gdk_x11_window_get_xid(window);

    if (glXMakeCurrent(display, id, context) == TRUE) {*/
    
    
    /*GdkWindow* window;
    Display* disp;
    
    window = gtk_widget_get_window(GTK_WIDGET(gla));
    display = */
	
	
	
	ret->vermaj = maj; ret->vermin = min;
	return ret;
}

static void areaAllocRenderbuffer(uiGLContext* glctx);

void databotte(GtkWidget* widget, uiGLContext* ctx)
{
    /*printf("is area realized: %d\n", gtk_widget_get_realized(GTK_WIDGET(gla)));
	ctx->gctx = gtk_gl_area_get_context(gla);
	printf("context: %p\n", ctx->gctx);*/
	
	printf("DATABOTTE\n");
	
	GdkWindow* gdkwin = gtk_widget_get_window(widget);
	printf("window=%p\n", gdkwin);
	
	GError* err = NULL;
	GdkGLContext* glctx = gdk_window_create_gl_context(gdkwin, &err);
	if (err != NULL)
	{
	    printf("CONTEXT SHAT ITSELF\n");
	    return;
	}
	
	gdk_gl_context_set_use_es(glctx, FALSE);
	gdk_gl_context_set_required_version(glctx, 3, 2);
	
	gdk_gl_context_realize(glctx, &err);
	if (err != NULL)
	{
	    printf("CONTEXT REALIZE SHAT ITSELF\n");
	    return;
	}
	
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);
	int window_scale = gdk_window_get_scale_factor(gdkwin);
	ctx->width = allocation.width;
	ctx->height = allocation.height;
	ctx->scale = window_scale;
	
	gdk_gl_context_make_current(glctx);
	_loadGLProcs();
	areaAllocRenderbuffer(ctx);
	
	ctx->widget = widget;
	ctx->window = gdkwin;
	ctx->gctx = glctx;
}

static void areaAllocRenderbuffer(uiGLContext* glctx)
{
    _glGenRenderbuffers(2, &glctx->renderbuffer[0]);printf("ylarg0 %04X, %d %d\n", glGetError(), glctx->width, glctx->height);
    //glGenTextures(2, &glctx->renderbuffer[0]);
    _glGenFramebuffers(1, &glctx->framebuffer);printf("ylarg1 %04X\n", glGetError());
    printf("FB %08X created under ctx %p (%p %p)\n", glctx?glctx->framebuffer:0, gdk_gl_context_get_current(), glctx?glctx->gctx:NULL, glctx);
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
    
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1]);printf("ylarg1 %04X\n", glGetError());
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);printf("ylarg1 %04X\n", glGetError());
    
    _glBindFramebuffer(GL_FRAMEBUFFER, glctx->framebuffer);printf("ylarg2 %04X\n", glGetError());
    /*_glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glctx->renderbuffer[0], 0);
    _glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, glctx->renderbuffer[1], 0);*/
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glctx->renderbuffer[0]);printf("ylarg3 %04X\n", glGetError());
    _glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glctx->renderbuffer[1]);printf("ylarg4 %04X\n", glGetError());
    //printf("ylarg: %08X, %04X, %08X %08X\n", glctx->framebuffer, glGetError(), glctx->renderbuffer[0], glctx->renderbuffer[1]);
    
    if(_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    printf("FRAMEBUFFER IS BAD!! %04X\n", _glCheckFramebufferStatus(GL_FRAMEBUFFER));
    
    int alpha_size;
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0]);
    _glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &alpha_size);
    printf("FRAMEBUFFER GOOD. ALPHA SIZE IS %d\n", alpha_size);
}

static void areaRellocRenderbuffer(uiGLContext* glctx)
{
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[0]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, glctx->width*glctx->scale, glctx->height*glctx->scale);
    _glBindRenderbuffer(GL_RENDERBUFFER, glctx->renderbuffer[1]);
    _glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, glctx->width*glctx->scale, glctx->height*glctx->scale);
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
                           glctx->renderbuffer[0], GL_RENDERBUFFER, 
                           1, 0, 0, glctx->width*glctx->scale, glctx->height*glctx->scale);
}

uiGLContext* FARTOMATIC(int major, int minor)
{
    uiGLContext *_ret = uiNew(uiGLContext);
    _ret->vermaj = major;
    _ret->vermin = minor;
    _ret->width = -1;
    _ret->height = -1;
    _ret->renderbuffer[0] = 0;
    _ret->renderbuffer[1] = 0;
    _ret->framebuffer = 0;
    
    return _ret;
    
    Display* disp;
    XVisualInfo* vizir;
    GLXFBConfig *cfg;
    Pixmap pm;
    GLXPixmap glxpm;
    GLXContext ctx;
    
    disp = XOpenDisplay(NULL);
    derp = disp;
    
    int kaa, baa;
    glXQueryVersion(disp, &kaa, &baa);
    printf("GL VERSION: %d.%d\n", kaa, baa);
    
    const int sbAttrib[] = { GLX_RGBA,
                            GLX_RED_SIZE, 1,
                            GLX_GREEN_SIZE, 1,
                            GLX_BLUE_SIZE, 1,
                            None };
   const int dbAttrib[] = { GLX_RGBA,
                            GLX_RED_SIZE, 1,
                            GLX_GREEN_SIZE, 1,
                            GLX_BLUE_SIZE, 1,
                            GLX_DOUBLEBUFFER,
                            None };
                            
    int scrnum = DefaultScreen( disp );
   Window root = RootWindow( disp, scrnum );

   vizir = glXChooseVisual( disp, scrnum, (int *) sbAttrib );
   if (!vizir) {
      vizir = glXChooseVisual( disp, scrnum, (int *) dbAttrib );
      if (!vizir) {
         printf("Error: couldn't get an RGB visual\n");
         return NULL;
      }
   }
   
    const int fb_attr[] = { 
      GLX_RENDER_TYPE,    GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE,  GLX_PBUFFER_BIT,
      GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
      GLX_DOUBLEBUFFER,   True,
      GLX_X_RENDERABLE,   True,
      GLX_RED_SIZE,       8,
      GLX_GREEN_SIZE,     8,
      GLX_BLUE_SIZE,      8,
      GLX_ALPHA_SIZE,     8,
      GLX_DEPTH_SIZE,     24,
      GLX_STENCIL_SIZE,   8,
      None 
    };
    int configs;
    cfg = glXChooseFBConfig(disp, scrnum, (int *)&fb_attr, &configs);

    if (!cfg)
    {
        printf("NO GOOD FBCONFIG\n");
        return NULL;
    }
   
    /*ctx = glXCreateContext( disp, vizir, NULL, True );
    if (!ctx)
    {
        printf("Error: glXCreateContext failed\n");
        return NULL;
    }*/

    PFNGLXCREATECONTEXTATTRIBSARBPROC createctx;
    createctx = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB("glXCreateContextAttribsARB");
    if (!createctx)
    {
        printf("bad shito\n");
        return NULL;
    }
   
    const int ctx_attr[] = 
    {
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,//major,
            GLX_CONTEXT_MINOR_VERSION_ARB, 2,//minor,
            None
    };
    
    ctx = glXCreateContextAttribsARB(disp, cfg[0], 0, True, ctx_attr);
    if (!ctx)
    {
        printf("FAILED TO CREATE FANCYPANTS GL CONTEXT\n");
        return NULL;
    }
   
   //printf("CONTEXT GOOD. Direct rendering: %s\n", glXIsDirect(disp, ctx) ? "Yes" : "No");
   
    printf("blorp: %d\n", vizir->depth);
    pm = XCreatePixmap(disp, root, 256, 384, vizir->depth);
    if (!pm) printf("PIXMAP SHAT ITSELF\n");
    else printf("PIXMAP GOOD\n");
    
    glxpm = glXCreateGLXPixmap(disp, vizir, pm);
    if (!glxpm) printf("GLXPIXMAP SHAT ITSELF\n");
    else printf("GLXPIXMAP GOOD\n");
    
    uiGLContext *ret = uiNew(uiGLContext);
    printf("CREATE CTX: %p, %p\n", ret, g_thread_self());
    ret->xdisp = disp;
    ret->glxpm = glxpm;
    ret->glxctx = ctx;
    
    return ret;
}

int uiGLGetFramebuffer(uiGLContext* ctx)
{
    return ctx->framebuffer;
}

float uiGLGetFramebufferScale(uiGLContext* ctx)
{
    return (float)ctx->scale;
}

void uiGLSwapBuffers(uiGLContext* ctx)
{
	if (!ctx) return;
	//gtk_gl_area_attach_buffers(ctx->gla);
}

static volatile int _ctxset_done;

gboolean _threadsafe_ctxset(gpointer data)
{
    uiGLContext* ctx = (uiGLContext*)data;
    gtk_gl_area_make_current(ctx->gla);
    _ctxset_done = 1;
    return FALSE;
}

void uiGLMakeContextCurrent(uiGLContext* ctx)
{
	//if (!ctx) return;
	/*if (g_thread_self() != gtkthread)
	{
	    _ctxset_done = 0;
	    g_idle_add(_threadsafe_ctxset, ctx);
	    while (!_ctxset_done);
	}
	else*/
	//gtk_gl_area_make_current(ctx->gla);
	/*printf("MAKE CONTEXT CURRENT %p, %p\n", ctx, g_thread_self());
	if (!ctx)
	{
	    // BLERUGEHZFZF
	    glXMakeCurrent(derp, None, NULL);
	    return;
	}
	Bool ret = True;
	if (glXGetCurrentContext() != ctx->glxctx)
	{printf("DZJSKFLD\n");
	    ret = glXMakeCurrent(ctx->xdisp, ctx->glxpm, ctx->glxctx);
	    //glXMakeContextCurrent(ctx->xdisp, ctx->glxpm, ctx->glxpm, ctx->glxctx);
	}
	printf("WE MAED IT CURRENT: %d\n", ret);*/
	
	printf("[%p] MAKE CONTEXT CURRENT %p, %p\n", g_thread_self(), ctx, ctx?ctx->gctx:NULL);
	if (!ctx)
	{
	    gdk_gl_context_clear_current();
	    return;
	}
	//gtk_gl_area_make_current(ctx->gla);
	gdk_gl_context_make_current(ctx->gctx);
	GdkGLContext* burp = gdk_gl_context_get_current();
	//gtk_gl_area_attach_buffers(ctx->gla);
	//printf("burp = %p / %p\n", burp, ctx->gctx);
}
void *uiGLGetProcAddress(const char* proc)
{
printf("get: %s - ", proc);
void* a = dlsym(NULL, proc);
void* b = glXGetProcAddress(proc);
void* c = glXGetProcAddressARB(proc);
printf("%p / %p / %p\n", a, b, c);
return c;
	// this *will* break for older systems that don't have libglvnd!
	// TODO: use a real solution
	return dlsym(NULL /* RTLD_DEFAULT */, proc);
}
unsigned int uiGLGetVersion(uiGLContext* ctx)
{
	if (!ctx) return 0;
	return uiGLVersion(ctx->vermaj, ctx->vermin);
}

