/* See LICENSE file for copyright and license details. */
#include "common.h"

USAGE("[-f] [file] ...");

static int suspend_expose = 0;

static cairo_pattern_t *chequer_pattern;
static GdkColor cached_chequer_colours[2];
static uint16_t cached_chequer_alphas[2];
static int cached_chequer_size;

static size_t
format_statusbar_message(char *buffer, size_t size, struct file *file, struct image *image)
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
	             "Image %zu of %zu | %ju%s %c | %i × %i pixels | %g%% scale",
	             current_file + 1u, nfiles,
	             fsize, fsize_decimals, units[unit],
	             image->width, image->height,
	             100.0 * get_zoom(zoom));

	if (r < 0)
		eprintf("snprintf:");
	return (size_t)r;
}

static void
update_status(void)
{
	struct file *file = get_file();
	char *message = NULL;
	size_t len;
	struct image image;
	if (file && get_original_image(&image)) {
		len = format_statusbar_message(NULL, 0u, file, &image);
		message = malloc(++len);
		format_statusbar_message(message, len, file, &image);
	}
	set_statusbar_message(message);
	free(message);
}

static cairo_pattern_t *
get_chequer_pattern(void)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	int i, row, column;
	double r[2], g[2], b[2], a[2], s;

	if (!chequer_pattern || cached_chequer_size != chequer_size)
		goto changed;
	for (i = 0; i < 2; i++) {
		if (cached_chequer_colours[i].red   != chequer_colours[i].red   ||
		    cached_chequer_colours[i].green != chequer_colours[i].green ||
		    cached_chequer_colours[i].blue  != chequer_colours[i].blue  ||
		    cached_chequer_alphas[i]        != chequer_alphas[i])
			goto changed;
	}
	return chequer_pattern;

changed:
	for (i = 0; i < 2; i++) {
		cached_chequer_colours[i] = chequer_colours[i];
		cached_chequer_alphas[i] = chequer_alphas[i];
	}
	cached_chequer_size = chequer_size;

	for (i = 0; i < 2; i++) {
		r[i] = (1.0 / 65535.0) * chequer_colours[i].red;
		g[i] = (1.0 / 65535.0) * chequer_colours[i].green;
		b[i] = (1.0 / 65535.0) * chequer_colours[i].blue;
		a[i] = (1.0 / 65535.0) * chequer_alphas[i];
	}

	s = (double)chequer_size;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2 * chequer_size, 2 * chequer_size);
	cr = cairo_create(surface);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	for (row = 0; row < 2; row++) {
		for (column = 0; column < 2; column++) {
			i = (row ^ column) & 1;
			cairo_set_source_rgba(cr, r[i], g[i], b[i], a[i]);
			cairo_rectangle(cr, (double)column * s, (double)row * s, s, s);
			cairo_fill(cr);
		}
	}
	cairo_destroy(cr);

	if (chequer_pattern)
		cairo_pattern_destroy(chequer_pattern);
	chequer_pattern = cairo_pattern_create_for_surface(surface);
	cairo_pattern_set_extend(chequer_pattern, CAIRO_EXTEND_REPEAT);
	cairo_pattern_set_filter(chequer_pattern, CAIRO_FILTER_NEAREST);

	cairo_surface_destroy(surface);
	return chequer_pattern;
}

static void
draw_transparency_background(cairo_t *cr, struct image *image, double x0, double y0, double width, double height)
{
	cairo_matrix_t matrix;

	if (!chequered_background || gtk_toggle_action_get_active(fullscreen))
		return;
	if (!image->vector && !gdk_pixbuf_get_has_alpha(image->raster))
		return;

	cairo_matrix_init_identity(&matrix);
	cairo_pattern_set_matrix(get_chequer_pattern(), &matrix);

	cairo_save(cr);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_rectangle(cr, x0, y0, width, height);
	cairo_clip(cr);
	cairo_set_source(cr, chequer_pattern);
	cairo_paint(cr);
	cairo_restore(cr);
}

int
expose_image(GtkWidget *widget, GdkEventExpose *event, void *user)
{
	struct image image;
	cairo_t *cr;
	double height, width, x, y, s;
	RsvgRectangle viewport;

	(void) user;

	if (suspend_expose)
		return 1;

	cr = gdk_cairo_create(widget->window);
	gdk_cairo_set_source_color(cr, &widget->style->bg[GTK_STATE_NORMAL]);

	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_rectangle(cr, event->area.x, event->area.y, event->area.width, event->area.height);

	if (!get_image(&image)) {
		cairo_fill(cr);
		goto out;
	}

	s = get_zoom(zoom);
	width = s * (double)image.width;
	height = s * (double)image.height;
	if (width < widget->allocation.width)
		x = 0.5 * ((double)widget->allocation.width - width);
	else
		x = -logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll))->value;
	if (height < widget->allocation.height)
		y = 0.5 * ((double)widget->allocation.height - height);
	else
		y = -logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll))->value;

	cairo_fill(cr);
	draw_transparency_background(cr, &image, x, y, width, height);

	cairo_translate(cr, x, y);
	cairo_scale(cr, s, s);
	if (image.vector) {
		viewport = (RsvgRectangle){0.0, 0.0, image.width, image.height};
		cairo_save(cr);
		if (image.has_transformer)
			cairo_transform(cr, &image.transformer);
		if (rsvg_handle_render_document(image.vector, cr, &viewport, NULL))
			goto out;
		cairo_restore(cr);
	}
	gdk_cairo_set_source_pixbuf(cr, image.raster, 0.0, 0.0);
	cairo_pattern_set_filter(cairo_get_source(cr), scaling_filter);
	cairo_paint(cr);

