// 18 december 2015
#include "uipriv_windows.hpp"
#include "area.hpp"

// TODO: make those int rather than double
void loadAreaSize(uiArea *a, double *width, double *height)
{
	D2D1_SIZE_F size;

	if (a->width != -1)
    {
        *width = (double)a->width;
        *height = (double)a->height;
        return;
    }

	*width = 0;
	*height = 0;
	if (!a->scrolling) {
		/*if (rt == NULL)
			rt = a->rt;
		size = realGetSize(rt);
		*width = size.width;
		*height = size.height;
		dipToPixels(a, width, height);*/
		RECT rect;
        GetWindowRect(a->hwnd, &rect);
        *width = (double)(rect.right - rect.left);
        *height = (double)(rect.bottom - rect.top);
	}

	a->width = (int)*width;
	a->height = (int)*height;
}

void pixelsToDIP(uiArea *a, double *x, double *y)
{
	FLOAT dpix, dpiy;

	a->rt->GetDpi(&dpix, &dpiy);
	// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd756649%28v=vs.85%29.aspx (and others; search "direct2d mouse")
	*x = (*x * 96) / dpix;
	*y = (*y * 96) / dpiy;
}

void dipToPixels(uiArea *a, double *x, double *y)
{
	FLOAT dpix, dpiy;

	a->rt->GetDpi(&dpix, &dpiy);
	*x = (*x * dpix) / 96;
	*y = (*y * dpiy) / 96;
}
