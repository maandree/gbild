/* See LICENSE file for copyright and license details. */
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libsimple.h>
#include <libsimple-arg.h>
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wunsuffixed-float-constants"
#endif

USAGE("[-f] [file] ...");

#define SCROLL_AMOUNT 30

#define FLOAT_EQ(A, B) ((A) >= (B) && (A) <= (B))

#define COMMA ,

#define LIST_MENU(TOPLEVEL, SEPARATOR, ACTION, TOGGLE, D)\
	TOPLEVEL(0, "", "File", D)\
	ACTION(0, 0u, "", "Open", GTK_STOCK_OPEN, "<control>0", "Ctrl+0", open_action, D)\
	ACTION(0, 1u, "", "Reload", GTK_STOCK_REFRESH, "F5", "F5", reload_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 1u, "", "Print", GTK_STOCK_PRINT, "<control>P", "Ctrl+P", print_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 1u, "Proper", "ties", GTK_STOCK_PROPERTIES, "<alt>Return", "Alt+Return", properties_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 0u, "", "Close", GTK_STOCK_CLOSE, "<control>W", "Ctrl+W", close_action, D)\
	TOPLEVEL(0, "", "View", D)\
	TOGGLE(1, 1u, "F", "ullscreen", GTK_STOCK_FULLSCREEN, "F11", "F11", fullscreen_action, GROUP_NONE, 0, D)\
	SEPARATOR(1, D)\
	ACTION(0, 1u, "Re", "set transformations", "document-revert", "<alt>R", "Alt+R", reset_transformations_action, D)\
	ACTION(1, 1u, "Rotate anti", "clockwise", "image-rotate-left", "<shift>R", "Shift+R", rotate_anticlockwise_action, D)\
	ACTION(0, 1u, "R", "otate 180°", "image-rotate", "<control>space", "Ctrl+Space", rotate_180_degrees_action, D)\
	ACTION(1, 1u, "", "Rotate clockwise", "image-rotate-right", "R", "R", rotate_clockwise_action, D)\
	ACTION(1, 1u, "", "Mirror horizontally", "object-flip-horizontal", "H", "H", flop_action, D)\
	ACTION(1, 1u, "Mirror ", "vertically", "object-flip-vertical", "V", "V", flip_action, D)\
	ACTION(1, 1u, "", "Transpose", "object-transpose", "T", "T", transpose_action, D)\
	ACTION(1, 1u, "", "Antitranspose", "object-transpose-mirrored", "<shift>T", "Shift+T", antitranspose_action, D)\
	SEPARATOR(1, D)\
	TOGGLE(1, 1u, "", "Invert colours", "filters-invert", "<control>I", "Ctrl+I", invert_action, GROUP_NONE, 0, D)\
	SEPARATOR(1, D)\
	/* TODO these are not properly honoured */\
	ACTION(1, 1u, "", "Zoom in", "zoom-in", "plus", "+", zoom_in_action, D)\
	ACTION(1, 1u, "Zoom ", "out", "zoom-out", "minus", "-", zoom_out_action, D)\
	TOGGLE(1, 1u, "", "Normal size", "zoom-original", "0", "0", normal_size_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "Fit to ", "width", "zoom-fit-width", "<control>H", "Ctrl+H", width_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "Fit to ", "height", "zoom-fit-height", "<control>V", "Ctrl+V", height_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "", "Fit to window", "zoom-fit-page", "<control><shift>F", "Ctrl+Shift+F", window_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "", "Best fit", "zoom-fit-best", "<control>F", "Ctrl+F", best_fit_action, GROUP_ZOOM, 1, D)\
	TOPLEVEL(1, "", "Go", D)\
	ACTION(1, 2u, "", "Previous image", GTK_STOCK_GO_BACK, "<control>Left", "Ctrl+Left", previous_action, D)\
	ACTION(1, 2u, "", "Next image", GTK_STOCK_GO_FORWARD, "<control>Right", "Ctrl+Right", next_action, D)\
	SEPARATOR(0, D)\
	ACTION(1, 2u, "", "First image", GTK_STOCK_GOTO_FIRST, "<control>Home", "Ctrl+Home", first_action, D)\
	ACTION(1, 2u, "", "Last image", GTK_STOCK_GOTO_LAST, "<control>End", "Ctrl+End", last_action,)

enum toggle_group {
	GROUP_NONE,
	GROUP_ZOOM
};

enum transformation {
	TRANS_NONE,          /* 1   */
	TRANS_CLOCKWISE,     /* r   */
	TRANS_180_DEGREES,   /* r²  */
	TRANS_ANTICLOCKWISE, /* r³  */
	TRANS_TRANSPOSE,     /* t   */
	TRANS_FLOP,          /* rt  */
	TRANS_ANTITRANSPOSE, /* r²t */
	TRANS_FLIP,          /* r³t */
	/*
	 * t² = r⁴ = 1
	 * trⁿ = r⁻ⁿt
	 */
	TRANS_NEGATIVE = 0x08
};

