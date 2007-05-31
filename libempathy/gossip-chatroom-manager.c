/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-debug.h"
#include "gossip-chatroom-manager.h"
#include "gossip-utils.h"

#define DEBUG_DOMAIN "ChatroomManager"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerPriv))

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "gossip-chatroom-manager.dtd"

struct _GossipChatroomManagerPriv {
	GList *chatrooms;
};

static void     gossip_chatroom_manager_class_init (GossipChatroomManagerClass *klass);
static void     gossip_chatroom_manager_init       (GossipChatroomManager      *manager);
static void     chatroom_manager_finalize          (GObject                    *object);
static gboolean chatroom_manager_get_all           (GossipChatroomManager      *manager);
static gboolean chatroom_manager_file_parse        (GossipChatroomManager      *manager,
						    const gchar                *filename);
static void     chatroom_manager_parse_chatroom    (GossipChatroomManager      *manager,
						    xmlNodePtr                  node);
static gboolean chatroom_manager_file_save         (GossipChatroomManager      *manager);

enum {
	CHATROOM_ADDED,
	CHATROOM_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GossipChatroomManager, gossip_chatroom_manager, G_TYPE_OBJECT);

static void
gossip_chatroom_manager_class_init (GossipChatroomManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chatroom_manager_finalize;

	signals[CHATROOM_ADDED] =
		g_signal_new ("chatroom-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);
	signals[CHATROOM_REMOVED] =
		g_signal_new ("chatroom-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);

	g_type_class_add_private (object_class,
				  sizeof (GossipChatroomManagerPriv));
}

static void
gossip_chatroom_manager_init (GossipChatroomManager *manager)
{
}

static void
chatroom_manager_finalize (GObject *object)
{
	GossipChatroomManagerPriv *priv;

	priv = GET_PRIV (object);

	g_list_foreach (priv->chatrooms, (GFunc) g_object_unref, NULL);
	g_list_free (priv->chatrooms);

	(G_OBJECT_CLASS (gossip_chatroom_manager_parent_class)->finalize) (object);
}

GossipChatroomManager *
gossip_chatroom_manager_new (void)
{
	static GossipChatroomManager *manager = NULL;

	if (!manager) {
		GossipChatroomManagerPriv *priv;

		manager = g_object_new (GOSSIP_TYPE_CHATROOM_MANAGER, NULL);
		priv = GET_PRIV (manager);
		chatroom_manager_get_all (manager);
	
		g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
	} else {
		g_object_ref (manager);
	}

	return manager;
}

gboolean
gossip_chatroom_manager_add (GossipChatroomManager *manager,
			     GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (manager);

	/* don't add more than once */
	if (!gossip_chatroom_manager_find (manager,
					   gossip_chatroom_get_account (chatroom),
					   gossip_chatroom_get_room (chatroom))) {
		priv->chatrooms = g_list_append (priv->chatrooms, g_object_ref (chatroom));
		chatroom_manager_file_save (manager);

		g_signal_emit (manager, signals[CHATROOM_ADDED], 0, chatroom);

		return TRUE;
	}

	return FALSE;
}

void
gossip_chatroom_manager_remove (GossipChatroomManager *manager,
				GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *this_chatroom;

		this_chatroom = l->data;

		if (gossip_chatroom_equal (chatroom, this_chatroom)) {
			priv->chatrooms = g_list_delete_link (priv->chatrooms, l);

			chatroom_manager_file_save (manager);

			g_signal_emit (manager, signals[CHATROOM_REMOVED], 0, this_chatroom);
			g_object_unref (this_chatroom);
			break;
		}
	}
}

GossipChatroom *
gossip_chatroom_manager_find (GossipChatroomManager *manager,
			      McAccount             *account,
			      const gchar           *room)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (room != NULL, NULL);

	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		McAccount      *this_account;
		const gchar    *this_room;

		chatroom = l->data;
		this_account = gossip_chatroom_get_account (chatroom);
		this_room = gossip_chatroom_get_room (chatroom);

		if (gossip_account_equal (account, this_account) &&
		    strcmp (this_room, room) == 0) {
			return chatroom;
		}
	}

	return NULL;
}

