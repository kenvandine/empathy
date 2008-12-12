/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Raphaël Slinckx <raphael@slinckx.net>
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
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>

#include <libmissioncontrol/mission-control.h>
#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "nothere-applet.h"

G_DEFINE_TYPE(NotHereApplet, nothere_applet, PANEL_TYPE_APPLET)

static void nothere_applet_destroy  (GtkObject         *object);
static void nothere_applet_about_cb (BonoboUIComponent *uic,
				     NotHereApplet     *applet, 
				     const gchar       *verb_name);

static const BonoboUIVerb nothere_applet_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("about", nothere_applet_about_cb),
	BONOBO_UI_VERB_END
};

static const char* authors[] = {
	"Raphaël Slinckx <raphael@slinckx.net>", 
	NULL
};

static void
nothere_applet_class_init (NotHereAppletClass *class)
{
	GTK_OBJECT_CLASS (class)->destroy = nothere_applet_destroy;

	empathy_gtk_init ();
}

static gboolean
do_not_eat_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
        if (event->button != 1) {
                g_signal_stop_emission_by_name (widget, "button_press_event");
        }

        return FALSE;
}

static void
nothere_applet_init (NotHereApplet *applet)
{
	applet->presence_chooser = empathy_presence_chooser_new ();
	g_signal_connect (G_OBJECT (applet->presence_chooser), "button_press_event",
                          G_CALLBACK (do_not_eat_button_press), NULL);

	gtk_widget_show (applet->presence_chooser);

	gtk_container_add (GTK_CONTAINER (applet), applet->presence_chooser);

	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_EXPAND_MINOR);
	panel_applet_set_background_widget (PANEL_APPLET (applet), GTK_WIDGET (applet));
}

static void
nothere_applet_destroy (GtkObject *object)
{
	NotHereApplet *applet = NOTHERE_APPLET (object);

	applet->presence_chooser = NULL;

	(* GTK_OBJECT_CLASS (nothere_applet_parent_class)->destroy) (object);
}

static void
nothere_applet_about_cb (BonoboUIComponent *uic, 
			 NotHereApplet     *applet, 
			 const gchar       *verb_name)
{
	gtk_show_about_dialog (NULL,
			       "name", "Presence", 
			       "version", PACKAGE_VERSION,
			       "copyright", "Copyright \xc2\xa9 2007 Raphaël Slinckx",
			       "comments", _("Set your own presence"),
			       "authors", authors,
			       "logo-icon-name", "stock_people",
			       NULL);
}

static gboolean
nothere_applet_factory (PanelApplet *applet, 
			const gchar *iid, 
			gpointer     data)
{
	if (strcmp (iid, "OAFIID:GNOME_NotHere_Applet") != 0) {
		return FALSE;
	}
	
	/* Set up the menu */
	panel_applet_setup_menu_from_file (applet,
					   PKGDATADIR,
					   "GNOME_NotHere_Applet.xml",
					   NULL,
					   nothere_applet_menu_verbs,
					   applet);

	gtk_widget_show (GTK_WIDGET (applet));
	return TRUE;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_NotHere_Applet_Factory",
			     NOTHERE_TYPE_APPLET,
			     "Presence", PACKAGE_VERSION,
			     nothere_applet_factory,
			     NULL);