struct file {
	char *path;
	off_t size;
	GdkPixbuf *image;
};

struct filter {
	char *name;
	size_t suffixes_start;
	size_t suffixes_end;
};

static struct file *files = NULL;
static size_t nfiles = 0u;
static size_t current_file = 0u;
static enum transformation trans = TRANS_NONE;
static GdkPixbuf *transformed_image = NULL;

static double zoom = 1.0;
static int clearing_toggles = 0;
static int dragging = 0;
static int drag_x, drag_y;

static GtkWidget *window;
static GtkWidget *menubar;
static GtkWidget *statusbar;
static GtkWidget *toolbar;
static GtkWidget *display_area;
static GtkWidget *scroll;
static GtkToggleAction *fullscreen;
static GtkActionGroup *actions;

static const char *home_directory;
static char *suffixes = NULL;
static struct filter *filters = NULL;
static size_t nfilters = 0u;
static struct filter anyfilter = {NULL, 0u, 0u};
static char image_target_name[] = "text/uri-list";
static GtkTargetEntry image_target = {image_target_name, 0u, 0u};

static double
zoom_in(double z)
{
	return z * 1.25;
}

static double
zoom_out(double z)
{
	return z * 0.8;
}

static GtkAction *
get_action(const char *name)
{
	return gtk_action_group_get_action(actions, name);
}

static void
set_actions_sensitive(void)
{
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#define X_SKIP(...)
#define X_ACTION(T, N, A, B, ICON, ACCEL, ATEXT, F, ...)\
	if (N) gtk_action_set_sensitive(get_action(A B), nfiles >= N) __VA_ARGS__
#define X_TOGGLE(T, N, A, B, ICON, ACCEL, ATEXT, F, GROUP, ACTIVE, ...)\
	if (N) gtk_action_set_sensitive(get_action(A B), nfiles >= N) __VA_ARGS__

	LIST_MENU(X_SKIP, X_SKIP, X_ACTION, X_TOGGLE, ;);

#undef X_SKIP
#undef X_ACTION
#undef X_TOGGLE

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
}

static size_t
format_statusbar_message(char *buffer, size_t size, struct file *file)
{
	char fsize_decimals[3];
	const char units[] = "BKMGTPEZYRQ";
	uintmax_t fsize;
	size_t unit;
	int r;

	fsize = (uintmax_t)file->size;
	unit = 0u;
	if (fsize < 1024lu) {
		fsize_decimals[0] = '\0';
		goto done;
	}
	fsize *= 10u;
	while (fsize >= 10240lu && units[unit + 1u]) {
		fsize >>= 10;
		unit += 1u;
	}
	fsize_decimals[0] = '.';
	fsize_decimals[1] = (char)('0' + (char)(fsize % 10u));
	fsize_decimals[2] = '\0';
	fsize /= 10u;

done:
	r = snprintf(buffer, size,
	             "Image %zu of %zu | %ju%s %c | %i × %i pixels", /* TODO LATER add zoom */
	             current_file + 1u, nfiles,
	             fsize, fsize_decimals, units[unit],
	             gdk_pixbuf_get_width(file->image),
	             gdk_pixbuf_get_height(file->image));

	if (r < 0)
		eprintf("snprintf:");
	return (size_t)r;
}

static void
update_status(void)
{
	char *message;
	size_t len;

	gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0u);

	if (current_file >= nfiles || !files[current_file].image)
		return;

	len = format_statusbar_message(NULL, 0u, &files[current_file]);
	message = malloc(++len);
	format_statusbar_message(message, len, &files[current_file]);

	gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0u, message);

	free(message);
}

