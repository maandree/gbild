/* See LICENSE file for copyright and license details. */
#include "common.h"

#define LIST_SCALING_FILTERS(X, ...)\
	X(0, CAIRO_FILTER_NEAREST,  "Nearest") __VA_ARGS__\
	X(1, CAIRO_FILTER_BILINEAR, "Bilinear") __VA_ARGS__\
	X(2, CAIRO_FILTER_GAUSSIAN, "Gaussian")

cairo_filter_t scaling_filter = CAIRO_FILTER_BILINEAR;
GdkColor chequer_colours[2] = {
	{0u, 0xD3D3u, 0xD3D3u, 0xD3D3u},
	{0u, 0xFFFFu, 0xFFFFu, 0xFFFFu}
};
uint16_t chequer_alphas[2] = {0xFFFFu, 0xFFFFu};
int chequered_background = 0;
int chequer_size = DEFAULT_CHEQUER_SIZE;
int apply_embedded_orientation = 0;
int normal_size_is_physical_size = 0;

static GtkWidget *preferences_window;
static GtkWidget *transparency_controls;

static void
preferences_response(GtkDialog *dialog, int response, void *user)
{
	(void) response;
	(void) user;
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
preferences_destroyed(GtkWidget *widget, void *user)
{
	(void) widget;
	(void) user;
	preferences_window = NULL;
}

static void
scaling_filter_changed(GtkComboBox *box, void *user)
{
	(void) user;
	switch (gtk_combo_box_get_active(box)) {
#define X(I, V, T) case I: scaling_filter = V; break
	LIST_SCALING_FILTERS(X, ;);
#undef X
	default:
		abort();
	}
	gtk_widget_queue_draw(display_area);
}

static void
chequered_background_changed(GtkToggleButton *button, void *user)
{
	(void) user;
	chequered_background = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(transparency_controls, chequered_background);
	gtk_widget_queue_draw(display_area);
}

static void
transparency_colour_changed(GtkColorButton *button, void *user)
{
	GdkColor *colour = user;
	size_t index = (size_t)(colour - chequer_colours);
	gtk_color_button_get_color(button, colour);
	chequer_alphas[index] = gtk_color_button_get_alpha(button);
	gtk_widget_queue_draw(display_area);
}

static void
chequer_size_changed(GtkSpinButton *button, void *user)
{
	(void) user;
	chequer_size = gtk_spin_button_get_value_as_int(button);
	gtk_widget_queue_draw(display_area);
}

static void
apply_embedded_orientation_changed(GtkToggleButton *button, void *user)
{
	struct file *file;
	enum transformation t;
	int active = gtk_toggle_button_get_active(button);
	(void) user;
	if (apply_embedded_orientation == active)
		return;
	apply_embedded_orientation = active;
	file = get_file();
	if (file && (t = file->image.embedded_transformation))
		transform_redraw(active ? t : inverse_transformation(t));
	collection_redraw_embedded_orientations();
}

static void
normal_size_is_physical_size_changed(GtkToggleButton *button, void *user)
{
	int active = gtk_toggle_button_get_active(button);
	(void) user;
	if (normal_size_is_physical_size == active)
		return;
	normal_size_is_physical_size = active;
	/* TODO implement */
}

static GtkWidget *
new_section(const char *title)
{
	GtkWidget *frame = gtk_frame_new(title);
	GtkWidget *box = gtk_vbox_new(0, 6);
	gtk_container_set_border_width(GTK_CONTAINER(box), 6u);
	gtk_container_add(GTK_CONTAINER(frame), box);
	return frame;
}

static GtkWidget *
new_scaling_section(void)
{
	GtkWidget *section = new_section("Scaling");
	GtkWidget *box = gtk_bin_get_child(GTK_BIN(section));
	GtkWidget *row = gtk_hbox_new(0, 6);
	GtkWidget *label = gtk_label_new_with_mnemonic("_Algorithm:");
	GtkWidget *filters = gtk_combo_box_new_text();

#define X(I, V, T) gtk_combo_box_append_text(GTK_COMBO_BOX(filters), T)
	LIST_SCALING_FILTERS(X, ;);
#undef X

	switch (scaling_filter) {
#define X(I, V, T) case V: gtk_combo_box_set_active(GTK_COMBO_BOX(filters), I); break
	LIST_SCALING_FILTERS(X, ;);
#undef X
	case CAIRO_FILTER_FAST:
	case CAIRO_FILTER_GOOD:
	case CAIRO_FILTER_BEST:
	default:
		abort();
	}

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), filters);
	gtk_box_pack_start(GTK_BOX(row), label, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(row), filters, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), row, 0, 0, 0u);
	g_signal_connect(filters, "changed", G_CALLBACK(&scaling_filter_changed), NULL);
	return section;
}