out:
	cairo_destroy(cr);
	return 1;
}

static void
show_image(int update_zoom)
{
	struct image original_image, new_image, *display_image;
	int transformed;
	double s;

	if (!get_original_image(&original_image)) {
		logical_scroll_pane_set_content_size(LOGICAL_SCROLL_PANE(scroll), 1, 1);
		gtk_widget_queue_draw(display_area);
		return;
	}

	transformed = apply_transformations(&original_image, &new_image);
	display_image = transformed ? &new_image : &original_image;

	if (update_zoom && zoom_mode != ZOOM_CUSTOM && zoom_mode != ZOOM_NORMAL)
		zoom = calculate_zoom_mode(display_image, zoom_mode);

	s = get_zoom(zoom);
	logical_scroll_pane_set_content_size(LOGICAL_SCROLL_PANE(scroll),
	                                     (int)(s * display_image->width),
	                                     (int)(s * display_image->height));

	forget_transformed_raster();
	if (transformed)
		transformed_raster = new_image.raster;
}

static void
set_images(struct file *new_files, size_t nnew_files)
{
	forget_transformed_raster();
	free_files();
	files = new_files;
	nfiles = nnew_files;
	collection_files_changed();
	navigate_redraw();
	set_actions_sensitive();
}

void
add_images(struct file *new_files, size_t nnew_files)
{
	files = ereallocarray(files, nfiles + nnew_files, sizeof(*files));
	memcpy(&files[nfiles], new_files, nnew_files * sizeof(*files));
	nfiles += nnew_files;
	if (nfiles == nnew_files)
		navigate_redraw();
	else
		update_status();
	collection_files_changed();
	set_actions_sensitive();
}

void
set_images_from_array(struct file *selected_files, size_t nselected_files)
{
	size_t n = files_from_array(selected_files, nselected_files);
	if (n)
		set_images(selected_files, n);
	else
		free(selected_files);
}

void
set_images_from_list(GSList *selected_files)
{
	struct file *new_files;
	size_t n = files_from_list(&new_files, selected_files);
	if (n)
		set_images(new_files, n);
}

void
scroll_by(double horizontally, double vertically)
{
	GtkAdjustment *adjustment;
	double min, val, max;

	adjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	min = adjustment->lower;
	val = adjustment->value + horizontally;
	max = adjustment->upper - adjustment->page_size;
	val = MIN(MAX(min, val), max);
	gtk_adjustment_set_value(adjustment, val);

	adjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	min = adjustment->lower;
	val = adjustment->value + vertically;
	max = adjustment->upper - adjustment->page_size;
	val = MIN(MAX(min, val), max);
	gtk_adjustment_set_value(adjustment, val);
}

void
set_zoom_around_point(Zoom new_zoom, double x, double y)
{
	GtkAdjustment *hadjustment, *vadjustment;
	double old_s, new_s, old_x0, old_y0, new_x0, new_y0;
	double iw, ih, vw, vh, hx, hy, adj_x, adj_y, max;
	struct image image;

	if (zoom_eq(new_zoom, zoom))
		return;
	if (!get_image(&image))
		goto out;

	old_s = get_zoom(zoom);
	new_s = get_zoom(new_zoom);
	zoom = new_zoom;

	hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	vw = hadjustment->page_size;
	vh = vadjustment->page_size;

	iw = old_s * (double)image.width;
	ih = old_s * (double)image.height;
	old_x0 = vw > iw ? 0.5 * (vw - iw) : 0.0;
	old_y0 = vh > ih ? 0.5 * (vh - ih) : 0.0;

	iw = new_s * (double)image.width;
	ih = new_s * (double)image.height;
	new_x0 = vw > iw ? 0.5 * (vw - iw) : 0.0;
	new_y0 = vh > ih ? 0.5 * (vh - ih) : 0.0;

	hx = (x - old_x0 + hadjustment->value) / old_s;
	hy = (y - old_y0 + vadjustment->value) / old_s;
	adj_x = new_x0 + hx * new_s - x;
	adj_y = new_y0 + hy * new_s - y;

	suspend_expose++;

	show_image(0);

	max = hadjustment->upper - hadjustment->page_size;
	x = MAX(hadjustment->lower, adj_x);
	x = MIN(x, max);
	gtk_adjustment_set_value(hadjustment, x);

	max = vadjustment->upper - vadjustment->page_size;
	y = MAX(vadjustment->lower, adj_y);
	y = MIN(y, max);
	gtk_adjustment_set_value(vadjustment, y);

	suspend_expose--;
	if (!suspend_expose)
		gtk_widget_queue_draw(display_area);
out:
	update_status();
}

