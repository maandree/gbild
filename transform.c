/* See LICENSE file for copyright and license details. */
#include "common.h"

enum transformation trans = TRANS_NONE;

static GdkPixbuf *
make_negative_raster(GdkPixbuf *raster)
{
	GdkPixbuf *new_raster;
	uint8_t *pixels;
	int width, height, rowstride, channels, x, y;

	new_raster = gdk_pixbuf_copy(raster);
	if (!new_raster)
		exit(1);

	pixels    = gdk_pixbuf_get_pixels(new_raster);
	width     = gdk_pixbuf_get_width(new_raster);
	height    = gdk_pixbuf_get_height(new_raster);
	rowstride = gdk_pixbuf_get_rowstride(new_raster);
	channels  = gdk_pixbuf_get_n_channels(new_raster);

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

	return new_raster;
}

static GdkPixbuf *
apply_rotation_and_reflection(GdkPixbuf *raster)
{
	GdkPixbuf *new_raster, *newer_raster;

	switch (trans & 7u) {
	case TRANS_NONE:
		return NULL;
	case TRANS_CLOCKWISE:
		new_raster = gdk_pixbuf_rotate_simple(raster, GDK_PIXBUF_ROTATE_CLOCKWISE);
		break;
	case TRANS_180_DEGREES:
		new_raster = gdk_pixbuf_rotate_simple(raster, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
		break;
	case TRANS_ANTICLOCKWISE:
		new_raster = gdk_pixbuf_rotate_simple(raster, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
		break;
	case TRANS_FLIP:
	case TRANS_TRANSPOSE:
		new_raster = gdk_pixbuf_flip(raster, 0);
		break;
	case TRANS_FLOP:
	case TRANS_ANTITRANSPOSE:
		new_raster = gdk_pixbuf_flip(raster, 1);
		break;
	}
	if (!new_raster)
		exit(1);

	switch (trans & 7u) {
	case TRANS_TRANSPOSE:
	case TRANS_ANTITRANSPOSE:
		newer_raster = gdk_pixbuf_rotate_simple(new_raster, GDK_PIXBUF_ROTATE_CLOCKWISE);
		if (!newer_raster)
			exit(1);
		g_object_unref(new_raster);
		return newer_raster;
	default:
		return new_raster;
	}
}

int
apply_transformations(struct image *image, struct image *image_out)
{
	GdkPixbuf *negative_raster, *new_raster;

	if (trans == TRANS_NONE)
		return 0;

	if (image->raster) {
		negative_raster = (trans & TRANS_NEGATIVE) ? make_negative_raster(image->raster) : NULL;
		new_raster = apply_rotation_and_reflection(negative_raster ? negative_raster : image->raster);
		if (!new_raster)
			new_raster = negative_raster;
		else if (negative_raster)
			g_object_unref(negative_raster);
		image_out->raster = new_raster;
		if (!image_out->raster)
			return 0;
	}

	image_out->vector = image->vector;

	if (swaps_dimensions(trans)) {
		image_out->width = image->height;
		image_out->height = image->width;
		image_out->has_x_dpi = image->has_y_dpi;
		image_out->has_y_dpi = image->has_x_dpi;
		image_out->x_dpi = image->y_dpi;
		image_out->y_dpi = image->x_dpi;
	} else {
		image_out->width = image->width;
		image_out->height = image->height;
		image_out->has_x_dpi = image->has_x_dpi;
		image_out->has_y_dpi = image->has_y_dpi;
		image_out->x_dpi = image->x_dpi;
		image_out->y_dpi = image->y_dpi;
	}
	image_out->has_transformer = get_affine_matrix(&image_out->transformer, trans, image->width, image->height);

	return 1;
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

int
swaps_dimensions(enum transformation t)
{
	switch (t & 7u) {
	case TRANS_NONE:
	case TRANS_180_DEGREES:
	case TRANS_FLIP:
	case TRANS_FLOP:
		return 0;
	case TRANS_CLOCKWISE:
	case TRANS_ANTICLOCKWISE:
	case TRANS_TRANSPOSE:
	case TRANS_ANTITRANSPOSE:
		return 1;
	}
	abort();
}

void
transform_point(enum transformation t, double *x, double *y, double *w, double *h)
{
	double x1 = *x, y1 = *y, x2 = *w, y2 = *h;
	switch (t & 7u) {
	case TRANS_NONE:
		break;
	case TRANS_CLOCKWISE:
		*x = y2 - y1;
		*y = x1;
		break;
	case TRANS_180_DEGREES:
		*x = x2 - x1;
		*y = y2 - y1;
		break;
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
		break;
	case TRANS_ANTITRANSPOSE:
		*x = y2 - y1;
		*y = x2 - x1;
		break;
	case TRANS_FLOP:
		*x = x2 - x1;
		break;
	}
	if (swaps_dimensions(t)) {
		*w = y2;
		*h = x2;
	}
}

int
get_affine_matrix(cairo_matrix_t *matrix_out, enum transformation t, int width, int height)
{
	double w = (double)width;
	double h = (double)height;
	switch (t & 7u) {
	case TRANS_NONE:           cairo_matrix_init(matrix_out, +1.0,  0.0,  0.0, +1.0, 0.0, 0.0);  return 0;
	case TRANS_CLOCKWISE:      cairo_matrix_init(matrix_out,  0.0, +1.0, -1.0,  0.0, h,   0.0);  break;
	case TRANS_180_DEGREES:    cairo_matrix_init(matrix_out, -1.0,  0.0,  0.0, -1.0, w,   h);    break;
	case TRANS_ANTICLOCKWISE:  cairo_matrix_init(matrix_out,  0.0, -1.0, +1.0,  0.0, 0.0, w);    break;
	case TRANS_TRANSPOSE:      cairo_matrix_init(matrix_out,  0.0, +1.0, +1.0,  0.0, 0.0, 0.0);  break;
	case TRANS_FLOP:           cairo_matrix_init(matrix_out, -1.0,  0.0,  0.0, +1.0, w,   0.0);  break;
	case TRANS_ANTITRANSPOSE:  cairo_matrix_init(matrix_out,  0.0, -1.0, -1.0,  0.0, h,   w);    break;
	case TRANS_FLIP:           cairo_matrix_init(matrix_out, +1.0,  0.0,  0.0, -1.0, 0.0, h);    break;
	}
	return 1;
}