static void
show_image(void)
{
	GdkPixbuf *new_image, *current_image;
	int channels, height, rowstride, width, x, y;
	uint8_t *pixels;

	if (current_file >= nfiles || !files[current_file].image) {
		if (transformed_image)
			g_object_unref(transformed_image);
		gtk_image_clear(GTK_IMAGE(display_area));
		return;
	}

	current_image = files[current_file].image;

	switch (trans & 7u) {
	case TRANS_NONE:
		new_image = NULL;
		goto transformed;
	case TRANS_CLOCKWISE:
		new_image = gdk_pixbuf_rotate_simple(current_image, GDK_PIXBUF_ROTATE_CLOCKWISE);
		break;
	case TRANS_180_DEGREES:
		new_image = gdk_pixbuf_rotate_simple(current_image, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
		break;
	case TRANS_ANTICLOCKWISE:
		new_image = gdk_pixbuf_rotate_simple(current_image, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
		break;
	case TRANS_FLIP:
	case TRANS_TRANSPOSE:
		new_image = gdk_pixbuf_flip(current_image, 0);
		break;
	case TRANS_FLOP:
	case TRANS_ANTITRANSPOSE:
		new_image = gdk_pixbuf_flip(current_image, 1);
		break;
	}
	if (!new_image)
		exit(1);

	switch (trans & 7u) {
	case TRANS_TRANSPOSE:
	case TRANS_ANTITRANSPOSE:
		current_image = new_image;
		new_image = gdk_pixbuf_rotate_simple(current_image, GDK_PIXBUF_ROTATE_CLOCKWISE);
		if (!new_image)
			exit(1);
		g_object_unref(current_image);
		break;
	default:
		break;
	}

transformed:
	if (trans & TRANS_NEGATIVE) {
		if (!new_image) {
			new_image = gdk_pixbuf_copy(current_image);
			if (!new_image)
				exit(1);
		}

		pixels    = gdk_pixbuf_get_pixels(new_image);
		width     = gdk_pixbuf_get_width(new_image);
		height    = gdk_pixbuf_get_height(new_image);
		rowstride = gdk_pixbuf_get_rowstride(new_image);
		channels  = gdk_pixbuf_get_n_channels(new_image);

		for (y = 0; y < height; y++) {
			for (x = 0; x < width * channels; x += channels) {
				/* TODO LATER perceptual inversion rather than sRGB */
				pixels[x + 0] = 255 - pixels[x + 0];
				pixels[x + 1] = 255 - pixels[x + 1];
				pixels[x + 2] = 255 - pixels[x + 2];
			}
			pixels = &pixels[rowstride];
		}
	}

	current_image = new_image ? new_image : files[current_file].image;

	if (FLOAT_EQ(zoom, 1.0)) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(display_area), current_image);
	} else {
		/* TODO do not create an up scaled image, draw it dynamically upscaled (just the exposed region) */
		current_image = gdk_pixbuf_scale_simple(current_image,
		                                         (int)(zoom * gdk_pixbuf_get_width(current_image)),
		                                         (int)(zoom * gdk_pixbuf_get_height(current_image)),
		                                         GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(GTK_IMAGE(display_area), current_image);
		g_object_unref(current_image);
	}

	if (transformed_image)
		g_object_unref(transformed_image);
	transformed_image = new_image;
}

static void
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

static void
clear_images(void)
{
	gtk_image_clear(GTK_IMAGE(display_area));
	gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0u);
	g_clear_object(&transformed_image);
	free_files();
}

static int
load_image(struct file *file, int errors_to_stderr)
{
	const char *error_message;
	GError *error = NULL;
	GtkWidget *dialog;
	GdkPixbuf *image;
	GdkPixbufLoader *loader = NULL;
	struct stat st;
	unsigned char buffer[8 << 10];
	ssize_t r;
	int fd, ret = 1;

	fd = open(file->path, O_RDONLY | O_NOCTTY);
	if (fd < 0)
		goto fail_errno;

	if (fstat(fd, &st))
		goto fail_errno;

	if (!S_ISREG(st.st_mode)) {
		error_message = "Not a regular file";
		goto fail;
	}

	loader = gdk_pixbuf_loader_new();
	if (!loader)
		goto fail_errno;
	for (;;) {
		r = read(fd, buffer, sizeof(buffer));
		if (r <= 0) {
			if (!r)
				break;
			if (errno == EINTR)
				continue;
			goto fail_errno;
		}
		if (!gdk_pixbuf_loader_write(loader, buffer, (size_t)r, &error))
			goto fail_gerror;
	}
	if (!gdk_pixbuf_loader_close(loader, &error)) {
	fail_gerror:
		error_message = error->message;
		goto fail;
	}
	image = gdk_pixbuf_loader_get_pixbuf(loader);
	if (!image) {
		error_message = "Unsupported image format";
		goto fail;
	}
	g_object_ref(image);

	g_clear_object(&file->image);

	file->size = st.st_size;
	file->image = image;

out:
	g_clear_object(&loader);
	if (fd >= 0)
		close(fd);
	return ret;

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
set_images_from_array(struct file *selected_files, size_t nselected_files)
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

	if (!n) {
		free(selected_files);
		return;
	}

	clear_images();
	files = selected_files;
	nfiles = n;
	show_image();
	update_status();
	set_actions_sensitive();
}

static void
set_images_from_list(GSList *selected_files)
{
	struct file *new_files;
	GSList *file;
	size_t i;

	for (file = selected_files, i = 0u; file; file = file->next)
		i += 1u;
	if (!i)
		return;
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
		return;
	}

	clear_images();
	files = new_files;
	nfiles = i;
	show_image();
	update_status();
	set_actions_sensitive();
}

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

