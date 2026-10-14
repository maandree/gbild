/* See LICENSE file for copyright and license details. */
#include "common.h"

static char url_list_name[] = "text/uri-list";
static GtkTargetEntry url_list = {url_list_name, 0u, 0u};

static int dragging = 0;
static int drag_source_disabled = 0;
static int drag_x, drag_y;

static void
set_pan_cursor(GtkWidget *widget, int active)
{
	GdkWindow *gdk_window = gtk_widget_get_window(widget);
	GdkCursor *cursor;

	if (!gdk_window)
		return;
	if (!active) {
		gdk_window_set_cursor(gdk_window, NULL);
		return;
	}
	cursor = gdk_cursor_new_from_name(gtk_widget_get_display(widget), "grabbing");
	if (!cursor)
		cursor = gdk_cursor_new(GDK_HAND1);
	if (!cursor)
		return;
	gdk_window_set_cursor(gdk_window, cursor);
	gdk_cursor_unref(cursor);
}

static void
set_file_drag_source(GtkWidget *widget)
{
	gtk_drag_source_set(widget, GDK_BUTTON1_MASK, &url_list, 1u,
	                    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
}

static int
button_pressed(GtkWidget *widget, GdkEventButton *event, void *user)
{
	struct file *file;
	(void) user;
	switch (event->button) {
	case 1u:
		if (~event->state & GDK_CONTROL_MASK) {
			file = get_file();
			if (file && file->path)
				return 0;
			gtk_drag_source_unset(widget);
			drag_source_disabled = 1;
			return 0;
		}
		gtk_drag_source_unset(widget);
		drag_source_disabled = 1;
		/* fall through */
	case 2u:
		dragging = 1;
		set_pan_cursor(widget, 1);
		drag_x = (int)event->x_root;
		drag_y = (int)event->y_root;
		return 1;
	case 8u:
		previous_action(NULL, NULL);
		return 1;
	case 9u:
		next_action(NULL, NULL);
		return 1;
	default:
		return 0;
	}
}

static int
button_released(GtkWidget *widget, GdkEventButton *event, void *user)
{
	(void) event;
	(void) user;
	if (dragging)
		set_pan_cursor(widget, 0);
	dragging = 0;
	if (drag_source_disabled) {
		set_file_drag_source(widget);
		drag_source_disabled = 0;
	}
	return 0;
}

static int
pointer_moved(GtkWidget *widget, GdkEventMotion *event, void *user)
{
	int x, y;
	(void) event;
	(void) user;
	if (dragging) {
		gdk_display_get_pointer(gtk_widget_get_display(widget), NULL, &x, &y, NULL);
		scroll_by(drag_x - x, drag_y - y);
		drag_x = x;
		drag_y = y;
	}
	return dragging;
}

void
set_image_drag_icon(GdkDragContext *context, struct image *image)
{
	GdkPixbuf *icon, *icon2;
	enum transformation orientation;
	intmax_t height, width;
	int w, h;

	if (!image)
		return;

	orientation = image->embedded_transformation;
	width = (intmax_t)image->width;
	height = (intmax_t)image->height;
	if (width > DRAG_ICON_SIZE || height > DRAG_ICON_SIZE) {
		if (width >= height) {
			height = height * DRAG_ICON_SIZE / width;
			height = MAX(1, height);
			width = DRAG_ICON_SIZE;
		} else {
			width = width * DRAG_ICON_SIZE / height;
			width = MAX(1, width);
			height = DRAG_ICON_SIZE;
		}
	}
	w = (int)width;
	h = (int)height;

	if (image->raster && width == image->width && height == image->height) {
		if (orientation && (icon = transform_raster(image->raster, orientation)))
			goto set_icon;
		gtk_drag_set_icon_pixbuf(context, image->raster, 0, 0);
	} else if (image->vector && (icon = rasterise(image->vector, w, h))) {
		goto set_icon;
	} else if (image->raster && (icon = gdk_pixbuf_scale_simple(image->raster, w, h, GDK_INTERP_BILINEAR))) {
		if (orientation && (icon2 = transform_raster(icon, orientation))) {
			g_object_unref(icon);
			icon = icon2;
		}
	set_icon:
		gtk_drag_set_icon_pixbuf(context, icon, 0, 0);
		g_object_unref(icon);
	}
}

static void
drag_begin(GtkWidget *widget, GdkDragContext *context, void *user)
{
	struct image image;

	(void) widget;
	(void) user;

	if (get_original_image(&image))
		set_image_drag_icon(context, &image);
}

static void
drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data,
              unsigned int info, unsigned int time, void *user)
{
	struct file *file = get_file();
	GError *error = NULL;
	char *uri;

	(void) widget;
	(void) context;
	(void) info;
	(void) time;
	(void) user;

	if (!file || !file->path)
		return;

	uri = g_filename_to_uri(file->path, NULL, &error);
	if (!uri) {
		weprintf("g_filename_to_uri: %s", error->message);
		g_error_free(error);
		return;
	}
	gtk_selection_data_set_uris(data, (char *[]){uri, NULL});
	g_free(uri);
}

void
set_up_dragging(void)
{
	set_file_drag_source(display_area);

	g_signal_connect(display_area, "button-press-event", G_CALLBACK(&button_pressed), NULL);
	g_signal_connect(display_area, "button-release-event", G_CALLBACK(&button_released), NULL);
	g_signal_connect(display_area, "motion-notify-event", G_CALLBACK(&pointer_moved), NULL);

	g_signal_connect(display_area, "drag-begin", G_CALLBACK(&drag_begin), NULL);
	g_signal_connect(display_area, "drag-data-get", G_CALLBACK(&drag_data_get), NULL);
}
