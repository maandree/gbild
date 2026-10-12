/* See LICENSE file for copyright and license details. */
#include "common.h"

static int
get_dpi_option(GdkPixbuf *image, const char *name, double *dpi)
{
	const char *text = gdk_pixbuf_get_option(image, name);
	char *end;
	double value;
	if (!text)
		return 0;
	value = strtod(text, &end);
	while (isspace(*(unsigned char *)end))
		end++;
	if (end == text || *end || !isfinite(value) || value <= 0.0)
		return 0;
	*dpi = value;
	return 1;
}

static GtkWidget *
new_metadata_label(const char *text)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_selectable(GTK_LABEL(label), 1);
	gtk_label_set_line_wrap(GTK_LABEL(label), 1);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	return label;
}

static void
add_metadata_row(GtkTable *table, guint row, const char *name, const char *value)
{
	GtkWidget *name_label = new_metadata_label(name);
	GtkWidget *value_label = new_metadata_label(value);
	gtk_table_attach(table, name_label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(table, value_label, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 12, 0);
}

static int
is_unrecognised_metadata(const char *name)
{
	return strcmp(name, "orientation") &&
	       strcmp(name, "original-width") &&
	       strcmp(name, "original-height");
}

static char *
format_title(struct file *file)
{
	char *title;
	const char *name = strrchr(file->path, '/');
	name = name ? &name[1u] : file->path;
	title = emalloc(sizeof("Properties: ") + strlen(name));
	stpcpy(stpcpy(title, "Properties: "), name);
	return title;
}

static char *
format_file_size(off_t size)
{
	static const char units[] = " KMGTPEZYRQ";
	uintmax_t bytes = size < 0 ? 0u : (uintmax_t)size;
	uintmax_t bytes0 = bytes;
	uintmax_t divisions[sizeof(units) - 1u];
	size_t ndivisions = 0u;
	size_t low = SIZE_MAX;
	char textbuf[sizeof("1024") * sizeof(units) + sizeof(" _B ( bytes)") + 7u * sizeof(bytes)];
	char subbuf[3u * sizeof(bytes) + 1u];
	size_t i, off = 0u;
	int r, p, len;

	if (bytes < 1024u) {
		if (sprintf(textbuf, "%ju bytes", bytes) < 0)
			eprintf("sprintf:");
		return estrdup(textbuf);
	}

	while (bytes && units[ndivisions]) {
		divisions[ndivisions] = (bytes & 1023u);
		bytes >>= 10;
		if (divisions[ndivisions] && low > ndivisions)
			low = ndivisions;
		ndivisions++;
	}
	if (bytes)
		divisions[ndivisions - 1] |= bytes << 10;

	for (i = ndivisions; i-- > low;) {
		r = sprintf(&textbuf[off], ":%ju", divisions[i]);
		if (r < 0)
			eprintf("sprintf:");
		off += (size_t)r;
	}

	r = sprintf(&textbuf[off], " %cB (", units[ndivisions - 1]);
	if (r < 0)
		eprintf("sprintf:");
	off += (size_t)r;

	len = sprintf(subbuf, "%ju", bytes0);
	if (len < 0)
		eprintf("sprintf:");
	r = len % 3;
	r = r ? r : 3;
	memcpy(&textbuf[off], subbuf, (size_t)r);
	off += (size_t)r;
	for (p = r; p < len; p += 3) {
		textbuf[off++] = ' ';
		memcpy(&textbuf[off], &subbuf[p], 3u);
		off += 3u;
	}

	r = sprintf(&textbuf[off], " bytes)");
	if (r < 0)
		eprintf("sprintf:");

	return estrdup(&textbuf[1u]);
}

static char *
format_pixel_size(GdkPixbuf *image)
{
	int w = gdk_pixbuf_get_width(image);
	int h = gdk_pixbuf_get_height(image);
	char *text = emalloc(2u * 3u * sizeof(w) + sizeof(" ×  pixels"));
	if (sprintf(text, "%i × %i pixels", w, h) < 0)
		eprintf("sprintf:");
	return text;
}

static char *
format_orientation(const char *value)
{
	static const char *const descriptions[] = {
		"Normal",
		"Flip horizontally",
		"Rotate 180°",
		"Flip vertically",
		"Transpose",
		"Rotate 90° clockwise",
		"Antitranspose",
		"Rotate 90° counterclockwise",
	};
	char *end, *text;
	long int orientation = strtol(value, &end, 10);
	while (isspace((unsigned char)*end))
		end++;
	if (end == value || *end || orientation < 1 || orientation > 8)
		return estrdup(value);
	if (asprintf(&text, "%s (%s)", descriptions[orientation - 1], value) < 0)
		eprintf("asprintf:");
	return text;
}

static char *
format_resolution(double dpi)
{
	char *text;
	if (asprintf(&text, "%g DPI", dpi) < 0)
		eprintf("asprintf:");
	return text;
}

static char *
format_physical_size(int pixels, double resolution)
{
	char *text;
	if (asprintf(&text, "%g cm", 2.54 * (double)pixels / resolution) < 0)
		eprintf("asprintf:");
	return text;
}

static char *
format_physical_width(GdkPixbuf *image, double resolution)
{
	return format_physical_size(gdk_pixbuf_get_width(image), resolution);
}

static char *
format_physical_height(GdkPixbuf *image, double resolution)
{
	return format_physical_size(gdk_pixbuf_get_height(image), resolution);
}

static GtkWidget *
create_property_table(struct file *file)
{
	GtkWidget *table;
	unsigned i, row = 0u, rows = 3u;
	const char *orientation;
	char *text, orientation_buf[3u * sizeof(long int) + 1u];
	double x_dpi, y_dpi;
	int has_x_dpi, has_y_dpi;
	GList *keys, *key;
	GHashTable *options;
	GError *error;
	long int li;

	options = gdk_pixbuf_get_options(file->image);

	keys = g_list_sort(g_hash_table_get_keys(options), (GCompareFunc)&strcmp);
	for (key = keys; key; key = key->next)
		rows += (unsigned)is_unrecognised_metadata(key->data);

	rows += file->exif_tags ? (unsigned)g_strv_length(file->exif_tags) : 0u;

	orientation = gdk_pixbuf_get_option(file->image, "orientation");
	if (!orientation && file->exif) {
		error = NULL;
		li = gexiv2_metadata_try_get_tag_long(file->exif, "Exif.Image.Orientation", &error);
		if (error)
			g_clear_error(&error);
		else if (sprintf(orientation_buf, "%li", li) < 0)
			eprintf("sprintf:");
		else
			orientation = orientation_buf;
	}
	rows += orientation ? 1u : 0u;

	has_x_dpi = get_dpi_option(file->image, "x-dpi", &x_dpi);
	has_y_dpi = get_dpi_option(file->image, "y-dpi", &y_dpi);
	rows += 2u * (unsigned)(has_x_dpi + has_y_dpi);

	table = gtk_table_new(rows, 2, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 12);

	add_metadata_row(GTK_TABLE(table), row++, "File", file->path);
	add_metadata_row(GTK_TABLE(table), row++, "File size", text = format_file_size(file->size));
	free(text);
	add_metadata_row(GTK_TABLE(table), row++, "Pixel dimensions", text = format_pixel_size(file->image));
	free(text);
	if (orientation) {
		add_metadata_row(GTK_TABLE(table), row++, "Orientation", text = format_orientation(orientation));
		free(text);
	}
	if (has_x_dpi) {
		add_metadata_row(GTK_TABLE(table), row++, "Horizontal resolution", text = format_resolution(x_dpi));
		free(text);
	}
	if (has_y_dpi) {
		add_metadata_row(GTK_TABLE(table), row++, "Vertical resolution", text = format_resolution(y_dpi));
		free(text);
	}
	if (has_x_dpi) {
		add_metadata_row(GTK_TABLE(table), row++, "Physical width", text = format_physical_width(file->image, x_dpi));
		free(text);
	}
	if (has_y_dpi) {
		add_metadata_row(GTK_TABLE(table), row++, "Physical height", text = format_physical_height(file->image, x_dpi));
		free(text);
	}

	for (key = keys; key; key = key->next)
		if (is_unrecognised_metadata(key->data))
			add_metadata_row(GTK_TABLE(table), row++, key->data, g_hash_table_lookup(options, key->data));
	g_list_free(keys);

	if (file->exif_tags) {
		for (i = 0u; file->exif_tags[i]; i++) {
			error = NULL;
			text = gexiv2_metadata_try_get_tag_interpreted_string(file->exif, file->exif_tags[i], &error);
			g_clear_error(&error);

			add_metadata_row(GTK_TABLE(table), row++, file->exif_tags[i], text ? text : "");
			g_free(text);
		}
	}

	return table;
}

void
open_properties(struct file *file)
{
	GtkWidget *dialog, *scrolled;
	char *title;

	if (!file || !file->path || !file->image)
		return;

	dialog = gtk_dialog_new_with_buttons(title = format_title(file),
	                                     GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
	                                     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	free(title);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolled, 1, 1, 0);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), create_property_table(file));

	g_signal_connect_swapped(dialog, "response", G_CALLBACK(&gtk_widget_destroy), dialog);
	gtk_widget_show_all(dialog);
}
