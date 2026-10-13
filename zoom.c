/* See LICENSE file for copyright and license details. */
#include "common.h"

Zoom zoom = {1.0};
enum zoom_mode zoom_mode = ZOOM_BEST_FIT;

int
zoom_eq(Zoom a, Zoom b)
{
	return FLOAT_EQ(a, b);
}

double
get_zoom(Zoom z)
{
	return z;
}

Zoom
make_zoom(double s)
{
	return s;
}

Zoom
zoom_in(Zoom z)
{
	return get_zoom(z) * 1.25;
}

Zoom
zoom_out(Zoom z)
{
	return get_zoom(z) * (1.0 / 1.25);
}

Zoom
calculate_fit(struct image *image, int xfit, int yfit, double cap)
{
	GtkAdjustment *adjustment;
	double s = 1.0, y;

	if (xfit) {
		adjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
		s = (double)image->width;
		s = adjustment->page_size / s;
	}

	if (yfit) {
		adjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
		y = (double)image->height;
		y = adjustment->page_size / y;
		s = xfit ? MIN(s, y) : y;
	}

	if (s > cap)
		s = cap;

	return make_zoom(s);
}

Zoom
calculate_zoom_mode(struct image *image, enum zoom_mode mode)
{
	switch (mode) {
	case ZOOM_CUSTOM:      break;
	case ZOOM_NORMAL:      return calculate_fit(image, 0, 0, 1.0);
	case ZOOM_FIT_WIDTH:   return calculate_fit(image, 1, 0, HUGE_VAL);
	case ZOOM_FIT_HEIGHT:  return calculate_fit(image, 0, 1, HUGE_VAL);
	case ZOOM_FIT_WINDOW:  return calculate_fit(image, 1, 1, HUGE_VAL);
	case ZOOM_BEST_FIT:    return calculate_fit(image, 1, 1, 1.0);
	}
	return zoom;
}
