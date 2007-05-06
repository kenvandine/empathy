/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include "gossip-presence.h"
#include "gossip-time.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_PRESENCE, GossipPresencePriv))

typedef struct _GossipPresencePriv GossipPresencePriv;

struct _GossipPresencePriv {
	McPresence  state;
	gchar      *status;
	GossipTime  timestamp;
};

static void         presence_finalize     (GObject      *object);
static void         presence_get_property (GObject      *object,
					   guint         param_id,
					   GValue       *value,
					   GParamSpec   *pspec);
static void         presence_set_property (GObject      *object,
					   guint         param_id,
					   const GValue *value,
					   GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_STATE,
	PROP_STATUS
};

G_DEFINE_TYPE (GossipPresence, gossip_presence, G_TYPE_OBJECT);

static void
gossip_presence_class_init (GossipPresenceClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = presence_finalize;
	object_class->get_property = presence_get_property;
	object_class->set_property = presence_set_property;

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_int ("state",
							   "Presence State",
							   "The current state of the presence",
							   MC_PRESENCE_UNSET,
							   LAST_MC_PRESENCE,
							   MC_PRESENCE_AVAILABLE,
							   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Presence Status",
							      "Status string set on presence",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipPresencePriv));
}

static void
gossip_presence_init (GossipPresence *presence)
{
	GossipPresencePriv *priv;

	priv = GET_PRIV (presence);

	priv->state = MC_PRESENCE_AVAILABLE;
	priv->status = NULL;
	priv->timestamp = gossip_time_get_current ();
}

static void
presence_finalize (GObject *object)
{
	GossipPresencePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->status);

	(G_OBJECT_CLASS (gossip_presence_parent_class)->finalize) (object);
}

static void
presence_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	GossipPresencePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_STATE:
		g_value_set_int (value, priv->state);
		break;
	case PROP_STATUS:
		g_value_set_string (value,
				    gossip_presence_get_status (GOSSIP_PRESENCE (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
presence_set_property (GObject      *object,
		       guint         param_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
	GossipPresencePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_STATE:
		priv->state = g_value_get_int (value);
		break;
	case PROP_STATUS:
		gossip_presence_set_status (GOSSIP_PRESENCE (object),
					    g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

GossipPresence *
gossip_presence_new (void)
{
	return g_object_new (GOSSIP_TYPE_PRESENCE, NULL);
}

GossipPresence *
gossip_presence_new_full (McPresence   state,
			  const gchar *status)
{
	return g_object_new (GOSSIP_TYPE_PRESENCE,
			     "state", state,
			     "status", status,
			     NULL);
}

const gchar *
gossip_presence_get_status (GossipPresence *presence)
{
	GossipPresencePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRESENCE (presence),
			      _("Offline"));

	priv = GET_PRIV (presence);

	return priv->status;
}

McPresence
gossip_presence_get_state (GossipPresence *presence)
{
	GossipPresencePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRESENCE (presence),
			      MC_PRESENCE_AVAILABLE);

	priv = GET_PRIV (presence);

	return priv->state;
}

void
gossip_presence_set_state (GossipPresence *presence,
			   McPresence      state)
{
	GossipPresencePriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (presence);

	priv->state = state;

	g_object_notify (G_OBJECT (presence), "state");
}

void
gossip_presence_set_status (GossipPresence *presence,
			    const gchar    *status)
{
	GossipPresencePriv *priv;

	priv = GET_PRIV (presence);
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	g_free (priv->status);

	if (status) {
		priv->status = g_strdup (status);
	} else {
		priv->status = NULL;
	}

	g_object_notify (G_OBJECT (presence), "status");
}

gint
gossip_presence_sort_func (gconstpointer a,
			   gconstpointer b)
{
	GossipPresencePriv *priv_a;
	GossipPresencePriv *priv_b;
	gint                diff;

	g_return_val_if_fail (GOSSIP_IS_PRESENCE (a), 0);
	g_return_val_if_fail (GOSSIP_IS_PRESENCE (b), 0);
	 
	priv_a = GET_PRIV (a);
	priv_b = GET_PRIV (b);

	/* 1. State */
	diff = priv_a->state - priv_b->state;
	if (diff != 0) {
		return diff < 1 ? -1 : +1;
	}

	/* 3. Time (newest first) */
	diff = priv_b->timestamp - priv_a->timestamp;
	if (diff != 0) {
		return diff < 1 ? -1 : +1;
	}
		
	/* No real difference */
	return 0;
}

const gchar *
gossip_presence_state_get_default_status (McPresence state)
{
	switch (state) {
	case MC_PRESENCE_AVAILABLE:
		return _("Available");
	case MC_PRESENCE_DO_NOT_DISTURB:
		return _("Busy");
	case MC_PRESENCE_AWAY:
	case MC_PRESENCE_EXTENDED_AWAY:
		return _("Away");
	case MC_PRESENCE_HIDDEN:
		return _("Unavailable");
	case MC_PRESENCE_OFFLINE:
		return _("Offline");
	case MC_PRESENCE_UNSET:
		return _("Unset");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

const gchar *
gossip_presence_state_to_str (McPresence state)
{
	switch (state) {
	case MC_PRESENCE_AVAILABLE:
		return "available";
	case MC_PRESENCE_DO_NOT_DISTURB:
		return "busy";
	case MC_PRESENCE_AWAY:
		return "away";
	case MC_PRESENCE_EXTENDED_AWAY:
		return "ext_away";
	case MC_PRESENCE_HIDDEN:
		return "hidden";
	case MC_PRESENCE_OFFLINE:
		return "offline";
	case MC_PRESENCE_UNSET:
		return "unset";
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

McPresence
gossip_presence_state_from_str (const gchar *str)
{
	if (strcmp (str, "available") == 0) {
		return MC_PRESENCE_AVAILABLE;
	} else if ((strcmp (str, "dnd") == 0) || (strcmp (str, "busy") == 0)) {
		return MC_PRESENCE_DO_NOT_DISTURB;
	} else if ((strcmp (str, "away") == 0) || (strcmp (str, "brb") == 0)) {
		return MC_PRESENCE_AWAY;
	} else if ((strcmp (str, "xa") == 0) || (strcmp (str, "ext_away") == 0)) {
		return MC_PRESENCE_EXTENDED_AWAY;
	} else if (strcmp (str, "hidden") == 0) {
		return MC_PRESENCE_HIDDEN;
	} else if (strcmp (str, "offline") == 0) {
		return MC_PRESENCE_OFFLINE;
	} else if (strcmp (str, "unset") == 0) {
		return MC_PRESENCE_UNSET;
	}

	return MC_PRESENCE_AVAILABLE;
}

