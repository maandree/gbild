/* See LICENSE file for copyright and license details. */
#include "common.h"

struct file *files = NULL;
size_t nfiles = 0u;
size_t current_file = 0u;

static void
null_image_data(struct file *file)
{
	file->image.raster = NULL;
	file->image.vector = NULL;
	file->exif_tags = NULL;
	file->exif = NULL;
}

void
destroy_file(struct file *file)
{
	free(file->path);
	g_clear_object(&file->image.raster);
	g_clear_object(&file->image.vector);
	if (file->exif_tags)
		g_strfreev(file->exif_tags);
	g_clear_object(&file->exif);
}

void
free_files(void)
{
	while (nfiles)
		destroy_file(&files[--nfiles]);
	free(files);
	files = NULL;
	current_file = 0u;
}

size_t
files_from_array(struct file *selected_files, size_t nselected_files)
{
	size_t i, n;

	for (i = 0u, n = 0u; i < nselected_files; i++) {
		null_image_data(&selected_files[i]);
		if (load_image(&selected_files[i], 0)) {
			if (i != n)
				selected_files[n] = selected_files[i];
			n += 1u;
		} else {
			free(selected_files[i].path);
		}
	}

	return n;
}

size_t
files_from_list(struct file **files_out, GSList *selected_files)
{
	struct file *new_files = NULL;
	GSList *file;
	size_t i;

	for (file = selected_files, i = 0u; file; file = file->next)
		i += 1u;
	if (!i)
		goto out;
	new_files = ecalloc(i, sizeof(*files));

	for (file = selected_files, i = 0u; file; file = file->next) {
		new_files[i].path = estrdup(file->data);
		null_image_data(&new_files[i]);
		g_free(file->data);

		if (load_image(&new_files[i], 0))
			i += 1u;
		else
			free(new_files[i].path);
	}
	g_slist_free(selected_files);

	if (!i) {
		free(new_files);
		new_files = NULL;
	}

out:
	*files_out = new_files;
	return i;
}

int
set_files_from_argv(int argc, char **argv)
{
	int ok = 1;
	size_t i;

	nfiles = (size_t)argc;
	if (!nfiles) {
		files = NULL;
		return 1;
	}

	files = ecalloc(nfiles, sizeof(*files));
	for (i = 0u; i < nfiles; i++) {
		files[i].path = estrdup(argv[i]);
		null_image_data(&files[i]);
		if (!load_image(&files[i], 1)) {
			free(files[i].path);
			ok = 0;
		}
	}

	return ok;

}

struct file *
get_file(void)
{
	return current_file < nfiles ? &files[current_file] : NULL;
}

int
navigate_without_redraw(size_t jump, size_t move)
{
	size_t old_file = current_file;
	if (!nfiles)
		return 0;
	current_file = jump;
	current_file += move;
	current_file %= nfiles;
	return current_file != old_file;
}
