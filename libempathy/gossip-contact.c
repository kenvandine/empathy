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
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include "gossip-contact.h"
#include "gossip-utils.h"
#include "gossip-debug.h"
#include "empathy-contact-manager.h"

#define DEBUG_DOMAIN "Contact"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT, GossipContactPriv))

typedef struct _GossipContactPriv GossipContactPriv;

struct _GossipContactPriv {
	gchar              *id;
	gchar              *name;
	GossipAvatar       *avatar;
	McAccount          *account;
	GossipPresence     *presence;
	GList              *groups;
	GossipSubscription  subscription;
	guint               handle;
};

static void contact_class_init    (GossipContactClass *class);
static void contact_init          (GossipContact      *contact);
static void contact_finalize      (GObject            *object);
static void contact_get_property  (GObject            *object,
				   guint               param_id,
				   GValue             *value,
				   GParamSpec         *pspec);
static void contact_set_property  (GObject            *object,
				   guint               param_id,
				   const GValue       *value,
				   GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_AVATAR,
	PROP_ACCOUNT,
	PROP_PRESENCE,
	PROP_GROUPS,
	PROP_SUBSCRIPTION,
	PROP_HANDLE
};

static gpointer parent_class = NULL;

GType
gossip_contact_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipContactClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) contact_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipContact),
			0,    /* n_preallocs */
			(GInstanceInitFunc) contact_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipContact",
					       &info, 0);
	}

	return type;
}

static void
contact_class_init (GossipContactClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = contact_finalize;
	object_class->get_property = contact_get_property;
	object_class->set_property = contact_set_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Contact id",
							      "String identifying contact",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Contact Name",
							      "The name of the contact",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AVATAR,
					 g_param_spec_boxed ("avatar",
							     "Avatar image",
							     "The avatar image",
							     GOSSIP_TYPE_AVATAR,
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Contact Account",
							      "The account associated with the contact",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PRESENCE,
					 g_param_spec_object ("presence",
							      "Contact presence",
							      "Presence of contact",
							      GOSSIP_TYPE_PRESENCE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_GROUPS,
					 g_param_spec_pointer ("groups",
							       "Contact groups",
							       "Groups of contact",
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SUBSCRIPTION,
					 g_param_spec_int ("subscription",
							   "Contact Subscription",
							   "The subscription status of the contact",
							   GOSSIP_SUBSCRIPTION_NONE,
							   GOSSIP_SUBSCRIPTION_BOTH,
							   GOSSIP_SUBSCRIPTION_NONE,
							   G_PARAM_READWRITE));


	g_object_class_install_property (object_class,
					 PROP_HANDLE,
					 g_param_spec_uint ("handle",
							    "Contact Handle",
							    "The handle of the contact",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipContactPriv));
}

static void
contact_init (GossipContact *contact)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (contact);

	priv->id       = NULL;
	priv->name     = NULL;
	priv->avatar   = NULL;
	priv->account  = NULL;
	priv->presence = NULL;
	priv->groups   = NULL;
	priv->handle   = 0;
}

static void
contact_finalize (GObject *object)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	gossip_debug (DEBUG_DOMAIN, "finalize: %p", object);

	g_free (priv->name);
	g_free (priv->id);

	if (priv->avatar) {
		gossip_avatar_unref (priv->avatar);
	}

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->groups) {
		g_list_foreach (priv->groups, (GFunc) g_free, NULL);
		g_list_free (priv->groups);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
contact_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value,
				    gossip_contact_get_id (GOSSIP_CONTACT (object)));
		break;
	case PROP_NAME:
		g_value_set_string (value,
				    gossip_contact_get_name (GOSSIP_CONTACT (object)));
		break;
	case PROP_AVATAR:
		g_value_set_boxed (value, priv->avatar);
		break;
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_PRESENCE:
		g_value_set_object (value, priv->presence);
		break;
	case PROP_GROUPS:
		g_value_set_pointer (value, priv->groups);
		break;
	case PROP_SUBSCRIPTION:
		g_value_set_int (value, priv->subscription);
		break;
	case PROP_HANDLE:
		g_value_set_uint (value, priv->handle);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		gossip_contact_set_id (GOSSIP_CONTACT (object),
				       g_value_get_string (value));
		break;
	case PROP_NAME:
		gossip_contact_set_name (GOSSIP_CONTACT (object),
					 g_value_get_string (value));
		break;
	case PROP_AVATAR:
		gossip_contact_set_avatar (GOSSIP_CONTACT (object),
					   g_value_get_boxed (value));
		break;
	case PROP_ACCOUNT:
		gossip_contact_set_account (GOSSIP_CONTACT (object),
					    MC_ACCOUNT (g_value_get_object (value)));
		break;
	case PROP_PRESENCE:
		gossip_contact_set_presence (GOSSIP_CONTACT (object),
					     GOSSIP_PRESENCE (g_value_get_object (value)));
		break;
	case PROP_GROUPS:
		gossip_contact_set_groups (GOSSIP_CONTACT (object),
					   g_value_get_pointer (value));
		break;
	case PROP_SUBSCRIPTION:
		gossip_contact_set_subscription (GOSSIP_CONTACT (object),
						 g_value_get_int (value));
		break;
	case PROP_HANDLE:
		gossip_contact_set_handle (GOSSIP_CONTACT (object),
					   g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipContact *
gossip_contact_new (McAccount *account)
{
	return g_object_new (GOSSIP_TYPE_CONTACT,
			     "account", account,
			     NULL);
}

GossipContact *
gossip_contact_new_full (McAccount   *account,
			 const gchar *id,
			 const gchar *name)
{
	return g_object_new (GOSSIP_TYPE_CONTACT,
			     "account", account,
			     "name", name,
			     "id", id,
			     NULL);
}

const gchar *
gossip_contact_get_id (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->id) {
		return priv->id;
	}

	return "";
}