void
set_zoom_around_centre(Zoom new_zoom)
{
	GtkAdjustment *hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	GtkAdjustment *vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));

	set_zoom_around_point(new_zoom, 0.5 * hadjustment->page_size, 0.5 * vadjustment->page_size);
}

static void
centre_image(void)
{
	GtkAdjustment *hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	GtkAdjustment *vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));

	gtk_adjustment_set_value(hadjustment, 0.5 * (hadjustment->upper - hadjustment->page_size));
	gtk_adjustment_set_value(vadjustment, 0.5 * (vadjustment->upper - vadjustment->page_size));
}

void
set_zoom_and_recentre(Zoom new_zoom)
{
	if (zoom_eq(new_zoom, zoom))
		return;

	suspend_expose++;

	zoom = new_zoom;
	show_image(0);
	centre_image();

	suspend_expose--;
	if (!suspend_expose)
		gtk_widget_queue_draw(display_area);
	update_status();
}

void
reload_redraw(void)
{
	GtkAdjustment *hadjustment, *vadjustment;
	double x, y, max;

	hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));

	x = hadjustment->value + 0.5 * hadjustment->page_size;
	x = hadjustment->upper > hadjustment->page_size ? x / hadjustment->upper : 0.5;
	y = vadjustment->value + 0.5 * vadjustment->page_size;
	y = vadjustment->upper > vadjustment->page_size ? y / vadjustment->upper : 0.5;

	suspend_expose++;

	show_image(0);

	max = hadjustment->upper - hadjustment->page_size;
	x = x * hadjustment->upper - 0.5 * hadjustment->page_size;
	x = MIN(MAX(hadjustment->lower, x), max);
	gtk_adjustment_set_value(hadjustment, x);

	max = vadjustment->upper - vadjustment->page_size;
	y = y * vadjustment->upper - 0.5 * vadjustment->page_size;
	y = MIN(MAX(vadjustment->lower, y), max);
	gtk_adjustment_set_value(vadjustment, y);

	suspend_expose--;
	if (!suspend_expose)
		gtk_widget_queue_draw(display_area);
	update_property_pane();
	collection_files_changed();
	update_status();
}

void
transform_redraw(enum transformation augment)
{
	struct image image;
	GtkAdjustment *hadjustment, *vadjustment;
	double s, x, y, width, height, max;

	hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	if (!get_image(&image))
		return;

	s = get_zoom(zoom);
	width  = (double)image.width;
	height = (double)image.height;

	x = hadjustment->value + 0.5 * hadjustment->page_size;
	y = vadjustment->value + 0.5 * vadjustment->page_size;
	x = s * width  > hadjustment->page_size ? x / s : 0.5 * width;
	y = s * height > vadjustment->page_size ? y / s : 0.5 * height;
	transform_point(augment, &x, &y, &width, &height);

	suspend_expose++;

	show_image(0);

	max = hadjustment->upper - hadjustment->page_size;
	x = s * x - 0.5 * hadjustment->page_size;
	x = MIN(MAX(hadjustment->lower, x), max);
	gtk_adjustment_set_value(hadjustment, x);

	max = vadjustment->upper - vadjustment->page_size;
	y = s * y - 0.5 * vadjustment->page_size;
	y = MIN(MAX(vadjustment->lower, y), max);
	gtk_adjustment_set_value(vadjustment, y);

	suspend_expose--;
	if (!suspend_expose)
		gtk_widget_queue_draw(display_area);
}

void
navigate_redraw(void)
{
	suspend_expose++;

	show_image(1);
	centre_image();

	suspend_expose--;
	if (!suspend_expose)
		gtk_widget_queue_draw(display_area);
	update_property_pane();
	update_collection();
	update_status();
}

int
recalculate_zoom_mode(void *user)
{
	struct image image;
	(void) user;
	if (get_image(&image) && zoom_mode != ZOOM_CUSTOM && zoom_mode != ZOOM_NORMAL)
		set_zoom_and_recentre(calculate_zoom_mode(&image, zoom_mode));
	return 0;
}

static int
initial_draw(void *user)
{
	(void) user;
	navigate_redraw();
	return 0;
}

int
main(int argc, char *argv[])
{
	int start_fullscreen = 0;
	int failed = 0;

	ARGBEGIN {
	case 'f':
		start_fullscreen = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (!set_files_from_argv(argc, argv))
		goto fail;

	gtk_init(NULL, NULL);

	set_up_ui();
	set_up_dragging();
	set_up_dropping();

	set_actions_sensitive();
	gtk_widget_show_all(window);
	g_idle_add(&initial_draw, NULL);
	if (start_fullscreen && nfiles)
		g_idle_add(&activate_fullscreen, NULL);

	gtk_main();

	if (chequer_pattern)
		cairo_pattern_destroy(chequer_pattern);
	forget_transformed_raster();
	free_ui();
out:
	free_files();
	free_image_filters();
	return failed;

fail:
	failed = 1;
	goto out;
}