static void
add_image_filters(GtkFileChooser *chooser)
{
	GtkFileFilter *filter;
	size_t i;

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

static void
preview_image(GtkFileChooser *chooser, GtkWidget *preview_widget)
{
	char *filename;
	GdkPixbuf *preview;

	filename = gtk_file_chooser_get_preview_filename(chooser);
	preview = filename ? gdk_pixbuf_new_from_file_at_scale(filename, 192, 192, 1, NULL) : NULL;
	gtk_image_set_from_pixbuf(GTK_IMAGE(g_object_get_data(G_OBJECT(preview_widget), "image")), preview);
	if (preview)
		g_object_unref(preview);
	g_free(filename);
}

static void
open_action(GtkAction *action, void *user)
{
	/* TODO The preview widget should have a fixed size (although it may be subject
	 *      to manual resize by the user). When there is no preview the preview widge
	 *      shall just say "Preview" (centered on both dimensions) */

	/* TODO The preview widget shall not have a filename header */

	GtkWidget *chooser_widget, *image, *preview;
	GtkFileChooser *chooser;

	(void) action;
	(void) user;

	chooser_widget = gtk_file_chooser_dialog_new("Open",
	                                             GTK_WINDOW(window),
	                                             GTK_FILE_CHOOSER_ACTION_OPEN,
	                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                             NULL);
	chooser = GTK_FILE_CHOOSER(chooser_widget);

	g_object_set(gtk_settings_get_default(), "gtk-recent-files-enabled", 0, NULL);
	gtk_file_chooser_set_select_multiple(chooser, 1);
	gtk_file_chooser_set_current_folder(chooser, home_directory);
	gtk_file_chooser_set_local_only(chooser, 1);
	gtk_file_chooser_set_do_overwrite_confirmation(chooser, 0);

	preview = gtk_vbox_new(0, 6);
	image = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(preview), image, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(preview), gtk_label_new("Preview"), 0, 0, 0u);
	g_object_set_data(G_OBJECT(preview), "image", image);
	gtk_file_chooser_set_preview_widget(chooser, preview);
	gtk_file_chooser_set_preview_widget_active(chooser, 1);
	g_signal_connect(chooser, "update-preview", G_CALLBACK(preview_image), preview);

	add_image_filters(chooser);
	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
		set_images_from_list(gtk_file_chooser_get_filenames(chooser));
	gtk_widget_destroy(GTK_WIDGET(chooser));
}

static void
reload_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	if (current_file >= nfiles)
		return;
	if (load_image(&files[current_file], 0)) {
		show_image();
		update_status();
	}
}

static void
close_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	gtk_main_quit();
}

static void
begin_print(GtkPrintOperation *operation, GtkPrintContext *context, void *user)
{
	(void) context;
	(void) user;
	gtk_print_operation_set_n_pages(operation, 1);
}

static void
draw_page(GtkPrintOperation *operation, GtkPrintContext *context, int page, void *user)
{
	GdkPixbuf *image = user;
	double height, width, x, y;
	double scale_x, scale_y, scale;
	cairo_t *cr;

	(void) operation;
	(void) page;

	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);
	scale_x = width / gdk_pixbuf_get_width(image);
	scale_y = height / gdk_pixbuf_get_height(image);
	scale = MIN(scale_x, scale_y);
	x = 0.5 * (width - scale * gdk_pixbuf_get_width(image));
	y = 0.5 * (height - scale * gdk_pixbuf_get_height(image));

	cr = gtk_print_context_get_cairo_context(context);
	cairo_translate(cr, x, y);
	cairo_scale(cr, scale, scale);
	gdk_cairo_set_source_pixbuf(cr, image, 0.0f, 0.0f);
	cairo_paint(cr);
}

static void
print_action(GtkAction *action, void *user)
{
	/* TODO the print dialogue needs a positioning tab that lets the user scale and pan */

	GdkPixbuf *image;
	GtkPrintOperation *operation;
	GtkPageSetup *setup;
	GtkPageOrientation orientation = GTK_PAGE_ORIENTATION_PORTRAIT;

	(void) action;
	(void) user;

	if (current_file >= nfiles || !files[current_file].image)
		return;

	image = transformed_image ? transformed_image : files[current_file].image;
	g_object_ref(image);
	operation = gtk_print_operation_new();

	if (gdk_pixbuf_get_width(image) > gdk_pixbuf_get_height(image))
		orientation = GTK_PAGE_ORIENTATION_LANDSCAPE;
	setup = gtk_page_setup_new();
	gtk_page_setup_set_orientation(setup, orientation);
	gtk_print_operation_set_default_page_setup(operation, setup);
	g_object_unref(setup);

	gtk_print_operation_set_embed_page_setup(operation, 1);
	g_signal_connect(operation, "begin-print", G_CALLBACK(begin_print), image);
	g_signal_connect(operation, "draw-page", G_CALLBACK(draw_page), image);
	gtk_print_operation_run(operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(window), NULL);
	g_object_unref(operation);
	g_object_unref(image);
}