static GtkWidget *
new_image_handling_section(void)
{
	GtkWidget *section = new_section("Image handling");
	GtkWidget *box = gtk_bin_get_child(GTK_BIN(section));
	GtkWidget *orientation;
	GtkWidget *physical_size;

	orientation = gtk_check_button_new_with_mnemonic("Apply embedded _orientation");
	physical_size = gtk_check_button_new_with_mnemonic("Normal size is _physical size");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(orientation), apply_embedded_orientation);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(physical_size), normal_size_is_physical_size);
	gtk_box_pack_start(GTK_BOX(box), orientation, 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), physical_size, 0, 0, 0u);
	g_signal_connect(orientation, "toggled", G_CALLBACK(&apply_embedded_orientation_changed), NULL);
	g_signal_connect(physical_size, "toggled", G_CALLBACK(&normal_size_is_physical_size_changed), NULL);
	return section;
}

static GtkWidget *
new_transparency_section(void)
{
	GtkWidget *section = new_section("Transparent images");
	GtkWidget *box = gtk_bin_get_child(GTK_BIN(section));
	GtkWidget *enabled;
	GtkWidget *first;
	GtkWidget *second;
	GtkWidget *size;
	GtkWidget *label;
	GtkWidget *units;

	enabled = gtk_check_button_new_with_mnemonic("Use che_quered background");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled), chequered_background);
	gtk_box_pack_start(GTK_BOX(box), enabled, 0, 0, 0u);
	transparency_controls = gtk_table_new(3u, 3u, 0);
	gtk_table_set_row_spacings(GTK_TABLE(transparency_controls), 3u);
	gtk_table_set_col_spacings(GTK_TABLE(transparency_controls), 6u);
	gtk_box_pack_start(GTK_BOX(box), transparency_controls, 0, 0, 0u);

	label = gtk_label_new_with_mnemonic("_First colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	first = gtk_color_button_new_with_color(&chequer_colours[0]);
	gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(first), 1);
	gtk_color_button_set_alpha(GTK_COLOR_BUTTON(first), chequer_alphas[0]);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), first);
	gtk_table_attach(GTK_TABLE(transparency_controls), label, 0u, 1u, 0u, 1u, GTK_FILL, GTK_FILL, 0u, 0u);
	gtk_table_attach(GTK_TABLE(transparency_controls), first, 1u, 2u, 0u, 1u, GTK_FILL, GTK_FILL, 0u, 0u);

	label = gtk_label_new_with_mnemonic("_Second colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	second = gtk_color_button_new_with_color(&chequer_colours[1]);
	gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(second), 1);
	gtk_color_button_set_alpha(GTK_COLOR_BUTTON(second), chequer_alphas[1]);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), second);
	gtk_table_attach(GTK_TABLE(transparency_controls), label, 0u, 1u, 1u, 2u, GTK_FILL, GTK_FILL, 0u, 0u);
	gtk_table_attach(GTK_TABLE(transparency_controls), second, 1u, 2u, 1u, 2u, GTK_FILL, GTK_FILL, 0u, 0u);

	label = gtk_label_new_with_mnemonic("Chequer si_ze:");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	size = gtk_spin_button_new_with_range(1.0, 256.0, 1.0);
	units = gtk_label_new("pixels");
	gtk_misc_set_alignment(GTK_MISC(units), 0.0, 0.5);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(size), (double)chequer_size);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), size);
	gtk_table_attach(GTK_TABLE(transparency_controls), label, 0u, 1u, 2u, 3u, GTK_FILL, GTK_FILL, 0u, 0u);
	gtk_table_attach(GTK_TABLE(transparency_controls), size, 1u, 2u, 2u, 3u, GTK_FILL, GTK_FILL, 0u, 0u);
	gtk_table_attach(GTK_TABLE(transparency_controls), units, 2u, 3u, 2u, 3u, GTK_FILL, GTK_FILL, 0u, 0u);

	gtk_widget_set_sensitive(transparency_controls, chequered_background);
	g_signal_connect(enabled, "toggled", G_CALLBACK(&chequered_background_changed), NULL);
	g_signal_connect(first, "color-set", G_CALLBACK(&transparency_colour_changed), &chequer_colours[0]);
	g_signal_connect(second, "color-set", G_CALLBACK(&transparency_colour_changed), &chequer_colours[1]);
	g_signal_connect(size, "value-changed", G_CALLBACK(&chequer_size_changed), NULL);
	return section;
}

