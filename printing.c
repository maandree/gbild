/* See LICENSE file for copyright and license details. */
#include "common.h"
#include <math.h>

#define LIST_UNITS(X, ...)\
	X(UNIT_MILLIMETRES,        "Millimetres",                       72.0 / 25.4)  __VA_ARGS__\
	X(UNIT_CENTIMETRES,        "Centimetres",                       72.0 / 2.54)  __VA_ARGS__\
	X(UNIT_INCHES,             "Inches",                            72.0)         __VA_ARGS__\
	X(UNIT_MODERN_POINTS,      "Modern points (1/72 inch)",         1.0)          __VA_ARGS__\
	X(UNIT_TRADITIONAL_POINTS, "Traditional points (1/72.27 inch)", 72.0 / 72.27) __VA_ARGS__\
	X(UNIT_PERCENT,            "Percent of printable area",         0.01)

#define LIST_HORIZONTAL_FIELDS(X, ...)\
	X(FIELD_WIDTH, "Width") __VA_ARGS__\
	X(FIELD_LEFT,  "Left")  __VA_ARGS__\
	X(FIELD_RIGHT, "Right")

#define LIST_VERTICAL_FIELDS(X, ...)\
	X(FIELD_HEIGHT, "Height") __VA_ARGS__\
	X(FIELD_TOP,    "Top")    __VA_ARGS__\
	X(FIELD_BOTTOM, "Bottom")

#define LIST_FIELDS(X, ...)\
	LIST_HORIZONTAL_FIELDS(X, __VA_ARGS__) __VA_ARGS__\
	LIST_VERTICAL_FIELDS(X, __VA_ARGS__)

enum unit {
#define X(E, N, S) E
	LIST_UNITS(X, COMMA)
#undef X
};

enum field {
#define X(E, N) E
	LIST_FIELDS(X, COMMA)
#undef X
};

#define X_COUNT(...) 1u
#define NFIELDS (LIST_FIELDS(X_COUNT, +))

struct print_data {
	GdkPixbuf *image;
	double width;
	double height;
	double left;
	double top;
};

struct positioning_widget {
	struct print_data *data;
	GtkWidget *unit;
	GtkWidget *entry[NFIELDS];
	GtkWidget *preview;
	double page_width;
	double page_height;
	double drag_start_x;
	double drag_start_y;
	double drag_start_left;
	double drag_start_top;
	int updating;
	int dragging;
};

#define X(E, T) "_"T":"
static const char *const field_labels[] = { LIST_FIELDS(X, COMMA) };
#undef X

static int
horizontal(enum field field)
{
#define X(E, T) if (field == E) return 1
	LIST_HORIZONTAL_FIELDS(X, ;);
#undef X
	return 0;
}

static double
unit_in_points(struct positioning_widget *widget, enum field field)
{
	enum unit unit = gtk_combo_box_get_active(GTK_COMBO_BOX(widget->unit));
	double ret;
	switch (unit) {
#define X(E, N, S) case E:  ret = (S);  break
		LIST_UNITS(X, ;);
#undef X
	}
	if (unit == UNIT_PERCENT)
		ret *= horizontal(field) ? widget->page_width : widget->page_height;
	return ret;
}

static double
points_to_unit(struct positioning_widget *widget, enum field field, double value)
{
	return value / unit_in_points(widget, field);
}

static double
unit_to_points(struct positioning_widget *widget, enum field field, double value)
{
	return value * unit_in_points(widget, field);
}

static void
get_printable_size(GtkPageSetup *setup, double *width, double *height)
{
	*width = gtk_page_setup_get_paper_width(setup, GTK_UNIT_POINTS) -
	         gtk_page_setup_get_left_margin(setup, GTK_UNIT_POINTS) -
	         gtk_page_setup_get_right_margin(setup, GTK_UNIT_POINTS);

	*height = gtk_page_setup_get_paper_height(setup, GTK_UNIT_POINTS) -
	          gtk_page_setup_get_top_margin(setup, GTK_UNIT_POINTS) -
	          gtk_page_setup_get_bottom_margin(setup, GTK_UNIT_POINTS);

	*width = MAX(*width, 0.1);
	*height = MAX(*height, 0.1);
}

static double
get_preview_scale(GtkWidget *preview, struct positioning_widget *widget)
{
	int width  = MAX(preview->allocation.width  - 16, 1);
	int height = MAX(preview->allocation.height - 16, 1);
	double x = (double)width  / widget->page_width;
	double y = (double)height / widget->page_height;
	return MIN(x, y);
}

