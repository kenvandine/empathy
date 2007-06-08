/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __GOSSIP_CONTACT_H__
#define __GOSSIP_CONTACT_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

#include "gossip-avatar.h"
#include "gossip-presence.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CONTACT         (gossip_contact_get_gtype ())
#define GOSSIP_CONTACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT, GossipContact))
#define GOSSIP_CONTACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CONTACT, GossipContactClass))
#define GOSSIP_IS_CONTACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT))
#define GOSSIP_IS_CONTACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT))
#define GOSSIP_CONTACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT, GossipContactClass))

typedef struct _GossipContact      GossipContact;
typedef struct _GossipContactClass GossipContactClass;

struct _GossipContact {
	GObject parent;
};

struct _GossipContactClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_SUBSCRIPTION_NONE = 0,
	GOSSIP_SUBSCRIPTION_TO   = 1 << 0,	/* We send our presence to that contact */
	GOSSIP_SUBSCRIPTION_FROM = 1 << 1,	/* That contact sends his presence to us */
	GOSSIP_SUBSCRIPTION_BOTH = GOSSIP_SUBSCRIPTION_TO | GOSSIP_SUBSCRIPTION_FROM
} GossipSubscription;

GType              gossip_contact_get_gtype                 (void) G_GNUC_CONST;

GossipContact *    gossip_contact_new                       (McAccount          *account);
GossipContact *    gossip_contact_new_full                  (McAccount          *account,
							     const gchar        *id,
							     const gchar        *name);
const gchar *      gossip_contact_get_id                    (GossipContact      *contact);
const gchar *      gossip_contact_get_name                  (GossipContact      *contact);
GossipAvatar *     gossip_contact_get_avatar                (GossipContact      *contact);
McAccount *        gossip_contact_get_account               (GossipContact      *contact);
GossipPresence *   gossip_contact_get_presence              (GossipContact      *contact);
GList *            gossip_contact_get_groups                (GossipContact      *contact);
GossipSubscription gossip_contact_get_subscription          (GossipContact      *contact);
guint              gossip_contact_get_handle                (GossipContact      *contact);
void               gossip_contact_set_id                    (GossipContact      *contact,
							     const gchar        *id);
void               gossip_contact_set_name                  (GossipContact      *contact,
							     const gchar        *name);
void               gossip_contact_set_avatar                (GossipContact      *contact,
							     GossipAvatar       *avatar);
void               gossip_contact_set_account               (GossipContact      *contact,
							     McAccount          *account);
void               gossip_contact_set_presence              (GossipContact      *contact,
							     GossipPresence     *presence);
void               gossip_contact_set_groups                (GossipContact      *contact,
							     GList              *categories);
void               gossip_contact_set_subscription          (GossipContact      *contact,
							     GossipSubscription  subscription);
void               gossip_contact_set_handle                (GossipContact      *contact,
							     guint               handle);
void               gossip_contact_add_group                 (GossipContact      *contact,
							     const gchar        *group);
void               gossip_contact_remove_group              (GossipContact      *contact,
							     const gchar        *group);
gboolean           gossip_contact_is_online                 (GossipContact      *contact);
gboolean           gossip_contact_is_in_group               (GossipContact      *contact,
							     const gchar        *group);
const gchar *      gossip_contact_get_status                (GossipContact      *contact);
GossipContact *    gossip_contact_get_user                  (GossipContact      *contact);
gboolean           gossip_contact_equal                     (gconstpointer       v1,
							     gconstpointer       v2);
guint              gossip_contact_hash                      (gconstpointer       key);

G_END_DECLS

#endif /* __GOSSIP_CONTACT_H__ */

