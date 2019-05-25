// 26 may 2019
#include "uipriv_unix.h"

/*
 *(melonDS:17013): Gtk-CRITICAL **: 00:28:09.095: gtk_gl_area_set_required_version: assertion 'GTK_IS_GL_AREA (area)' failed

(melonDS:17013): GLib-GObject-WARNING **: 00:28:09.096: invalid cast from 'GtkGLArea' to 'areaWidget'
 */

struct uiGLContext {
	GtkGLArea *gla;
	GdkGLContext *gctx;
	int vermaj, vermin;
};

uiGLContext *createGLContext(GtkGLArea* gla, int maj, int min)
{
	uiGLContext *ret = uiAlloc(sizeof(uiGLContext), "uiGLContext");
	ret->gla = gla;
	ret->gctx = gtk_gl_area_get_context(gla);
	ret->vermaj = maj; ret->vermin = min;
	return ret;
}

void uiGLSwapBuffers(uiGLContext* ctx)
{
	if (!ctx) return;
	gtk_gl_area_attach_buffers(ctx->gla);
}

void uiGLMakeContextCurrent(uiGLContext* ctx)
{
	if (!ctx) return;
	gtk_gl_area_make_current(ctx->gla);
}
void *uiGLGetProcAddress(const char* proc)
{
	// this *will* break for older systems that don't have libglvnd!
	// TODO: use a real solution
	return dlsym(NULL /* RTLD_DEFAULT */, proc);
}
unsigned int uiGLGetVersion(uiGLContext* ctx)
{
	if (!ctx) return 0;
	return uiGLVersion(ctx->vermaj, ctx->vermin);
}

