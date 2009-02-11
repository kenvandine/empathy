/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "empathy-tp-chat.h"
#include "empathy-chatroom-manager.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "empathy-chatroom-manager.dtd"
#define SAVE_TIMER 4

static EmpathyChatroomManager *chatroom_manager_singleton = NULL;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyChatroomManager)
typedef struct {
	GList      *chatrooms;
  gchar *file;
  /* source id of the autosave timer */
  gint save_timer_id;
} EmpathyChatroomManagerPriv;

static void     chatroom_manager_finalize          (GObject                    *object);
static gboolean chatroom_manager_get_all           (EmpathyChatroomManager      *manager);
static gboolean chatroom_manager_file_parse        (EmpathyChatroomManager      *manager,
						    const gchar                *filename);
static void     chatroom_manager_parse_chatroom    (EmpathyChatroomManager      *manager,
						    xmlNodePtr                  node);
static gboolean chatroom_manager_file_save         (EmpathyChatroomManager      *manager);
static void reset_save_timeout (EmpathyChatroomManager *self);

enum {
	CHATROOM_ADDED,
	CHATROOM_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* properties */
enum
{
  PROP_FILE = 1,
  LAST_PROPERTY
};

G_DEFINE_TYPE (EmpathyChatroomManager, empathy_chatroom_manager, G_TYPE_OBJECT);

static void
empathy_chatroom_manager_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
  EmpathyChatroomManager *self = EMPATHY_CHATROOM_MANAGER (object);
  EmpathyChatroomManagerPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_FILE:
        g_value_set_string (value, priv->file);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_chatroom_manager_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
  EmpathyChatroomManager *self = EMPATHY_CHATROOM_MANAGER (object);
  EmpathyChatroomManagerPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_FILE:
        g_free (priv->file);
        priv->file = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static GObject *
empathy_chatroom_manager_constructor (GType type,
                                      guint n_props,
                                      GObjectConstructParam *props)
{
  GObject *obj;
  EmpathyChatroomManager *self;
  EmpathyChatroomManagerPriv *priv;

  if (chatroom_manager_singleton != NULL)
    return g_object_ref (chatroom_manager_singleton);

  /* Parent constructor chain */
  obj = G_OBJECT_CLASS (empathy_chatroom_manager_parent_class)->
        constructor (type, n_props, props);

  self = EMPATHY_CHATROOM_MANAGER (obj);
  priv = GET_PRIV (self);

  chatroom_manager_singleton = self;
  g_object_add_weak_pointer (obj, (gpointer) &chatroom_manager_singleton);

  if (priv->file == NULL)
    {
      /* Set the default file path */
      gchar *dir;

      dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
      if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
        g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);

      priv->file = g_build_filename (dir, CHATROOMS_XML_FILENAME, NULL);
      g_free (dir);
    }

  chatroom_manager_get_all (self);
  return obj;
}

