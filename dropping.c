/* See LICENSE file for copyright and license details. */
#include "common.h"

static char url_list_name[] = "text/uri-list";
static GtkTargetEntry url_list = {url_list_name, 0u, 0u};

static size_t
get_dropped_files(struct file **files_out, char **uris)
{
	GError *error = NULL;
	char **uri, *path;
	struct file *dropped_files = NULL;
	size_t i = 0u;

	if (!uris)
		goto out;

	for (uri = uris; *uri; uri++)
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
		/* the other fields are set in the next code block */
	}

out:
	*files_out = dropped_files;
	return i;
}

static void
drop_files(GtkWidget *widget, GdkDragContext *context, int x, int y,
           GtkSelectionData *data, unsigned int info, unsigned int time, void *user)
{
	char **uris = NULL;
	struct file *dropped_files;
	size_t n;
	int accepted = 1;
	GdkModifierType modifiers;
	GdkWindow *event_window;

	(void) x;
	(void) y;
	(void) info;
	(void) user;

	if (gtk_drag_get_source_widget(context) == display_area) {
		accepted = 0;
		goto out;
	}
	event_window = gtk_widget_get_window(widget);
	if (!event_window)
		goto out;
	if (!gdk_window_get_pointer(event_window, NULL, NULL, &modifiers))
		goto out;

	uris = gtk_selection_data_get_uris(data);
	n = get_dropped_files(&dropped_files, uris);
	if (!n)
		goto out;

	if ((modifiers & GDK_CONTROL_MASK) && nfiles) {
		n = files_from_array(dropped_files, n);
		add_images(dropped_files, n);
		free(dropped_files);
	} else {
		set_images_from_array(dropped_files, n);
	}

out:
	gtk_drag_finish(context, accepted, 0, time);
	if (uris)
		g_strfreev(uris);
}

static int
drop_motion(GtkWidget *widget, GdkDragContext *context, int x, int y, unsigned int time, void *user)
{
	GdkWindow *event_window = gtk_widget_get_window(widget);
	GdkModifierType modifiers;
	GdkDragAction action = 0;

	(void) x;
	(void) y;
	(void) user;

	if (!gtk_get_current_event_state(&modifiers))
		if (!event_window || !gdk_window_get_pointer(event_window, NULL, NULL, &modifiers))
			goto out;
	if (gtk_drag_dest_find_target(widget, context, NULL) == GDK_NONE)
		goto out;

	action = (modifiers & GDK_CONTROL_MASK) ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
	if (!(gdk_drag_context_get_actions(context) & action))
		 action = 0;

out:
	gdk_drag_status(context, action, time);
	return 1;
}

void
set_up_dropping(void)
{
	gtk_drag_dest_set(window, GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
	                  &url_list, 1u, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(window, "drag-motion", G_CALLBACK(&drop_motion), NULL);
	g_signal_connect(window, "drag-data-received", G_CALLBACK(&drop_files), NULL);
}