GList *
gossip_chatroom_manager_get_chatrooms (GossipChatroomManager *manager,
				       McAccount             *account)
{
	GossipChatroomManagerPriv *priv;
	GList                     *chatrooms, *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_copy (priv->chatrooms);
	}

	chatrooms = NULL;
	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;

		chatroom = l->data;

		if (gossip_account_equal (account,
					  gossip_chatroom_get_account (chatroom))) {
			chatrooms = g_list_append (chatrooms, chatroom);
		}
	}

	return chatrooms;
}

guint
gossip_chatroom_manager_get_count (GossipChatroomManager *manager,
				   McAccount             *account)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;
	guint                      count = 0;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), 0);

	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_length (priv->chatrooms);
	}

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;

		chatroom = l->data;

		if (gossip_account_equal (account,
					  gossip_chatroom_get_account (chatroom))) {
			count++;
		}
	}

	return count;
}

void
gossip_chatroom_manager_store (GossipChatroomManager *manager)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));

	chatroom_manager_file_save (manager);
}

/*
 * API to save/load and parse the chatrooms file.
 */

static gboolean
chatroom_manager_get_all (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	gchar                     *dir;
	gchar                     *file_with_path = NULL;

	priv = GET_PRIV (manager);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	file_with_path = g_build_filename (dir, CHATROOMS_XML_FILENAME, NULL);
	g_free (dir);

	/* read file in */
	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS) &&
	    !chatroom_manager_file_parse (manager, file_with_path)) {
		g_free (file_with_path);
		return FALSE;
	}

	g_free (file_with_path);

	return TRUE;
}

static gboolean
chatroom_manager_file_parse (GossipChatroomManager *manager,
			     const gchar           *filename)
{
	GossipChatroomManagerPriv *priv;
	xmlParserCtxtPtr           ctxt;
	xmlDocPtr                  doc;
	xmlNodePtr                 chatrooms;
	xmlNodePtr                 node;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, "Attempting to parse file:'%s'...", filename);

	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	if (!gossip_xml_validate (doc, CHATROOMS_DTD_FILENAME)) {
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

	gossip_debug (DEBUG_DOMAIN,
		      "Parsed %d chatrooms",
		      g_list_length (priv->chatrooms));

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static void
chatroom_manager_parse_chatroom (GossipChatroomManager *manager,
				 xmlNodePtr             node)
{
	GossipChatroomManagerPriv *priv;
	GossipChatroom            *chatroom;
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

	chatroom = gossip_chatroom_new_full (account,
					     room,
					     name,
					     auto_connect);

	priv->chatrooms = g_list_prepend (priv->chatrooms, chatroom);

	g_object_unref (account);
	g_free (name);
	g_free (room);
	g_free (account_id);
}

static gboolean
chatroom_manager_file_save (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	xmlDocPtr                  doc;
	xmlNodePtr                 root;
	GList                     *l;
	gchar                     *dir;
	gchar                     *file;

	priv = GET_PRIV (manager);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	file = g_build_filename (dir, CHATROOMS_XML_FILENAME, NULL);
	g_free (dir);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "chatrooms");
	xmlDocSetRootElement (doc, root);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		xmlNodePtr      node;
		const gchar    *account_id;

		chatroom = l->data;
		account_id = mc_account_get_unique_name (gossip_chatroom_get_account (chatroom));

		node = xmlNewChild (root, NULL, "chatroom", NULL);
		xmlNewTextChild (node, NULL, "name", gossip_chatroom_get_name (chatroom));
		xmlNewTextChild (node, NULL, "room", gossip_chatroom_get_room (chatroom));
		xmlNewTextChild (node, NULL, "account", account_id);
		xmlNewTextChild (node, NULL, "auto_connect", gossip_chatroom_get_auto_connect (chatroom) ? "yes" : "no");
	}

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", file);
	xmlSaveFormatFileEnc (file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();
	xmlMemoryDump ();

	g_free (file);

	return TRUE;
}
