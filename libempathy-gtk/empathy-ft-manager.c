/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003, 2004 Xan Lopez
 * Copyright (C) 2007 Marco Barisione <marco@barisione.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* The original file transfer manager code was copied from Epiphany */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/libgnomeui.h>

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-file.h>
#include <libempathy/empathy-file.h>
#include <libempathy/empathy-utils.h>

#include "empathy-conf.h"
#include "empathy-ft-manager.h"
#include "empathy-ui-utils.h"
#include "empathy-geometry.h"
#include "empathy-images.h"


/**
 * SECTION:empathy-ft-manager
 * @short_description: File transfer dialog
 * @see_also: #EmpathyTpFile, #EmpathyFile, empathy_send_file(),
 * empathy_send_file_from_stream()
 * @include: libempthy-gtk/empathy-ft-manager.h
 *
 * The #EmpathyFTManager object represents the file transfer dialog,
 * it can show multiple file transfers at the same time (added
 * with empathy_ft_manager_add_file()).
 */

enum
{
	COL_PERCENT,
	COL_IMAGE,
	COL_MESSAGE,
	COL_REMAINING,
	COL_FT_OBJECT
};

enum
{
	PROGRESS_COL_POS,
	FILE_COL_POS,
	REMAINING_COL_POS
};

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManagerPriv))

/**
 * EmpathyFTManagerPriv:
 *
 * Private fields of the #EmpathyFTManager class.
 */
struct _EmpathyFTManagerPriv
{
	GtkTreeModel *model;
	GHashTable *file_to_row_ref;

	/* Widgets */
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *open_button;
	GtkWidget *abort_button;

	guint save_geometry_id;
};

enum
{
	RESPONSE_OPEN  = 1,
	RESPONSE_STOP  = 2,
	RESPONSE_CLEAR = 3
};

static void empathy_ft_manager_class_init    (EmpathyFTManagerClass *klass);
static void empathy_ft_manager_init          (EmpathyFTManager      *ft_manager);
static void empathy_ft_manager_finalize      (GObject               *object);

static void ft_manager_build_ui              (EmpathyFTManager      *ft_manager);
static void ft_manager_response_cb           (GtkWidget             *dialog,
					      gint                   response,
					      EmpathyFTManager      *ft_manager);
static void ft_manager_add_file_to_list      (EmpathyFTManager      *ft_manager,
					      EmpathyFile           *file);
static void ft_manager_remove_file_from_list (EmpathyFTManager      *ft_manager,
					      EmpathyFile           *file);
static void ft_manager_display_accept_dialog (EmpathyFTManager      *ft_manager,
					      EmpathyFile           *file);

G_DEFINE_TYPE (EmpathyFTManager, empathy_ft_manager, G_TYPE_OBJECT);

static void
empathy_ft_manager_class_init (EmpathyFTManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = empathy_ft_manager_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyFTManagerPriv));
}

static void
empathy_ft_manager_init (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;

	priv = GET_PRIV (ft_manager);

	priv->file_to_row_ref = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
						     (GDestroyNotify) gtk_tree_row_reference_free);

	ft_manager_build_ui (ft_manager);
}

static void
empathy_ft_manager_finalize (GObject *object)
{
	EmpathyFTManagerPriv *priv;

	DEBUG ("Finalizing: %p", object);

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->file_to_row_ref);

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	G_OBJECT_CLASS (empathy_ft_manager_parent_class)->finalize (object);
}

EmpathyFTManager *
empathy_ft_manager_get_default (void)
{
	static EmpathyFTManager *manager;

	if (!manager)
		manager = empathy_ft_manager_new ();

	return manager;
}

/**
 * empathy_ft_manager_new:
 *
 * Creates a new #EmpathyFTManager.
 *
 * Returns: a new #EmpathyFTManager
 */
EmpathyFTManager *
empathy_ft_manager_new (void)
{
	return g_object_new (EMPATHY_TYPE_FT_MANAGER, NULL);
}

