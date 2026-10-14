/* See LICENSE file for copyright and license details. */
#include "common.h"

static GExiv2Metadata *
load_exif_metadata(const unsigned char *data, size_t length, char ***tags_out)
{
	GExiv2Metadata *metadata;
	GError *error = NULL;
	*tags_out = NULL;
	metadata = gexiv2_metadata_new();
	if (gexiv2_metadata_open_buf(metadata, data, (long int)length, &error)) {
		*tags_out = gexiv2_metadata_get_exif_tags(metadata);
		return metadata;
	}
	g_clear_error(&error);
	g_object_unref(metadata);
	return NULL;
}

static int
read_file(int fd, unsigned char **buf_out, size_t *size_out)
{
	unsigned char *buf = NULL, *new;
	size_t off = 0u;
	size_t size = 0u;
	ssize_t r;
	int saved_errno;

	*buf_out = NULL;
	*size_out = 0u;

	for (;;) {
		if (off == size) {
			if (size > SIZE_MAX - (8u << 10)) {
				errno = ENOMEM;
				goto fail;
			}
			new = realloc(buf, size += 8u << 10);
			if (!new)
				goto fail;
			buf = new;
		}

		r = read(fd, &buf[off], size - off);
		if (r <= 0) {
			if (!r)
				break;
			if (errno == EINTR)
				continue;
			goto fail;
		}
		off += (size_t)r;
	}

	*buf_out = buf;
	*size_out = off;

	return 0;

fail:
	saved_errno = errno;
	free(buf);
	errno = saved_errno;
	return -1;
}

static int
get_raster_dpi_option(GdkPixbuf *raster, const char *name, double *dpi)
{
	const char *text = gdk_pixbuf_get_option(raster, name);
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

static int
parse_orientation(const char *s)
{
	char c;
	if (!s) return 0;
	while (isspace(*s)) s++;
	c = *s;
	if (!c) return 0;
	while (isspace(*s)) s++;
	return (!*s || c < '1' || c > '8') ? 0 : (int)c - '0';
}

int
load_image(struct file *file, int errors_to_stderr)
{
	const char *error_message;
	GError *error = NULL;
	GtkWidget *dialog;
	GdkPixbuf *raster;
	GdkPixbufLoader *loader = NULL;
	GdkPixbufFormat *format;
	RsvgHandle *vector = NULL;
	struct stat st;
	unsigned char *buffer = NULL;
	size_t size;
	int fd, ret = 1;
	int orientation;

	if (!file)
		return 0;

	fd = open(file->path, O_RDONLY | O_NOCTTY);
	if (fd < 0)
		goto fail_errno;
	if (fstat(fd, &st))
		goto fail_errno;
	if (!S_ISREG(st.st_mode)) {
		error_message = "Not a regular file";
		goto fail;
	}
	if (read_file(fd, &buffer, &size))
		goto fail_errno;
	file->size = st.st_size;

	loader = gdk_pixbuf_loader_new();
	if (!loader)
		goto fail_errno;
	if (!gdk_pixbuf_loader_write(loader, buffer, size, &error))
		goto fail_gerror;
	if (!gdk_pixbuf_loader_close(loader, &error))
		goto fail_gerror;
	raster = gdk_pixbuf_loader_get_pixbuf(loader);
	if (!raster) {
		error_message = "Unsupported image format";
		goto fail;
	}
	format = gdk_pixbuf_loader_get_format(loader);
	if (format) {
		char *name = gdk_pixbuf_format_get_name(format);
		if (!strcmp(name, "svg"))
			vector = rsvg_handle_new_from_file(file->path, &error);
		g_free(name);
		if (!vector)
			g_clear_error(&error);
	}
	g_object_ref(raster);
	g_clear_object(&file->image.raster);
	g_clear_object(&file->image.vector);
	file->image.raster = raster;
	file->image.vector = vector;
	vector = NULL;

	if (file->image.vector) {
		/* TODO get dimensions and dpi from file->image.vector */
	}
	file->image.width = gdk_pixbuf_get_width(file->image.raster);
	file->image.height = gdk_pixbuf_get_height(file->image.raster);
	file->image.has_x_dpi = get_raster_dpi_option(file->image.raster, "x-dpi", &file->image.x_dpi);
	file->image.has_y_dpi = get_raster_dpi_option(file->image.raster, "y-dpi", &file->image.y_dpi);

	orientation = parse_orientation(gdk_pixbuf_get_option(file->image.raster, "orientation"));
	file->image.embedded_transformation = orientation_to_transformation(orientation);

	if (file->exif_tags) {
		g_strfreev(file->exif_tags);
		file->exif_tags = NULL;
	}
	g_clear_object(&file->exif);

	if (size <= (size_t)LONG_MAX)
		file->exif = load_exif_metadata(buffer, size, &file->exif_tags);

out:
	free(buffer);
	g_clear_object(&vector);
	g_clear_object(&loader);
	if (fd >= 0)
		close(fd);
	return ret;

fail_gerror:
	error_message = error->message;
	goto fail;

fail_errno:
	error_message = strerror(errno);

fail:
	if (errors_to_stderr) {
		weprintf("Cannot open %s: %s", file->path, error_message);
	} else {
		dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		                                GTK_BUTTONS_CLOSE, "Cannot open %s", file->path);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error_message);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	g_clear_error(&error);
	ret = 0;
	goto out;
}

