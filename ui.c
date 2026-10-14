/* See LICENSE file for copyright and license details. */
#include "common.h"

#define LIST_MENU(TOPLEVEL, SEPARATOR, ACTION, TOGGLE, D)\
	TOPLEVEL(0, "", "File", D)\
	ACTION(0, 0u, "", "Open", GTK_STOCK_OPEN, "<control>0", "Ctrl+0", open_action, D)\
	ACTION(0, 1u, "", "Reload", GTK_STOCK_REFRESH, "F5", "F5", reload_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 1u, "", "Print", GTK_STOCK_PRINT, "<control>P", "Ctrl+P", print_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 1u, "Proper", "ties", GTK_STOCK_PROPERTIES, "<alt>Return", "Alt+Return", properties_action, D)\
	ACTION(0, 0u, "Pr", "eferences", GTK_STOCK_PREFERENCES, NULL, "", preferences_action, D)\
	SEPARATOR(0, D)\
	ACTION(0, 0u, "", "Close", GTK_STOCK_CLOSE, "<control>W", "Ctrl+W", close_action, D)\
	TOPLEVEL(0, "", "View", D)\
	TOGGLE(1, 1u, "F", "ullscreen", GTK_STOCK_FULLSCREEN, "F11", "F11", fullscreen_action, GROUP_NONE, 0, D)\
	TOGGLE(0, 1u, "", "Property pane", GTK_STOCK_PROPERTIES, "F9", "F9", property_pane_action, GROUP_NONE, 0, D)\
	TOGGLE(0, 0u, "Image ", "collection", GTK_STOCK_INDEX, "<control>F9", "Ctrl+F9", collection_pane_action, GROUP_NONE, 0, D)\
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
	ACTION(1, 1u, "", "Zoom in", "zoom-in", "plus", "+", zoom_in_action, D)\
	ACTION(1, 1u, "Zoom ", "out", "zoom-out", "minus", "-", zoom_out_action, D)\
	ACTION(1, 1u, "", "Normal size", "zoom-original", "1", "1", normal_size_action, D)\
	TOGGLE(1, 1u, "Fit to ", "width", "zoom-fit-width", "<control>H", "Ctrl+H", width_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "Fit to ", "height", "zoom-fit-height", "<control>V", "Ctrl+V", height_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "", "Fit to window", "zoom-fit-page", "<control><shift>F", "Ctrl+Shift+F", window_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(1, 1u, "", "Best fit", "zoom-fit-best", "<control>F", "Ctrl+F", best_fit_action, GROUP_ZOOM, 0, D)\
	TOGGLE(0 /* TODO there is no good icon */, 1u, "", "Stretch to window", NULL, "S", "S", stretch_action, GROUP_ZOOM, 0, D)\
	TOPLEVEL(1, "", "Go", D)\
	ACTION(1, 2u, "", "Previous image", GTK_STOCK_GO_BACK, "<control>Left", "Ctrl+Left", previous_action, D)\
	ACTION(1, 2u, "", "Next image", GTK_STOCK_GO_FORWARD, "<control>Right", "Ctrl+Right", next_action, D)\
	SEPARATOR(0, D)\
	ACTION(1, 2u, "", "First image", GTK_STOCK_GOTO_FIRST, "<control>Home", "Ctrl+Home", first_action, D)\
	ACTION(1, 2u, "", "Last image", GTK_STOCK_GOTO_LAST, "<control>End", "Ctrl+End", last_action,)

GtkWidget *window;
GtkWidget *display_area;
GtkWidget *scroll;
GtkToggleAction *fullscreen;

static GtkWidget *statusbar;
static GtkWidget *menubar;
static GtkWidget *toolbar;
static GtkWidget *property_pane;
static GtkActionGroup *actions;
static GdkColor fullscreen_background = {0u, 0u, 0u, 0u};

#define X_SKIP(...)
#define X_TOPLEVEL(T, A, B, ...)                                    {A B, NULL, A "_" B, NULL, NULL, NULL} __VA_ARGS__
#define X_ACTION(T, N, A, B, ICON, ACCEL, ATEXT, F, ...)            {A B, ICON, A "_" B, ACCEL, NULL, G_CALLBACK(&F)} __VA_ARGS__
#define X_TOGGLE(T, N, A, B, ICON, ACCEL, ATEXT, F, G, ACTIVE, ...) {A B, ICON, A "_" B, ACCEL, NULL, G_CALLBACK(&F), ACTIVE} __VA_ARGS__

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

static GtkAction *
get_action(const char *name)
{
	return gtk_action_group_get_action(actions, name);
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
window_state_changed(GtkWidget *widget, GdkEventWindowState *event, void *user)
{
	(void) widget;
	(void) user;
	gtk_toggle_action_set_active(fullscreen, !!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN));
	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
		g_idle_add(&recalculate_zoom_mode, NULL);
	return 0;
}

static int
display_area_button_pressed(GtkWidget *widget, GdkEventButton *event, void *user)
{
	(void) event;
	(void) user;
	gtk_widget_grab_focus(widget);
	return 0;
}

void
set_up_ui(void)
{
	GtkAccelGroup *accelerators;
	GtkWidget *box, *paned, *content;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "gbild");
	gtk_window_set_icon_name(GTK_WINDOW(window), "image-viewer");
	gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
	gtk_window_set_wmclass(GTK_WINDOW(window), "gbild", "gbild");

	g_signal_connect(window, "destroy", G_CALLBACK(&gtk_main_quit), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(&key_pressed), NULL);
	g_signal_connect(window, "window-state-event", G_CALLBACK(&window_state_changed), NULL);

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
	display_area = gtk_drawing_area_new();
	gtk_widget_set_can_focus(display_area, 1);
	gtk_widget_set_app_paintable(display_area, 1);
	scroll = logical_scroll_pane_new();
	logical_scroll_pane_set_child(LOGICAL_SCROLL_PANE(scroll), display_area);
	gtk_widget_add_events(scroll, GDK_SCROLL_MASK);
	paned = gtk_hpaned_new();
	property_pane = gtk_vbox_new(0, 0);
	gtk_widget_set_no_show_all(property_pane, 1);
	gtk_paned_pack1(GTK_PANED(paned), property_pane, 0, 1);
	gtk_paned_pack2(GTK_PANED(paned), scroll, 1, 0);
	gtk_paned_set_position(GTK_PANED(paned), INITIAL_PROPERTY_PANE_HEIGHT);
	content = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(content), paned, 1, 1, 0u);
	gtk_box_pack_end(GTK_BOX(content), create_collection_pane(), 0, 0, 0u);
	gtk_box_pack_start(GTK_BOX(box), content, 1, 1, 0u);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(box), statusbar, 0, 0, 0u);

	gtk_widget_add_events(display_area, GDK_BUTTON_PRESS_MASK |
	                                    GDK_BUTTON_RELEASE_MASK |
	                                    GDK_POINTER_MOTION_MASK);
	g_signal_connect(display_area, "button-press-event", G_CALLBACK(&display_area_button_pressed), NULL);
	g_signal_connect(display_area, "expose-event", G_CALLBACK(&expose_image), NULL);
	g_signal_connect(scroll, "scroll-event", G_CALLBACK(&scroll_event), NULL);
}