static double
get_field_value(struct positioning_widget *widget, enum field field)
{
	struct print_data *data = widget->data;
	switch (field) {
	case FIELD_WIDTH:   return data->width;
	case FIELD_HEIGHT:  return data->height;
	case FIELD_TOP:     return data->top;
	case FIELD_LEFT:    return data->left;
	case FIELD_RIGHT:   return widget->page_width - data->left - data->width;
	case FIELD_BOTTOM:  return widget->page_height - data->top - data->height;
	}
	abort();
}

static int
set_field_value(struct positioning_widget *widget, enum field field, double value)
{
	struct print_data *data = widget->data;
	int image_width  = gdk_pixbuf_get_width(data->image);
	int image_height = gdk_pixbuf_get_height(data->image);
	double aspect, pos, size, page, pos_opp, size_opp, page_opp;
	double new_pos, new_size, new_pos_opp, new_size_opp;
	double *posp, *pos_oppp, *sizep, *size_oppp, limit;

	switch (field) {
	case FIELD_WIDTH:
	case FIELD_LEFT:
	case FIELD_RIGHT:
		posp = &data->left;
		sizep = &data->width;
		page = widget->page_width;
		pos_oppp = &data->top;
		size_oppp = &data->height;
		page_opp = widget->page_height;
		aspect = (double)image_height / (double)image_width;
		break;

	case FIELD_HEIGHT:
	case FIELD_TOP:
	case FIELD_BOTTOM:
		posp = &data->top;
		sizep = &data->height;
		page = widget->page_height;
		pos_oppp = &data->left;
		size_oppp = &data->width;
		page_opp = widget->page_width;
		aspect = (double)image_width / (double)image_height;
		break;
	}

	pos = *posp;
	size = *sizep;
	pos_opp = *pos_oppp;
	size_opp = *size_oppp;

	switch (field) {
	case FIELD_WIDTH:
	case FIELD_HEIGHT:
		limit = page_opp / aspect;
		limit = MIN(page, limit);
		new_size = MIN(MAX(0.0, value), limit);
		new_size_opp = new_size * aspect;
		new_pos = pos + 0.5 * (size - new_size);
		new_pos = MIN(MAX(0.0, new_pos), page - new_size);
		new_pos_opp = pos_opp + 0.5 * (size_opp - new_size_opp);
		new_pos_opp = MIN(MAX(0.0, new_pos_opp), page_opp - new_size_opp);
		break;

	case FIELD_LEFT:
	case FIELD_TOP:
		value = MAX(value, 0.0);
		new_pos = value;
		new_size = pos + size - new_pos;
		new_size_opp = new_size * aspect;
		if (new_size < 0.0 || new_size_opp > page_opp) {
			new_size = size;
			new_size_opp = size_opp;
			new_pos = MIN(MAX(0.0, value), page - new_size);
			new_pos_opp = pos_opp;
		} else {
			new_pos_opp = pos_opp + 0.5 * (size_opp - new_size_opp);
			new_pos_opp = MIN(MAX(0.0, new_pos_opp), page_opp - new_size_opp);
		}
		break;

	case FIELD_RIGHT:
	case FIELD_BOTTOM:
		value = MAX(value, 0.0);
		new_pos = pos;
		new_size = page - new_pos - value;
		new_size_opp = new_size * aspect;
		if (new_size < 0.0 || new_size_opp > page_opp) {
			new_size = size;
			new_size_opp = size_opp;
			limit = page - new_size;
			new_pos = limit - value;
			new_pos = MIN(MAX(0.0, new_pos), limit);
			new_pos_opp = pos_opp;
		} else {
			new_pos_opp = pos_opp + 0.5 * (size_opp - new_size_opp);
			new_pos_opp = MIN(MAX(0.0, new_pos_opp), page_opp - new_size_opp);
		}
		break;
	}

	*posp = new_pos;
	*sizep = new_size;
	*pos_oppp = new_pos_opp;
	*size_oppp = new_size_opp;
	return 1;
}

static enum field
get_positioning_field(struct positioning_widget *widget, GtkWidget *entry)
{
	enum field field = 0u;
	while (field < NFIELDS && widget->entry[field] != entry)
		field++;
	return field;
}