static void
properties_action(GtkAction *action, void *user)
{
	/* TODO LATER What shall "Properties" display? */

	GtkWidget *dialog;
	struct file *file;
	const char *name;

	(void) action;
	(void) user;

	if (current_file >= nfiles || !files[current_file].image)
		return;

	file = &files[current_file];
	name = strrchr(file->path, '/');
	name = name ? &name[1u] : file->path;

	dialog = gtk_message_dialog_new(GTK_WINDOW(window), 0, GTK_MESSAGE_INFO,
	                                GTK_BUTTONS_CLOSE, "%s", name);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
	                                         "%i × %i pixels",
	                                         gdk_pixbuf_get_width(file->image),
	                                         gdk_pixbuf_get_height(file->image));
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

static void
scroll_by(int horizontally, double vertically)
{
	GtkAdjustment *adjustment;
	double min, val, max;

	adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scroll));
	min = adjustment->lower;
	val = adjustment->value + (double)horizontally;
	max = adjustment->upper - adjustment->page_size;
	val = MIN(MAX(min, val), max);
	gtk_adjustment_set_value(adjustment, val);

	adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
	min = adjustment->lower;
	val = adjustment->value + (double)vertically;
	max = adjustment->upper - adjustment->page_size;
	val = MIN(MAX(min, val), max);
	gtk_adjustment_set_value(adjustment, val);
}

static void
set_zoom(double new_zoom, double x, double y)
{
	GtkAdjustment *hadjustment, *vadjustment;
	double old_zoom;

	if (current_file >= nfiles || !files[current_file].image)
		return;
	if (FLOAT_EQ(new_zoom, zoom))
		return;

	hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scroll));
	vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
	old_zoom = zoom;
	zoom = new_zoom;
	show_image();
	gtk_adjustment_set_value(hadjustment, (hadjustment->value + x) * zoom / old_zoom - x);
	gtk_adjustment_set_value(vadjustment, (vadjustment->value + y) * zoom / old_zoom - y);
	update_status();
}

static void
clear_toggles(enum toggle_group group, GtkToggleAction *except)
{
	GtkToggleAction *action;

	clearing_toggles = 1;

#define X_SKIP(...)
#define X_TOGGLE(T, N, A, B, ICON, ACCEL, ATEXT, F, GROUP, ACTIVE, ...)\
	do {\
		if (GROUP == group) {\
			action = GTK_TOGGLE_ACTION(get_action(A B));\
			if (action != except)\
				gtk_toggle_action_set_active(action, 0);\
		}\
	} while (0) __VA_ARGS__

	LIST_MENU(X_SKIP, X_SKIP, X_SKIP, X_TOGGLE, ;);

#undef X_SKIP
#undef X_TOGGLE

	clearing_toggles = 0;
}

static int
scroll_event(GtkWidget *widget, GdkEventScroll *event, void *user)
{
	int shift = !!(event->state & GDK_SHIFT_MASK);
	int ctrl  = !!(event->state & GDK_CONTROL_MASK);
	int up    = event->direction == GDK_SCROLL_UP;
	int down  = event->direction == GDK_SCROLL_DOWN;
	int left  = event->direction == GDK_SCROLL_LEFT;
	int right = event->direction == GDK_SCROLL_RIGHT;

	(void) widget;
	(void) user;

	if (shift) {
		left = up, up = 0;
		right = down, down = 0;
	}

	if (!ctrl) {
		scroll_by(left ? -SCROLL_AMOUNT : right ? +SCROLL_AMOUNT : 0,
		          up   ? -SCROLL_AMOUNT : down  ? +SCROLL_AMOUNT : 0);
	} else if (up | down) {
		clear_toggles(GROUP_ZOOM, NULL);
		set_zoom(up ? zoom_in(zoom) : zoom_out(zoom), event->x, event->y);
	} else {
		return 0;
	}

	return 1;
}

static int
button_pressed(GtkWidget *widget, GdkEventButton *event, void *user)
{
	(void) widget;
	(void) user;

	if (event->button != 2u && !(event->state & GDK_CONTROL_MASK))
		return 0;

	dragging = 1;
	drag_x = (int)event->x;
	drag_y = (int)event->y;
	return 1;
}

static int
button_released(GtkWidget *widget, GdkEventButton *event, void *user)
{
	(void) widget;
	(void) event;
	(void) user;
	dragging = 0;
	return 0;
}

static int
pointer_moved(GtkWidget *widget, GdkEventMotion *event, void *user)
{
	double x, y;

	(void) widget;
	(void) user;

	if (!dragging)
		return 0;

	gdk_event_get_coords((GdkEvent *)event, &x, &y);
	scroll_by(drag_x - (int)x, drag_y - (int)y);
	drag_x = (int)x;
	drag_y = (int)y;

	return 1;
}

static void
zoom_in_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	clear_toggles(GROUP_ZOOM, NULL);
	set_zoom(zoom_in(zoom), 0.0, 0.0);
	/* TODO keep center point */
}

static void
zoom_out_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	clear_toggles(GROUP_ZOOM, NULL);
	set_zoom(zoom_out(zoom), 0.0, 0.0);
	/* TODO keep center point */
}

