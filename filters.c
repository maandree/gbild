/* See LICENSE file for copyright and license details. */
#include "common.h"

struct filter {
	char *name;
	size_t suffixes_start;
	size_t suffixes_end;
};

static char *suffixes = NULL;
static struct filter *filters = NULL;
static size_t nfilters = 0u;
static struct filter anyfilter = {NULL, 0u, 0u};

static int
file_filter(const GtkFileFilterInfo *info, void *user)
{
	struct filter *filter = user;
	size_t pos, name_len, suffix_len;
	const char *name, *suffix;
	size_t backup;

	if (!info->filename)
		return 0;
	if (!filter || g_file_test(info->filename, G_FILE_TEST_IS_DIR))
		return 1;

	name = info->filename;
	name_len = strlen(name);
	if (!name_len)
		return 0;

	for (pos = filter->suffixes_start; pos != filter->suffixes_end; pos += suffix_len + 1u) {
		suffix = &suffixes[pos];
		suffix_len = strlen(suffix);

		backup = (size_t)(name[name_len - 1u] == '~');

		if (name_len <= 1u + suffix_len + backup)
			continue;
		if (name[name_len - (1u + suffix_len + backup)] != '.')
			continue;
		if (memcmp(&name[name_len - (suffix_len + backup)], suffix, suffix_len))
			continue;

		return 1;
	}

	return 0;
}

static int
filtercmp(const void *a_void, const void *b_void)
{
	const struct filter *a = a_void, *b = b_void;
	return strcmp(a->name, b->name);
}

static void
populate_filters(void)
{
	GSList *formats, *item;
	GdkPixbufFormat *format;
	char **extensions, **extension, *p;
	const char *name;
	size_t i, len = 0u;
	size_t ext_len, need, count;

	formats = gdk_pixbuf_get_formats();

	for (item = formats; item; item = item->next) {
		format = item->data;
		if (gdk_pixbuf_format_is_disabled(format))
			continue;

		name = gdk_pixbuf_format_get_description(format);
		extensions = gdk_pixbuf_format_get_extensions(format);

		need = 0u;
		count = 0u;
		for (extension = extensions; *extension; extension++) {
			ext_len = strlen(*extension);
			if (need > SIZE_MAX - 1u - ext_len)
				eprintf("Out of address space");
			need += ext_len + 1u;
			count += 1u;
		}
		if (need > SIZE_MAX - len)
			eprintf("Out of address space");
		if (!count)
			goto next;

		filters = ereallocarray(filters, nfilters + 1u, sizeof(*filters));
		filters[nfilters].suffixes_start = len;
		filters[nfilters].name = emalloc(strlen(name) + sizeof(" ()") + need + (count - 1u));

		p = stpcpy(stpcpy(filters[nfilters].name, name), " (");
		suffixes = erealloc(suffixes, need + len);
		for (extension = extensions;;) {
			ext_len = strlen(*extension);
			memcpy(&suffixes[len], *extension, ext_len + 1u);
			len += ext_len + 1u;

			memcpy(p, *extension, ext_len);
			p = &p[ext_len];

			if (!*++extension)
				break;
			p = stpcpy(p, ", ");
		}
		p = stpcpy(p, ")");

		for (i = 0u; i < nfilters; i++) {
			if (!strcmp(filters[nfilters].name, filters[i].name)) {
				free(filters[nfilters].name);
				len = filters[nfilters].suffixes_start;
				goto next;
			}
		}

		filters[nfilters].suffixes_end = len;
		nfilters += 1u;

	next:
		g_strfreev(extensions);
	}

	qsort(filters, nfilters, sizeof(*filters), &filtercmp);

	anyfilter.suffixes_end = len;

	g_slist_free(formats);
}

void
add_image_filters(GtkFileChooser *chooser)
{
	static int populated = 0;

	GtkFileFilter *filter;
	size_t i;

	if (!populated) {
		populate_filters();
		populated = 1;
	}

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All images");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME, file_filter, &anyfilter, NULL);
	gtk_file_chooser_add_filter(chooser, filter);

	for (i = 0u; i < nfilters; i++) {
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, filters[i].name);
		gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME, file_filter, &filters[i], NULL);
		gtk_file_chooser_add_filter(chooser, filter);
	}

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All files");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME, file_filter, NULL, NULL);
	gtk_file_chooser_add_filter(chooser, filter);
}

void
free_image_filters(void)
{
	free(suffixes);
	while (nfilters)
		free(filters[--nfilters].name);
	free(filters);
}
