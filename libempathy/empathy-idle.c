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

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libtelepathy/tp-helpers.h>

#include "empathy-idle.h"
#include "gossip-utils.h" 
#include "gossip-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_IDLE, EmpathyIdlePriv))

#define DEBUG_DOMAIN "Idle"

/* Number of seconds before entering extended autoaway. */
#define EXT_AWAY_TIME (30*60)

typedef enum {
	NM_STATE_UNKNOWN = 0,
	NM_STATE_ASLEEP,
	NM_STATE_CONNECTING,
	NM_STATE_CONNECTED,
	NM_STATE_DISCONNECTED
} NMState;

struct _EmpathyIdlePriv {
	MissionControl *mc;
	DBusGProxy     *gs_proxy;
	DBusGProxy     *nm_proxy;
	gboolean        is_idle;
	McPresence      state;
	McPresence      flash_state;
	gchar          *status;
	McPresence      saved_state;
	gchar          *saved_status;
	gboolean        nm_connected;
	guint           ext_away_timeout;
};

static void     empathy_idle_class_init      (EmpathyIdleClass *klass);
static void     empathy_idle_init            (EmpathyIdle      *idle);
static void     idle_finalize                (GObject          *object);
static void     idle_get_property            (GObject          *object,
					      guint             param_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void     idle_set_property            (GObject          *object,
					      guint             param_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void     idle_presence_changed_cb     (MissionControl   *mc,
					      McPresence        state,
					      EmpathyIdle      *idle);
static void     idle_session_idle_changed_cb (DBusGProxy       *gs_proxy,
					      gboolean          is_idle,
					      EmpathyIdle      *idle);
static void     idle_nm_state_change_cb      (DBusGProxy       *proxy,
					      guint             state,
					      EmpathyIdle      *idle);
static void     idle_ext_away_start          (EmpathyIdle      *idle);
static void     idle_ext_away_stop           (EmpathyIdle      *idle);
static gboolean idle_ext_away_cb             (EmpathyIdle      *idle);

enum {
	PROP_0,
	PROP_STATE,
	PROP_STATUS,
	PROP_FLASH_STATE
};

G_DEFINE_TYPE (EmpathyIdle, empathy_idle, G_TYPE_OBJECT)

static void
empathy_idle_class_init (EmpathyIdleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = idle_finalize;
	object_class->get_property = idle_get_property;
	object_class->set_property = idle_set_property;

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_uint ("state",
							    "state",
							    "state",
							    MC_PRESENCE_UNSET,
							    LAST_MC_PRESENCE,
							    MC_PRESENCE_AVAILABLE,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "status",
							      "status",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FLASH_STATE,
					 g_param_spec_uint ("flash-state",
							    "flash-state",
							    "flash-state",
							    MC_PRESENCE_UNSET,
							    LAST_MC_PRESENCE,
							    MC_PRESENCE_UNSET,
							    G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyIdlePriv));
}

static void
empathy_idle_init (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;
	DBusGConnection *system_bus;
	GError          *error = NULL;

	priv = GET_PRIV (idle);

	priv->is_idle = FALSE;
	priv->mc = gossip_mission_control_new ();
	priv->state = mission_control_get_presence_actual (priv->mc, NULL);
	idle_presence_changed_cb (priv->mc, priv->state, idle);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "PresenceStatusActual",
				     G_CALLBACK (idle_presence_changed_cb),
				     idle, NULL);

	priv->gs_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						    "org.gnome.ScreenSaver",
						    "/org/gnome/ScreenSaver",
						    "org.gnome.ScreenSaver");
	if (priv->gs_proxy) {
		dbus_g_proxy_add_signal (priv->gs_proxy, "SessionIdleChanged",
					 G_TYPE_BOOLEAN,
					 G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->gs_proxy, "SessionIdleChanged",
					     G_CALLBACK (idle_session_idle_changed_cb),
					     idle, NULL);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Failed to get gs proxy");
	}


	system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!system_bus) {
		gossip_debug (DEBUG_DOMAIN, "Failed to get system bus: %s",
			      error ? error->message : "No error given");
	} else {
		priv->nm_proxy = dbus_g_proxy_new_for_name (system_bus,
							    "org.freedesktop.NetworkManager",
							    "/org/freedesktop/NetworkManager",
							    "org.freedesktop.NetworkManager");
	}
	if (priv->nm_proxy) {
		dbus_g_proxy_add_signal (priv->nm_proxy, "StateChange",
					 G_TYPE_UINT, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->nm_proxy, "StateChange",
					     G_CALLBACK (idle_nm_state_change_cb),
					     idle, NULL);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Failed to get nm proxy");
	}
	/* FIXME: get value */
	priv->nm_connected = TRUE;
}

static void
idle_finalize (GObject *object)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->status);
	g_free (priv->saved_status);
	g_object_unref (priv->mc);

	if (priv->gs_proxy) {
		g_object_unref (priv->gs_proxy);
	}

	idle_ext_away_stop (EMPATHY_IDLE (object));
}

static void
idle_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		g_value_set_uint (value, empathy_idle_get_state (idle));
		break;
	case PROP_STATUS:
		g_value_set_string (value, empathy_idle_get_status (idle));
		break;
	case PROP_FLASH_STATE:
		g_value_set_uint (value, empathy_idle_get_flash_state (idle));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
