/* See LICENSE file for copyright and license details. */
#include "common.h"

GdkPixbuf *transformed_image = NULL;

static void
forget_transformed_image(void)
{
	if (transformed_image) {
		g_object_unref(transformed_image);
		transformed_image = NULL;
	}
}

GdkPixbuf *
get_image(void)
{
	struct file *file = get_file();
	if (!file || !file->image) {
		forget_transformed_image();
		return NULL;
	}
	return transformed_image ? transformed_image : file->image;
}

GdkPixbuf *
get_original_image(void)
{
	struct file *file = get_file();
	if (!file || !file->image) {
		forget_transformed_image();
		return NULL;
	}
	return file->image;
}
