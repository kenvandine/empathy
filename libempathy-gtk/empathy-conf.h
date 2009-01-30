/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#ifndef __EMPATHY_CONF_H__
#define __EMPATHY_CONF_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONF         (empathy_conf_get_type ())
#define EMPATHY_CONF(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONF, EmpathyConf))
#define EMPATHY_CONF_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CONF, EmpathyConfClass))
#define EMPATHY_IS_CONF(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONF))
#define EMPATHY_IS_CONF_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONF))
#define EMPATHY_CONF_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONF, EmpathyConfClass))

typedef struct _EmpathyConf      EmpathyConf;
typedef struct _EmpathyConfClass EmpathyConfClass;

struct _EmpathyConf  {
	GObject parent;
	gpointer priv;
};

struct _EmpathyConfClass {
	GObjectClass parent_class;
};

#define EMPATHY_PREFS_PATH "/apps/empathy"
#define EMPATHY_PREFS_NOTIFICATIONS_ENABLED        EMPATHY_PREFS_PATH "/notifications/notifications_enabled"
#define EMPATHY_PREFS_NOTIFICATIONS_DISABLED_AWAY  EMPATHY_PREFS_PATH "/notifications/notifications_disabled_away"
#define EMPATHY_PREFS_NOTIFICATIONS_FOCUS          EMPATHY_PREFS_PATH "/notifications/notifications_focus"
#define EMPATHY_PREFS_SOUNDS_ENABLED               EMPATHY_PREFS_PATH "/sounds/sounds_enabled"
#define EMPATHY_PREFS_SOUNDS_DISABLED_AWAY         EMPATHY_PREFS_PATH "/sounds/sounds_disabled_away"
#define EMPATHY_PREFS_SOUNDS_INCOMING_MESSAGE      EMPATHY_PREFS_PATH "/sounds/sounds_incoming_message"
#define EMPATHY_PREFS_SOUNDS_OUTGOING_MESSAGE      EMPATHY_PREFS_PATH "/sounds/sounds_outgoing_message"
#define EMPATHY_PREFS_SOUNDS_NEW_CONVERSATION      EMPATHY_PREFS_PATH "/sounds/sounds_new_conversation"
#define EMPATHY_PREFS_SOUNDS_SERVICE_LOGIN         EMPATHY_PREFS_PATH "/sounds/sounds_service_login"
#define EMPATHY_PREFS_SOUNDS_SERVICE_LOGOUT        EMPATHY_PREFS_PATH "/sounds/sounds_service_logout"
#define EMPATHY_PREFS_SOUNDS_CONTACT_LOGIN         EMPATHY_PREFS_PATH "/sounds/sounds_contact_login"
#define EMPATHY_PREFS_SOUNDS_CONTACT_LOGOUT        EMPATHY_PREFS_PATH "/sounds/sounds_contact_logout"
#define EMPATHY_PREFS_POPUPS_WHEN_AVAILABLE        EMPATHY_PREFS_PATH "/notifications/popups_when_available"
#define EMPATHY_PREFS_CHAT_SHOW_SMILEYS            EMPATHY_PREFS_PATH "/conversation/graphical_smileys"
#define EMPATHY_PREFS_CHAT_THEME                   EMPATHY_PREFS_PATH "/conversation/theme"
#define EMPATHY_PREFS_CHAT_SPELL_CHECKER_LANGUAGES EMPATHY_PREFS_PATH "/conversation/spell_checker_languages"
#define EMPATHY_PREFS_CHAT_SPELL_CHECKER_ENABLED   EMPATHY_PREFS_PATH "/conversation/spell_checker_enabled"
#define EMPATHY_PREFS_CHAT_NICK_COMPLETION_CHAR    EMPATHY_PREFS_PATH "/conversation/nick_completion_char"
#define EMPATHY_PREFS_CHAT_AVATAR_IN_ICON          EMPATHY_PREFS_PATH "/conversation/avatar_in_icon"
#define EMPATHY_PREFS_UI_SEPARATE_CHAT_WINDOWS     EMPATHY_PREFS_PATH "/ui/separate_chat_windows"
#define EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN        EMPATHY_PREFS_PATH "/ui/main_window_hidden"
#define EMPATHY_PREFS_UI_AVATAR_DIRECTORY          EMPATHY_PREFS_PATH "/ui/avatar_directory"
#define EMPATHY_PREFS_UI_SHOW_AVATARS              EMPATHY_PREFS_PATH "/ui/show_avatars"
#define EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST      EMPATHY_PREFS_PATH "/ui/compact_contact_list"
#define EMPATHY_PREFS_CONTACTS_SHOW_OFFLINE        EMPATHY_PREFS_PATH "/contacts/show_offline"
#define EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM      EMPATHY_PREFS_PATH "/contacts/sort_criterium"
#define EMPATHY_PREFS_HINTS_CLOSE_MAIN_WINDOW      EMPATHY_PREFS_PATH "/hints/close_main_window"
#define EMPATHY_PREFS_SALUT_ACCOUNT_CREATED        EMPATHY_PREFS_PATH "/accounts/salut_created"
#define EMPATHY_PREFS_USE_NM                       EMPATHY_PREFS_PATH "/use_nm"
#define EMPATHY_PREFS_AUTOCONNECT                  EMPATHY_PREFS_PATH "/autoconnect"
#define EMPATHY_PREFS_IMPORT_ASKED                 EMPATHY_PREFS_PATH "/import_asked"
#define EMPATHY_PREFS_FILE_TRANSFER_DEFAULT_FOLDER EMPATHY_PREFS_PATH "/file_transfer/default_folder"

typedef void (*EmpathyConfNotifyFunc) (EmpathyConf  *conf, 
				      const gchar *key,
				      gpointer     user_data);

GType       empathy_conf_get_type        (void) G_GNUC_CONST;
EmpathyConf *empathy_conf_get             (void);
void        empathy_conf_shutdown        (void);
guint       empathy_conf_notify_add      (EmpathyConf            *conf,
					 const gchar           *key,
					 EmpathyConfNotifyFunc   func,
					 gpointer               data);
gboolean    empathy_conf_notify_remove   (EmpathyConf            *conf,
					 guint                  id);
gboolean    empathy_conf_set_int         (EmpathyConf            *conf,
					 const gchar           *key,
					 gint                   value);
gboolean    empathy_conf_get_int         (EmpathyConf            *conf,
					 const gchar           *key,
					 gint                  *value);
gboolean    empathy_conf_set_bool        (EmpathyConf            *conf,
					 const gchar           *key,
					 gboolean               value);
gboolean    empathy_conf_get_bool        (EmpathyConf            *conf,
					 const gchar           *key,
					 gboolean              *value);
gboolean    empathy_conf_set_string      (EmpathyConf            *conf,
					 const gchar           *key,
					 const gchar           *value);
gboolean    empathy_conf_get_string      (EmpathyConf            *conf,
					 const gchar           *key,
					 gchar                **value);
gboolean    empathy_conf_set_string_list (EmpathyConf            *conf,
					 const gchar           *key,
					 GSList                *value);
gboolean    empathy_conf_get_string_list (EmpathyConf            *conf,
					 const gchar           *key,
					 GSList              **value);

G_END_DECLS

#endif /* __EMPATHY_CONF_H__ */

