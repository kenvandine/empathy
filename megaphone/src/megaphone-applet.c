/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Raphaël Slinckx <raphael@slinckx.net>
 * Copyright (C) 2007 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Raphaël Slinckx <raphael@slinckx.net>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>
#include <panel-2.0/panel-applet-gconf.h>
#include <gconf/gconf-client.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-contact-list-view.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "megaphone-applet.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, MegaphoneApplet)
typedef struct {
	EmpathyContactFactory *factory;
	GtkWidget             *image;
	gint                   image_size;
	EmpathyContact        *contact;
	GConfClient           *gconf;
	guint                  gconf_cnxn;
} MegaphoneAppletPriv;

static void megaphone_applet_finalize                  (GObject            *object);
static void megaphone_applet_size_allocate_cb          (GtkWidget          *widget,
							GtkAllocation      *allocation,
							MegaphoneApplet    *applet);
static gboolean megaphone_applet_button_press_event_cb (GtkWidget          *widget,
							GdkEventButton     *event, 
							MegaphoneApplet    *applet);
static void megaphone_applet_information_cb            (BonoboUIComponent  *uic,
							MegaphoneApplet    *applet, 
							const gchar        *verb_name);
static void megaphone_applet_preferences_cb            (BonoboUIComponent  *uic,
							MegaphoneApplet    *applet, 
							const gchar        *verb_name);
static void megaphone_applet_about_cb                  (BonoboUIComponent  *uic,
							MegaphoneApplet    *applet, 
							const gchar        *verb_name);

G_DEFINE_TYPE(MegaphoneApplet, megaphone_applet, PANEL_TYPE_APPLET)

static const BonoboUIVerb megaphone_applet_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("information", megaphone_applet_information_cb),
	BONOBO_UI_UNSAFE_VERB ("preferences", megaphone_applet_preferences_cb),
	BONOBO_UI_UNSAFE_VERB ("about",       megaphone_applet_about_cb),
	BONOBO_UI_VERB_END
};

static const char* authors[] = {
	"Raphaël Slinckx <raphael@slinckx.net>", 
	"Xavier Claessens <xclaesse@gmail.com>", 
	NULL
};

static void
megaphone_applet_class_init (MegaphoneAppletClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = megaphone_applet_finalize;

	g_type_class_add_private (object_class, sizeof (MegaphoneAppletPriv));
	empathy_gtk_init ();
}

static void
megaphone_applet_init (MegaphoneApplet *applet)
{
	MegaphoneAppletPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (applet,
		MEGAPHONE_TYPE_APPLET, MegaphoneAppletPriv);

	applet->priv = priv;
	priv->factory = empathy_contact_factory_dup_singleton ();
	priv->gconf = gconf_client_get_default ();

	/* Image holds the contact avatar */
	priv->image = gtk_image_new ();
	gtk_widget_show (priv->image);
	gtk_container_add (GTK_CONTAINER (applet), priv->image);

	/* We want transparency */
	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_EXPAND_MINOR);
	panel_applet_set_background_widget (PANEL_APPLET (applet), GTK_WIDGET (applet));

	/* Listen for clicks on the applet to dispatch a channel */
	g_signal_connect (applet, "button-press-event",
			  G_CALLBACK (megaphone_applet_button_press_event_cb),
			  applet);

	/* Allow to resize our avatar when needed */
	g_signal_connect (applet, "size-allocate",
			  G_CALLBACK (megaphone_applet_size_allocate_cb),
			  applet);
}

static void
megaphone_applet_finalize (GObject *object)
{
	MegaphoneAppletPriv *priv = GET_PRIV (object);
	
	if (priv->contact) {
		g_object_unref (priv->contact);
	}
	g_object_unref (priv->factory);

	if (priv->gconf_cnxn != 0) {
		gconf_client_notify_remove (priv->gconf, priv->gconf_cnxn);
	}
	g_object_unref (priv->gconf);

	G_OBJECT_CLASS (megaphone_applet_parent_class)->finalize (object);
}