const gchar *
gossip_contact_get_name (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->name == NULL) {
		return gossip_contact_get_id (contact);
	}

	return priv->name;
}

GossipAvatar *
gossip_contact_get_avatar (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->avatar;
}

McAccount *
gossip_contact_get_account (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->account;
}

GossipPresence *
gossip_contact_get_presence (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->presence;
}

GList *
gossip_contact_get_groups (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->groups;
}

GossipSubscription
gossip_contact_get_subscription (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact),
			      GOSSIP_SUBSCRIPTION_NONE);

	priv = GET_PRIV (contact);

	return priv->subscription;
}

guint
gossip_contact_get_handle (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), 0);

	priv = GET_PRIV (contact);

	return priv->handle;
}

void
gossip_contact_set_id (GossipContact *contact,
		       const gchar   *id)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (id != NULL);

	priv = GET_PRIV (contact);

	if (priv->id && strcmp (id, priv->id) == 0) {
		return;
	}

	g_free (priv->id);
	priv->id = g_strdup (id);

	g_object_notify (G_OBJECT (contact), "id");
}

void
gossip_contact_set_name (GossipContact *contact,
			 const gchar   *name)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (contact);

	if (priv->name && strcmp (name, priv->name) == 0) {
		return;
	}

	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (contact), "name");
}

void
gossip_contact_set_avatar (GossipContact *contact,
			   GossipAvatar  *avatar)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->avatar == avatar) {
		return;
	}

	if (priv->avatar) {
		gossip_avatar_unref (priv->avatar);
		priv->avatar = NULL;
	}

	if (avatar) {
		priv->avatar = gossip_avatar_ref (avatar);
	}

	g_object_notify (G_OBJECT (contact), "avatar");
}

void
gossip_contact_set_account (GossipContact *contact,
			    McAccount     *account)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (MC_IS_ACCOUNT (account));

	priv = GET_PRIV (contact);

	if (account == priv->account) {
		return;
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}
	priv->account = g_object_ref (account);

	g_object_notify (G_OBJECT (contact), "account");
}