/**
 * empathy_ft_manager_add_file:
 * @ft_manager: an #EmpathyFTManager
 * @ft: an #EmpathyFT
 *
 * Adds a file transfer to the file transfer manager dialog @ft_manager.
 * The manager dialog then shows the progress and other information about
 * @ft.
 */
void
empathy_ft_manager_add_file (EmpathyFTManager *ft_manager,
			     EmpathyFile      *file)
{
	EmpFileTransferState state;

	g_return_if_fail (EMPATHY_IS_FT_MANAGER (ft_manager));
	g_return_if_fail (EMPATHY_IS_FILE (file));

	DEBUG ("Adding a file transfer: contact=%s, filename=%s",
	       empathy_contact_get_name (empathy_file_get_contact (file)),
	       empathy_file_get_filename (file));

	state = empathy_file_get_state (file);
	if (state == EMP_FILE_TRANSFER_STATE_LOCAL_PENDING)
		ft_manager_display_accept_dialog (ft_manager, file);
	else
		ft_manager_add_file_to_list (ft_manager, file);
}

/**
 * empathy_ft_manager_get_dialog:
 * @ft_manager: an #EmpathyFTManager
 *
 * Returns the #GtkWidget of @ft_manager.
 *
 * Returns: the dialog
 */
GtkWidget *
empathy_ft_manager_get_dialog (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_FT_MANAGER (ft_manager), NULL);

	priv = GET_PRIV (ft_manager);

	return priv->window;
}

static gchar *
format_interval (gint interval)
{
	gint hours, mins, secs;

	hours = interval / 3600;
	interval -= hours * 3600;
	mins = interval / 60;
	interval -= mins * 60;
	secs = interval;

	if (hours > 0)
		return g_strdup_printf (_("%u:%02u.%02u"), hours, mins, secs);
	else
		return g_strdup_printf (_("%02u.%02u"), mins, secs);
}

static GtkTreeRowReference *
get_row_from_file (EmpathyFTManager *ft_manager, EmpathyFile *file)
{
	EmpathyFTManagerPriv *priv;

	priv = GET_PRIV (ft_manager);

	return g_hash_table_lookup (priv->file_to_row_ref, file);
}

static void
update_buttons (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	GtkTreeSelection     *selection;
	GtkTreeModel         *model;
	GtkTreeIter           iter;
	GValue                val = {0, };
	EmpathyFile          *file;
	gboolean              open_enabled = FALSE;
	gboolean              abort_enabled = FALSE;

	priv = GET_PRIV (ft_manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get_value (model, &iter, COL_FT_OBJECT, &val);
		file = g_value_get_object (&val);
		g_value_unset (&val);

		if (empathy_file_get_state (file) == EMP_FILE_TRANSFER_STATE_COMPLETED) {
			if (empathy_file_get_direction (file) ==
			    EMP_FILE_TRANSFER_DIRECTION_INCOMING)
				open_enabled = TRUE;
			else
				open_enabled = FALSE;
			abort_enabled = FALSE;
		} else if (empathy_file_get_state (file) ==
			   EMP_FILE_TRANSFER_STATE_CANCELED) {
			open_enabled = FALSE;
			abort_enabled = FALSE;
		} else {
			open_enabled = FALSE;
			abort_enabled = TRUE;
		}
	}

	gtk_widget_set_sensitive (priv->open_button, open_enabled);
	gtk_widget_set_sensitive (priv->abort_button, abort_enabled);
}

/*static const gchar *
get_state_change_reason_description (EmpFileTransferStateChangeReason reason)
{
	switch (reason) {
	case EMP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE:
		return _("File transfer not completed");
	case EMP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_STOPPED:
		return _("You canceled the file transfer");
	case EMP_FILE_TRANSFER_STATE_CHANGE_REASON_REMOTE_STOPPED:
		return _("The other participant canceled the file transfer");
	case EMP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_ERROR:
		return _("Error while trying to transfer the file");
	case EMP_FILE_TRANSFER_STATE_CHANGE_REASON_REMOTE_ERROR:
		return _("The other participant is unable to transfer the file");
	default:
		g_return_val_if_reached ("");
	}
}*/

