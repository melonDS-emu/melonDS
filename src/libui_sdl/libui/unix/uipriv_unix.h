// 22 april 2015
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_40
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_40
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_10
#define GDK_VERSION_MAX_ALLOWED GDK_VERSION_3_10
#include <gtk/gtk.h>
#include <math.h>
#include <dlfcn.h>		// see drawtext.c, gl.c
#include <langinfo.h>
#include <string.h>
#include <stdlib.h>
#include "../ui.h"
#include "../ui_unix.h"
#include "../common/uipriv.h"

#define gtkXMargin 12
#define gtkYMargin 12
#define gtkXPadding 12
#define gtkYPadding 6

// menu.c
extern GtkWidget *makeMenubar(uiWindow *);
extern void freeMenubar(GtkWidget *);
extern void uninitMenus(void);

// alloc.c
extern void initAlloc(void);
extern void uninitAlloc(void);

// util.c
extern void setMargined(GtkContainer *, int);

// child.c
extern struct child *newChild(uiControl *child, uiControl *parent, GtkContainer *parentContainer);
extern struct child *newChildWithBox(uiControl *child, uiControl *parent, GtkContainer *parentContainer, int margined);
extern void childRemove(struct child *c);
extern void childDestroy(struct child *c);
extern GtkWidget *childWidget(struct child *c);
extern int childFlag(struct child *c);
extern void childSetFlag(struct child *c, int flag);
extern GtkWidget *childBox(struct child *c);
extern void childSetMargined(struct child *c, int margined);

// draw.c
extern uiDrawContext *newContext(cairo_t *);
extern void freeContext(uiDrawContext *);

// drawtext.c
extern uiDrawTextFont *mkTextFont(PangoFont *f, gboolean add);
extern PangoFont *pangoDescToPangoFont(PangoFontDescription *pdesc);

// graphemes.c
extern ptrdiff_t *graphemes(const char *text, PangoContext *context);

// image.c
/*TODO remove this*/typedef struct uiImage uiImage;
extern cairo_surface_t *imageAppropriateSurface(uiImage *i, GtkWidget *w);

// cellrendererbutton.c
extern GtkCellRenderer *newCellRendererButton(void);

// future.c
extern void loadFutures(void);
extern PangoAttribute *FUTURE_pango_attr_foreground_alpha_new(guint16 alpha);
extern gboolean FUTURE_gtk_widget_path_iter_set_object_name(GtkWidgetPath *path, gint pos, const char *name);

// gl.c
extern uiGLContext *createGLContext(GtkWidget* widget, int maj, int min);
extern void freeGLContext(uiGLContext* glctx);
extern void areaPreRedrawGL(uiGLContext* glctx);
extern void areaPostRedrawGL(uiGLContext* glctx);

// notes:
// - G_DECLARE_DERIVABLE/FINAL_INTERFACE() requires glib 2.44 and that's starting with debian stretch (testing) (GTK+ 3.18) and ubuntu 15.04 (GTK+ 3.14) - debian jessie has 2.42 (GTK+ 3.14)
#define areaWidgetType (areaWidget_get_type())
#define areaWidget(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), areaWidgetType, areaWidget))
#define isAreaWidget(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), areaWidgetType))
#define areaWidgetClass(class) (G_TYPE_CHECK_CLASS_CAST((class), areaWidgetType, areaWidgetClass))
#define isAreaWidgetClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), areaWidget))
#define getAreaWidgetClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), areaWidgetType, areaWidgetClass))

typedef struct areaWidget areaWidget;
typedef struct areaWidgetClass areaWidgetClass;

extern void areaPreRedraw(areaWidget* widget);
extern void areaPostRedraw(areaWidget* widget);

