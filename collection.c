/* See LICENSE file for copyright and license details. */
#include "common.h"

GtkWidget *collection_view;

static GtkWidget *collection_pane;
static GtkCellRenderer *collection_renderer;
static GtkListStore *collection_model;
static int collection_height = -1;
static size_t popup_file;

static char url_list_name[] = "text/uri-list";
static GtkTargetEntry url_list = {url_list_name, 0u, 0u};

enum {
	COLLECTION_THUMBNAIL,
	COLLECTION_INDEX,
	COLLECTION_COLUMNS
};

static void
free_paths(GList *paths)
{
	GList *path;
	for (path = paths; path; path = path->next)
		gtk_tree_path_free(path->data);
	g_list_free(paths);
}

static int
collection_thumbnail_size(void)
{
	return MAX(1, collection_view->allocation.height - 16);
}

static GdkPixbuf *
create_thumbnail(struct file *file, int maximum_size)
{
	GdkPixbuf *thumbnail = NULL;
	int width = file->image.width;
	int height = file->image.height;
	double w = (double)width;
	double h = (double)height;
	double s = (double)maximum_size / MAX(w, h);
	s = MIN(1.0, s);
	width = (int)(s * w);
	height = (int)(s * h);
	width = MAX(1, width);
	height = MAX(1, height);
	if (file->image.vector) {
		thumbnail = rasterise(file->image.vector, width, height);
		if (thumbnail)
			return thumbnail;
	}
	if (file->image.raster)
		return gdk_pixbuf_scale_simple(file->image.raster, width, height, GDK_INTERP_BILINEAR);
	return NULL;
}

static void
populate_collection(void)
{
	GtkTreeIter iter;
	GdkPixbuf *thumbnail;
	size_t i;
	int thumbnail_size = collection_thumbnail_size();
	int item_size = thumbnail_size + 2 * gtk_icon_view_get_item_padding(GTK_ICON_VIEW(collection_view));
	gtk_icon_view_set_columns(GTK_ICON_VIEW(collection_view), nfiles ? (int)nfiles : 1);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(collection_view), item_size);
	gtk_cell_renderer_set_fixed_size(collection_renderer, thumbnail_size, thumbnail_size);
	gtk_list_store_clear(collection_model);
	for (i = 0u; i < nfiles; i++) {
		thumbnail = create_thumbnail(&files[i], thumbnail_size);
		if (!thumbnail)
			continue;
		gtk_list_store_append(collection_model, &iter);
		gtk_list_store_set(collection_model, &iter, COLLECTION_THUMBNAIL,
		                   thumbnail, COLLECTION_INDEX, (unsigned long)i, -1);
		g_object_unref(thumbnail);
	}
}

static void
select_current_file(void)
{
	GtkTreeIter iter;
	unsigned long index;
	GtkTreePath *path;
	if (!nfiles || !gtk_tree_model_get_iter_first(GTK_TREE_MODEL(collection_model), &iter))
		return;
	do {
		gtk_tree_model_get(GTK_TREE_MODEL(collection_model), &iter, COLLECTION_INDEX, &index, -1);
		if ((size_t)index == current_file) {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(collection_model), &iter);
			gtk_icon_view_select_path(GTK_ICON_VIEW(collection_view), path);
			gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(collection_view), path, 1, 0.5, 0.5);
			gtk_tree_path_free(path);
			return;
		}
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(collection_model), &iter));
}

static int
get_selected_index(size_t *index)
{
	GList *paths;
	GtkTreeIter iter;
	unsigned long value;
	int found = 0;
	paths = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(collection_view));
	if (!paths)
		return 0;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(collection_model), &iter, paths->data)) {
		gtk_tree_model_get(GTK_TREE_MODEL(collection_model), &iter, COLLECTION_INDEX, &value, -1);
		*index = (size_t)value;
		found = 1;
	}
	free_paths(paths);
	return found;
}

static void
collection_size_allocated(GtkWidget *widget, GtkAllocation *allocation, void *user)
{
	(void) widget;
	(void) user;
	if (allocation->height == collection_height)
		return;
	collection_height = allocation->height;
	populate_collection();
	select_current_file();
}

static int
collection_index_at(int x, int y, size_t *index)
{
	GtkTreeIter iter;
	unsigned long value;
	int found = 0;
	GtkTreePath *path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(collection_view), x, y);
	if (!path)
		return 0;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(collection_model), &iter, path)) {
		gtk_tree_model_get(GTK_TREE_MODEL(collection_model), &iter, COLLECTION_INDEX, &value, -1);
		*index = (size_t)value;
		found = 1;
	}
	gtk_tree_path_free(path);
	return found;
}

void
navigate_collection_drag(void)
{
	size_t index;
	if (get_selected_index(&index) && navigate_without_redraw(index, 0u))
		navigate_redraw();
}

static void
collection_selection_changed(GtkIconView *view, void *user)
{
	(void) view;
	(void) user;
	navigate_collection_drag();
}

static void
reorder_file(size_t source, size_t target)
{
	struct file moved;
	if (target >= nfiles)
		target = nfiles - 1u;
	if (!nfiles || source >= nfiles || source == target)
		return;
	moved = files[source];
	if (source < target) {
		memmove(&files[source], &files[source + 1u], (target - source) * sizeof(*files));
		if (current_file == source)
			current_file = target;
		else if (current_file > source && current_file <= target)
			current_file -= 1u;
	} else {
		memmove(&files[target + 1u], &files[target], (source - target) * sizeof(*files));
		if (current_file == source)
			current_file = target;
		else if (current_file >= target && current_file < source)
			current_file += 1u;
	}
	files[target] = moved;
	collection_files_changed();
	navigate_redraw();
}

