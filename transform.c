/* See LICENSE file for copyright and license details. */
#include "common.h"

enum transformation trans = TRANS_NONE;

static GdkPixbuf *
make_negative(GdkPixbuf *image)
{
	GdkPixbuf *new_image;
	uint8_t *pixels;
	int width, height, rowstride, channels, x, y;

	new_image = gdk_pixbuf_copy(image);
	if (!new_image)
		exit(1);

	pixels    = gdk_pixbuf_get_pixels(new_image);
	width     = gdk_pixbuf_get_width(new_image);
	height    = gdk_pixbuf_get_height(new_image);
	rowstride = gdk_pixbuf_get_rowstride(new_image);
	channels  = gdk_pixbuf_get_n_channels(new_image);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width * channels; x += channels) {
			/* TODO
			 *    use libcolour to convert to CIE LChuv
			 *    (or CIE LChab which isn't supported yet),
			 *    then invert the lightness and the hue
			 *    and then convert back to sRGB; however
			 *    we must account for the fact that some
			 *    pixels may be out of gamut
			 *
			 * TODO
			 *    this may be a bit slow, so we to use
			 *    opencl if available and otherwise use
			 *    libcolour_convert_en_masse as much as
			 *    possible and cache the negative for
			 *    each file
			 *
			 * TODO
			 *    it shall be a configuration in Prefernces
			 *    what model is used for colour inversion
			 */
			pixels[x + 0] = 255 - pixels[x + 0];
			pixels[x + 1] = 255 - pixels[x + 1];
			pixels[x + 2] = 255 - pixels[x + 2];
		}
		pixels = &pixels[rowstride];
	}

	return new_image;
}

static GdkPixbuf *
apply_rotation_and_reflection(GdkPixbuf *image)
{
	GdkPixbuf *new_image, *newer_image;

	switch (trans & 7u) {
	case TRANS_NONE:
		return NULL;
	case TRANS_CLOCKWISE:
		new_image = gdk_pixbuf_rotate_simple(image, GDK_PIXBUF_ROTATE_CLOCKWISE);
		break;
	case TRANS_180_DEGREES:
		new_image = gdk_pixbuf_rotate_simple(image, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
		break;
	case TRANS_ANTICLOCKWISE:
		new_image = gdk_pixbuf_rotate_simple(image, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
		break;
	case TRANS_FLIP:
	case TRANS_TRANSPOSE:
		new_image = gdk_pixbuf_flip(image, 0);
		break;
	case TRANS_FLOP:
	case TRANS_ANTITRANSPOSE:
		new_image = gdk_pixbuf_flip(image, 1);
		break;
	}
	if (!new_image)
		exit(1);

	switch (trans & 7u) {
	case TRANS_TRANSPOSE:
	case TRANS_ANTITRANSPOSE:
		newer_image = gdk_pixbuf_rotate_simple(new_image, GDK_PIXBUF_ROTATE_CLOCKWISE);
		if (!newer_image)
			exit(1);
		g_object_unref(new_image);
		return newer_image;
	default:
		return new_image;
	}
}

GdkPixbuf *
apply_transformations(GdkPixbuf *image)
{
	GdkPixbuf *negative_image, *new_image;
	negative_image = (trans & TRANS_NEGATIVE) ? make_negative(image) : NULL;
	new_image = apply_rotation_and_reflection(negative_image ? negative_image : image);
	if (!new_image)
		new_image = negative_image ? negative_image : image;
	else if (negative_image)
		g_object_unref(negative_image);
	return new_image == image ? NULL : new_image;
}

static enum transformation
inverse_transformation(enum transformation t)
{
	return ((4u - t) & 3u) | (t & ~3u);
}

enum transformation
reset_transformations_without_redraw(void)
{
	enum transformation augment = inverse_transformation(trans & 7u);
	trans &= ~7u;
	return augment;
}

int
add_transformation_without_redraw(enum transformation augment)
{
	enum transformation old = trans;
	if (augment & TRANS_TRANSPOSE)
		trans = ((augment - trans) & 3u) | ((augment ^ trans) & ~3u);
	else
		trans = ((augment + trans) & 3u) | ((augment ^ trans) & ~3u);
	return trans != old;
}

void
transform_point(enum transformation t, double *x, double *y, double *w, double *h)
{
	double x1 = *x, y1 = *y, x2 = *w, y2 = *h;

	switch (t & 7u) {
	case TRANS_NONE:
		return;
	case TRANS_CLOCKWISE:
		*x = y2 - y1;
		*y = x1;
		break;
	case TRANS_180_DEGREES:
		*x = x2 - x1;
		*y = y2 - y1;
		return;
	case TRANS_ANTICLOCKWISE:
		*x = y1;
		*y = x2 - x1;
		break;
	case TRANS_TRANSPOSE:
		*x = y1;
		*y = x1;
		break;
	case TRANS_FLIP:
		*y = y2 - y1;
		return;
	case TRANS_ANTITRANSPOSE:
		*x = y2 - y1;
		*y = x2 - x1;
		break;
	case TRANS_FLOP:
		*x = x2 - x1;
		return;
	}

	*w = y2;
	*h = x2;
}
