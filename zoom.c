/* See LICENSE file for copyright and license details. */
#include "common.h"

Zoom zoom = {1.0, INT_MIN};
enum zoom_mode zoom_mode = ZOOM_BEST_FIT;
const Zoom normal_zoom = {1.0, 0};

int
zoom_eq(Zoom a, Zoom b)
{
	return FLOAT_EQ(a.s, b.s);
}

double
get_zoom(Zoom z)
{
	return z.s;
}

Zoom
make_zoom(double s)
{
	return (Zoom){s, INT_MIN};
}

static Zoom
next_zoom(Zoom z, int dir)
{
	double s = z.s;
	int i;

	if (z.i != INT_MIN) {
		i = z.i + dir;
		goto have_new;
	}

#define LIMIT 70
#define EXPONENT 1.1

	s = log(s) / log(EXPONENT);
	s = round(s);
	i = (int)s;
	i += dir;

have_new:
	i = i < -LIMIT ? -LIMIT : i > +LIMIT ? +LIMIT : i;
	s = (double)i;
	s = pow(EXPONENT, s);
	return (Zoom){s, i};

#undef LIMIT
#undef EXPONENT
}

Zoom
zoom_in(Zoom z)
{
	return next_zoom(z, +1);
}

Zoom
zoom_out(Zoom z)
{
	return next_zoom(z, -1);
}

static Zoom
calculate_fit(struct image *image, int xfit, int yfit, int maxfit, double cap)
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
		s = !xfit ? y : maxfit ? MAX(s, y) : MIN(s, y);
	}

	if (s > cap)
		s = cap;

	return make_zoom(s);
}

Zoom
calculate_zoom_mode(struct image *image, enum zoom_mode mode)
{
	switch (mode) {
	case ZOOM_CUSTOM:
		break;
#define X(E, A, O, T, ...) case E: return calculate_fit(image, __VA_ARGS__)
	LIST_ZOOM_MODES(X, ;);
#undef X
	}
	return zoom;
}

int
parse_and_set_zoom_mode(const char *s)
{
	double a, b, scale;
	char *p;

#define X(E, A, OPT, ...)\
	if (!strcasecmp(s, OPT)) {\
		zoom_mode = E;\
		return 1;\
	}
	LIST_ZOOM_MODES(X,);
#undef X

	errno = 0;
	if (isspace(*s)) return 0;
	a = strtod(s, &p);
	if (a <= 0.0 || errno) return 0;
	while (*p == ' ') p++;

	if (*p == '%') {
		if (*++p) return 0;
		scale = 0.01 * a;
	} else if (*p == ':') {
		while (*++p == ' ');
		if (isspace(*p)) return 0;
		b = strtod(p, &p);
		if (b <= 0.0 || errno) return 0;
		scale = a / b;
		if (*p) return 0;
	} else {
		return 0;
	}

	if (!isfinite(scale) && scale <= 0.0)
		return 0;

	zoom_mode = ZOOM_CUSTOM;
	zoom = make_zoom(scale);
	return 1;
}