static void
drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data,
              unsigned int info, unsigned int time, void *user)
{
	GError *error = NULL;
	size_t index;
	char *uri;
	(void) widget;
	(void) context;
	(void) info;
	(void) time;
	(void) user;
	if (!get_selected_index(&index) || index >= nfiles || !files[index].path)
		return;
	uri = g_filename_to_uri(files[index].path, NULL, &error);
	if (!uri) {
		weprintf("g_filename_to_uri: %s", error->message);
		g_error_free(error);
		return;
	}
	gtk_selection_data_set_uris(data, (char *[]){uri, NULL});
	g_free(uri);
}

static void
drag_data_received(GtkWidget *widget, GdkDragContext *context, int x, int y,
                   GtkSelectionData *data, unsigned int info, unsigned int time, void *user)
{
	struct file *dropped_files;
	char **uris;
	size_t source_index, n;
	GtkTreePath *path = NULL;
	GtkWidget *source = gtk_drag_get_source_widget(context);
	size_t target;
	(void) info;
	(void) user;
	if (source == collection_view) {
		if (get_selected_index(&source_index))
			goto reorder;
	} else if (source == display_area) {
		if (source == display_area)
			source_index = current_file;
	reorder:
		if (gtk_icon_view_get_dest_item_at_pos(GTK_ICON_VIEW(widget), x, y, &path, NULL))
			target = (size_t)gtk_tree_path_get_indices(path)[0];
		else
			target = nfiles;
		if (path)
			gtk_tree_path_free(path);
		reorder_file(source_index, target);
	} else {
		uris = gtk_selection_data_get_uris(data);
		n = get_dropped_files(&dropped_files, uris);
		if (n) {
			add_images(dropped_files, files_from_array(dropped_files, n));
			free(dropped_files);
		}
		if (uris)
			g_strfreev(uris);
	}
	gtk_drag_finish(context, 1, 0, time);
}

static void
remove_from_collection(GtkMenuItem *item, void *user)
{
	struct file removed;
	(void) item;
	(void) user;
	if (popup_file >= nfiles)
		return;
	removed = files[popup_file];
	if (popup_file + 1u < nfiles)
		memmove(&files[popup_file], &files[popup_file + 1u], (nfiles - (popup_file + 1u)) * sizeof(*files));
	nfiles -= 1u;
	destroy_file(&removed);
	forget_transformed_raster();
	if (current_file > popup_file)
		current_file -= 1u;
	else if (current_file >= nfiles)
		current_file = nfiles ? nfiles - 1u : 0u;
	collection_files_changed();
	navigate_redraw();
	set_actions_sensitive();
}

static int
collection_button_pressed(GtkWidget *widget, GdkEventButton *event, void *user)
{
	GtkWidget *menu, *item;
	size_t index;
	(void) widget;
	(void) user;
	if (event->button != 3u || !collection_index_at((int)event->x, (int)event->y, &index))
		return 0;
	popup_file = index;
	menu = gtk_menu_new();
	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	gtk_menu_item_set_label(GTK_MENU_ITEM(item), "_Remove from collection");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(&remove_from_collection), NULL);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
	return 1;
}

GtkWidget *
create_collection_pane(void)
{
	collection_pane = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(collection_pane), GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
	gtk_widget_set_size_request(collection_pane, -1, INITIAL_COLLECTION_PANE_HEIGHT);
	gtk_widget_set_no_show_all(collection_pane, 1);

	collection_model = gtk_list_store_new(COLLECTION_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_ULONG);
	collection_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(collection_model));
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(collection_view), GTK_SELECTION_SINGLE);
	gtk_icon_view_set_orientation(GTK_ICON_VIEW(collection_view), GTK_ORIENTATION_HORIZONTAL);

	collection_renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(collection_renderer, "xalign", 0.5, "yalign", 0.5, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(collection_view), collection_renderer, 0);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(collection_view), collection_renderer, "pixbuf", COLLECTION_THUMBNAIL);
	gtk_container_add(GTK_CONTAINER(collection_pane), collection_view);

	gtk_widget_add_events(collection_view, GDK_BUTTON_PRESS_MASK);
	gtk_drag_source_set(collection_view, GDK_BUTTON1_MASK, &url_list, 1u, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set(collection_view, GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
	                  &url_list, 1u, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(collection_view, "size-allocate", G_CALLBACK(&collection_size_allocated), NULL);
	g_signal_connect(collection_view, "selection-changed", G_CALLBACK(&collection_selection_changed), NULL);
	g_signal_connect(collection_view, "button-press-event", G_CALLBACK(&collection_button_pressed), NULL);
	g_signal_connect(collection_view, "drag-data-get", G_CALLBACK(&drag_data_get), NULL);
	g_signal_connect(collection_view, "drag-data-received", G_CALLBACK(&drag_data_received), NULL);

	return collection_pane;
}

void
set_collection_pane_visible(int visible)
{
	if (visible) {
		gtk_widget_show_all(collection_view);
		gtk_widget_show(collection_pane);
		populate_collection();
		select_current_file();
		gtk_widget_grab_focus(collection_view);
	} else {
		gtk_widget_hide(collection_pane);
		gtk_widget_grab_focus(display_area);
	}
	g_idle_add(&recalculate_zoom_mode, NULL);
}

void
collection_files_changed(void)
{
	if (collection_pane && gtk_widget_get_visible(collection_pane)) {
		populate_collection();
		select_current_file();
	}
}

void
update_collection(void)
{
	if (collection_pane && gtk_widget_get_visible(collection_pane))
		select_current_file();
}