static void
set_zoom_fit(GtkToggleAction *action, int xfit, int yfit, int cap)
{
	GtkAdjustment *adjustment;
	GdkPixbuf *image;
	double s = 1.0, y;
	int w, h;

	if (!gtk_toggle_action_get_active(action) || current_file >= nfiles || !files[current_file].image) {
		if (!clearing_toggles)
			clear_toggles(GROUP_ZOOM, action);
		return;
	}

	image = transformed_image ? transformed_image : files[current_file].image;

	if (xfit) {
		adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scroll));
		w = gdk_pixbuf_get_width(image);
		s = adjustment->page_size / (double)w;
	}

	if (yfit) {
		adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
		h = gdk_pixbuf_get_height(image);
		y = adjustment->page_size / (double)h;
		s = xfit ? MIN(s, y) : y;
	}

	if (cap && s > 1.0)
		s = 1.0;

	clear_toggles(GROUP_ZOOM, action);
	set_zoom(s, 0.0, 0.0);
	/* TODO center image */
}

static void
normal_size_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, 0, 0, 0);
}

static void
width_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, 1, 0, 0);
}

static void
height_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, 0, 1, 0);
}

static void
window_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, 1, 1, 0);
}

static void
best_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, 1, 1, 1);
}

static int
key_pressed(GtkWidget *widget, GdkEventKey *event, void *user)
{
	int left, right, up, down, amount;

	(void) widget;
	(void) user;

	if (event->keyval == GDK_Escape && gtk_toggle_action_get_active(fullscreen)) {
		gtk_toggle_action_set_active(fullscreen, 0);
		return 1;
	}

	if (event->state & GDK_CONTROL_MASK)
		return 0;

	amount = (event->state & GDK_MOD1_MASK) ? 1 : SCROLL_AMOUNT;

	left  = event->keyval == GDK_Left;
	right = event->keyval == GDK_Right;
	up    = event->keyval == GDK_Up;
	down  = event->keyval == GDK_Down;

	if (left | right | up | down) {
		scroll_by(left ? -amount : right ? +amount : 0,
		          up   ? -amount : down  ? +amount : 0);
		return 1;
	}

	return 0;
}

static int
window_state_changed(GtkWidget *widget, GdkEventWindowState *event, void *user)
{
	(void) widget;
	(void) user;
	gtk_toggle_action_set_active(fullscreen, !!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN));
	return 0;
}

static void
fullscreen_action(GtkToggleAction *action, void *user)
{
	int active = gtk_toggle_action_get_active(action);

	(void) user;

	if (active && current_file >= nfiles) {
		gtk_toggle_action_set_active(action, 0);
		return;
	}

	gtk_widget_set_visible(menubar, !active);
	gtk_widget_set_visible(toolbar, !active);
	gtk_widget_set_visible(statusbar, !active);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               active ? GTK_POLICY_NEVER : GTK_POLICY_ALWAYS,
	                               active ? GTK_POLICY_NEVER : GTK_POLICY_ALWAYS);

	if (active)
		gtk_window_fullscreen(GTK_WINDOW(window));
	else
		gtk_window_unfullscreen(GTK_WINDOW(window));
}

static void
reset_transformations_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	trans &= ~7u;
	show_image();
	/* TODO keep center point */
}

static void
transform(enum transformation augment)
{
	if (augment & TRANS_TRANSPOSE)
		trans = ((augment - trans) & 3u) | ((augment ^ trans) & ~3u);
	else
		trans = ((augment + trans) & 3u) | ((augment ^ trans) & ~3u);
	show_image();
	/* TODO keep center point */
}

static void
rotate_clockwise_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_CLOCKWISE);
}

static void
rotate_180_degrees_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_180_DEGREES);
}

static void
rotate_anticlockwise_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_ANTICLOCKWISE);
}

static void
flop_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_FLOP);
}

static void
flip_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_FLIP);
}

static void
transpose_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_TRANSPOSE);
}

static void
antitranspose_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_ANTITRANSPOSE);
}

static void
invert_action(GtkToggleAction *action, void *user)
{

	enum transformation tr;
	(void) user;
	tr = gtk_toggle_action_get_active(action) ? TRANS_NEGATIVE : TRANS_NONE;
	transform(tr ^ (trans & TRANS_NEGATIVE));
}

static void
navigate(size_t jump, size_t move)
{
	if (!nfiles)
		return;
	current_file = jump;
	current_file += move;
	current_file %= nfiles;
	show_image();
	update_status();
}

static void
next_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(current_file, +(size_t)1);
}

static void
previous_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(current_file ? current_file : nfiles, -(size_t)1);
}

static void
first_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(0u, 0u);
}

static void
last_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(nfiles, -(size_t)1);
}