void
open_preferences(void)
{
	GtkWidget *alignment, *box;

	if (preferences_window) {
		gtk_window_present(GTK_WINDOW(preferences_window));
		return;
	}

	preferences_window = gtk_dialog_new_with_buttons("Preferences", GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

	alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 6u, 6u, 6u, 6u);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(preferences_window))), alignment, 1, 1, 0u);
	box = gtk_vbox_new(0, 6);
	gtk_container_add(GTK_CONTAINER(alignment), box);

	gtk_box_pack_start(GTK_BOX(box), new_scaling_section(), 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), new_image_handling_section(), 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), new_transparency_section(), 0, 0, 0u);

	g_signal_connect(preferences_window, "response", G_CALLBACK(&preferences_response), NULL);
	g_signal_connect(preferences_window, "destroy", G_CALLBACK(&preferences_destroyed), NULL);

	gtk_widget_show_all(preferences_window);
}

int
parse_and_set_scaling_filter(const char *s)
{
#define X(I, V, T) if (!strcasecmp(s, T)) { scaling_filter = V; return 1; }
	LIST_SCALING_FILTERS(X, )
#undef X
	return 0;
}

int
parse_and_set_chequer_size(const char *s)
{
	char *end;
	long int size;
	errno = 0;
	size = strtol(s, &end, 10);
	if (errno || end == s || *end || size < 0 || size > INT_MAX)
		return 0;
	chequer_size = (int)size;
	chequered_background = size != 0;
	return 1;
}

static const char *
parse_chequer_colour(const char *s, GdkColor *colour_out, uint16_t *alpha_out)
{
	int has_precent_alpha = 0;
	size_t i, j, len, digits, nchannels;
	uint16_t mul, channels[] = {0u, 0u, 0u, 65535u};

	if ('0' <= *s && *s <= '9' && strchr(s, '%')) {
		while ('0' <= *s && *s <= '9' && channels[3] < 100u)
			channels[3] = (uint16_t)(channels[3] * 10u + (unsigned)(*s++ - '9'));
		if (channels[3] > 100u || *s != '%')
			return NULL;
		s = &s[1];
		channels[3] = (uint16_t)(655.35 * (double)channels[3]);
		has_precent_alpha = 1;
	}

	if (*s == '#')
		s = &s[1u];
	else if (*s == '0' && (s[1u] == 'x' || s[1u] == 'X'))
		s = &s[2u];

	len = strspn(s, "0123456789abcdefABCDEF");
	switch (len) {
	case 3u:   digits = 1u;  mul = 0x1111u;  nchannels = 3u;  break;
	case 4u:   digits = 1u;  mul = 0x1111u;  nchannels = 4u;  break;
	case 6u:   digits = 2u;  mul = 0x0101u;  nchannels = 3u;  break;
	case 8u:   digits = 2u;  mul = 0x0101u;  nchannels = 4u;  break;
	case 12u:  digits = 4u;  mul = 0x0001u;  nchannels = 3u;  break;
	case 16u:  digits = 4u;  mul = 0x0001u;  nchannels = 4u;  break;
	default:
		return NULL;
	}
	if (has_precent_alpha && nchannels == 4u)
		return NULL;

	for (i = 0u; i < nchannels; i++) {
		for (j = 0u; j < digits; j++, s++) {
			channels[i] <<= 4;
			channels[i] |= (uint16_t)(((int)*s & 15) + (*s > '9' ? 9 : 0));
		}
		channels[i] = (uint16_t)(mul * channels[i]);
	}

	colour_out->red   = channels[0];
	colour_out->green = channels[1];
	colour_out->blue  = channels[2];
	*alpha_out        = channels[3];
	return s;
}

int
parse_and_set_chequer_colours(const char *s)
{
	GdkColor colours[2];
	uint16_t alphas[2];
	s = parse_chequer_colour(s, &colours[0u], &alphas[0u]);
	if (!s)
		return 0;
	if (*s == ':') {
		s = parse_chequer_colour(&s[1u], &colours[1u], &alphas[1u]);
	} else {
		colours[1] = colours[0];
		alphas[1] = alphas[0];
	}
	if (!s || *s != ':')
		return 0;
	chequer_colours[0] = colours[0];
	chequer_colours[1] = colours[1];
	chequer_alphas[0] = alphas[0];
	chequer_alphas[1] = alphas[1];
	chequered_background = 1;
	return 1;
}
