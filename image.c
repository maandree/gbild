/* See LICENSE file for copyright and license details. */
#include "common.h"

GdkPixbuf *transformed_raster = NULL;

void
forget_transformed_raster(void)
{
	g_clear_object(&transformed_raster);
}

int
get_image(struct image *image)
{
	struct file *file = get_file();
	if (!file || !file->image.raster)
		forget_transformed_raster();
	if (!file)
		return 0;
	image->raster = transformed_raster ? transformed_raster : file->image.raster;
	image->vector = (trans & ~7u) ? NULL : file->image.vector; /* TODO implement support for colour manipulation */
	if (swaps_dimensions(trans)) {
		image->width = file->image.height;
		image->height = file->image.width;
		image->has_x_dpi = file->image.has_y_dpi;
		image->has_y_dpi = file->image.has_x_dpi;
		image->x_dpi = file->image.y_dpi;
		image->y_dpi = file->image.x_dpi;
	} else {
		image->width = file->image.width;
		image->height = file->image.height;
		image->has_x_dpi = file->image.has_x_dpi;
		image->has_y_dpi = file->image.has_y_dpi;
		image->x_dpi = file->image.x_dpi;
		image->y_dpi = file->image.y_dpi;
	}
	image->has_transformer = get_affine_matrix(&image->transformer, trans, file->image.width, file->image.height);

	/* TODO there is a bug in cairo or rsvg that prevents rendering when the
	 *      linear part of the transformation has a negative component */
	if (trans)
		image->vector = NULL;

	return 1;
}

int
get_original_image(struct image *image)
{
	struct file *file = get_file();
	if (!file || !file->image.raster)
		forget_transformed_raster();
	if (!file)
		return 0;
	image->raster = file->image.raster;
	image->vector = file->image.vector;
	image->width = file->image.width;
	image->height = file->image.height;
	image->has_x_dpi = file->image.has_x_dpi;
	image->has_y_dpi = file->image.has_y_dpi;
	image->has_transformer = 0;
	image->x_dpi = file->image.x_dpi;
	image->y_dpi = file->image.y_dpi;
	return 1;
}
