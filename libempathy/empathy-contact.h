/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_CONTACT_H__
#define __EMPATHY_CONTACT_H__

#include <glib-object.h>

#include <telepathy-glib/contact.h>
#include <libempathy/empathy-account.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT         (empathy_contact_get_type ())
#define EMPATHY_CONTACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT, EmpathyContact))
#define EMPATHY_CONTACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CONTACT, EmpathyContactClass))
#define EMPATHY_IS_CONTACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT))
#define EMPATHY_IS_CONTACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT))
#define EMPATHY_CONTACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT, EmpathyContactClass))

typedef struct _EmpathyContact      EmpathyContact;
typedef struct _EmpathyContactClass EmpathyContactClass;

struct _EmpathyContact
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyContactClass
{
  GObjectClass parent_class;
};

typedef struct {
  guchar *data;
  gsize len;
  gchar *format;
  gchar *token;
  gchar *filename;
  guint refcount;
} EmpathyAvatar;

typedef enum {
  EMPATHY_CAPABILITIES_NONE = 0,
  EMPATHY_CAPABILITIES_AUDIO = 1 << 0,
  EMPATHY_CAPABILITIES_VIDEO = 1 << 1,
  EMPATHY_CAPABILITIES_FT = 1 << 2,
  EMPATHY_CAPABILITIES_STREAM_TUBE = 1 << 3,
  EMPATHY_CAPABILITIES_UNKNOWN = 1 << 7
} EmpathyCapabilities;

GType empathy_contact_get_type (void) G_GNUC_CONST;
EmpathyContact * empathy_contact_new (TpContact *tp_contact);
EmpathyContact * empathy_contact_new_for_log (EmpathyAccount *account,
    const gchar *id, const gchar *name, gboolean is_user);
TpContact * empathy_contact_get_tp_contact (EmpathyContact *contact);
const gchar * empathy_contact_get_id (EmpathyContact *contact);
void empathy_contact_set_id (EmpathyContact *contact, const gchar *id);
const gchar * empathy_contact_get_name (EmpathyContact *contact);
void empathy_contact_set_name (EmpathyContact *contact, const gchar *name);
EmpathyAvatar * empathy_contact_get_avatar (EmpathyContact *contact);
void empathy_contact_set_avatar (EmpathyContact *contact,
    EmpathyAvatar *avatar);
EmpathyAccount * empathy_contact_get_account (EmpathyContact *contact);
TpConnection * empathy_contact_get_connection (EmpathyContact *contact);
TpConnectionPresenceType empathy_contact_get_presence (EmpathyContact *contact);
void empathy_contact_set_presence (EmpathyContact *contact,
    TpConnectionPresenceType presence);
const gchar * empathy_contact_get_presence_message (EmpathyContact *contact);
void empathy_contact_set_presence_message (EmpathyContact *contact,
    const gchar *message);
guint empathy_contact_get_handle (EmpathyContact *contact);
void empathy_contact_set_handle (EmpathyContact *contact, guint handle);
EmpathyCapabilities empathy_contact_get_capabilities (EmpathyContact *contact);
void empathy_contact_set_capabilities (EmpathyContact *contact,
    EmpathyCapabilities capabilities);
gboolean empathy_contact_is_user (EmpathyContact *contact);
void empathy_contact_set_is_user (EmpathyContact *contact,
    gboolean is_user);
gboolean empathy_contact_is_online (EmpathyContact *contact);
const gchar * empathy_contact_get_status (EmpathyContact *contact);
gboolean empathy_contact_can_voip (EmpathyContact *contact);
gboolean empathy_contact_can_send_files (EmpathyContact *contact);
gboolean empathy_contact_can_use_stream_tube (EmpathyContact *contact);
guint empathy_contact_hash (gconstpointer key);

void empathy_contact_load_avatar_data (EmpathyContact *contact,
    const guchar *data, const gsize len, const gchar *format,
    const gchar *token);
gboolean empathy_contact_load_avatar_cache (EmpathyContact *contact,
    const gchar *token);


#define EMPATHY_TYPE_AVATAR (empathy_avatar_get_type ())
GType empathy_avatar_get_type (void) G_GNUC_CONST;
EmpathyAvatar * empathy_avatar_new (guchar *data,
    gsize len,
    gchar *format,
    gchar *token,
    gchar *filename);
EmpathyAvatar * empathy_avatar_ref (EmpathyAvatar *avatar);
void empathy_avatar_unref (EmpathyAvatar *avatar);

gboolean empathy_avatar_save_to_file (EmpathyAvatar *avatar,
    const gchar *filename, GError **error);

GHashTable * empathy_contact_get_location (EmpathyContact *contact);
void empathy_contact_set_location (EmpathyContact *contact,
    GHashTable *location);
gboolean empathy_contact_equal (gconstpointer contact1,
    gconstpointer contact2);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_H__ */