static void
update_ft_row (EmpathyFTManager *ft_manager,
	       EmpathyFile      *file)
{
	EmpathyFTManagerPriv *priv;
	GtkTreeRowReference  *row_ref;
	GtkTreePath          *path;
	GtkTreeIter           iter;
	const gchar          *filename;
	const gchar          *contact_name;
	gchar                *msg;
	gchar                *remaining_str;
	gchar                *first_line_format;
	gchar                *first_line;
	gchar                *second_line;
	guint64               transferred_bytes;
	guint64               total_size;
	gint                  remaining = -1;
	gint                  percent;
	EmpFileTransferState  state;

	priv = GET_PRIV (ft_manager);

	row_ref = get_row_from_file (ft_manager, file);
	g_return_if_fail (row_ref != NULL);

	filename = empathy_file_get_filename (file);
	contact_name = empathy_contact_get_name (empathy_file_get_contact (file));
	transferred_bytes = empathy_file_get_transferred_bytes (file);
	total_size = empathy_file_get_size (file);
	state = empathy_file_get_state (file);

	/* The state is changed asynchronously, so we can get local pending
	 * transfers just before their state is changed to open.
	 * Just treat them as open file transfers. */
	if (state == EMP_FILE_TRANSFER_STATE_LOCAL_PENDING)
		state = EMP_FILE_TRANSFER_STATE_OPEN;

	switch (state)
	{
	case EMP_FILE_TRANSFER_STATE_REMOTE_PENDING:
	case EMP_FILE_TRANSFER_STATE_OPEN:
		if (empathy_file_get_direction (file) ==
		    EMP_FILE_TRANSFER_DIRECTION_INCOMING)
			/* translators: first %s is filename, second %s is the contact name */
			first_line_format = _("Receiving \"%s\" from %s");
		else
			/* translators: first %s is filename, second %s is the contact name */
			first_line_format = _("Sending \"%s\" to %s");
		first_line = g_strdup_printf (first_line_format, filename, contact_name);

		if (state == EMP_FILE_TRANSFER_STATE_OPEN) {
			gchar *total_size_str;
			gchar *transferred_bytes_str;

			if (total_size == EMPATHY_FILE_UNKNOWN_SIZE)
				/* translators: the text before the "|" is context to
				 * help you decide on the correct translation. You MUST
				 * OMIT it in the translated string. */
				total_size_str = g_strdup (Q_("file size|Unknown"));
			else
				total_size_str = gnome_vfs_format_file_size_for_display (total_size);

			transferred_bytes_str = gnome_vfs_format_file_size_for_display (transferred_bytes);

			/* translators: first %s is the transferred size, second %s is
			 * the total file size */
			second_line = g_strdup_printf (_("%s of %s"), transferred_bytes_str,
						       total_size_str);
			g_free (transferred_bytes_str);
			g_free (total_size_str);
		} else {
			second_line = g_strdup (_("Wating the other participant's response"));
		}

		remaining = empathy_file_get_remaining_time (file);
		break;

	case EMP_FILE_TRANSFER_STATE_COMPLETED:
		if (empathy_file_get_direction (file) ==
		    EMP_FILE_TRANSFER_DIRECTION_INCOMING)
			/* translators: first %s is filename, second %s
			 * is the contact name */
			first_line = g_strdup_printf (
				_("\"%s\" received from %s"), filename,
				contact_name);
		else
			/* translators: first %s is filename, second %s
			 * is the contact name */
			first_line = g_strdup_printf (
					_("\"%s\" sent to %s"), filename,
					contact_name);
		second_line = g_strdup ("File transfer completed");
		break;

	case EMP_FILE_TRANSFER_STATE_CANCELED:
		if (empathy_file_get_direction (file) ==
		    EMP_FILE_TRANSFER_DIRECTION_INCOMING)
			/* translators: first %s is filename, second %s
			 * is the contact name */
			first_line = g_strdup_printf (
				_("\"%s\" receiving from %s"), filename,
				contact_name);
		else
			/* translators: first %s is filename, second %s
			 * is the contact name */
			first_line = g_strdup_printf (
					_("\"%s\" sending to %s"), filename,
					contact_name);
		second_line = g_strdup ("File transfer cancelled");

	default:
		g_return_if_reached ();
	}

	if (total_size != EMPATHY_FILE_UNKNOWN_SIZE)
		percent = transferred_bytes * 100 / total_size;
	else
		percent = -1;

	if (remaining < 0) {
		if (state == EMP_FILE_TRANSFER_STATE_COMPLETED ||
		    state == EMP_FILE_TRANSFER_STATE_CANCELED)
			remaining_str = g_strdup ("");
		else
			/* translators: the text before the "|" is context to
			 * help you decide on the correct translation. You
			 * MUST OMIT it in the translated string. */
			remaining_str = g_strdup (Q_("remaining time|Unknown"));
	} else {
		remaining_str = format_interval (remaining);
	}

	msg = g_strdup_printf ("%s\n%s", first_line, second_line);

	path = gtk_tree_row_reference_get_path (row_ref);
	gtk_tree_model_get_iter (priv->model, &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    &iter,
			    COL_PERCENT, percent,
			    COL_MESSAGE, msg,
			    COL_REMAINING, remaining_str,
			    -1);
	gtk_tree_path_free (path);

	g_free (msg);
	g_free (first_line);
	g_free (second_line);
	g_free (remaining_str);

	update_buttons (ft_manager);
}