static void
empathy_chatroom_manager_class_init (EmpathyChatroomManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->constructor = empathy_chatroom_manager_constructor;
  object_class->get_property = empathy_chatroom_manager_get_property;
  object_class->set_property = empathy_chatroom_manager_set_property;
	object_class->finalize = chatroom_manager_finalize;

  param_spec = g_param_spec_string (
      "file",
      "path of the favorite file",
      "The path of the XML file containing user's favorites",
      NULL,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_FILE, param_spec);

	signals[CHATROOM_ADDED] =
		g_signal_new ("chatroom-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CHATROOM);
	signals[CHATROOM_REMOVED] =
		g_signal_new ("chatroom-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CHATROOM);

	g_type_class_add_private (object_class,
				  sizeof (EmpathyChatroomManagerPriv));
}

static void
empathy_chatroom_manager_init (EmpathyChatroomManager *manager)
{
	EmpathyChatroomManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
		EMPATHY_TYPE_CHATROOM_MANAGER, EmpathyChatroomManagerPriv);

	manager->priv = priv;
}

static void
chatroom_changed_cb (EmpathyChatroom *chatroom,
                     GParamSpec *spec,
                     EmpathyChatroomManager *self)
{
  reset_save_timeout (self);
}

static void
chatroom_manager_finalize (GObject *object)
{
  EmpathyChatroomManager *self = EMPATHY_CHATROOM_MANAGER (object);
	EmpathyChatroomManagerPriv *priv;
  GList *l;

	priv = GET_PRIV (object);

  if (priv->save_timer_id > 0)
    {
      /* have to save before destroy the object */
      g_source_remove (priv->save_timer_id);
      priv->save_timer_id = 0;
      chatroom_manager_file_save (self);
    }

  for (l = priv->chatrooms; l != NULL; l = g_list_next (l))
    {
      EmpathyChatroom *chatroom = l->data;

      g_signal_handlers_disconnect_by_func (chatroom, chatroom_changed_cb,
          self);

      g_object_unref (chatroom);
    }

	g_list_free (priv->chatrooms);
  g_free (priv->file);

	(G_OBJECT_CLASS (empathy_chatroom_manager_parent_class)->finalize) (object);
}

EmpathyChatroomManager *
empathy_chatroom_manager_dup_singleton (const gchar *file)
{
	return EMPATHY_CHATROOM_MANAGER (g_object_new (EMPATHY_TYPE_CHATROOM_MANAGER,
		"file", file, NULL));
}

static gboolean
save_timeout (EmpathyChatroomManager *self)
{
  EmpathyChatroomManagerPriv *priv = GET_PRIV (self);

  priv->save_timer_id = 0;
  chatroom_manager_file_save (self);

  return FALSE;
}

static void
reset_save_timeout (EmpathyChatroomManager *self)
{
  EmpathyChatroomManagerPriv *priv = GET_PRIV (self);

  if (priv->save_timer_id > 0)
    {
      g_source_remove (priv->save_timer_id);
    }

  priv->save_timer_id = g_timeout_add_seconds (SAVE_TIMER,
      (GSourceFunc) save_timeout, self);
}

static void
add_chatroom (EmpathyChatroomManager *self,
              EmpathyChatroom *chatroom)
{
  EmpathyChatroomManagerPriv *priv = GET_PRIV (self);

  priv->chatrooms = g_list_prepend (priv->chatrooms, g_object_ref (chatroom));

  g_signal_connect (chatroom, "notify",
      G_CALLBACK (chatroom_changed_cb), self);
}

gboolean
empathy_chatroom_manager_add (EmpathyChatroomManager *manager,
			     EmpathyChatroom        *chatroom)
{
	EmpathyChatroomManagerPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM_MANAGER (manager), FALSE);
	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (manager);

	/* don't add more than once */
	if (!empathy_chatroom_manager_find (manager,
					   empathy_chatroom_get_account (chatroom),
					   empathy_chatroom_get_room (chatroom))) {
      gboolean favorite;

      g_object_get (chatroom, "favorite", &favorite, NULL);

    add_chatroom (manager, chatroom);

    if (favorite)
      {
        reset_save_timeout (manager);
      }

		g_signal_emit (manager, signals[CHATROOM_ADDED], 0, chatroom);

		return TRUE;
	}

	return FALSE;
}

void
empathy_chatroom_manager_remove (EmpathyChatroomManager *manager,
				EmpathyChatroom        *chatroom)
{
	EmpathyChatroomManagerPriv *priv;
	GList                     *l;

	g_return_if_fail (EMPATHY_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (EMPATHY_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		EmpathyChatroom *this_chatroom;

		this_chatroom = l->data;

		if (this_chatroom == chatroom ||
        empathy_chatroom_equal (chatroom, this_chatroom)) {
        gboolean favorite;
			priv->chatrooms = g_list_delete_link (priv->chatrooms, l);

      g_object_get (chatroom, "favorite", &favorite, NULL);

      if (favorite)
        {
          reset_save_timeout (manager);
        }

			g_signal_emit (manager, signals[CHATROOM_REMOVED], 0, this_chatroom);

      g_signal_handlers_disconnect_by_func (chatroom, chatroom_changed_cb,
          manager);

			g_object_unref (this_chatroom);
			break;
		}
	}
}

EmpathyChatroom *
empathy_chatroom_manager_find (EmpathyChatroomManager *manager,
			      McAccount             *account,
			      const gchar           *room)
{
	EmpathyChatroomManagerPriv *priv;
	GList                     *l;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (room != NULL, NULL);

	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		EmpathyChatroom *chatroom;
		McAccount      *this_account;
		const gchar    *this_room;

		chatroom = l->data;
		this_account = empathy_chatroom_get_account (chatroom);
		this_room = empathy_chatroom_get_room (chatroom);

		if (this_account && this_room &&
		    empathy_account_equal (account, this_account) &&
		    strcmp (this_room, room) == 0) {
			return chatroom;
		}
	}

	return NULL;
}