static void
megaphone_applet_update_icon (MegaphoneApplet *applet)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	EmpathyAvatar       *avatar = NULL;
	GdkPixbuf           *avatar_pixbuf;

	if (priv->contact) {
		avatar = empathy_contact_get_avatar (priv->contact);
	} else {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      GTK_STOCK_PREFERENCES,
					      GTK_ICON_SIZE_MENU);
		return;
	}

	if (!avatar) {
		gchar *avatar_token;

		/* Try to take avatar from cache */
		avatar_token = panel_applet_gconf_get_string (PANEL_APPLET (applet),
							      "avatar_token",
							      NULL);
		if (!EMP_STR_EMPTY (avatar_token)) {
			empathy_contact_load_avatar_cache (priv->contact, avatar_token);
			avatar = empathy_contact_get_avatar (priv->contact);
		}
		g_free (avatar_token);
	}

	if (avatar) {
		avatar_pixbuf = empathy_pixbuf_from_avatar_scaled (avatar,
								   priv->image_size - 2,
								   priv->image_size - 2);
	} else {
		GtkIconTheme *icon_theme;

		/* Load the default icon when no avatar is found */
		icon_theme = gtk_icon_theme_get_default ();
		avatar_pixbuf = gtk_icon_theme_load_icon (icon_theme,
							  "stock_contact",
							  priv->image_size - 2,
							  0, NULL);
	}

	/* Now some desaturation if the contact is offline */
	if (!empathy_contact_is_online (priv->contact)) {
		GdkPixbuf *offline_avatar;

		offline_avatar = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE,
						 8,
						 gdk_pixbuf_get_height (avatar_pixbuf),
						 gdk_pixbuf_get_width (avatar_pixbuf));
		gdk_pixbuf_saturate_and_pixelate (avatar_pixbuf,
						  offline_avatar,
						  0.0,
						  TRUE);
		g_object_unref (avatar_pixbuf);
		avatar_pixbuf = offline_avatar;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), avatar_pixbuf);
	g_object_unref (avatar_pixbuf);
}

static void
megaphone_applet_update_contact (MegaphoneApplet *applet)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	const gchar         *name;
	const gchar         *status;
	gchar               *tip;
	const gchar         *avatar_token = NULL;

	if (priv->contact) {
		EmpathyAvatar *avatar;

		avatar = empathy_contact_get_avatar (priv->contact);
		if (avatar) {
			avatar_token = avatar->token;
		}
	}

	if (avatar_token) {
		panel_applet_gconf_set_string (PANEL_APPLET (applet),
					       "avatar_token", avatar_token,
					       NULL);
	}

	megaphone_applet_update_icon (applet);

	if (priv->contact ) {
		name = empathy_contact_get_name (priv->contact);
		status = empathy_contact_get_status (priv->contact);
		tip = g_strdup_printf ("<b>%s</b>: %s", name, status);
		gtk_widget_set_tooltip_markup (GTK_WIDGET (applet), tip);
		g_free (tip);
	} else {
		gtk_widget_set_tooltip_markup (GTK_WIDGET (applet),
					       _("Please configure a contact."));
	}

}

static void
megaphone_applet_set_contact (MegaphoneApplet *applet,
			      const gchar     *str)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	McAccount           *account = NULL;
	gchar              **strv = NULL;

	DEBUG ("Setting new contact %s", str);

	/* Release old contact, if any */
	if (priv->contact) {
		g_signal_handlers_disconnect_by_func (priv->contact,
						      megaphone_applet_update_contact,
						      applet);
		g_object_unref (priv->contact),
		priv->contact = NULL;
	}

	/* Lookup the new contact */
	if (str) {
		strv = g_strsplit (str, "/", 2);
		account = mc_account_lookup (strv[0]);
	}
	if (account) {
		priv->contact = empathy_contact_factory_get_from_id (priv->factory,
								     account,
								     strv[1]);
		g_object_unref (account);
	}
	g_strfreev (strv);

	/* Take hold of the new contact if any */
	if (priv->contact) {
		/* Listen for updates on the contact, and force a first update */
		g_signal_connect_swapped (priv->contact, "notify",
					  G_CALLBACK (megaphone_applet_update_contact),
					  applet);
	}

	megaphone_applet_update_contact (applet);
}