idle_set_property (GObject      *object,
		   guint         param_id,
		   const GValue *value,
		   GParamSpec   *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		empathy_idle_set_state (idle, g_value_get_uint (value));
		break;
	case PROP_STATUS:
		empathy_idle_set_status (idle, g_value_get_string (value));
		break;
	case PROP_FLASH_STATE:
		empathy_idle_set_flash_state (idle, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
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

McPresence
empathy_idle_get_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->state;
}

void
empathy_idle_set_state (EmpathyIdle *idle,
			McPresence   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, state, priv->status);
}

const gchar *
empathy_idle_get_status (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (!priv->status) {
		return gossip_presence_state_get_default_status (priv->state);
	}

	return priv->status;
}

void
empathy_idle_set_status (EmpathyIdle *idle,
			 const gchar *status)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, priv->state, status);
}

McPresence
empathy_idle_get_flash_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->flash_state;
}

void
empathy_idle_set_flash_state (EmpathyIdle *idle,
			      McPresence   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	priv->flash_state = state;

	if (state == MC_PRESENCE_UNSET) {
	}

	g_object_notify (G_OBJECT (idle), "flash-state");
}

void
empathy_idle_set_presence (EmpathyIdle *idle,
			   McPresence   state,
			   const gchar *status)
{
	EmpathyIdlePriv *priv;
	const gchar     *default_status;

	priv = GET_PRIV (idle);

	if (!priv->nm_connected) {
		g_free (priv->saved_status);
		priv->saved_state = state;
		priv->saved_status = g_strdup (status);
		return;
	}

	/* Do not set translated default messages */
	default_status = gossip_presence_state_get_default_status (state);
	if (status && strcmp (status, default_status) == 0) {
		status = NULL;
	}

	mission_control_set_presence (priv->mc,
				      state,
				      status,
				      NULL, NULL);
}

static void
idle_presence_changed_cb (MissionControl *mc,
			  McPresence      state,
			  EmpathyIdle    *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	g_free (priv->status);
	priv->state = state;
	priv->status = mission_control_get_presence_message_actual (priv->mc, NULL);

	if (G_STR_EMPTY (priv->status)) {
		g_free (priv->status);
		priv->status = NULL;
	}

	g_object_notify (G_OBJECT (idle), "state");
	g_object_notify (G_OBJECT (idle), "status");
}

static void
idle_session_idle_changed_cb (DBusGProxy  *gs_proxy,
			      gboolean     is_idle,
			      EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	gossip_debug (DEBUG_DOMAIN, "Session idle state changed, %s -> %s",
		      priv->is_idle ? "yes" : "no",
		      is_idle ? "yes" : "no");

	if (priv->state <= MC_PRESENCE_OFFLINE ||
	    priv->state == MC_PRESENCE_HIDDEN) {
		/* We are not online so nothing to do here */
		priv->is_idle = is_idle;
		return;
	}

	if (is_idle && !priv->is_idle) {
		McPresence new_state;
		/* We are now idle */

		if (priv->state == MC_PRESENCE_AWAY ||
			   priv->state == MC_PRESENCE_EXTENDED_AWAY) {
			/* User set away manually, when coming back we restore
			 * default presence. */
			new_state = priv->state;
			priv->saved_state = MC_PRESENCE_AVAILABLE;
			priv->saved_status = NULL;
		} else {
			new_state = MC_PRESENCE_AWAY;
			priv->saved_state = priv->state;
			priv->saved_status = g_strdup (priv->status);
		}

		gossip_debug (DEBUG_DOMAIN, "Going to autoaway");
		empathy_idle_set_state (idle, new_state);

		idle_ext_away_start (idle);
	} else if (!is_idle && priv->is_idle) {
		/* We are no more idle, restore state */
		idle_ext_away_stop (idle);

		gossip_debug (DEBUG_DOMAIN, "Restoring state to %d %s",
			      priv->saved_state,
			      priv->saved_status);

		empathy_idle_set_presence (idle,
					   priv->saved_state,
					   priv->saved_status);

		g_free (priv->saved_status);
		priv->saved_status = NULL;
	}

	priv->is_idle = is_idle;
}

static void
idle_nm_state_change_cb (DBusGProxy  *proxy,
			 guint        state,
			 EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	gossip_debug (DEBUG_DOMAIN, "New network state (%d)", state);

	if (state != NM_STATE_CONNECTED &&
	    priv->state > MC_PRESENCE_OFFLINE) {
		/* We are no more connected */
		idle_ext_away_stop (idle);
		g_free (priv->saved_status);
		priv->saved_state = priv->state;
		priv->saved_status = g_strdup (priv->status);

		empathy_idle_set_state (idle, MC_PRESENCE_OFFLINE);
		priv->nm_connected = FALSE;
	}
	else if (priv->state <= MC_PRESENCE_OFFLINE &&
		 state == NM_STATE_CONNECTED) {
		/* We are now connected */
		priv->nm_connected = TRUE;
		empathy_idle_set_presence (idle,
					   priv->saved_state,
					   priv->saved_status);
	}
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
	empathy_idle_set_state (idle, MC_PRESENCE_EXTENDED_AWAY);
	priv->ext_away_timeout = 0;

	return FALSE;
}

