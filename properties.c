/* See LICENSE file for copyright and license details. */
#include "common.h"

static const char *svg_units[] = {"%", "px", "em", "ex", "in", "cm", "mm", "pt", "pc", "ch"};

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
add_metadata_row(GtkTable *table, guint row, PangoAttrList *name_attributes, const char *name, const char *value)
{
	GtkWidget *property = gtk_vbox_new(0, 0);
	GtkWidget *name_label = new_metadata_label(name);
	GtkWidget *value_label = new_metadata_label(value);
	gtk_label_set_line_wrap(GTK_LABEL(name_label), 0);
	gtk_label_set_line_wrap(GTK_LABEL(value_label), 0);
	gtk_label_set_attributes(GTK_LABEL(name_label), name_attributes);
	gtk_box_pack_start(GTK_BOX(property), name_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(property), value_label, 0, 0, 0);
	gtk_table_attach(table, property, 0, 1, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
}

static void
add_thumbnail_row(GtkTable *table, guint row, PangoAttrList *name_attributes, GdkPixbuf *thumbnail)
{
	GtkWidget *property = gtk_vbox_new(0, 0);
	GtkWidget *name_label = new_metadata_label("EXIF thumbnail");
	GtkWidget *image = gtk_image_new_from_pixbuf(thumbnail);
	gtk_label_set_line_wrap(GTK_LABEL(name_label), 0);
	gtk_label_set_attributes(GTK_LABEL(name_label), name_attributes);
	gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(property), name_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(property), image, 0, 0, 0);
	gtk_table_attach(table, property, 0, 1, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
}

static GdkPixbuf *
load_exif_thumbnail(GExiv2Metadata *metadata, enum transformation orientation)
{
	GdkPixbuf *thumbnail = NULL;
	GdkPixbufLoader *loader;
	GError *error = NULL;
	guint8 *data = NULL;
	gint size = 0;

	if (!metadata || !gexiv2_metadata_get_exif_thumbnail(metadata, &data, &size) || size <= 0)
		goto out;

	loader = gdk_pixbuf_loader_new();
	if (loader && gdk_pixbuf_loader_write(loader, data, (size_t)size, &error) &&
	    gdk_pixbuf_loader_close(loader, &error) &&
	    (thumbnail = gdk_pixbuf_loader_get_pixbuf(loader)))
		g_object_ref(thumbnail);
	g_clear_error(&error);
	g_clear_object(&loader);

out:
	g_free(data);
	if (thumbnail && orientation != TRANS_NONE) {
		GdkPixbuf *oriented = transform_raster(thumbnail, orientation);
		g_object_unref(thumbnail);
		thumbnail = oriented;
	}
	return thumbnail;
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
format_pixel_size(GdkPixbuf *raster)
{
	int w = gdk_pixbuf_get_width(raster);
	int h = gdk_pixbuf_get_height(raster);
	char *text = emalloc(2u * 3u * sizeof(w) + sizeof(" ×  pixels"));
	if (sprintf(text, "%i × %i pixels", w, h) < 0)
		eprintf("sprintf:");
	return text;
}

static char *
format_orientation(const char *value)
{
#define X(I, E, T) T
	static const char *const descriptions[] = { LIST_ORIENTATIONS(X, COMMA) };
#undef X
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
format_svg_lengths(RsvgLength width, RsvgLength height)
{
	char *text;
	double x = width.length;
	double y = height.length;
	const char *xunit = "";
	const char *yunit = "";
	if (width.unit == RSVG_UNIT_PERCENT)
		x *= 100.0;
	if (height.unit == RSVG_UNIT_PERCENT)
		y *= 100.0;
	if ((unsigned)width.unit < ELEMSOF(svg_units))
		xunit = svg_units[width.unit];
	if ((unsigned)height.unit < ELEMSOF(svg_units))
		yunit = svg_units[height.unit];
	if (width.unit == height.unit) {
		if (asprintf(&text, "%g × %g %s", x, y, xunit) < 0)
			eprintf("asprintf:");
	} else {
		if (asprintf(&text, "%g %s × %g %s", x, xunit, y, yunit) < 0)
			eprintf("asprintf:");
	}
	return text;
}

static char *
format_svg_length(RsvgLength length)
{
	char *text;
	double value = length.length;
	const char *unit = "";
	if (length.unit == RSVG_UNIT_PERCENT)
		value *= 100.0;
	if ((unsigned)length.unit < ELEMSOF(svg_units))
		unit = svg_units[length.unit];
	if (asprintf(&text, "%g %s", value, unit) < 0)
		eprintf("asprintf:");
	return text;
}

static char *
format_svg_viewbox(RsvgRectangle viewbox)
{
	char *text;
	if (asprintf(&text, "%g × %g from (%g, %g)", viewbox.width, viewbox.height, viewbox.x, viewbox.y) < 0)
		eprintf("asprintf:");
	return text;
}

GtkWidget *
create_property_table(struct file *file)
{
	GtkWidget *table;
	GdkPixbuf *thumbnail;
	PangoContext *context;
	PangoFontMetrics *metrics;
	PangoAttrList *title_attributes;
	unsigned i, row = 0u, rows = 2u;
	const char *orientation = NULL;
	RsvgLength svg_width, svg_height;
	RsvgRectangle svg_viewbox;
	gboolean svg_has_width = 0, svg_has_height = 0, svg_has_viewbox = 0;
	char *text, *tag, orientation_buf[3u * sizeof(long int) + 1u];
	int spacing;
	GList *keys, *key;
	GHashTable *options;
	GError *error;
	long int li;

	title_attributes = pango_attr_list_new();
	pango_attr_list_insert(title_attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));

	options = gdk_pixbuf_get_options(file->image.raster);

	thumbnail = load_exif_thumbnail(file->exif, file->image.embedded_transformation);
	rows += thumbnail ? 1u : 0u;

	if (file->image.vector) {
		rsvg_handle_get_intrinsic_dimensions(file->image.vector,
		                                     &svg_has_width, &svg_width,
		                                     &svg_has_height, &svg_height,
		                                     &svg_has_viewbox, &svg_viewbox);
		rows += (unsigned)((svg_has_width | svg_has_height) + svg_has_viewbox);
	}

	keys = g_list_sort(g_hash_table_get_keys(options), (GCompareFunc)&strcmp);
	for (key = keys; key; key = key->next)
		rows += (unsigned)is_unrecognised_metadata(key->data);

	rows += file->exif_tags ? (unsigned)g_strv_length(file->exif_tags) : 0u;

	if (file->image.raster && !file->image.vector) {
		rows += 1u;

		orientation = gdk_pixbuf_get_option(file->image.raster, "orientation");
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
	}

	table = gtk_table_new(rows, 1, 0);
	context = gtk_widget_get_pango_context(table);
	metrics = pango_context_get_metrics(context, NULL, pango_context_get_language(context));
	spacing = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics));
	spacing /= 2;
	pango_font_metrics_unref(metrics);
	gtk_table_set_row_spacings(GTK_TABLE(table), (guint)spacing);

#define ROW(NAME, VALUE)\
	(text = NULL, add_metadata_row(GTK_TABLE(table), row++, title_attributes, NAME, VALUE), free(text))

	ROW("File", file->path);
	ROW("File size", text = format_file_size(file->size));
	if (svg_has_width && svg_has_height)
		ROW("Dimensions", text = format_svg_lengths(svg_width, svg_height));
	else if (svg_has_width)
		ROW("Width", text = format_svg_length(svg_width));
	else if (svg_has_height)
		ROW("Height", text = format_svg_length(svg_height));
	if (svg_has_viewbox)
		ROW("View box", text = format_svg_viewbox(svg_viewbox));
	if (file->image.raster && !file->image.vector)
		ROW("Dimensions", text = format_pixel_size(file->image.raster));
	if (orientation)
		ROW("Orientation", text = format_orientation(orientation));
	if (file->image.has_x_dpi)
		ROW("Horizontal resolution", text = format_resolution(file->image.x_dpi));
	if (file->image.has_y_dpi)
		ROW("Vertical resolution", text = format_resolution(file->image.y_dpi));
	if (file->image.has_x_dpi)
		ROW("Physical width", text = format_physical_size(file->image.width, file->image.x_dpi));
	if (file->image.has_y_dpi)
		ROW("Physical height", text = format_physical_size(file->image.height, file->image.y_dpi));

	for (key = keys; key; key = key->next)
		if (is_unrecognised_metadata(key->data))
			ROW(key->data, g_hash_table_lookup(options, key->data));
	g_list_free(keys);

	if (file->exif_tags) {
		for (i = 0u; file->exif_tags[i]; i++) {
			error = NULL;
			tag = gexiv2_metadata_try_get_tag_interpreted_string(file->exif, file->exif_tags[i], &error);
			g_clear_error(&error);
			if (tag)
				ROW(file->exif_tags[i], tag);
			g_free(tag);
		}
	}

	if (thumbnail)
		add_thumbnail_row(GTK_TABLE(table), row++, title_attributes, thumbnail);

#undef ROW

	g_clear_object(&thumbnail);
	pango_attr_list_unref(title_attributes);
	return table;
}

void
open_properties(struct file *file)
{
	GtkWidget *dialog, *scrolled;
	char *title;

	if (!file || !file->path)
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