static void
drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data,
              unsigned int info, unsigned int time, void *user)
{
	GError *error = NULL;
	char *uri;

	(void) widget;
	(void) context;
	(void) info;
	(void) time;
	(void) user;

	if (current_file >= nfiles || !files[current_file].path)
		return;

	uri = g_filename_to_uri(files[current_file].path, NULL, &error);
	if (!uri) {
		weprintf("g_filename_to_uri: %s", error->message);
		g_error_free(error);
		return;
	}
	gtk_selection_data_set_uris(data, (char *[]){uri, NULL});
	g_free(uri);
}

static void
drop_files(GtkWidget *widget, GdkDragContext *context, int x, int y,
           GtkSelectionData *data, unsigned int info, unsigned int time, void *user)
{
	GError *error = NULL;
	char **uris, **uri, *path;
	struct file *dropped_files;
	size_t i, j;
	GdkModifierType modifiers;
	GdkWindow *event_window;

	(void) widget;
	(void) x;
	(void) y;
	(void) info;
	(void) user;

	uris = gtk_selection_data_get_uris(data);
	if (!uris) {
		gtk_drag_finish(context, 0, 0, time);
		return;
	}

	for (uri = uris, i = 0u; *uri; uri++)
		i += 1u;
	if (!i)
		goto out;
	dropped_files = ecalloc(i, sizeof(*dropped_files));

	for (uri = uris, i = 0u; *uri; uri++) {
		error = NULL; /* g_filename_from_uri requires this initialisation */
		path = g_filename_from_uri(*uri, NULL, &error);
		if (!path) {
			weprintf("g_filename_from_uri: %s", error->message);
			g_error_free(error);
			continue;
		}
		dropped_files[i++].path = estrdup(path);
		g_free(path);
		/* the other fields are set by set_images_from_array */
	}

	event_window = gtk_widget_get_window(widget);
	if (!event_window)
		goto out;
	if (!gdk_window_get_pointer(event_window, NULL, NULL, &modifiers))
		goto out;
	if ((modifiers & GDK_CONTROL_MASK) && nfiles) {
		files = ereallocarray(files, nfiles + i, sizeof(*files));
		for (j = 0u; j < i; j++) {
			if (load_image(&dropped_files[j], 0))
				files[nfiles++] = dropped_files[j];
			else
				free(dropped_files[j].path);
		}
		free(dropped_files);
		update_status();
	} else {
		set_images_from_array(dropped_files, i);
	}

out:
	gtk_drag_finish(context, 1, 0, time);
	g_strfreev(uris);
}

static GtkWidget *
append_toplevel_menu(int add_separator_to_toolbar, const char *name)
{
	GtkWidget *item, *menu;

	if (add_separator_to_toolbar)
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

	item = gtk_action_create_menu_item(get_action(name));
	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
	return gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
}

static void
append_separator(int add_to_toolbar, GtkWidget *menu)
{
	if (add_to_toolbar)
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

static void
append_action(int add_to_toolbar, GtkWidget *menu, const char *name, const char *tooltip)
{
	GtkWidget *item;
	if (add_to_toolbar) {
		item = gtk_action_create_tool_item(get_action(name));
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);
		if (tooltip)
			gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(item), tooltip);
	}
	if (menu) {
		item = gtk_action_create_menu_item(get_action(name));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
}

static void
create_menu_and_toolbar(void)
{
	GtkWidget *menu;

	menubar = gtk_menu_bar_new();
	toolbar = gtk_toolbar_new();

#define X_TOPLEVEL(T, A, B, D)                                         menu = append_toplevel_menu(T, A B) D
#define X_SEPARTOR(T, D)                                               append_separator(T, menu) D
#define X_ACTION(T, N, A, B, ICON, ACCEL, ACCEL_TEXT, F, D)            append_action(T, menu, A B, A B " (" ACCEL_TEXT ")") D
#define X_TOGGLE(T, N, A, B, ICON, ACCEL, ACCEL_TEXT, F, G, ACTIVE, D) append_action(T, menu, A B, A B " (" ACCEL_TEXT ")") D

	LIST_MENU(X_TOPLEVEL, X_SEPARTOR, X_ACTION, X_TOGGLE, ;);

#undef X_TOPLEVEL
#undef X_SEPARTOR
#undef X_ACTION
#undef X_TOGGLE
}

static void
connect_accelerators(GtkAccelGroup *accelerators)
{
	GList *item, *list;

	list = gtk_action_group_list_actions(actions);
	for (item = list; item; item = item->next) {
		gtk_action_set_accel_group(item->data, accelerators);
		gtk_action_connect_accelerator(item->data);
	}
	g_list_free(list);
}

static int
activate_fullscreen(void *user)
{
	(void) user;
	gtk_toggle_action_set_active(fullscreen, 1);
	return 0;
}

