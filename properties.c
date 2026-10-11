/* See LICENSE file for copyright and license details. */
#include "common.h"

void
open_properties(struct file *file)
{
	/* TODO What shall "Properties" display? */

	const char *name;
	GtkWidget *dialog;

	if (!file || !file->path || !file->image)
		return;

	name = strrchr(file->path, '/');
	name = name ? &name[1u] : file->path;

	dialog = gtk_message_dialog_new(GTK_WINDOW(window), 0, GTK_MESSAGE_INFO,
	                                GTK_BUTTONS_CLOSE, "%s", name);

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
	                                         "%i × %i pixels",
	                                         gdk_pixbuf_get_width(file->image),
	                                         gdk_pixbuf_get_height(file->image));

	g_signal_connect_swapped(dialog, "response", G_CALLBACK(&gtk_widget_destroy), dialog);
	gtk_widget_show(dialog);
}