static int
positioning_preview_expose(GtkWidget *preview, GdkEventExpose *event, void *user)
{
	struct positioning_widget *widget = user;
	struct print_data *data = widget->data;
	double scale = get_preview_scale(preview, widget);
	double w = scale * widget->page_width;
	double h = scale * widget->page_height;
	double image_scale;
	double page_x = preview->allocation.width - 8.0 - w;
	double page_y = 8.0;
	int image_width;
	cairo_t *cr;

	cr = gdk_cairo_create(preview->window);

	cairo_rectangle(cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip(cr);
	gdk_cairo_set_source_color(cr, &preview->style->bg[GTK_STATE_NORMAL]);
	cairo_paint(cr);

	cairo_rectangle(cr, page_x, page_y, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_save(cr);

	cairo_rectangle(cr, page_x, page_y, w, h);
	cairo_clip(cr);
	image_width = gdk_pixbuf_get_width(data->image);
	image_scale = scale * data->width / (double)image_width;
	cairo_translate(cr, page_x + scale * data->left, page_y + scale * data->top);
	cairo_scale(cr, image_scale, image_scale);
	gdk_cairo_set_source_pixbuf(cr, data->image, 0.0, 0.0);
	cairo_paint(cr);

	cairo_restore(cr);

	cairo_rectangle(cr, page_x, page_y, w, h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	cairo_destroy(cr);
	return 0;
}

static int
positioning_preview_button_pressed(GtkWidget *preview, GdkEventButton *event, void *user)
{
	struct positioning_widget *widget = user;
	struct print_data *data = widget->data;
	double scale = get_preview_scale(preview, widget);
	double page_x = preview->allocation.width - 8.0 - scale * widget->page_width;
	double page_y = 8.0;
	double image_x = page_x + scale * data->left;
	double image_y = page_y + scale * data->top;

	if (event->button != 1 ||
	    image_x > event->x || event->x > image_x + scale * data->width ||
	    image_y > event->y || event->y > image_y + scale * data->height)
		return 0;

	widget->drag_start_x = event->x;
	widget->drag_start_y = event->y;
	widget->drag_start_left = data->left;
	widget->drag_start_top = data->top;
	widget->dragging = 1;
	return 1;
}

static void
update_positioning_widget(struct positioning_widget *widget, enum field active_field)
{
	struct print_data *data = widget->data;
	double x, y, value, scale = 1.0;
	enum field field;
	char *text;
	int len, r;

	if (data->width > 0.0 && data->height > 0.0) {
		x = widget->page_width / data->width;
		y = widget->page_height / data->height;
		scale = MIN(x, y);
		if (scale < 1.0) {
			data->width *= scale;
			data->height *= scale;
		}
	}

	x = widget->page_width - data->width;
	y = widget->page_height - data->height;
	data->left = MIN(MAX(0.0, data->left), x);
	data->top = MIN(MAX(0.0, data->top), y);

	widget->updating = 1;

	for (field = 0u; field < NFIELDS; field++) {
		if (field == active_field)
			continue;

		value = points_to_unit(widget, field, get_field_value(widget, field));

		len = snprintf(NULL, 0u, "%.2f", value);
		if (len < 0) eprintf("snprintf:");

		text = malloc((size_t)len + 1u);
		if (!text) abort();

		r = sprintf(text, "%.2f", value);
		if (r < 0)   eprintf("snprintf:");
		if (r > len) abort();

		gtk_entry_set_text(GTK_ENTRY(widget->entry[field]), text);
		free(text);
	}

	widget->updating = 0;
	if (widget->preview)
		gtk_widget_queue_draw(widget->preview);
}

static int
positioning_preview_motion(GtkWidget *preview, GdkEventMotion *event, void *user)
{
	struct positioning_widget *widget = user;
	struct print_data *data = widget->data;
	double scale;
	double left;
	double top;

	if (!widget->dragging)
		return 0;

	scale = get_preview_scale(preview, widget);
	left = widget->drag_start_left + (event->x - widget->drag_start_x) / scale;
	left = MIN(MAX(0.0, left), widget->page_width - data->width);
	top = widget->drag_start_top + (event->y - widget->drag_start_y) / scale;
	top = MIN(MAX(0.0, top), widget->page_height - data->height);
	data->left = left;
	data->top = top;
	update_positioning_widget(widget, NFIELDS);
	return 1;
}

static int
positioning_preview_button_released(GtkWidget *preview, GdkEventButton *event, void *user)
{
	struct positioning_widget *widget = user;
	(void) preview;
	if (event->button != 1 || !widget->dragging)
		return 0;
	widget->dragging = 0;
	return 1;
}

static int
positioning_preview_scrolled(GtkWidget *preview, GdkEventScroll *event, void *user)
{
	struct positioning_widget *widget = user;
	struct print_data *data = widget->data;
	int image_width, image_height;
	double aspect, amount, scale, page_x;
	double pointer_x, pointer_y;
	double maximum, width, height;

	if (!(event->state & GDK_CONTROL_MASK))
		return 0;

	switch (event->direction) {
	case GDK_SCROLL_UP:    amount = +0.02;  break;
	case GDK_SCROLL_DOWN:  amount = -0.02;  break;
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_RIGHT:
	default:
		return 0;
	}
	amount *= widget->page_width;

	image_width  = gdk_pixbuf_get_width(data->image);
	image_height = gdk_pixbuf_get_height(data->image);
	aspect = (double)image_width / (double)image_height;

	maximum = MIN(widget->page_width, widget->page_height * aspect);
	width = CLAMP(data->width + amount, 0.0, maximum);
	height = width / aspect;

	scale = get_preview_scale(preview, widget);
	page_x = preview->allocation.width - 8.0 - scale * widget->page_width;
	pointer_x = (event->x - page_x) / scale;
	pointer_y = (event->y - 8.0) / scale;
	if (data->width > 0.0) {
		data->left = pointer_x - (pointer_x - data->left) * width / data->width;
		data->top = pointer_y - (pointer_y - data->top) * height / data->height;
	}
	data->width = width;
	data->height = height;
	update_positioning_widget(widget, NFIELDS);
	return 1;
}

static int
get_entry_value(GtkEntry *entry, double *value)
{
	const char *text = gtk_entry_get_text(entry);
	char *end;
	double parsed = strtod(text, &end);
	while (isspace(*(unsigned char *)end))
		end++;
	if (end == text || *end || !isfinite(parsed))
		return 0;
	*value = parsed;
	return 1;
}

static void
adjust_positioning_field(struct positioning_widget *widget, enum field field, double amount)
{
	double value;

	if (field == NFIELDS)
		return;

	value = get_field_value(widget, field);
	value = points_to_unit(widget, field, value);
	value += amount;
	value = unit_to_points(widget, field, value);

	if (set_field_value(widget, field, value))
		update_positioning_widget(widget, NFIELDS);
}

static int
positioning_entry_key_pressed(GtkWidget *entry, GdkEventKey *event, void *user)
{
	struct positioning_widget *widget = user;
	enum field field = get_positioning_field(widget, entry);
	double amount;
	switch (event->keyval) {
	case GDK_Up:        amount = +1.0;   break;
	case GDK_Down:      amount = -1.0;   break;
	case GDK_Page_Up:   amount = +10.0;  break;
	case GDK_Page_Down: amount = -10.0;  break;
	default:
		return 0;
	}
	adjust_positioning_field(widget, field, amount);
	return 1;
}

static int
positioning_entry_scrolled(GtkWidget *entry, GdkEventScroll *event, void *user)
{
	struct positioning_widget *widget = user;
	enum field field = get_positioning_field(widget, entry);
	if (event->direction == GDK_SCROLL_UP)
		adjust_positioning_field(widget, field, +1.0);
	else if (event->direction == GDK_SCROLL_DOWN)
		adjust_positioning_field(widget, field, -1.0);
	else
		return 0;
	return 1;
}

static void
positioning_entry_inserted(GtkEditable *editable, const char *text, int length, int *position, void *user)
{
	(void) position;
	(void) user;
	if (memchr(text, '-', (size_t)length))
		g_signal_stop_emission_by_name(editable, "insert-text");
}

static void
positioning_entry_changed(GtkEditable *editable, void *user)
{
	struct positioning_widget *widget = user;
	enum field field;
	double value;

	if (widget->updating)
		return;

	for (field = 0; field < NFIELDS; field++)
		if (GTK_EDITABLE(widget->entry[field]) == editable)
			goto have_field;
	return;

have_field:
	if (get_entry_value(GTK_ENTRY(widget->entry[field]), &value)) {
		value = unit_to_points(widget, field, value);
		if (!set_field_value(widget, field, value))
			field = NFIELDS;
		update_positioning_widget(widget, field);
	}
}

static void
positioning_unit_changed(GtkComboBox *combo, void *user)
{
	struct positioning_widget *widget = user;
	(void) combo;
	if (!widget->updating)
		update_positioning_widget(widget, NFIELDS);
}

static void
begin_print(GtkPrintOperation *operation, GtkPrintContext *context, void *user)
{
	(void) context;
	(void) user;
	gtk_print_operation_set_n_pages(operation, 1);
}

static int
positioning_entry_focus_out(GtkWidget *entry, GdkEventFocus *event, void *user)
{
	struct positioning_widget *widget = user;
	(void) entry;
	(void) event;
	if (!widget->updating)
		update_positioning_widget(widget, NFIELDS);
	return 0;
}

static GtkWidget *
create_positioning_widget(GtkPrintOperation *operation, void *user)
{
#define X(E, N, S) N
	static const char *const unit_names[] = {LIST_UNITS(X, COMMA)};
#undef X

#define X(E, T) E
	static const enum field field_layout[2][3] = {
		{LIST_HORIZONTAL_FIELDS(X, COMMA)},
		{LIST_VERTICAL_FIELDS(X, COMMA)}
	};
#undef X

	struct print_data *data = user;
	struct positioning_widget *positioning;
	GtkPageSetup *setup;
	GtkWidget *box, *grid, *label;
	enum field field;
	size_t unit;
	unsigned int row, column;

	positioning = g_new0(struct positioning_widget, 1);
	positioning->data = data;
	setup = gtk_print_operation_get_default_page_setup(operation);
	get_printable_size(setup, &positioning->page_width, &positioning->page_height);

	box = gtk_hbox_new(0, 12);
	grid = gtk_table_new(4, 4, 0);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
	gtk_table_set_row_spacings(GTK_TABLE(grid), 6);
	gtk_table_set_col_spacings(GTK_TABLE(grid), 6);
	label = gtk_label_new_with_mnemonic("_Units:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(GTK_TABLE(grid), label, 0, 1, 0, 1, GTK_FILL, 0, 4u, 4u);
	positioning->unit = gtk_combo_box_new_text();
	for (unit = 0; unit < ELEMSOF(unit_names); unit++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(positioning->unit), unit_names[unit]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(positioning->unit), UNIT_MILLIMETRES);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), positioning->unit);
	gtk_table_attach(GTK_TABLE(grid), positioning->unit, 1, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 4u, 4u);

	for (field = 0; field < NFIELDS; field++) {
		positioning->entry[field] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(positioning->entry[field]), 12);
		gtk_widget_add_events(positioning->entry[field], GDK_SCROLL_MASK);
		g_signal_connect(positioning->entry[field], "changed",
		                 G_CALLBACK(&positioning_entry_changed), positioning);
		g_signal_connect(positioning->entry[field], "focus-out-event",
		                 G_CALLBACK(&positioning_entry_focus_out), positioning);
		g_signal_connect(positioning->entry[field], "insert-text",
		                 G_CALLBACK(&positioning_entry_inserted), positioning);
		g_signal_connect(positioning->entry[field], "key-press-event",
		                 G_CALLBACK(&positioning_entry_key_pressed), positioning);
		g_signal_connect(positioning->entry[field], "scroll-event",
		                 G_CALLBACK(&positioning_entry_scrolled), positioning);
	}
	for (row = 0; row < 3; row++) {
		for (column = 0; column < 2; column++) {
			field = field_layout[column][row];
			label = gtk_label_new_with_mnemonic(field_labels[field]);
			gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
			gtk_label_set_mnemonic_widget(GTK_LABEL(label), positioning->entry[field]);
			gtk_table_attach(GTK_TABLE(grid), label, 2 * column, 2 * column + 1,
			                 row + 1, row + 2, GTK_FILL, 0, 4u, 4u);
			gtk_table_attach(GTK_TABLE(grid), positioning->entry[field],
			                 2 * column + 1, 2 * column + 2, row + 1, row + 2,
			                 GTK_FILL, 0, 4u, 4u);
		}
	}
	positioning->preview = gtk_drawing_area_new();
	gtk_widget_set_size_request(positioning->preview, 200, 200);
	gtk_widget_add_events(positioning->preview,
	                      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	g_signal_connect(positioning->preview, "expose-event",
	                 G_CALLBACK(&positioning_preview_expose), positioning);
	g_signal_connect(positioning->preview, "button-press-event",
	                 G_CALLBACK(&positioning_preview_button_pressed), positioning);
	g_signal_connect(positioning->preview, "motion-notify-event",
	                 G_CALLBACK(&positioning_preview_motion), positioning);
	g_signal_connect(positioning->preview, "scroll-event",
	                 G_CALLBACK(&positioning_preview_scrolled), positioning);
	g_signal_connect(positioning->preview, "button-release-event",
	                 G_CALLBACK(&positioning_preview_button_released), positioning);
	gtk_box_pack_start(GTK_BOX(box), grid, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), positioning->preview, 1, 1, 0u);
	g_signal_connect(positioning->unit, "changed", G_CALLBACK(&positioning_unit_changed), positioning);
	g_object_set_data_full(G_OBJECT(box), "positioning", positioning, g_free);
	update_positioning_widget(positioning, NFIELDS);
	gtk_widget_show_all(box);
	return box;
}

static void
apply_print_positioning(GtkPrintOperation *operation, GtkWidget *widget, void *user)
{
	struct positioning_widget *positioning = g_object_get_data(G_OBJECT(widget), "positioning");
	(void) operation;
	(void) user;
	if (positioning)
		update_positioning_widget(positioning, NFIELDS);
}

static void
update_print_positioning(GtkPrintOperation *operation, GtkWidget *widget, GtkPageSetup *setup,
			 GtkPrintSettings *settings, void *user)
{
	struct positioning_widget *positioning = g_object_get_data(G_OBJECT(widget), "positioning");
	(void) operation;
	(void) settings;
	(void) user;
	if (positioning) {
		get_printable_size(setup, &positioning->page_width, &positioning->page_height);
		update_positioning_widget(positioning, NFIELDS);
	}
}

static void
draw_page(GtkPrintOperation *operation, GtkPrintContext *context, int page, void *user)
{
	struct print_data *data = user;
	GdkPixbuf *image = data->image;
	double scale;
	cairo_t *cr;

	(void) operation;
	(void) page;

	scale = data->width / gdk_pixbuf_get_width(image);
	cr = gtk_print_context_get_cairo_context(context);
	cairo_translate(cr, data->left, data->top);
	cairo_scale(cr, scale, scale);
	gdk_cairo_set_source_pixbuf(cr, image, 0.0, 0.0);
	cairo_paint(cr);
}

void
open_print(GdkPixbuf *image)
{
	struct print_data data;
	GtkPrintOperation *operation;
	GtkPageSetup *setup;
	GtkPageOrientation orientation = GTK_PAGE_ORIENTATION_PORTRAIT;
	double page_width, page_height, s, x, y;
	int width, height;

	if (!image)
		return;

	data.image = image;
	g_object_ref(data.image);

	width  = gdk_pixbuf_get_width(data.image);
	height = gdk_pixbuf_get_height(data.image);

	if (width > height)
		orientation = GTK_PAGE_ORIENTATION_LANDSCAPE;

	setup = gtk_page_setup_new();
	gtk_page_setup_set_orientation(setup, orientation);
	get_printable_size(setup, &page_width, &page_height);

	x = page_width / (double)width;
	y = page_height / (double)height;
	s = MIN(x, y);
	data.width  = s * width;
	data.height = s * height;
	data.left = 0.5 * (page_width - data.width);
	data.top  = 0.5 * (page_height - data.height);

	operation = gtk_print_operation_new();
	gtk_print_operation_set_default_page_setup(operation, setup);
	g_object_unref(setup);
	gtk_print_operation_set_embed_page_setup(operation, 1);
	gtk_print_operation_set_custom_tab_label(operation, "Positioning");

	g_signal_connect(operation, "create-custom-widget", G_CALLBACK(&create_positioning_widget), &data);
	g_signal_connect(operation, "custom-widget-apply", G_CALLBACK(&apply_print_positioning), &data);
	g_signal_connect(operation, "update-custom-widget", G_CALLBACK(&update_print_positioning), &data);
	g_signal_connect(operation, "begin-print", G_CALLBACK(&begin_print), &data);
	g_signal_connect(operation, "draw-page", G_CALLBACK(&draw_page), &data);

	gtk_print_operation_run(operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(window), NULL);
	g_object_unref(operation);
	g_object_unref(data.image);
}
