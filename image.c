/* See LICENSE file for copyright and license details. */
#include "common.h"

GdkPixbuf *transformed_raster = NULL;

void
forget_transformed_raster(void)
{
	g_clear_object(&transformed_raster);
}

int
get_image(struct image *image_out)
{
	struct file *file = get_file();
	enum transformation t;
	if (!file || !file->image.raster)
		forget_transformed_raster();
	if (!file)
		return 0;
	t = apply_embedded_orientation ? multiply_transformations(trans, file->image.embedded_transformation) : trans;
	image_out->raster = transformed_raster ? transformed_raster : file->image.raster;
	image_out->vector = (t & ~7u) ? NULL : file->image.vector; /* TODO implement support for colour manipulation */
	if (swaps_dimensions(t)) {
		image_out->width = file->image.height;
		image_out->height = file->image.width;
		image_out->has_x_dpi = file->image.has_y_dpi;
		image_out->has_y_dpi = file->image.has_x_dpi;
		image_out->x_dpi = file->image.y_dpi;
		image_out->y_dpi = file->image.x_dpi;
	} else {
		image_out->width = file->image.width;
		image_out->height = file->image.height;
		image_out->has_x_dpi = file->image.has_x_dpi;
		image_out->has_y_dpi = file->image.has_y_dpi;
		image_out->x_dpi = file->image.x_dpi;
		image_out->y_dpi = file->image.y_dpi;
	}
	image_out->embedded_transformation = file->image.embedded_transformation;
	image_out->has_transformer = get_affine_matrix(&image_out->transformer, t, file->image.width, file->image.height);

	/* TODO there is a bug in cairo or rsvg that prevents rendering when the
	 *      linear part of the transformation has a negative component */
	if (t)
		image_out->vector = NULL;

	return 1;
}

int
get_original_image(struct image *image_out)
{
	struct file *file = get_file();
	if (!file || !file->image.raster)
		forget_transformed_raster();
	if (!file)
		return 0;
	image_out->raster = file->image.raster;
	image_out->vector = file->image.vector;
	image_out->width = file->image.width;
	image_out->height = file->image.height;
	image_out->has_x_dpi = file->image.has_x_dpi;
	image_out->has_y_dpi = file->image.has_y_dpi;
	image_out->embedded_transformation = file->image.embedded_transformation;
	image_out->has_transformer = 0;
	image_out->x_dpi = file->image.x_dpi;
	image_out->y_dpi = file->image.y_dpi;
	return 1;
}
