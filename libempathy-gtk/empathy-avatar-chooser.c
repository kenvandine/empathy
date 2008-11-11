/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB.
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Based on Novell's e-image-chooser.
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libempathy/empathy-utils.h>
#include "empathy-avatar-chooser.h"
#include "empathy-conf.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#define AVATAR_SIZE_SAVE 96
#define AVATAR_SIZE_VIEW 64
#define DEFAULT_DIR DATADIR"/pixmaps/faces"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyAvatarChooser)
typedef struct {
	EmpathyContactFactory   *contact_factory;
	McAccount               *account;
	EmpathyTpContactFactory *tp_contact_factory;

	gulong ready_handler_id;

	gchar *image_data;
	gsize  image_data_size;
	gchar *mime_type;
} EmpathyAvatarChooserPriv;

static void       avatar_chooser_finalize              (GObject              *object);
static void       avatar_chooser_set_account           (EmpathyAvatarChooser *self,
							McAccount            *account);
static void       avatar_chooser_set_image_from_data   (EmpathyAvatarChooser *chooser,
							gchar                *data,
							gsize                 size,
							gboolean              set_locally);
static gboolean   avatar_chooser_drag_motion_cb        (GtkWidget            *widget,
							GdkDragContext       *context,
							gint                  x,
							gint                  y,
							guint                 time,
							EmpathyAvatarChooser *chooser);
static void       avatar_chooser_drag_leave_cb         (GtkWidget            *widget,
							GdkDragContext       *context,
							guint                 time,
							EmpathyAvatarChooser *chooser);
static gboolean   avatar_chooser_drag_drop_cb          (GtkWidget            *widget,
							GdkDragContext       *context,
							gint                  x,
							gint                  y,
							guint                 time,
							EmpathyAvatarChooser *chooser);
static void       avatar_chooser_drag_data_received_cb (GtkWidget            *widget,
							GdkDragContext       *context,
							gint                  x,
							gint                  y,
							GtkSelectionData     *selection_data,
							guint                 info,
							guint                 time,
							EmpathyAvatarChooser *chooser);
static void       avatar_chooser_clicked_cb            (GtkWidget            *button,
							EmpathyAvatarChooser *chooser);

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_CONTACT_FACTORY,
	PROP_ACCOUNT
};

static guint signals [LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyAvatarChooser, empathy_avatar_chooser, GTK_TYPE_BUTTON);

/*
 * Drag and drop stuff
 */
#define URI_LIST_TYPE "text/uri-list"

enum DndTargetType {
	DND_TARGET_TYPE_URI_LIST
};

static const GtkTargetEntry drop_types[] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_TYPE_URI_LIST },
};

