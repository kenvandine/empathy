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

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mission-control.h>

#include "empathy-idle.h"
#include "gossip-utils.h" 
#include "gossip-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_IDLE, EmpathyIdlePriv))

#define DEBUG_DOMAIN "Idle"

/* Number of seconds before entering extended autoaway. */
#define EXT_AWAY_TIME (30*60)

enum {
	LAST_SIGNAL
};

struct _EmpathyIdlePriv {
	MissionControl *mc;
	DBusGProxy     *gs_proxy;
	gboolean        is_active;
	McPresence      last_state;
	gchar          *last_status;
	guint           ext_away_timeout;
};

static void     empathy_idle_class_init (EmpathyIdleClass *klass);
static void     empathy_idle_init       (EmpathyIdle      *idle);
static void     idle_finalize           (GObject          *object);
static void     idle_active_changed_cb  (DBusGProxy       *gs_proxy,
					 gboolean          is_active,
					 EmpathyIdle      *idle);
static void     idle_ext_away_start     (EmpathyIdle      *idle);
static void     idle_ext_away_stop      (EmpathyIdle      *idle);
static gboolean idle_ext_away_cb        (EmpathyIdle      *idle);

//static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyIdle, empathy_idle, G_TYPE_OBJECT)

static void
empathy_idle_class_init (EmpathyIdleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = idle_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyIdlePriv));
}

static void
empathy_idle_init (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	priv->is_active = FALSE;
	priv->mc = gossip_mission_control_new ();
	priv->gs_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						    "org.gnome.ScreenSaver",
						    "/org/gnome/ScreenSaver",
						    "org.gnome.ScreenSaver");
	if (!priv->gs_proxy) {
		gossip_debug (DEBUG_DOMAIN, "Failed to get gs proxy");
		return;
	}

	dbus_g_proxy_add_signal (priv->gs_proxy, "ActiveChanged",
				 G_TYPE_BOOLEAN,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->gs_proxy, "ActiveChanged",
				     G_CALLBACK (idle_active_changed_cb),
				     idle, NULL);
}

static void
idle_finalize (GObject *object)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->last_status);
	g_object_unref (priv->mc);

	if (priv->gs_proxy) {
		g_object_unref (priv->gs_proxy);
	}

	idle_ext_away_stop (EMPATHY_IDLE (object));
}

EmpathyIdle *
empathy_idle_new (void)
{
	static EmpathyIdle *idle = NULL;

	if (!idle) {
		idle = g_object_new (EMPATHY_TYPE_IDLE, NULL);
		g_object_add_weak_pointer (G_OBJECT (idle), (gpointer) &idle);
	} else {
		g_object_ref (idle);
	}

	return idle;
}

static void
idle_active_changed_cb (DBusGProxy  *gs_proxy,
			gboolean     is_active,
			EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	gossip_debug (DEBUG_DOMAIN, "Screensaver state changed, %s -> %s",
		      priv->is_active ? "yes" : "no",
		      is_active ? "yes" : "no");

	if (is_active && !priv->is_active) {
		/* The screensaver is now running */
		g_free (priv->last_status);
		idle_ext_away_stop (idle);

		priv->last_state = mission_control_get_presence_actual (priv->mc, NULL);
		priv->last_status = mission_control_get_presence_message_actual (priv->mc, NULL);

		gossip_debug (DEBUG_DOMAIN, "Going to autoaway");
		mission_control_set_presence (priv->mc,
					      MC_PRESENCE_AWAY,
					      _("Autoaway"),
					      NULL, NULL);
		idle_ext_away_start (idle);
	} else if (!is_active && priv->is_active) {
		/* The screensaver stoped */
		idle_ext_away_stop (idle);

		gossip_debug (DEBUG_DOMAIN, "Restoring state to %d %s",
			      priv->last_state,
			      priv->last_status);

		mission_control_set_presence (priv->mc,
					      priv->last_state,
					      priv->last_status,
					      NULL, NULL);
	}

	priv->is_active = is_active;
}

static void
idle_ext_away_start (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	idle_ext_away_stop (idle);
	priv->ext_away_timeout = g_timeout_add (EXT_AWAY_TIME * 1000,
						(GSourceFunc) idle_ext_away_cb,
						idle);
}

static void
idle_ext_away_stop (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (priv->ext_away_timeout) {
		g_source_remove (priv->ext_away_timeout);
		priv->ext_away_timeout = 0;
	}
}

static gboolean
idle_ext_away_cb (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	gossip_debug (DEBUG_DOMAIN, "Going to extended autoaway");
	mission_control_set_presence (priv->mc,
				      MC_PRESENCE_EXTENDED_AWAY,
				      _("Extended autoaway"),
				      NULL, NULL);

	priv->ext_away_timeout = 0;

	return FALSE;
}