GList *
empathy_chatroom_manager_get_chatrooms (EmpathyChatroomManager *manager,
				       McAccount             *account)
{
	EmpathyChatroomManagerPriv *priv;
	GList                     *chatrooms, *l;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_copy (priv->chatrooms);
	}

	chatrooms = NULL;
	for (l = priv->chatrooms; l; l = l->next) {
		EmpathyChatroom *chatroom;

		chatroom = l->data;

		if (empathy_account_equal (account,
					  empathy_chatroom_get_account (chatroom))) {
			chatrooms = g_list_append (chatrooms, chatroom);
		}
	}

	return chatrooms;
}

guint
empathy_chatroom_manager_get_count (EmpathyChatroomManager *manager,
				   McAccount             *account)
{
	EmpathyChatroomManagerPriv *priv;
	GList                     *l;
	guint                      count = 0;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM_MANAGER (manager), 0);

	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_length (priv->chatrooms);
	}

	for (l = priv->chatrooms; l; l = l->next) {
		EmpathyChatroom *chatroom;

		chatroom = l->data;

		if (empathy_account_equal (account,
					  empathy_chatroom_get_account (chatroom))) {
			count++;
		}
	}

	return count;
}

/*
 * API to save/load and parse the chatrooms file.
 */

static gboolean
chatroom_manager_get_all (EmpathyChatroomManager *manager)
{
	EmpathyChatroomManagerPriv *priv;

	priv = GET_PRIV (manager);

	/* read file in */
	if (g_file_test (priv->file, G_FILE_TEST_EXISTS) &&
	    !chatroom_manager_file_parse (manager, priv->file))
    return FALSE;

	return TRUE;
}

