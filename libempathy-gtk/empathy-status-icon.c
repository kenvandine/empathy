/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>

#include "empathy-status-icon.h"
#include "gossip-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv))

#define DEBUG_DOMAIN "StatusIcon"

struct _EmpathyStatusIconPriv {
	MissionControl *mc;
	GtkStatusIcon  *icon;
};

static void empathy_status_icon_class_init  (EmpathyStatusIconClass          *klass);
static void empathy_status_icon_init        (EmpathyStatusIcon               *icon);
static void status_icon_finalize            (GObject                         *object);
static void status_icon_presence_changed_cb (MissionControl                  *mc,
					     McPresence                       state,
					     EmpathyStatusIcon               *icon);
static void status_icon_activate_cb         (GtkStatusIcon                   *status_icon,
					     EmpathyStatusIcon               *icon);

enum {
	ACTIVATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

static void
empathy_status_icon_class_init (EmpathyStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	signals[ACTIVATE] =
		g_signal_new ("activate",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyStatusIconPriv));
}

static void
empathy_status_icon_init (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	McPresence             state;

	priv = GET_PRIV (icon);

	priv->mc = mission_control_new (tp_get_bus ());
	priv->icon = gtk_status_icon_new ();

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "PresenceStatusActual",
				     G_CALLBACK (status_icon_presence_changed_cb),
				     icon, NULL);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);

	state = mission_control_get_presence_actual (priv->mc, NULL);
	status_icon_presence_changed_cb (priv->mc, state, icon);
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (object);

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
					"PresenceStatusActual",
					G_CALLBACK (status_icon_presence_changed_cb),
					object);
	g_signal_handlers_disconnect_by_func (priv->icon,
					      status_icon_activate_cb,
					      object);

	g_object_unref (priv->mc);
	g_object_unref (priv->icon);
}

EmpathyStatusIcon *
empathy_status_icon_new (void)
{
	return g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
}

static void
status_icon_presence_changed_cb (MissionControl    *mc,
				 McPresence         state,
				 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	const gchar           *icon_name;
	gchar                 *status;

	priv = GET_PRIV (icon);

	icon_name = gossip_icon_name_for_presence_state (state);
	status = mission_control_get_presence_message_actual (priv->mc, NULL);
	if (G_STR_EMPTY (status)) {
		g_free (status);
		status = g_strdup (gossip_presence_state_get_default_status (state));
	}

	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
	gtk_status_icon_set_tooltip (priv->icon, status);

	g_free (status);
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	g_signal_emit (icon, signals[ACTIVATE], 0);
}