#define X_SKIP(...)
#define X_TOPLEVEL(T, A, B, ...)                                    {A B, NULL, A "_" B, NULL, NULL, NULL} __VA_ARGS__
#define X_ACTION(T, N, A, B, ICON, ACCEL, ATEXT, F, ...)            {A B, ICON, A "_" B, ACCEL, NULL, G_CALLBACK(F)} __VA_ARGS__
#define X_TOGGLE(T, N, A, B, ICON, ACCEL, ATEXT, F, G, ACTIVE, ...) {A B, ICON, A "_" B, ACCEL, NULL, G_CALLBACK(F), ACTIVE} __VA_ARGS__

static GtkActionEntry entries[] = {
	LIST_MENU(X_TOPLEVEL, X_SKIP, X_ACTION, X_SKIP, COMMA)
};

static GtkToggleActionEntry toggle_entries[] = {
	LIST_MENU(X_SKIP, X_SKIP, X_SKIP, X_TOGGLE, COMMA)
};

#undef X_SKIP
#undef X_TOPLEVEL
#undef X_ACTION
#undef X_TOGGLE

int
main(int argc, char *argv[])
{
	struct passwd *pwd;
	GtkAccelGroup *accelerators;
	GtkWidget *box;
	size_t i;
	int failed = 0;
	int start_fullscreen = 0;

	ARGBEGIN {
	case 'f':
		start_fullscreen = 1;
		break;
	default:
		usage();
	} ARGEND;

	nfiles = (size_t)argc;
	if (nfiles) {
		files = ecalloc(nfiles, sizeof(*files));
		for (i = 0u; i < nfiles; i++) {
			files[i].path = estrdup(argv[i]);
			files[i].image = NULL;
			if (!load_image(&files[i], 1)) {
				free(files[i].path);
				failed = 1;
			}
		}
	}
	if (failed)
		goto fail;

	home_directory = getenv("HOME");
	if (!home_directory || !*home_directory) {
		pwd = getpwuid(getuid());
		if (pwd && pwd->pw_dir && *pwd->pw_dir)
			home_directory = pwd->pw_dir;
		else
			home_directory = ".";
	}

	gtk_init(NULL, NULL);

	populate_filters();

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "gbild");
	gtk_window_set_icon_name(GTK_WINDOW(window), "image-viewer");
	gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
	gtk_window_set_wmclass(GTK_WINDOW(window), "gbild", "gbild");

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_pressed), NULL);
	g_signal_connect(window, "window-state-event", G_CALLBACK(window_state_changed), NULL);

	actions = gtk_action_group_new("gbild");
	gtk_action_group_add_actions(actions, entries, ELEMSOF(entries), NULL);
	gtk_action_group_add_toggle_actions(actions, toggle_entries, ELEMSOF(toggle_entries), NULL);
	fullscreen = GTK_TOGGLE_ACTION(gtk_action_group_get_action(actions, "Fullscreen"));
	accelerators = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accelerators);
	connect_accelerators(accelerators);
	create_menu_and_toolbar();

	box = gtk_vbox_new(0, 0);
	gtk_container_add(GTK_CONTAINER(window), box);
	gtk_box_pack_start(GTK_BOX(box), menubar, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), toolbar, 0, 0, 0u);
	display_area = gtk_image_new();
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), display_area);
	gtk_widget_add_events(scroll, GDK_SCROLL_MASK);
	g_signal_connect(scroll, "scroll-event", G_CALLBACK(scroll_event), NULL);
	gtk_box_pack_start(GTK_BOX(box), scroll, 1, 1, 0u);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(box), statusbar, 0, 0, 0u);

	gtk_widget_add_events(display_area, GDK_BUTTON_PRESS_MASK |
	                                    GDK_BUTTON_RELEASE_MASK |
	                                    GDK_POINTER_MOTION_MASK);
	g_signal_connect(display_area, "button-press-event", G_CALLBACK(button_pressed), NULL);
	g_signal_connect(display_area, "button-release-event", G_CALLBACK(button_released), NULL);
	g_signal_connect(display_area, "motion-notify-event", G_CALLBACK(pointer_moved), NULL);

	gtk_drag_source_set(display_area, GDK_BUTTON1_MASK, &image_target, 1u, GDK_ACTION_COPY);
	g_signal_connect(display_area, "drag-data-get", G_CALLBACK(drag_data_get), NULL);

	gtk_drag_dest_set(window, GTK_DEST_DEFAULT_ALL, &image_target, 1u, GDK_ACTION_COPY);
	g_signal_connect(window, "drag-data-received", G_CALLBACK(drop_files), NULL);

	show_image();
	update_status();
	set_actions_sensitive();
	gtk_widget_show_all(window);
	if (start_fullscreen && nfiles)
		g_idle_add(activate_fullscreen, NULL);

	gtk_main();

	g_object_unref(actions);
	if (transformed_image)
		g_object_unref(transformed_image);
fail:
	free_files();
	free(suffixes);
	while (nfilters)
		free(filters[--nfilters].name);
	free(filters);
	return failed;
}