static void
megaphone_applet_preferences_response_cb (GtkWidget       *dialog,
					  gint             response,
					  MegaphoneApplet *applet) 
{
	if (response == GTK_RESPONSE_ACCEPT) {
		EmpathyContactListView *contact_list;
		EmpathyContact         *contact;

		/* Retrieve the selected contact, if any and set it up in gconf.
		 * GConf will notify us from the change and we will adjust ourselves */
		contact_list = g_object_get_data (G_OBJECT (dialog), "contact-list");
		contact = empathy_contact_list_view_get_selected (contact_list);
		if (contact) {
			McAccount   *account;
			const gchar *account_id;
			const gchar *contact_id;
			gchar       *str;

			account = empathy_contact_get_account (contact);
			account_id = mc_account_get_unique_name (account);
			contact_id = empathy_contact_get_id (contact);

			str = g_strconcat (account_id, "/", contact_id, NULL);
			panel_applet_gconf_set_string (PANEL_APPLET (applet),
						       "avatar_token", "",
						       NULL);
			panel_applet_gconf_set_string (PANEL_APPLET (applet),
						       "contact_id", str,
						       NULL);
			g_free (str);
		}
	}
	gtk_widget_destroy (dialog);
}

static void
megaphone_applet_show_preferences (MegaphoneApplet *applet)
{
	GtkWidget               *dialog;
	GtkWidget               *scroll;
	EmpathyContactListView  *contact_list;
	EmpathyContactListStore *contact_store;
	EmpathyContactManager   *contact_manager;

	dialog = gtk_dialog_new_with_buttons (_("Select contact..."),
					      NULL, 0,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_REJECT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      NULL);

	/* Show all contacts, even offline and sort alphabetically */
	contact_manager = empathy_contact_manager_dup_singleton ();
	contact_store = empathy_contact_list_store_new (EMPATHY_CONTACT_LIST (contact_manager));
	g_object_set (contact_store,
		      "is-compact", TRUE,
		      "show-avatars", TRUE,
		      "show-offline", TRUE,
		      "sort-criterium", EMPATHY_CONTACT_LIST_STORE_SORT_NAME,
		      NULL);
	contact_list = empathy_contact_list_view_new (contact_store,
						      EMPATHY_CONTACT_LIST_FEATURE_NONE,
						      EMPATHY_CONTACT_FEATURE_NONE);
	g_object_unref (contact_manager);
	g_object_unref (contact_store);
	gtk_widget_show (GTK_WIDGET (contact_list));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 500);
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (contact_list));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), scroll);
	gtk_widget_show (scroll);
	
	g_object_set_data (G_OBJECT (dialog), "contact-list", contact_list);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (megaphone_applet_preferences_response_cb),
			  applet);

	gtk_widget_show (dialog);
}

static void
megaphone_applet_information_cb (BonoboUIComponent *uic,
				 MegaphoneApplet   *applet,
				 const gchar       *verb_name)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);

	/* FIXME: We should grey out the menu item if there are no available contact */
	if (priv->contact) {
		empathy_contact_information_dialog_show (priv->contact, NULL, FALSE, FALSE);
	}
}

static void
megaphone_applet_preferences_cb (BonoboUIComponent *uic, 
				 MegaphoneApplet   *applet, 
				 const gchar       *verb_name)
{
	megaphone_applet_show_preferences (applet);
}

static void
megaphone_applet_about_cb (BonoboUIComponent *uic, 
			   MegaphoneApplet   *applet, 
			   const gchar       *verb_name)
{
	gtk_show_about_dialog (NULL,
			       "name", "Megaphone", 
			       "version", PACKAGE_VERSION,
			       "copyright", "Raphaël Slinckx 2007\nCollabora Ltd 2007",
			       "comments", _("Talk!"),
			       "authors", authors,
			       "logo-icon-name", "stock_people",
			       NULL);
}

