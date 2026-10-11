/* See LICENSE file for copyright and license details. */
#include "common.h"

static void
reload_current_file(void)
{
	if (load_image(get_file(), 0))
		reload_redraw();
}

static void
reset_transformations(void)
{
	enum transformation augment = reset_transformations_without_redraw();
	if (augment != TRANS_NONE)
		transform_redraw(augment);
}

static void
transform(enum transformation augment)
{
	if (add_transformation_without_redraw(augment))
		transform_redraw(augment);
}

static void
set_zoom_fit(GtkToggleAction *action, enum zoom_mode mode)
{
	GdkPixbuf *image;

	if (gtk_toggle_action_get_active(action)) {
		zoom_mode = mode;
		if ((image = get_image()))
			set_zoom_and_recentre(calculate_zoom_mode(image, zoom_mode));
	} else if (zoom_mode == mode) {
		zoom_mode = ZOOM_CUSTOM; /* deselect zoom */
	}

	clear_toggles(GROUP_ZOOM, action);
}

static void
navigate(size_t jump, size_t move)
{
	if (navigate_without_redraw(jump, move))
		navigate_redraw();
}

void
open_action(GtkAction *action, void *user)
{
	GtkFileChooser *chooser;
	(void) action;
	(void) user;
	chooser = construct_chooser();
	add_image_filters(chooser);
	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
		set_images_from_list(gtk_file_chooser_get_filenames(chooser));
	gtk_widget_destroy(GTK_WIDGET(chooser));
}

void
reload_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	reload_current_file();
}

void
print_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	open_print(get_image());
}

void
properties_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	open_properties(get_file());
}

void
close_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	gtk_main_quit();
}

void
fullscreen_action(GtkToggleAction *action, void *user)
{
	int active = gtk_toggle_action_get_active(action);
	(void) user;
	if (active && current_file >= nfiles)
		gtk_toggle_action_set_active(action, 0);
	else
		set_fullscreen_mode(active);
}

void
reset_transformations_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	reset_transformations();
}

void
rotate_clockwise_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_CLOCKWISE);
}

void
rotate_180_degrees_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_180_DEGREES);
}

void
rotate_anticlockwise_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_ANTICLOCKWISE);
}

void
flop_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_FLOP);
}

void
flip_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_FLIP);
}

void
transpose_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_TRANSPOSE);
}

void
antitranspose_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	transform(TRANS_ANTITRANSPOSE);
}

void
invert_action(GtkToggleAction *action, void *user)
{
	enum transformation tr;
	(void) user;
	tr = gtk_toggle_action_get_active(action) ? TRANS_NEGATIVE : TRANS_NONE;
	transform(tr ^ (trans & TRANS_NEGATIVE));
}

void
zoom_in_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	clear_toggles(GROUP_ZOOM, NULL);
	set_zoom_around_centre(zoom_in(zoom));
}

void
zoom_out_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	clear_toggles(GROUP_ZOOM, NULL);
	set_zoom_around_centre(zoom_out(zoom));
}

void
normal_size_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, ZOOM_NORMAL);
}

void
width_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, ZOOM_FIT_WIDTH);
}

void
height_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, ZOOM_FIT_HEIGHT);
}

void
window_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, ZOOM_FIT_WINDOW);
}

void
best_fit_action(GtkToggleAction *action, void *user)
{
	(void) user;
	set_zoom_fit(action, ZOOM_BEST_FIT);
}

void
next_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(current_file, +(size_t)1);
}

void
previous_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(current_file ? current_file : nfiles, -(size_t)1);
}

void
first_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(0u, 0u);
}

void
last_action(GtkAction *action, void *user)
{
	(void) action;
	(void) user;
	navigate(nfiles, -(size_t)1);
}

int
key_pressed(GtkWidget *widget, GdkEventKey *event, void *user)
{
	GtkAdjustment *hadjustment, *vadjustment;
	int left, right, up, down, amount;
	double hamount, vamount;

	(void) widget;
	(void) user;

	if (event->keyval == GDK_Escape) {
		deactivate_fullscreen(NULL);
		return 1;
	}

	if (event->state & GDK_CONTROL_MASK)
		return 0;

	amount = (event->state & GDK_MOD1_MASK) ? 1 : SCROLL_AMOUNT;
	hamount = (double)amount;
	vamount = (double)amount;
	if (event->state & GDK_SHIFT_MASK) {
		hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
		vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
		hamount = 0.25 * hadjustment->page_size;
		vamount = 0.25 * vadjustment->page_size;
	}

	left  = event->keyval == GDK_Left;
	right = event->keyval == GDK_Right;
	up    = event->keyval == GDK_Up;
	down  = event->keyval == GDK_Down;

	if (left | right | up | down) {
		scroll_by(right * hamount - left * hamount,
		          down  * vamount -   up * vamount);
		return 1;
	}

	return 0;
}

int
scroll_event(GtkWidget *widget, GdkEventScroll *event, void *user)
{
	GtkAdjustment *hadjustment = logical_scroll_pane_get_hadjustment(LOGICAL_SCROLL_PANE(scroll));
	GtkAdjustment *vadjustment = logical_scroll_pane_get_vadjustment(LOGICAL_SCROLL_PANE(scroll));
	double hamount = hadjustment->step_increment;
	double vamount = vadjustment->step_increment;

	int shift = !!(event->state & GDK_SHIFT_MASK);
	int ctrl  = !!(event->state & GDK_CONTROL_MASK);
	int up    = event->direction == GDK_SCROLL_UP;
	int down  = event->direction == GDK_SCROLL_DOWN;
	int left  = event->direction == GDK_SCROLL_LEFT;
	int right = event->direction == GDK_SCROLL_RIGHT;

	int x0, y0;

	(void) widget;
	(void) user;

	if (shift) {
		left = up, up = 0;
		right = down, down = 0;
	}

	if (!ctrl) {
		/* needed for shift+scroll to work */
		scroll_by(right * hamount - left * hamount,
		          down  * vamount - up   * vamount);
	} else if (up | down) {
		gdk_window_get_origin(display_area->window, &x0, &y0);
		clear_toggles(GROUP_ZOOM, NULL);
		set_zoom_around_point(up ? zoom_in(zoom) : zoom_out(zoom), event->x_root - x0, event->y_root - y0);
	} else {
		return 0;
	}

	return 1;
}