static gboolean
chatroom_manager_file_parse (EmpathyChatroomManager *manager,
			     const gchar           *filename)
{
	EmpathyChatroomManagerPriv *priv;
	xmlParserCtxtPtr           ctxt;
	xmlDocPtr                  doc;
	xmlNodePtr                 chatrooms;
	xmlNodePtr                 node;

	priv = GET_PRIV (manager);

	DEBUG ("Attempting to parse file:'%s'...", filename);

	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	if (!empathy_xml_validate (doc, CHATROOMS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc(doc);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	/* The root node, chatrooms. */
	chatrooms = xmlDocGetRootElement (doc);

	for (node = chatrooms->children; node; node = node->next) {
		if (strcmp ((gchar *) node->name, "chatroom") == 0) {
			chatroom_manager_parse_chatroom (manager, node);
		}
	}

	DEBUG ("Parsed %d chatrooms", g_list_length (priv->chatrooms));

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static void
chatroom_manager_parse_chatroom (EmpathyChatroomManager *manager,
				 xmlNodePtr             node)
{
	EmpathyChatroomManagerPriv *priv;
	EmpathyChatroom            *chatroom;
	McAccount                 *account;
	xmlNodePtr                 child;
	gchar                     *str;
	gchar                     *name;
	gchar                     *room;
	gchar                     *account_id;
	gboolean                   auto_connect;

	priv = GET_PRIV (manager);

	/* default values. */
	name = NULL;
	room = NULL;
	auto_connect = TRUE;
	account_id = NULL;

	for (child = node->children; child; child = child->next) {
		gchar *tag;

		if (xmlNodeIsText (child)) {
			continue;
		}

		tag = (gchar *) child->name;
		str = (gchar *) xmlNodeGetContent (child);

		if (strcmp (tag, "name") == 0) {
			name = g_strdup (str);
		}
		else if (strcmp (tag, "room") == 0) {
			room = g_strdup (str);
		}
		else if (strcmp (tag, "auto_connect") == 0) {
			if (strcmp (str, "yes") == 0) {
				auto_connect = TRUE;
			} else {
				auto_connect = FALSE;
			}
		}
		else if (strcmp (tag, "account") == 0) {
			account_id = g_strdup (str);
		}

		xmlFree (str);
	}

	account = mc_account_lookup (account_id);
	if (!account) {
		g_free (name);
		g_free (room);
		g_free (account_id);
		return;
	}

	chatroom = empathy_chatroom_new_full (account, room, name, auto_connect);
  g_object_set (chatroom, "favorite", TRUE, NULL);
  add_chatroom (manager, chatroom);
	g_signal_emit (manager, signals[CHATROOM_ADDED], 0, chatroom);

	g_object_unref (account);
	g_free (name);
	g_free (room);
	g_free (account_id);
}

static gboolean
chatroom_manager_file_save (EmpathyChatroomManager *manager)
{
	EmpathyChatroomManagerPriv *priv;
	xmlDocPtr                  doc;
	xmlNodePtr                 root;
	GList                     *l;

	priv = GET_PRIV (manager);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "chatrooms");
	xmlDocSetRootElement (doc, root);

	for (l = priv->chatrooms; l; l = l->next) {
		EmpathyChatroom *chatroom;
		xmlNodePtr      node;
		const gchar    *account_id;
    gboolean favorite;

		chatroom = l->data;

    g_object_get (chatroom, "favorite", &favorite, NULL);
    if (!favorite)
      continue;

		account_id = mc_account_get_unique_name (empathy_chatroom_get_account (chatroom));

		node = xmlNewChild (root, NULL, "chatroom", NULL);
		xmlNewTextChild (node, NULL, "name", empathy_chatroom_get_name (chatroom));
		xmlNewTextChild (node, NULL, "room", empathy_chatroom_get_room (chatroom));
		xmlNewTextChild (node, NULL, "account", account_id);
		xmlNewTextChild (node, NULL, "auto_connect", empathy_chatroom_get_auto_connect (chatroom) ? "yes" : "no");
	}

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	DEBUG ("Saving file:'%s'", priv->file);
	xmlSaveFormatFileEnc (priv->file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();
	xmlMemoryDump ();

	return TRUE;
}

static void
chatroom_manager_chat_destroyed_cb (EmpathyTpChat *chat,
  gpointer user_data)
{
  EmpathyChatroomManager *manager = EMPATHY_CHATROOM_MANAGER (user_data);
  McAccount *account = empathy_tp_chat_get_account (chat);
  EmpathyChatroom *chatroom;
  const gchar *roomname;
  gboolean favorite;

  roomname = empathy_tp_chat_get_id (chat);
  chatroom = empathy_chatroom_manager_find (manager, account, roomname);

  if (chatroom == NULL)
    return;

  g_object_set (chatroom, "tp-chat", NULL, NULL);
  g_object_get (chatroom, "favorite", &favorite, NULL);

  if (!favorite)
    {
      /* Remove the chatroom from the list, unless it's in the list of
       * favourites..
       * FIXME this policy should probably not be in libempathy */
      empathy_chatroom_manager_remove (manager, chatroom);
    }
}

static void
chatroom_manager_observe_channel_cb (EmpathyDispatcher *dispatcher,
  EmpathyDispatchOperation *operation, gpointer user_data)
{
  EmpathyChatroomManager *manager = EMPATHY_CHATROOM_MANAGER (user_data);
  EmpathyChatroom *chatroom;
  TpChannel *channel;
  EmpathyTpChat *chat;
  const gchar *roomname;
  GQuark channel_type;
  TpHandleType handle_type;
  McAccount *account;

  channel_type = empathy_dispatch_operation_get_channel_type_id (operation);

  /* Observe Text channels to rooms only */
  if (channel_type != TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    return;

  channel = empathy_dispatch_operation_get_channel (operation);
  tp_channel_get_handle (channel, &handle_type);

  if (handle_type != TP_HANDLE_TYPE_ROOM)
    return;

  chat = EMPATHY_TP_CHAT (
    empathy_dispatch_operation_get_channel_wrapper (operation));
  account = empathy_tp_chat_get_account (chat);

  roomname = empathy_tp_chat_get_id (chat);

  chatroom = empathy_chatroom_manager_find (manager, account, roomname);

  if (chatroom == NULL)
    {
      chatroom = empathy_chatroom_new_full (account, roomname, roomname,
        FALSE);
      g_object_set (G_OBJECT (chatroom), "tp-chat", chat, NULL);
      empathy_chatroom_manager_add (manager, chatroom);
      g_object_unref (chatroom);
    }
  else
    {
      g_object_set (G_OBJECT (chatroom), "tp-chat", chat, NULL);
    }

  /* A TpChat is always destroyed as it only gets unreffed after the channel
   * has been invalidated in the dispatcher..  */
  g_signal_connect (chat, "destroy",
    G_CALLBACK (chatroom_manager_chat_destroyed_cb),
    manager);
}

void
empathy_chatroom_manager_observe (EmpathyChatroomManager *manager,
  EmpathyDispatcher *dispatcher)
{
  g_signal_connect (dispatcher, "observe",
    G_CALLBACK (chatroom_manager_observe_channel_cb), manager);
}
