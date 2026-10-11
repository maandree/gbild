/* See LICENSE file for copyright and license details. */
#include "common.h"

struct file *files = NULL;
size_t nfiles = 0u;
size_t current_file = 0u;

void
free_files(void)
{
	while (nfiles) {
		--nfiles;
		free(files[nfiles].path);
		g_clear_object(&files[nfiles].image);
	}
	free(files);
	files = NULL;
	current_file = 0u;
}

size_t
files_from_array(struct file *selected_files, size_t nselected_files)
{
	size_t i, n;

	for (i = 0u, n = 0u; i < nselected_files; i++) {
		selected_files[i].image = NULL;
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
		new_files[i].image = NULL;
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
		files[i].image = NULL;
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
	if (current_file >= nfiles)
		return NULL;
	return &files[current_file];
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