static void
transferred_bytes_changed_cb (EmpathyFile      *file,
			      GParamSpec       *pspec,
			      EmpathyFTManager *ft_manager)
{
	update_ft_row (ft_manager, file);
}

static void
state_changed_cb (EmpathyFile      *file,
		  GParamSpec       *pspec,
		  EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	gboolean              remove;

	priv = GET_PRIV (ft_manager);

	switch (empathy_file_get_state (file)) {
		case EMP_FILE_TRANSFER_STATE_COMPLETED:
			if (empathy_file_get_direction (file) ==
			    EMP_FILE_TRANSFER_DIRECTION_INCOMING) {
				GtkRecentManager *manager;
				const gchar      *uri;

				manager = gtk_recent_manager_get_default ();
				uri = g_object_get_data (G_OBJECT (file), "uri");
				gtk_recent_manager_add_item (manager, uri);
			}

		case EMP_FILE_TRANSFER_STATE_CANCELED:
			/* Automatically remove file transfers if the
			 * window if not visible. */
			/* FIXME how do the user know if the file transfer
			 * failed? */
			remove = !GTK_WIDGET_VISIBLE (priv->window);
			break;

		default:
			remove = FALSE;
			break;
	}

	if (remove)
		ft_manager_remove_file_from_list (ft_manager, file);
	else
		update_ft_row (ft_manager, file);
}

static void
ft_manager_add_file_to_list (EmpathyFTManager *ft_manager,
			     EmpathyFile      *file)
{
	EmpathyFTManagerPriv *priv;
	GtkTreeRowReference  *row_ref;
	GtkTreeIter           iter;
	GtkTreeSelection     *selection;
	GtkTreePath          *path;
	GtkIconTheme         *theme;
	GtkIconInfo          *icon_info;
	GdkPixbuf            *pixbuf;
	const gchar          *mime;
	gchar                *icon_name;
	gint                  width = 16;
	gint                  height = 16;

	priv = GET_PRIV (ft_manager);

	gtk_list_store_append (GTK_LIST_STORE (priv->model),
			       &iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    &iter, COL_FT_OBJECT, file, -1);

	path =  gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model), &iter);
	row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->model), path);
	gtk_tree_path_free (path);

	g_object_ref (file);
	g_hash_table_insert (priv->file_to_row_ref, file, row_ref);

	update_ft_row (ft_manager, file);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	g_signal_connect (file, "notify::state",
			  G_CALLBACK (state_changed_cb), ft_manager);
	g_signal_connect (file, "notify::transferred-bytes",
			  G_CALLBACK (transferred_bytes_changed_cb), ft_manager);

	mime = gnome_vfs_get_mime_type_for_name (empathy_file_get_filename (file));
	theme = gtk_icon_theme_get_default ();
	/* FIXME remove the dependency on libgnomeui replacing this function
	 * with gio/gvfs or copying the code from gtk-recent */
	icon_name = gnome_icon_lookup (theme, NULL, NULL, NULL, NULL,
				       mime, GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (priv->window),
					   GTK_ICON_SIZE_MENU, &width, &height);
	width *= 2;

	icon_info = gtk_icon_theme_lookup_icon (theme, icon_name, width, 0);
	g_free (icon_name);
	if (icon_info != NULL) {
		pixbuf = gdk_pixbuf_new_from_file_at_size
			(gtk_icon_info_get_filename (icon_info), width, width, NULL);
		gtk_icon_info_free (icon_info);

		gtk_list_store_set (GTK_LIST_STORE (priv->model),
				    &iter, COL_IMAGE, pixbuf, -1);
		if (pixbuf != NULL)
		{
			g_object_unref (pixbuf);
		}
	}

	gtk_window_present (GTK_WINDOW (priv->window));
}

