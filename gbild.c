/* See LICENSE file for copyright and license details. */
#include "common.h"

USAGE("[-f] [file] ...");

static int suspend_expose = 0;

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
	             "Image %zu of %zu | %ju%s %c | %i × %i pixels | %g%% scale",
	             current_file + 1u, nfiles,
	             fsize, fsize_decimals, units[unit],
	             gdk_pixbuf_get_width(file->image), gdk_pixbuf_get_height(file->image),
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
	if (file) {
		len = format_statusbar_message(NULL, 0u, file);
		message = malloc(++len);
		format_statusbar_message(message, len, file);
	}
	set_statusbar_message(message);
	free(message);
}

int
expose_image(GtkWidget *widget, GdkEventExpose *event, void *user)
{
	GdkPixbuf *image;
	cairo_t *cr;
	double height, width, x, y, s;

	(void) user;

	if (suspend_expose)
		return 1;

	cr = gdk_cairo_create(widget->window);
	gdk_cairo_set_source_color(cr, &widget->style->bg[GTK_STATE_NORMAL]);

	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_rectangle(cr, event->area.x, event->area.y, event->area.width, event->area.height);

	image = get_image();
	if (!image) {
		cairo_fill(cr);
		goto out;
	}

	s = get_zoom(zoom);
	width = s * gdk_pixbuf_get_width(image);
	height = s * gdk_pixbuf_get_height(image);
	if (width < widget->allocation.width)
		x = 0.5 * ((double)widget->allocation.width - width);
	else
		x = -logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll))->value;
	if (height < widget->allocation.height)
		y = 0.5 * ((double)widget->allocation.height - height);
	else
		y = -logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll))->value;

	cairo_fill(cr);

	cairo_translate(cr, x, y);
	cairo_scale(cr, s, s);
	gdk_cairo_set_source_pixbuf(cr, image, 0.0, 0.0);

out:
	cairo_paint(cr);
	cairo_destroy(cr);
	return 1;
}

static void
show_image(int update_zoom)
{
	GdkPixbuf *new_image, *original_image, *display_image;
	double s;

	original_image = get_original_image();
	if (!original_image) {
		logical_scroll_pane_set_content_size(LOGICAL_SCROLL_PANE(scroll), 1, 1);
		gtk_widget_queue_draw(display_area);
		return;
	}

	new_image = apply_transformations(original_image);
	display_image = new_image ? new_image : original_image;

	if (update_zoom && zoom_mode != ZOOM_CUSTOM && zoom_mode != ZOOM_NORMAL)
		zoom = calculate_zoom_mode(display_image, zoom_mode);

	s = get_zoom(zoom);
	logical_scroll_pane_set_content_size(LOGICAL_SCROLL_PANE(scroll),
	                                     (int)(s * gdk_pixbuf_get_width(display_image)),
	                                     (int)(s * gdk_pixbuf_get_height(display_image)));

	if (transformed_image)
		g_object_unref(transformed_image);
	transformed_image = new_image;
}

static void
set_images(struct file *new_files, size_t nnew_files)
{
	g_clear_object(&transformed_image);
	free_files();
	files = new_files;
	nfiles = nnew_files;
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
	int w, h;
	GdkPixbuf *image;

	if (zoom_eq(new_zoom, zoom))
		return;
	image = get_image();
	if (!image)
		goto out;

	old_s = get_zoom(zoom);
	new_s = get_zoom(new_zoom);
	zoom = new_zoom;

	w = gdk_pixbuf_get_width(image);
	h = gdk_pixbuf_get_height(image);

	hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	vw = hadjustment->page_size;
	vh = vadjustment->page_size;

	iw = old_s * (double)w;
	ih = old_s * (double)h;
	old_x0 = vw > iw ? 0.5 * (vw - iw) : 0.0;
	old_y0 = vh > ih ? 0.5 * (vh - ih) : 0.0;

	iw = new_s * (double)w;
	ih = new_s * (double)h;
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
	update_status();
}

void
transform_redraw(enum transformation augment)
{
	GdkPixbuf *image;
	GtkAdjustment *hadjustment, *vadjustment;
	double s, x, y, width, height, max;

	hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	image = get_image();
	if (!image)
		return;

	s = get_zoom(zoom);
	width = gdk_pixbuf_get_width(image);
	height = gdk_pixbuf_get_height(image);

	x = hadjustment->value + 0.5 * hadjustment->page_size;
	x = s * width > hadjustment->page_size ? x / s : 0.5 * width;
	y = vadjustment->value + 0.5 * vadjustment->page_size;
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
	update_status();
}

int
recalculate_zoom_mode(void *user)
{
	GdkPixbuf *image = get_image();
	(void) user;
	if (image && zoom_mode != ZOOM_CUSTOM && zoom_mode != ZOOM_NORMAL)
		set_zoom_and_recentre(calculate_zoom_mode(image, zoom_mode));
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

	if (transformed_image)
		g_object_unref(transformed_image);
	free_ui();
out:
	free_files();
	free_image_filters();
	return failed;

fail:
	failed = 1;
	goto out;
}