void
gossip_contact_set_presence (GossipContact  *contact,
			     GossipPresence *presence)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (presence == priv->presence) {
		return;
	}

	if (priv->presence) {
		g_object_unref (priv->presence);
		priv->presence = NULL;
	}

	if (presence) {
		priv->presence = g_object_ref (presence);
	}

	g_object_notify (G_OBJECT (contact), "presence");
}

void
gossip_contact_set_groups (GossipContact *contact,
			   GList         *groups)
{
	GossipContactPriv *priv;
	GList             *old_groups, *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	old_groups = priv->groups;
	priv->groups = NULL;

	for (l = groups; l; l = l->next) {
		priv->groups = g_list_append (priv->groups,
					      g_strdup (l->data));
	}

	g_list_foreach (old_groups, (GFunc) g_free, NULL);
	g_list_free (old_groups);

	g_object_notify (G_OBJECT (contact), "groups");
}

void
gossip_contact_set_subscription (GossipContact      *contact,
				 GossipSubscription  subscription)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->subscription == subscription) {
		return;
	}

	priv->subscription = subscription;

	g_object_notify (G_OBJECT (contact), "subscription");
}

void
gossip_contact_set_handle (GossipContact *contact,
			   guint          handle)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->handle == handle) {
		return;
	}

	priv->handle = handle;

	g_object_notify (G_OBJECT (contact), "handle");
}

void
gossip_contact_add_group (GossipContact *contact,
			  const gchar   *group)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (group != NULL);

	priv = GET_PRIV (contact);

	if (!g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		priv->groups = g_list_prepend (priv->groups, g_strdup (group));
		g_object_notify (G_OBJECT (contact), "groups");
	}
}

void
gossip_contact_remove_group (GossipContact *contact,
			     const gchar   *group)
{
	GossipContactPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (group != NULL);

	priv = GET_PRIV (contact);

	l = g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp);
	if (l) {
		g_free (l->data);
		priv->groups = g_list_delete_link (priv->groups, l);
		g_object_notify (G_OBJECT (contact), "groups");
	}
}

gboolean
gossip_contact_is_online (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (contact);

	return (priv->presence != NULL);
}

gboolean
gossip_contact_is_in_group (GossipContact *contact,
			    const gchar   *group)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (!G_STR_EMPTY (group), FALSE);

	priv = GET_PRIV (contact);

	if (g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		return TRUE;
	}

	return FALSE;
}

const gchar *
gossip_contact_get_status (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->presence) {
		const gchar *status;

		status = gossip_presence_get_status (priv->presence);
		if (!status) {
			McPresence state;

			state = gossip_presence_get_state (priv->presence);
			status = gossip_presence_state_get_default_status (state);
		}

		return status;
	}

	return gossip_presence_state_get_default_status (MC_PRESENCE_OFFLINE);
}

GossipContact *
gossip_contact_get_user (GossipContact *contact)
{
	GossipContactPriv     *priv;
	EmpathyContactManager *manager;
	GossipContact         *user_contact;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	manager = empathy_contact_manager_new ();
	user_contact = empathy_contact_manager_get_user (manager, priv->account);
	g_object_unref (manager);

	return user_contact;
}

gboolean
gossip_contact_equal (gconstpointer v1,
		      gconstpointer v2)
{
	McAccount   *account_a;
	McAccount   *account_b;
	const gchar *id_a;
	const gchar *id_b;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (v1), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (v2), FALSE);

	account_a = gossip_contact_get_account (GOSSIP_CONTACT (v1));
	account_b = gossip_contact_get_account (GOSSIP_CONTACT (v2));

	id_a = gossip_contact_get_id (GOSSIP_CONTACT (v1));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (v2));

	return gossip_account_equal (account_a, account_b) && g_str_equal (id_a, id_b);
}

guint
gossip_contact_hash (gconstpointer key)
{
	GossipContactPriv *priv;
	guint              hash;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (key), +1);

	priv = GET_PRIV (GOSSIP_CONTACT (key));

	hash = gossip_account_hash (gossip_contact_get_account (GOSSIP_CONTACT (key)));
	hash += g_str_hash (gossip_contact_get_id (GOSSIP_CONTACT (key)));

	return hash;
}