static void
selection_changed (GtkTreeSelection *selection,
		   EmpathyFTManager *ft_manager)
{
	update_buttons (ft_manager);
}

static void
progress_cell_data_func (GtkTreeViewColumn *col,
			 GtkCellRenderer   *renderer,
			 GtkTreeModel      *model,
			 GtkTreeIter       *iter,
			 gpointer           user_data)
{
	const gchar *text = NULL;
	gint         percent;

	gtk_tree_model_get (model, iter,
			    COL_PERCENT, &percent,
			    -1);

	if (percent < 0) {
		percent = 0;
		/* Translators: The text before the "|" is context to help you
		 * decide on the correct translation. You MUST OMIT it in the
		 * translated string. */
		text = Q_("file transfer percent|Unknown");
	}

	g_object_set (renderer, "text", text, "value", percent, NULL);
}

static void
ft_manager_clear_foreach_cb (gpointer key,
			     gpointer value,
			     gpointer user_data)
{
	GSList     **list = user_data;
	EmpathyFile *file = key;

	switch (empathy_file_get_state (file)) {
		case EMP_FILE_TRANSFER_STATE_COMPLETED:
		case EMP_FILE_TRANSFER_STATE_CANCELED:
			*list = g_slist_append (*list, file);
			break;
		default:
			break;
	}
}

static void
ft_manager_clear (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	GSList               *closed_files = NULL;
	GSList               *l;

	priv = GET_PRIV (ft_manager);

	DEBUG ("Clearing file transfer list");

	g_hash_table_foreach (priv->file_to_row_ref, ft_manager_clear_foreach_cb,
			      &closed_files);

	for (l = closed_files; l; l = l->next) {
		ft_manager_remove_file_from_list (ft_manager, l->data);
	}

	g_slist_free (closed_files);
}

static gboolean
ft_manager_delete_event_cb (GtkWidget        *widget,
			    GdkEvent         *event,
			    EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;

	priv = GET_PRIV (ft_manager);

	if (g_hash_table_size (priv->file_to_row_ref) == 0) {
		DEBUG ("Destroying window");
		return FALSE;
	} else {
		DEBUG ("Hiding window");
		gtk_widget_hide (widget);
		ft_manager_clear (ft_manager);
		return TRUE;
	}
}

static gboolean
ft_manager_save_geometry_timeout_cb (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	gint                  x, y, w, h;

	priv = GET_PRIV (ft_manager);

	gtk_window_get_size (GTK_WINDOW (priv->window), &w, &h);
	gtk_window_get_position (GTK_WINDOW (priv->window), &x, &y);

	empathy_geometry_save ("ft-manager", x, y, w, h);

	priv->save_geometry_id = 0;

	return FALSE;
}