static gboolean
megaphone_applet_button_press_event_cb (GtkWidget       *widget,
					GdkEventButton  *event, 
					MegaphoneApplet *applet)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	MissionControl      *mc;

	/* Only react on left-clicks */
	if (event->button != 1 || event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	/* If the contact is unavailable we display the preferences dialog */
	if (priv->contact == NULL) {
		megaphone_applet_show_preferences (applet);
		return TRUE;
	}
	
	DEBUG ("Requesting text channel for contact %s (%d)",
		empathy_contact_get_id (priv->contact),
		empathy_contact_get_handle (priv->contact));

	mc = empathy_mission_control_dup_singleton ();
	mission_control_request_channel (mc,
					 empathy_contact_get_account (priv->contact),
					 TP_IFACE_CHANNEL_TYPE_TEXT,
					 empathy_contact_get_handle (priv->contact),
					 TP_HANDLE_TYPE_CONTACT,
					 NULL, NULL);
	g_object_unref (mc);
	
	return TRUE;
}

static void
megaphone_applet_size_allocate_cb (GtkWidget       *widget,
				   GtkAllocation   *allocation,
				   MegaphoneApplet *applet)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	gint                 size;
	PanelAppletOrient    orient;

	orient = panel_applet_get_orient (PANEL_APPLET (widget));
	if (orient == PANEL_APPLET_ORIENT_LEFT ||
	    orient == PANEL_APPLET_ORIENT_RIGHT) {
		size = allocation->width;
	} else {
		size = allocation->height;
	}

	if (size != priv->image_size) {
		priv->image_size = size;
		megaphone_applet_update_icon (applet);
	}
}

static void
megaphone_applet_gconf_notify_cb (GConfClient     *client,
				  guint            cnxn_id,
				  GConfEntry      *entry,
				  MegaphoneApplet *applet)
{
	const gchar *key;
	GConfValue  *value;

	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);
	DEBUG ("GConf notification for key '%s'", key);

	if (value && g_str_has_suffix (key, "/contact_id")) {
		megaphone_applet_set_contact (applet,
					      gconf_value_get_string (value));
	}
}

static gboolean
megaphone_applet_factory (PanelApplet *applet, 
			  const gchar *iid, 
			  gpointer     data)
{
	MegaphoneAppletPriv *priv = GET_PRIV (applet);
	gchar               *pref_dir;
	gchar               *contact_id;

	/* Ensure it's us! */
	if (strcmp (iid, "OAFIID:GNOME_Megaphone_Applet") != 0) {
		return FALSE;
	}
	
	DEBUG ("Starting up new instance!");

	/* Set up the right-click menu */
	panel_applet_setup_menu_from_file (applet,
					   PKGDATADIR,
					   "GNOME_Megaphone_Applet.xml",
					   NULL,
					   megaphone_applet_menu_verbs,
					   applet);

	/* Define the schema to be used for each applet instance's preferences */
	panel_applet_add_preferences (applet,
				      "/schemas/apps/megaphone-applet/prefs",
				      NULL);
	
	/* We watch the preferences directory */
	pref_dir = panel_applet_gconf_get_full_key (applet, "");
	pref_dir[strlen (pref_dir)-1] = '\0';
	gconf_client_add_dir (priv->gconf, pref_dir,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
	gconf_client_notify_add (priv->gconf, pref_dir,
				 (GConfClientNotifyFunc) megaphone_applet_gconf_notify_cb,
				 applet,
				 NULL, NULL);
	g_free (pref_dir);

	/* Initial setup with the pre-existing gconf key, or contact_id=NULL if no previous pref */
	contact_id = panel_applet_gconf_get_string (PANEL_APPLET (applet), "contact_id", NULL);
	megaphone_applet_set_contact (MEGAPHONE_APPLET (applet), contact_id);
	g_free (contact_id);

	/* Let's go! */
	gtk_widget_show (GTK_WIDGET (applet));

	return TRUE;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Megaphone_Applet_Factory",
			     MEGAPHONE_TYPE_APPLET,
			     "Megaphone", PACKAGE_VERSION,
			     megaphone_applet_factory,
			     NULL);

