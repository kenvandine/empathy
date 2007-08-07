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

#include "empathy-contact.h"
#include "empathy-utils.h"
#include "empathy-debug.h"
#include "empathy-enum-types.h"

#define DEBUG_DOMAIN "Contact"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_CONTACT, EmpathyContactPriv))

typedef struct _EmpathyContactPriv EmpathyContactPriv;

struct _EmpathyContactPriv {
	gchar              *id;
	gchar              *name;
	EmpathyAvatar       *avatar;
	McAccount          *account;
	EmpathyPresence     *presence;
	GList              *groups;
	EmpathySubscription  subscription;
	guint               handle;
	gboolean            is_user;
};

static void contact_class_init    (EmpathyContactClass *class);
static void contact_init          (EmpathyContact      *contact);
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
	PROP_HANDLE,
	PROP_IS_USER
};

static gpointer parent_class = NULL;

GType
empathy_contact_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EmpathyContactClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) contact_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EmpathyContact),
			0,    /* n_preallocs */
			(GInstanceInitFunc) contact_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EmpathyContact",
					       &info, 0);
	}

	return type;
}

static void
contact_class_init (EmpathyContactClass *class)
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
							     EMPATHY_TYPE_AVATAR,
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
							      EMPATHY_TYPE_PRESENCE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_GROUPS,
					 g_param_spec_pointer ("groups",
							       "Contact groups",
							       "Groups of contact",
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SUBSCRIPTION,
					 g_param_spec_flags ("subscription",
							     "Contact Subscription",
							     "The subscription status of the contact",
							     EMPATHY_TYPE_SUBSCRIPTION,
							     EMPATHY_SUBSCRIPTION_NONE,
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
	g_object_class_install_property (object_class,
					 PROP_IS_USER,
					 g_param_spec_boolean ("is-user",
							       "Contact is-user",
							       "Is contact the user",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyContactPriv));
}

static void
contact_init (EmpathyContact *contact)
{
}

static void
contact_finalize (GObject *object)
{
	EmpathyContactPriv *priv;

	priv = GET_PRIV (object);

	empathy_debug (DEBUG_DOMAIN, "finalize: %p", object);

	g_free (priv->name);
	g_free (priv->id);

	if (priv->avatar) {
		empathy_avatar_unref (priv->avatar);
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
	EmpathyContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value,
				    empathy_contact_get_id (EMPATHY_CONTACT (object)));
		break;
	case PROP_NAME:
		g_value_set_string (value,
				    empathy_contact_get_name (EMPATHY_CONTACT (object)));
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
		g_value_set_flags (value, priv->subscription);
		break;
	case PROP_HANDLE:
		g_value_set_uint (value, priv->handle);
		break;
	case PROP_IS_USER:
		g_value_set_boolean (value, priv->is_user);
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
	EmpathyContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		empathy_contact_set_id (EMPATHY_CONTACT (object),
				       g_value_get_string (value));
		break;
	case PROP_NAME:
		empathy_contact_set_name (EMPATHY_CONTACT (object),
					 g_value_get_string (value));
		break;
	case PROP_AVATAR:
		empathy_contact_set_avatar (EMPATHY_CONTACT (object),
					   g_value_get_boxed (value));
		break;
	case PROP_ACCOUNT:
		empathy_contact_set_account (EMPATHY_CONTACT (object),
					    MC_ACCOUNT (g_value_get_object (value)));
		break;
	case PROP_PRESENCE:
		empathy_contact_set_presence (EMPATHY_CONTACT (object),
					     EMPATHY_PRESENCE (g_value_get_object (value)));
		break;
	case PROP_GROUPS:
		empathy_contact_set_groups (EMPATHY_CONTACT (object),
					   g_value_get_pointer (value));
		break;
	case PROP_SUBSCRIPTION:
		empathy_contact_set_subscription (EMPATHY_CONTACT (object),
						  g_value_get_flags (value));
		break;
	case PROP_HANDLE:
		empathy_contact_set_handle (EMPATHY_CONTACT (object),
					   g_value_get_uint (value));
		break;
	case PROP_IS_USER:
		empathy_contact_set_is_user (EMPATHY_CONTACT (object),
					    g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyContact *
empathy_contact_new (McAccount *account)
{
	return g_object_new (EMPATHY_TYPE_CONTACT,
			     "account", account,
			     NULL);
}

EmpathyContact *
empathy_contact_new_full (McAccount   *account,
			 const gchar *id,
			 const gchar *name)
{
	return g_object_new (EMPATHY_TYPE_CONTACT,
			     "account", account,
			     "name", name,
			     "id", id,
			     NULL);
}

const gchar *
empathy_contact_get_id (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->id) {
		return priv->id;
	}

	return "";
}

const gchar *
empathy_contact_get_name (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (G_STR_EMPTY (priv->name)) {
		return empathy_contact_get_id (contact);
	}

	return priv->name;
}

EmpathyAvatar *
empathy_contact_get_avatar (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->avatar;
}

McAccount *
empathy_contact_get_account (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->account;
}

EmpathyPresence *
empathy_contact_get_presence (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->presence;
}

GList *
empathy_contact_get_groups (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->groups;
}

EmpathySubscription
empathy_contact_get_subscription (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact),
			      EMPATHY_SUBSCRIPTION_NONE);

	priv = GET_PRIV (contact);

	return priv->subscription;
}