static gboolean
ft_manager_configure_event_cb (GtkWidget         *widget,
			       GdkEventConfigure *event,
			       EmpathyFTManager  *ft_manager)
{
	EmpathyFTManagerPriv *priv;

	priv = GET_PRIV (ft_manager);

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	priv->save_geometry_id =
		g_timeout_add (500,
			       (GSourceFunc) ft_manager_save_geometry_timeout_cb,
			       ft_manager);

	return FALSE;
}

static void
ft_manager_build_ui (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	gint                  x, y, w, h;
	GtkListStore         *liststore;
	GtkTreeViewColumn    *column;
	GtkCellRenderer      *renderer;
	GtkTreeSelection     *selection;
	gchar                *filename;

	priv = GET_PRIV (ft_manager);

	/* Keep this object alive until we have the dialog window */
	g_object_ref (ft_manager);

	filename = empathy_file_lookup ("empathy-ft-manager.glade",
					"libempathy-gtk");
	empathy_glade_get_file (filename,
			        "ft_manager_dialog",
			        NULL,
			        "ft_manager_dialog", &priv->window,
			        "ft_list", &priv->treeview,
			        "open_button", &priv->open_button,
			        "abort_button", &priv->abort_button,
			        NULL);
	g_free (filename);

	g_signal_connect (priv->window, "response",
			  G_CALLBACK (ft_manager_response_cb), ft_manager);
	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (ft_manager_delete_event_cb), ft_manager);
	g_signal_connect (priv->window, "configure-event",
			  G_CALLBACK (ft_manager_configure_event_cb), ft_manager);

	gtk_window_set_icon_name (GTK_WINDOW (priv->window), EMPATHY_IMAGE_DOCUMENT_SEND);

	/* Window geometry. */
	empathy_geometry_load ("ft-manager", &x, &y, &w, &h);

	if (x >= 0 && y >= 0) {
		/* Let the window manager position it if we don't have
		 * good x, y coordinates. */
		gtk_window_move (GTK_WINDOW (priv->window), x, y);
	}

	if (w > 0 && h > 0) {
		/* Use the defaults from the glade file if we don't have
		 * good w, h geometry. */
		gtk_window_resize (GTK_WINDOW (priv->window), w, h);
	}

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview)),
				     GTK_SELECTION_BROWSE);

	liststore = gtk_list_store_new (5,
					G_TYPE_INT,
					GDK_TYPE_PIXBUF,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_OBJECT);

	gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview),
				 GTK_TREE_MODEL (liststore));
	g_object_unref (liststore);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(priv->treeview),
					   TRUE);

	/* Icon and filename column*/
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("File"));
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "xpad", 3, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					    "pixbuf", COL_IMAGE,
					    NULL);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COL_MESSAGE,
					     NULL);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (priv->treeview), column,
				     FILE_COL_POS);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_MESSAGE);
	gtk_tree_view_column_set_spacing (column, 3);

	/* Progress column */
	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "xalign", 0.5, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     PROGRESS_COL_POS, _("%"),
						     renderer,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   PROGRESS_COL_POS);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						progress_cell_data_func,
						NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (column, COL_PERCENT);

	/* Remaining time column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 0.5, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     REMAINING_COL_POS, _("Remaining"),
						     renderer,
						     "text", COL_REMAINING,
						     NULL);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   REMAINING_COL_POS);
	gtk_tree_view_column_set_sort_column_id (column, COL_REMAINING);

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->treeview), FALSE);

	priv->model = GTK_TREE_MODEL (liststore);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), ft_manager);
}

static void
ft_manager_remove_file_from_list (EmpathyFTManager *ft_manager,
				  EmpathyFile      *file)
{
	EmpathyFTManagerPriv *priv;
	GtkTreeRowReference  *row_ref;
	GtkTreePath          *path = NULL;
	GtkTreeIter           iter, iter2;

	priv = GET_PRIV (ft_manager);

	row_ref = get_row_from_file (ft_manager, file);
	g_return_if_fail (row_ref);

	DEBUG ("Removing file transfer from window: contact=%s, filename=%s",
	       empathy_contact_get_name (empathy_file_get_contact (file)),
	       empathy_file_get_filename (file));

	/* Get the row we'll select after removal ("smart" selection) */

	path = gtk_tree_row_reference_get_path (row_ref);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model),
				 &iter, path);
	gtk_tree_path_free (path);

	row_ref = NULL;
	iter2 = iter;
	if (gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->model), &iter))
	{
		path = gtk_tree_model_get_path  (GTK_TREE_MODEL (priv->model), &iter);
		row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->model), path);
	}
	else
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model), &iter2);
		if (gtk_tree_path_prev (path))
		{
			row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->model),
							      path);
		}
	}
	gtk_tree_path_free (path);

	/* Removal */

	gtk_list_store_remove (GTK_LIST_STORE (priv->model), &iter2);
	g_hash_table_remove (priv->file_to_row_ref,
			     file);
	g_object_unref (file);

	/* Actual selection */

	if (row_ref != NULL)
	{
		path = gtk_tree_row_reference_get_path (row_ref);
		if (path != NULL)
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview),
						  path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		gtk_tree_row_reference_free (row_ref);
	}

	if (g_hash_table_size (priv->file_to_row_ref) == 0 &&
	    !GTK_WIDGET_VISIBLE (priv->window)) {
		DEBUG ("Destroying window");
		gtk_widget_destroy (priv->window);
	}
}

