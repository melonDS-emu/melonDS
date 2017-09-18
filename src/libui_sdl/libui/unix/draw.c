// 6 september 2015
#include "uipriv_unix.h"
#include "draw.h"

uiDrawContext *newContext(cairo_t *cr)
{
	uiDrawContext *c;

	c = uiNew(uiDrawContext);
	c->cr = cr;
	return c;
}

void freeContext(uiDrawContext *c)
{
	uiFree(c);
}

static cairo_pattern_t *mkbrush(uiDrawBrush *b)
{
	cairo_pattern_t *pat;
	size_t i;

	switch (b->Type) {
	case uiDrawBrushTypeSolid:
		pat = cairo_pattern_create_rgba(b->R, b->G, b->B, b->A);
		break;
	case uiDrawBrushTypeLinearGradient:
		pat = cairo_pattern_create_linear(b->X0, b->Y0, b->X1, b->Y1);
		break;
	case uiDrawBrushTypeRadialGradient:
		// make the start circle radius 0 to make it a point
		pat = cairo_pattern_create_radial(
			b->X0, b->Y0, 0,
			b->X1, b->Y1, b->OuterRadius);
		break;
//	case uiDrawBrushTypeImage:
	}
	if (cairo_pattern_status(pat) != CAIRO_STATUS_SUCCESS)
		implbug("error creating pattern in mkbrush(): %s",
			cairo_status_to_string(cairo_pattern_status(pat)));
	switch (b->Type) {
	case uiDrawBrushTypeLinearGradient:
	case uiDrawBrushTypeRadialGradient:
		for (i = 0; i < b->NumStops; i++)
			cairo_pattern_add_color_stop_rgba(pat,
				b->Stops[i].Pos,
				b->Stops[i].R,
				b->Stops[i].G,
				b->Stops[i].B,
				b->Stops[i].A);
	}
	return pat;
}

void uiDrawStroke(uiDrawContext *c, uiDrawPath *path, uiDrawBrush *b, uiDrawStrokeParams *p)
{
	cairo_pattern_t *pat;

	runPath(path, c->cr);
	pat = mkbrush(b);
	cairo_set_source(c->cr, pat);
	switch (p->Cap) {
	case uiDrawLineCapFlat:
		cairo_set_line_cap(c->cr, CAIRO_LINE_CAP_BUTT);
		break;
	case uiDrawLineCapRound:
		cairo_set_line_cap(c->cr, CAIRO_LINE_CAP_ROUND);
		break;
	case uiDrawLineCapSquare:
		cairo_set_line_cap(c->cr, CAIRO_LINE_CAP_SQUARE);
		break;
	}
	switch (p->Join) {
	case uiDrawLineJoinMiter:
		cairo_set_line_join(c->cr, CAIRO_LINE_JOIN_MITER);
		cairo_set_miter_limit(c->cr, p->MiterLimit);
		break;
	case uiDrawLineJoinRound:
		cairo_set_line_join(c->cr, CAIRO_LINE_JOIN_ROUND);
		break;
	case uiDrawLineJoinBevel:
		cairo_set_line_join(c->cr, CAIRO_LINE_JOIN_BEVEL);
		break;
	}
	cairo_set_line_width(c->cr, p->Thickness);
	cairo_set_dash(c->cr, p->Dashes, p->NumDashes, p->DashPhase);
	cairo_stroke(c->cr);
	cairo_pattern_destroy(pat);
}

void uiDrawFill(uiDrawContext *c, uiDrawPath *path, uiDrawBrush *b)
{
	cairo_pattern_t *pat;

	runPath(path, c->cr);
	pat = mkbrush(b);
	cairo_set_source(c->cr, pat);
	switch (pathFillMode(path)) {
	case uiDrawFillModeWinding:
		cairo_set_fill_rule(c->cr, CAIRO_FILL_RULE_WINDING);
		break;
	case uiDrawFillModeAlternate:
		cairo_set_fill_rule(c->cr, CAIRO_FILL_RULE_EVEN_ODD);
		break;
	}
	cairo_fill(c->cr);
	cairo_pattern_destroy(pat);
}

void uiDrawTransform(uiDrawContext *c, uiDrawMatrix *m)
{
	cairo_matrix_t cm;

	m2c(m, &cm);
	cairo_transform(c->cr, &cm);
}

void uiDrawClip(uiDrawContext *c, uiDrawPath *path)
{
	runPath(path, c->cr);
	switch (pathFillMode(path)) {
	case uiDrawFillModeWinding:
		cairo_set_fill_rule(c->cr, CAIRO_FILL_RULE_WINDING);
		break;
	case uiDrawFillModeAlternate:
		cairo_set_fill_rule(c->cr, CAIRO_FILL_RULE_EVEN_ODD);
		break;
	}
	cairo_clip(c->cr);
}

void uiDrawSave(uiDrawContext *c)
{
	cairo_save(c->cr);
}

void uiDrawRestore(uiDrawContext *c)
{
	cairo_restore(c->cr);
}


// bitmap API

uiDrawBitmap* uiDrawNewBitmap(uiDrawContext* c, int width, int height)
{
    /*uiDrawBitmap* bmp;
    HRESULT hr;

    bmp = uiNew(uiDrawBitmap);

    D2D1_BITMAP_PROPERTIES bp2 = D2D1::BitmapProperties();
    bp2.dpiX = 0;
    bp2.dpiY = 0;
    bp2.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    //bp2.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    // TODO: fallback: convert to BGRA if needed (RGBA only works in hardware mode)

    c->rt->BeginDraw();

    hr = c->rt->CreateBitmap(D2D1::SizeU(width,height), NULL, 0, &bp2, &bmp->bmp);
    if (hr != S_OK)
		logHRESULT(L"error creating bitmap", hr);

    c->rt->EndDraw();

    bmp->Width = width;
    bmp->Height = height;
    bmp->Stride = width*4;

    return bmp;*/
    return NULL;
}

void uiDrawBitmapUpdate(uiDrawBitmap* bmp, const void* data)
{
    //D2D1_RECT_U rekt = D2D1::RectU(0, 0, bmp->Width, bmp->Height);
    //bmp->bmp->CopyFromMemory(&rekt, data, bmp->Stride);
}

void uiDrawBitmapDraw(uiDrawContext* c, uiDrawBitmap* bmp, uiRect* srcrect, uiRect* dstrect)
{
    /*D2D_RECT_F _srcrect = D2D1::RectF(srcrect->X, srcrect->Y, srcrect->X+srcrect->Width, srcrect->Y+srcrect->Height);
    D2D_RECT_F _dstrect = D2D1::RectF(dstrect->X, dstrect->Y, dstrect->X+dstrect->Width, dstrect->Y+dstrect->Height);

    c->rt->DrawBitmap(bmp->bmp, &_dstrect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &_srcrect);*/
}

void uiDrawFreeBitmap(uiDrawBitmap* bmp)
{
    //bmp->bmp->Release();
    //uiFree(bmp);
}