void
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

void
clear_toggles(enum toggle_group group, GtkToggleAction *except)
{
	static int clearing_toggles = 0;

	GtkToggleAction *action;

	if (clearing_toggles)
		return;

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

void
set_fullscreen_mode(int active)
{
	gtk_widget_modify_bg(display_area, GTK_STATE_NORMAL, active ? &fullscreen_background : NULL);
	gtk_widget_queue_draw(display_area);
	gtk_widget_set_visible(menubar, !active);
	gtk_widget_set_visible(toolbar, !active);
	gtk_widget_set_visible(statusbar, !active);
	logical_scroll_pane_set_hscrollbar_visible(LOGICAL_SCROLL_PANE(scroll), !active);
	logical_scroll_pane_set_vscrollbar_visible(LOGICAL_SCROLL_PANE(scroll), !active);

	if (active)
		gtk_window_fullscreen(GTK_WINDOW(window));
	else
		gtk_window_unfullscreen(GTK_WINDOW(window));
}

void
set_visible_panes(int property, int collection)
{
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(get_action("Property pane")), property);
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(get_action("Image collection")), collection);
}

void
set_zoom_mode_toggle(void)
{
	const char *action = NULL;

#define X(E, A, O, T, ...) if (zoom_mode == E) action = T
	LIST_ZOOM_MODES(X, ;);
#undef X

	if (action)
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(get_action(action)), 1);
}

void
set_property_pane_visible(int visible)
{
	struct file *file;
	GtkWidget *properties;
	GList *children, *child;

	if (!visible) {
		gtk_widget_hide(property_pane);
		g_idle_add(&recalculate_zoom_mode, NULL);
		return;
	}

	children = gtk_container_get_children(GTK_CONTAINER(property_pane));
	for (child = children; child; child = child->next)
		gtk_widget_destroy(child->data);
	g_list_free(children);
	file = get_file();
	if (file && file->path) {
		properties = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(properties), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(property_pane), properties, 1, 1, 0);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(properties), create_property_table(file));
		gtk_widget_show_all(properties);
	}
	gtk_widget_show(property_pane);
	g_idle_add(&recalculate_zoom_mode, NULL);
}

void
update_property_pane(void)
{
	if (gtk_widget_get_visible(property_pane))
		set_property_pane_visible(1);
}

int
activate_fullscreen(void *user)
{
	(void) user;
	if (!gtk_toggle_action_get_active(fullscreen))
		gtk_toggle_action_set_active(fullscreen, 1);
	return 0;
}

int
deactivate_fullscreen(void *user)
{
	(void) user;
	if (gtk_toggle_action_get_active(fullscreen))
		gtk_toggle_action_set_active(fullscreen, 0);
	return 0;
}

void
set_statusbar_message(const char *message)
{
	gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0u);
	if (message)
		gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0u, message);
}

void
free_ui(void)
{
	g_object_unref(actions);
}
