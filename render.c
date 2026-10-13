/* See LICENSE file for copyright and license details. */
#include "common.h"

#include <string.h>

static uint8_t
unpremultiply(double colour, double alpha)
{
	if (colour <= 0.04045)
		colour = colour / 12.92;
	else
		colour = pow((colour + 0.055) / 1.055, 2.4);

	colour /= alpha;

	if (colour <= 0.0031308)
		colour = 12.92 * colour;
	else
		colour = fma(1.055, pow(colour, 1.0 / 2.4), -0.055);

	colour = fma(255.0, colour, 0.5);
	if (colour <= 0.0)
		return 0u;
	if (colour >= 255.0)
		return 255u;
	return (uint8_t)colour;
}

GdkPixbuf *
rasterise(RsvgHandle *vector, int width, int height)
{
	RsvgRectangle viewport = {0.0, 0.0, width, height};
	cairo_surface_t *surface = NULL;
	cairo_t *cr = NULL;
	GdkPixbuf *image;
	int x, y, sstride, dstride;
	const uint8_t *s, *sy;
	uint8_t *d, *dy;
	double a, r, g, b;
	uint32_t pixel;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
		goto fail;

	cr = cairo_create(surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
		goto fail;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	if (!rsvg_handle_render_document(vector, cr, &viewport, NULL))
		goto fail;
	cairo_destroy(cr);
	cr = NULL;

	image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 1, 8, width, height);
	if (!image)
		goto fail;

	/* TODO accelerate */
	cairo_surface_flush(surface);
	sy = s = cairo_image_surface_get_data(surface);
	dy = d = gdk_pixbuf_get_pixels(image);
	sstride = cairo_image_surface_get_stride(surface);
	dstride = gdk_pixbuf_get_rowstride(image);
	for (y = 0; y < height; y++, sy = s = &s[sstride], dy = d = &d[dstride]) {
		for (x = 0; x < width; x++, sy = &sy[4u], dy = &dy[4u]) {
			memcpy(&pixel, sy, 4u);
			if ((uint8_t)(pixel >> 24)) {
				a = (double)(uint8_t)(pixel >> 24) / 255.0;
				r = (double)(uint8_t)(pixel >> 16) / 255.0;
				g = (double)(uint8_t)(pixel >> 8) / 255.0;
				b = (double)(uint8_t)(pixel >> 0) / 255.0;
				dy[0] = unpremultiply(r, a);
				dy[1] = unpremultiply(g, a);
				dy[2] = unpremultiply(b, a);
				dy[3] = (uint8_t)(pixel >> 24);
			} else {
				dy[0] = dy[1] = dy[2] = dy[3] = 0u;
			}
		}
	}

	cairo_surface_destroy(surface);
	return image;

fail:
	if (cr)
		cairo_destroy(cr);
	if (surface)
		cairo_surface_destroy(surface);
	return NULL;
}

void
paint_image(cairo_t *cr, struct image *image)
{
	RsvgRectangle viewport = {0.0, 0.0, image->width, image->height};
	int rendered;

	if (image->vector) {
		cairo_save(cr);
		if (image->has_transformer)
			cairo_transform(cr, &image->transformer);
		rendered = rsvg_handle_render_document(image->vector, cr, &viewport, NULL);
		cairo_restore(cr);
		if (rendered)
			return;
	}

	if (image->raster) {
		gdk_cairo_set_source_pixbuf(cr, image->raster, 0.0, 0.0);
		cairo_paint(cr);
	}
}