guint
empathy_contact_get_handle (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), 0);

	priv = GET_PRIV (contact);

	return priv->handle;
}

gboolean
empathy_contact_is_user (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (contact);

	return priv->is_user;
}

void
empathy_contact_set_id (EmpathyContact *contact,
		       const gchar   *id)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
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
empathy_contact_set_name (EmpathyContact *contact,
			 const gchar   *name)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
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
empathy_contact_set_avatar (EmpathyContact *contact,
			   EmpathyAvatar  *avatar)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->avatar == avatar) {
		return;
	}

	if (priv->avatar) {
		empathy_avatar_unref (priv->avatar);
		priv->avatar = NULL;
	}

	if (avatar) {
		priv->avatar = empathy_avatar_ref (avatar);
	}

	g_object_notify (G_OBJECT (contact), "avatar");
}

void
empathy_contact_set_account (EmpathyContact *contact,
			    McAccount     *account)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
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
empathy_contact_set_presence (EmpathyContact  *contact,
			     EmpathyPresence *presence)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

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
empathy_contact_set_groups (EmpathyContact *contact,
			   GList         *groups)
{
	EmpathyContactPriv *priv;
	GList             *old_groups, *l;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

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
empathy_contact_set_subscription (EmpathyContact      *contact,
				 EmpathySubscription  subscription)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->subscription == subscription) {
		return;
	}

	priv->subscription = subscription;

	g_object_notify (G_OBJECT (contact), "subscription");
}

void
empathy_contact_set_handle (EmpathyContact *contact,
			   guint          handle)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->handle == handle) {
		return;
	}

	priv->handle = handle;

	g_object_notify (G_OBJECT (contact), "handle");
}

void
empathy_contact_set_is_user (EmpathyContact *contact,
			    gboolean       is_user)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	if (priv->is_user == is_user) {
		return;
	}

	priv->is_user = is_user;

	g_object_notify (G_OBJECT (contact), "is-user");
}

void
empathy_contact_add_group (EmpathyContact *contact,
			  const gchar   *group)
{
	EmpathyContactPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (group != NULL);

	priv = GET_PRIV (contact);

	if (!g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		priv->groups = g_list_prepend (priv->groups, g_strdup (group));
		g_object_notify (G_OBJECT (contact), "groups");
	}
}

void
empathy_contact_remove_group (EmpathyContact *contact,
			     const gchar   *group)
{
	EmpathyContactPriv *priv;
	GList             *l;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
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
empathy_contact_is_online (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (contact);

	if (!priv->presence) {
		return FALSE;
	}

	return (empathy_presence_get_state (priv->presence) > MC_PRESENCE_OFFLINE);
}

gboolean
empathy_contact_is_in_group (EmpathyContact *contact,
			    const gchar   *group)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (!G_STR_EMPTY (group), FALSE);

	priv = GET_PRIV (contact);

	if (g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		return TRUE;
	}

	return FALSE;
}

const gchar *
empathy_contact_get_status (EmpathyContact *contact)
{
	EmpathyContactPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->presence) {
		const gchar *status;

		status = empathy_presence_get_status (priv->presence);
		if (!status) {
			McPresence state;

			state = empathy_presence_get_state (priv->presence);
			status = empathy_presence_state_get_default_status (state);
		}

		return status;
	}

	return empathy_presence_state_get_default_status (MC_PRESENCE_OFFLINE);
}

gboolean
empathy_contact_equal (gconstpointer v1,
		      gconstpointer v2)
{
	McAccount   *account_a;
	McAccount   *account_b;
	const gchar *id_a;
	const gchar *id_b;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (v1), FALSE);
	g_return_val_if_fail (EMPATHY_IS_CONTACT (v2), FALSE);

	account_a = empathy_contact_get_account (EMPATHY_CONTACT (v1));
	account_b = empathy_contact_get_account (EMPATHY_CONTACT (v2));

	id_a = empathy_contact_get_id (EMPATHY_CONTACT (v1));
	id_b = empathy_contact_get_id (EMPATHY_CONTACT (v2));

	return empathy_account_equal (account_a, account_b) && g_str_equal (id_a, id_b);
}

guint
empathy_contact_hash (gconstpointer key)
{
	EmpathyContactPriv *priv;
	guint              hash;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (key), +1);

	priv = GET_PRIV (EMPATHY_CONTACT (key));

	hash = empathy_account_hash (empathy_contact_get_account (EMPATHY_CONTACT (key)));
	hash += g_str_hash (empathy_contact_get_id (EMPATHY_CONTACT (key)));

	return hash;
}