static void
ft_manager_open (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	GValue                val = {0, };
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	EmpathyFile          *file;
	const gchar          *uri;

	priv = GET_PRIV (ft_manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get_value (model, &iter, COL_FT_OBJECT, &val);

	file = g_value_get_object (&val);
	g_return_if_fail (file != NULL);

	uri = g_object_get_data (G_OBJECT (file), "uri");
	DEBUG ("Opening URI: %s", uri);
	empathy_url_show (uri);
}

static void
ft_manager_stop (EmpathyFTManager *ft_manager)
{
	EmpathyFTManagerPriv *priv;
	GValue                val = {0, };
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	EmpathyFile          *file;

	priv = GET_PRIV (ft_manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get_value (model, &iter, COL_FT_OBJECT, &val);

	file = g_value_get_object (&val);
	g_return_if_fail (file != NULL);

	DEBUG ("Stopping file transfer: contact=%s, filename=%s",
	       empathy_contact_get_name (empathy_file_get_contact (file)),
	       empathy_file_get_filename (file));

/*	empathy_file_cancel (ft, EMPATHY_TP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_STOPPED);*/
	/* cancel file transfer here */

	g_value_unset (&val);
}

static void
ft_manager_response_cb (GtkWidget        *dialog,
			gint              response,
			EmpathyFTManager *ft_manager)
{
	switch (response)
	{
	case RESPONSE_CLEAR:
		ft_manager_clear (ft_manager);
		break;
	case RESPONSE_OPEN:
		ft_manager_open (ft_manager);
		break;
	case RESPONSE_STOP:
		ft_manager_stop (ft_manager);
		break;
	}
}

/*
 * Receiving files
 */

typedef struct {
	EmpathyFTManager *ft_manager;
	EmpathyFile      *file;
} ReceiveResponseData;

static void
free_receive_response_data (ReceiveResponseData *response_data)
{
	if (!response_data)
		return;

	g_object_unref (response_data->file);
	g_object_unref (response_data->ft_manager);
	g_free (response_data);
}

static void
ft_manager_save_dialog_response_cb (GtkDialog           *widget,
				    gint                 response_id,
				    ReceiveResponseData *response_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		gchar *uri;
		gchar *folder;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (widget));

		if (uri) {
			GFile         *file;
			GOutputStream *out_stream;
			gchar         *filename;

			file = g_file_new_for_uri (uri);
			out_stream = G_OUTPUT_STREAM (g_file_replace (file, NULL,
								      FALSE, 0,
								      NULL, NULL));
			empathy_file_set_output_stream (response_data->file, out_stream);

			g_object_set_data_full (G_OBJECT (response_data->file),
						"uri", uri, g_free);

			filename = g_file_get_basename (file);
			empathy_file_set_filename (response_data->file, filename);

			empathy_file_accept (response_data->file);

			ft_manager_add_file_to_list (response_data->ft_manager,
						     response_data->file);

			g_free (filename);
			g_object_unref (file);
			if (out_stream)
				g_object_unref (out_stream);
		}

		folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget));
		if (folder) {
			empathy_conf_set_string (empathy_conf_get (),
						 EMPATHY_PREFS_FILE_TRANSFER_DEFAULT_FOLDER,
						 folder);
			g_free (folder);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (widget));
	free_receive_response_data (response_data);
}