static void
empathy_avatar_chooser_get_property (GObject    *object,
				     guint       param_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	EmpathyAvatarChooserPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONTACT_FACTORY:
		g_assert (priv->contact_factory != NULL);
		g_value_set_object (value, priv->contact_factory);
		break;
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
empathy_avatar_chooser_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	EmpathyAvatarChooser *self = EMPATHY_AVATAR_CHOOSER (object);
	EmpathyAvatarChooserPriv *priv = GET_PRIV (self);

	switch (param_id) {
	case PROP_CONTACT_FACTORY:
		priv->contact_factory = g_value_get_object (value);

		g_assert (priv->contact_factory != NULL);
		g_object_ref (priv->contact_factory);

		break;
	case PROP_ACCOUNT:
		avatar_chooser_set_account (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
empathy_avatar_chooser_class_init (EmpathyAvatarChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GParamSpec *param_spec;

	object_class->finalize = avatar_chooser_finalize;
	object_class->get_property = empathy_avatar_chooser_get_property;
	object_class->set_property = empathy_avatar_chooser_set_property;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	param_spec = g_param_spec_object ("contact-factory",
					  "EmpathyContactFactory instance",
					  "EmpathyContactFactory instance "
					  "(may not be NULL)",
					  EMPATHY_TYPE_CONTACT_FACTORY,
					  G_PARAM_CONSTRUCT_ONLY |
					  G_PARAM_READWRITE |
					  G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (object_class,
					 PROP_CONTACT_FACTORY,
					 param_spec);

	param_spec = g_param_spec_object ("account",
					  "McAccount",
					  "McAccount whose avatar should be "
					  "shown and modified by this widget",
					  MC_TYPE_ACCOUNT,
					  G_PARAM_READWRITE |
					  G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 param_spec);

	g_type_class_add_private (object_class, sizeof (EmpathyAvatarChooserPriv));
}

static void
empathy_avatar_chooser_init (EmpathyAvatarChooser *chooser)
{
	EmpathyAvatarChooserPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser,
		EMPATHY_TYPE_AVATAR_CHOOSER, EmpathyAvatarChooserPriv);

	chooser->priv = priv;
	gtk_drag_dest_set (GTK_WIDGET (chooser),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types,
			   G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY);

	g_signal_connect (chooser, "drag-motion",
			  G_CALLBACK (avatar_chooser_drag_motion_cb),
			  chooser);
	g_signal_connect (chooser, "drag-leave",
			  G_CALLBACK (avatar_chooser_drag_leave_cb),
			  chooser);
	g_signal_connect (chooser, "drag-drop",
			  G_CALLBACK (avatar_chooser_drag_drop_cb),
			  chooser);
	g_signal_connect (chooser, "drag-data-received",
			  G_CALLBACK (avatar_chooser_drag_data_received_cb),
			  chooser);
	g_signal_connect (chooser, "clicked",
			  G_CALLBACK (avatar_chooser_clicked_cb),
			  chooser);

	empathy_avatar_chooser_set (chooser, NULL);
}

static void
avatar_chooser_finalize (GObject *object)
{
	EmpathyAvatarChooserPriv *priv;

	priv = GET_PRIV (object);

	g_object_unref (priv->contact_factory);
	priv->contact_factory = NULL;

	avatar_chooser_set_account (EMPATHY_AVATAR_CHOOSER (object), NULL);
	g_assert (priv->account == NULL && priv->tp_contact_factory == NULL);

	g_free (priv->image_data);
	g_free (priv->mime_type);

	G_OBJECT_CLASS (empathy_avatar_chooser_parent_class)->finalize (object);
}

static void
avatar_chooser_tp_cf_ready_cb (EmpathyTpContactFactory *tp_cf,
			       GParamSpec              *unused,
			       EmpathyAvatarChooser    *self)
{
	EmpathyAvatarChooserPriv *priv = GET_PRIV (self);
	gboolean ready;

	/* sanity check that we're listening on the right ETpCF */
	g_assert (priv->tp_contact_factory == tp_cf);

	ready = empathy_tp_contact_factory_is_ready (tp_cf);
	gtk_widget_set_sensitive (GTK_WIDGET (self), ready);
}

static void
avatar_chooser_set_account (EmpathyAvatarChooser *self,
			    McAccount            *account)
{
	EmpathyAvatarChooserPriv *priv = GET_PRIV (self);

	if (priv->account != NULL) {
		g_object_unref (priv->account);
		priv->account = NULL;

		g_assert (priv->tp_contact_factory != NULL);

		g_signal_handler_disconnect (priv->tp_contact_factory,
			priv->ready_handler_id);
		priv->ready_handler_id = 0;

		g_object_unref (priv->tp_contact_factory);
		priv->tp_contact_factory = NULL;
	}

	if (account != NULL) {
		priv->account = g_object_ref (account);
		priv->tp_contact_factory = g_object_ref (
			empathy_contact_factory_get_tp_factory (
				priv->contact_factory, priv->account));

		priv->ready_handler_id = g_signal_connect (
			priv->tp_contact_factory, "notify::ready",
			G_CALLBACK (avatar_chooser_tp_cf_ready_cb), self);
		avatar_chooser_tp_cf_ready_cb (priv->tp_contact_factory, NULL,
			self);
	}
}


static void
avatar_chooser_set_image (EmpathyAvatarChooser *chooser,
			  GdkPixbuf            *pixbuf,
			  gchar                *image_data,
			  gsize                 image_data_size,
			  gchar                *mime_type,
			  gboolean              set_locally)
{
	EmpathyAvatarChooserPriv *priv = GET_PRIV (chooser);
	GtkWidget                *image;
	GdkPixbuf                *pixbuf_view = NULL;

	g_free (priv->image_data);
	g_free (priv->mime_type);

	priv->image_data = NULL;
	priv->image_data_size = 0;
	priv->mime_type = NULL;

	if (pixbuf) {
		priv->image_data = image_data;
		priv->image_data_size = image_data_size;
		priv->mime_type = mime_type;

		pixbuf_view = empathy_pixbuf_scale_down_if_necessary (pixbuf, AVATAR_SIZE_VIEW);
		image = gtk_image_new_from_pixbuf (pixbuf_view);
		g_object_unref (pixbuf_view);
	} else {
		image = gtk_image_new_from_icon_name ("stock_person",
						      GTK_ICON_SIZE_DIALOG);
	}

	gtk_button_set_image (GTK_BUTTON (chooser), image);
	g_signal_emit (chooser, signals[CHANGED], 0);
}

static void
avatar_chooser_clear_image (EmpathyAvatarChooser *chooser,
			    gboolean              set_locally)
{
	avatar_chooser_set_image (chooser, NULL, NULL, 0, NULL, set_locally);
}

static void
avatar_chooser_set_image_from_file (EmpathyAvatarChooser *chooser,
				    const gchar          *filename)
{
	gchar  *image_data = NULL;
	gsize   image_size = 0;
	GError *error = NULL;

	if (!g_file_get_contents (filename, &image_data, &image_size, &error)) {
		DEBUG ("Failed to load image from '%s': %s", filename,
			error ? error->message : "No error given");

		avatar_chooser_clear_image (chooser, TRUE);

		g_clear_error (&error);
		return;
	}

	avatar_chooser_set_image_from_data (chooser, image_data, image_size, TRUE);
}

static void
avatar_chooser_set_image_from_data (EmpathyAvatarChooser *chooser,
				    gchar                *data,
				    gsize                 size,
				    gboolean              set_locally)
{
	GdkPixbuf       *pixbuf;
	gchar           *mime_type = NULL;

	pixbuf = empathy_pixbuf_from_data (data, size, &mime_type);
	avatar_chooser_set_image (chooser, pixbuf, data, size, mime_type, set_locally);
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static gboolean
avatar_chooser_drag_motion_cb (GtkWidget          *widget,
			      GdkDragContext     *context,
			      gint                x,
			      gint                y,
			      guint               time,
			      EmpathyAvatarChooser *chooser)
{
	EmpathyAvatarChooserPriv *priv;
	GList                  *p;

	priv = GET_PRIV (chooser);

	for (p = context->targets; p != NULL; p = p->next) {
		gchar *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));

		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gdk_drag_status (context, GDK_ACTION_COPY, time);

			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
avatar_chooser_drag_leave_cb (GtkWidget          *widget,
			     GdkDragContext     *context,
			     guint               time,
			     EmpathyAvatarChooser *chooser)
{
}

static gboolean
avatar_chooser_drag_drop_cb (GtkWidget          *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time,
			    EmpathyAvatarChooser *chooser)
{
	EmpathyAvatarChooserPriv *priv;
	GList                  *p;

	priv = GET_PRIV (chooser);

	if (context->targets == NULL) {
		return FALSE;
	}

	for (p = context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gtk_drag_get_data (widget, context,
					   GDK_POINTER_TO_ATOM (p->data),
					   time);

			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
avatar_chooser_drag_data_received_cb (GtkWidget          *widget,
				     GdkDragContext     *context,
				     gint                x,
				     gint                y,
				     GtkSelectionData   *selection_data,
				     guint               info,
				     guint               time,
				     EmpathyAvatarChooser *chooser)
{
	gchar    *target_type;
	gboolean  handled = FALSE;

	target_type = gdk_atom_name (selection_data->target);
	if (!strcmp (target_type, URI_LIST_TYPE)) {
		GFile            *file;
		GFileInputStream *input_stream;
		gchar            *nl;
		gchar            *data = NULL;

		nl = strstr (selection_data->data, "\r\n");
		if (nl) {
			gchar *uri;

			uri = g_strndup (selection_data->data,
					 nl - (gchar*) selection_data->data);

			file = g_file_new_for_uri (uri);
			g_free (uri);
		} else {
			file = g_file_new_for_uri (selection_data->data);
		}

		input_stream = g_file_read (file, NULL, NULL);

		if (input_stream != NULL) {
			GFileInfo *info;
			
			info = g_file_query_info (file,
						  G_FILE_ATTRIBUTE_STANDARD_SIZE,
						  0, NULL, NULL);
			if (info != NULL) {
				goffset size;
				gssize bytes_read;
				
				size = g_file_info_get_size (info);
				data = g_malloc (size);

				bytes_read = g_input_stream_read (G_INPUT_STREAM (input_stream),
								  data, size,
								  NULL, NULL);
				if (bytes_read != -1) {
					avatar_chooser_set_image_from_data (chooser,
									    data,
									    (gsize) bytes_read,
									    TRUE);
					handled = TRUE;
				}

				g_free (data);
				g_object_unref (info);
			}

			g_object_unref (input_stream);
		}
		
		g_object_unref (file);
	}

	gtk_drag_finish (context, handled, FALSE, time);
}

static void
avatar_chooser_update_preview_cb (GtkFileChooser       *file_chooser,
				  EmpathyAvatarChooser *chooser)
{
	gchar *filename;

	filename = gtk_file_chooser_get_preview_filename (file_chooser);

	if (filename) {
		GtkWidget *image;
		GdkPixbuf *pixbuf = NULL;
		GdkPixbuf *scaled_pixbuf;

		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

		image = gtk_file_chooser_get_preview_widget (file_chooser);

		if (pixbuf) {
			scaled_pixbuf = empathy_pixbuf_scale_down_if_necessary (pixbuf, AVATAR_SIZE_SAVE);
			gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled_pixbuf);
			g_object_unref (scaled_pixbuf);
			g_object_unref (pixbuf);
		} else {
			gtk_image_set_from_stock (GTK_IMAGE (image),
						  "gtk-dialog-question",
						  GTK_ICON_SIZE_DIALOG);
		}
	}

	gtk_file_chooser_set_preview_widget_active (file_chooser, TRUE);
}

static void
avatar_chooser_response_cb (GtkWidget            *widget,
			    gint                  response,
			    EmpathyAvatarChooser *chooser)
{
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		gchar *path;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
		avatar_chooser_set_image_from_file (chooser, filename);
		g_free (filename);

		path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget));
		if (path) {
			empathy_conf_set_string (empathy_conf_get (),
						 EMPATHY_PREFS_UI_AVATAR_DIRECTORY,
						 path);
			g_free (path);
		}
	}
	else if (response == GTK_RESPONSE_NO) {
		avatar_chooser_clear_image (chooser, TRUE);
	}

	gtk_widget_destroy (widget);
}

static void
avatar_chooser_clicked_cb (GtkWidget            *button,
			   EmpathyAvatarChooser *chooser)
{
	GtkFileChooser *chooser_dialog;
	GtkWidget      *image;
	gchar          *saved_dir = NULL;
	const gchar    *default_dir = DEFAULT_DIR;
	const gchar    *pics_dir;
	GtkFileFilter  *filter;

	chooser_dialog = GTK_FILE_CHOOSER (
		gtk_file_chooser_dialog_new (_("Select Your Avatar Image"),
					     empathy_get_toplevel_window (GTK_WIDGET (chooser)),
					     GTK_FILE_CHOOSER_ACTION_OPEN,
					     _("No Image"),
					     GTK_RESPONSE_NO,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL,
					     GTK_STOCK_OPEN,
					     GTK_RESPONSE_OK,
					     NULL));

	/* Get special dirs */
	empathy_conf_get_string (empathy_conf_get (),
				 EMPATHY_PREFS_UI_AVATAR_DIRECTORY,
				 &saved_dir);
	if (saved_dir && !g_file_test (saved_dir, G_FILE_TEST_IS_DIR)) {
		g_free (saved_dir);
		saved_dir = NULL;
	}
	if (!g_file_test (default_dir, G_FILE_TEST_IS_DIR)) {
		default_dir = NULL;
	}
	pics_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
	if (pics_dir && !g_file_test (pics_dir, G_FILE_TEST_IS_DIR)) {
		pics_dir = NULL;
	}

	/* Set current dir to the last one or to DEFAULT_DIR or to home */
	if (saved_dir) {
		gtk_file_chooser_set_current_folder (chooser_dialog, saved_dir);
	}
	else if (pics_dir) {
		gtk_file_chooser_set_current_folder (chooser_dialog, pics_dir);
	}
	else if (default_dir) {
		gtk_file_chooser_set_current_folder (chooser_dialog, default_dir);
	} else {
		gtk_file_chooser_set_current_folder (chooser_dialog, g_get_home_dir ());
	}

	/* Add shortcuts to special dirs */
	if (saved_dir) {
		gtk_file_chooser_add_shortcut_folder (chooser_dialog, saved_dir, NULL);
	}
	else if (pics_dir) {
		gtk_file_chooser_add_shortcut_folder (chooser_dialog, pics_dir, NULL);
	}
	if (default_dir) {
		gtk_file_chooser_add_shortcut_folder (chooser_dialog, default_dir, NULL);
	}

	/* Setup preview image */
	image = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (chooser_dialog, image);
	gtk_widget_set_size_request (image, AVATAR_SIZE_SAVE, AVATAR_SIZE_SAVE);
	gtk_widget_show (image);
	gtk_file_chooser_set_use_preview_label (chooser_dialog,	FALSE);
	g_signal_connect (chooser_dialog, "update-preview",
			  G_CALLBACK (avatar_chooser_update_preview_cb),
			  chooser);

	/* Setup filers */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Images"));
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_chooser_add_filter (chooser_dialog, filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter (chooser_dialog, filter);

	/* Setup response */
	gtk_dialog_set_default_response (GTK_DIALOG (chooser_dialog), GTK_RESPONSE_OK);
	g_signal_connect (chooser_dialog, "response",
			  G_CALLBACK (avatar_chooser_response_cb),
			  chooser);

	gtk_widget_show (GTK_WIDGET (chooser_dialog));
	g_free (saved_dir);
}

GtkWidget *
empathy_avatar_chooser_new (EmpathyContactFactory *contact_factory)
{
	return g_object_new (EMPATHY_TYPE_AVATAR_CHOOSER,
			     "contact-factory", contact_factory,
			     NULL);
}

void
empathy_avatar_chooser_set (EmpathyAvatarChooser *chooser,
			    EmpathyAvatar        *avatar)
{
	g_return_if_fail (EMPATHY_IS_AVATAR_CHOOSER (chooser));

	if (avatar != NULL) {
		gchar *data = g_memdup (avatar->data, avatar->len);
		avatar_chooser_set_image_from_data (chooser, data, avatar->len, FALSE);
	} else {
		avatar_chooser_clear_image (chooser, FALSE);
	}
}

void
empathy_avatar_chooser_get_image_data (EmpathyAvatarChooser  *chooser,
				       const gchar          **data,
				       gsize                 *data_size,
				       const gchar          **mime_type)
{
	EmpathyAvatarChooserPriv *priv;

	g_return_if_fail (EMPATHY_IS_AVATAR_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (data) {
		*data = priv->image_data;
	}
	if (data_size) {
		*data_size = priv->image_data_size;
	}
	if (mime_type) {
		*mime_type = priv->mime_type;
	}
}