static void
preview_image(GtkFileChooser *chooser, GtkWidget *preview_widget)
{
	char *filename;
	GdkPixbuf *preview;
	GtkWidget *image, *label;

	filename = gtk_file_chooser_get_preview_filename(chooser);
	preview = !filename ? NULL :
		gdk_pixbuf_new_from_file_at_scale(filename, PREVIEW_WIDTH, PREVIEW_HEIGHT, 1, NULL);
	image = g_object_get_data(G_OBJECT(preview_widget), "image");
	label = g_object_get_data(G_OBJECT(preview_widget), "label");
	gtk_image_set_from_pixbuf(GTK_IMAGE(image), preview);
	if (preview) {
		gtk_widget_show(image);
		gtk_widget_hide(label);
	} else {
		gtk_widget_hide(image);
		gtk_widget_show(label);
	}
	gtk_file_chooser_set_preview_widget_active(chooser, 1);
	if (preview)
		g_object_unref(preview);
	g_free(filename);
}

static const char *
get_home_directory(void)
{
	static const char *home_directory = NULL;

	struct passwd *pwd;

	if (home_directory)
		return home_directory;

	home_directory = getenv("HOME");
	if (!home_directory || !*home_directory) {
		pwd = getpwuid(getuid());
		if (pwd && pwd->pw_dir && *pwd->pw_dir)
			home_directory = pwd->pw_dir;
		else
			home_directory = ".";
	}

	return home_directory;
}

GtkFileChooser *
construct_chooser(void)
{
	GtkWidget *widget, *image, *label, *preview;
	GtkFileChooser *chooser;

	widget = gtk_file_chooser_dialog_new("Open",
	                                     GTK_WINDOW(window),
	                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                     NULL);
	chooser = GTK_FILE_CHOOSER(widget);

	g_object_set(gtk_settings_get_default(), "gtk-recent-files-enabled", 0, NULL);
	gtk_file_chooser_set_select_multiple(chooser, 1);
	gtk_file_chooser_set_current_folder(chooser, get_home_directory());
	gtk_file_chooser_set_local_only(chooser, 1);
	gtk_file_chooser_set_do_overwrite_confirmation(chooser, 0);

	preview = gtk_vbox_new(0, 6);
	image = gtk_image_new();
	label = gtk_label_new("Preview");
	gtk_widget_set_size_request(preview, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	gtk_widget_set_size_request(image, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(preview), image, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(preview), label, 0, 0, 0u);
	g_object_set_data(G_OBJECT(preview), "image", image);
	g_object_set_data(G_OBJECT(preview), "label", label);
	gtk_file_chooser_set_preview_widget(chooser, preview);
	gtk_file_chooser_set_use_preview_label(chooser, 0);
	gtk_file_chooser_set_preview_widget_active(chooser, 1);
	gtk_widget_hide(image);
	g_signal_connect(chooser, "update-preview", G_CALLBACK(&preview_image), preview);

	return chooser;
}