static void
ft_manager_create_save_dialog (ReceiveResponseData *response_data)
{
	GtkWidget     *widget;
	gchar         *folder;
	GtkFileFilter *filter;

	DEBUG ("Creating save file chooser");

	widget = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			       "action", GTK_FILE_CHOOSER_ACTION_SAVE,
			       "select-multiple", FALSE,
			       "do-overwrite-confirmation", TRUE,
			       NULL);

	gtk_window_set_title (GTK_WINDOW (widget), _("Save file"));

	if (!empathy_conf_get_string (empathy_conf_get (),
				      EMPATHY_PREFS_FILE_TRANSFER_DEFAULT_FOLDER,
				      &folder) || !folder) {
		folder = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD));
	}

	if (folder)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget), folder);

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (widget),
					   empathy_file_get_filename (response_data->file));

	gtk_dialog_add_buttons (GTK_DIALOG (widget),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (widget),
					 GTK_RESPONSE_OK);

	g_signal_connect (widget, "response",
			  G_CALLBACK (ft_manager_save_dialog_response_cb),
			  response_data);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);

	gtk_widget_show (widget);

	g_free (folder);
}

static void
ft_manager_receive_file_response_cb (GtkWidget           *dialog,
				     gint                 response,
				     ReceiveResponseData *response_data)
{
	TpChannel *channel;

	if (response == GTK_RESPONSE_ACCEPT) {
		ft_manager_create_save_dialog (response_data);
	} else {
		channel = empathy_file_get_channel (response_data->file);
		tp_cli_channel_run_close (channel, -1, NULL, NULL);
		free_receive_response_data (response_data);
	}

	gtk_widget_destroy (dialog);
}

void
ft_manager_display_accept_dialog (EmpathyFTManager *ft_manager,
				  EmpathyFile      *file)
{
	GtkWidget           *dialog;
	GtkWidget           *image;
	GtkWidget           *button;
	const gchar         *contact_name;
	const gchar         *filename;
	guint64              size;
	gchar               *size_str;
	ReceiveResponseData *response_data;

	g_return_if_fail (EMPATHY_IS_FT_MANAGER (ft_manager));
	g_return_if_fail (EMPATHY_IS_FILE (file));

	DEBUG ("Creating accept dialog");

	contact_name = empathy_contact_get_name (empathy_file_get_contact (file));
	filename = empathy_file_get_filename (file);

	size = empathy_file_get_size (file);
	if (size == EMPATHY_FILE_UNKNOWN_SIZE)
		size_str = g_strdup (_("unknown size"));
	else
		size_str = gnome_vfs_format_file_size_for_display (size);

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_INFO,
					 GTK_BUTTONS_NONE,
					 _("%s would like to send you a file"),
					 contact_name);

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("Do you want to accept the file \"%s\" (%s)?"),
		 filename, size_str);

	/* Icon */
	image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);

	/* Decline button */
	button = gtk_button_new_with_mnemonic (_("_Decline"));
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_CANCEL,
							GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				      GTK_RESPONSE_REJECT);

	/* Accept button */
	button = gtk_button_new_with_mnemonic (_("_Accept"));
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_SAVE,
							GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				      GTK_RESPONSE_ACCEPT);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (button);

	response_data = g_new0 (ReceiveResponseData, 1);
	response_data->ft_manager = g_object_ref (ft_manager);
	response_data->file = g_object_ref (file);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ft_manager_receive_file_response_cb),
			  response_data);

	gtk_widget_show (dialog);

	g_free (size_str);
}

