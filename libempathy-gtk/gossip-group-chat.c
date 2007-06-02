/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-chatroom.h>
#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/gossip-debug.h>

#include "gossip-group-chat.h"
#include "gossip-chat.h"
#include "gossip-chat-view.h"
#include "gossip-contact-list-store.h"
#include "gossip-contact-list-view.h"
//#include "gossip-chat-invite.h"
//#include "gossip-sound.h"
#include "empathy-images.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "GroupChat"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_GROUP_CHAT, GossipGroupChatPriv))

struct _GossipGroupChatPriv {
	GossipContactListStore *store;
	GossipContactListView  *view;
	EmpathyTpChatroom      *tp_chat;

	GtkWidget              *widget;
	GtkWidget              *hpaned;
	GtkWidget              *vbox_left;
	GtkWidget              *scrolled_window_chat;
	GtkWidget              *scrolled_window_input;
	GtkWidget              *scrolled_window_contacts;
	GtkWidget              *hbox_topic;
	GtkWidget              *label_topic;

	gchar                  *topic;
	gchar                  *name;
	GCompletion            *completion;

	gint                    contacts_width;
	gboolean                contacts_visible;
};

static void          group_chat_finalize                 (GObject           *object);
static void          group_chat_create_ui                (GossipGroupChat   *chat);
static void          group_chat_widget_destroy_cb        (GtkWidget         *widget,
							  GossipGroupChat   *chat);
static void          group_chat_contact_added_cb         (EmpathyTpChatroom *tp_chat,
							  GossipContact     *contact,
							  GossipGroupChat   *chat);
static void          group_chat_contact_removed_cb       (EmpathyTpChatroom *tp_chat,
							  GossipContact     *contact,
							  GossipGroupChat   *chat);
/*static void          group_chat_topic_changed_cb         (EmpathyTpChatroom *tp_chat,
							  const gchar       *new_topic,
							  GossipGroupChat   *chat);*/
static void          group_chat_topic_entry_activate_cb  (GtkWidget         *entry,
							  GtkDialog         *dialog);
static void          group_chat_topic_response_cb        (GtkWidget         *dialog,
							  gint               response,			      
							  GossipGroupChat   *chat);
static const gchar * group_chat_get_name                 (GossipChat        *chat);
static gchar *       group_chat_get_tooltip              (GossipChat        *chat);
static const gchar * group_chat_get_status_icon_name     (GossipChat        *chat);
static GtkWidget *   group_chat_get_widget               (GossipChat        *chat);
static gboolean      group_chat_is_group_chat            (GossipChat        *chat);
/*static gboolean      group_chat_key_press_event          (GtkWidget         *widget,
							  GdkEventKey       *event,
							  GossipGroupChat   *chat);*/
static gint          group_chat_contacts_completion_func (const gchar       *s1,
							  const gchar       *s2,
							  gsize              n);

G_DEFINE_TYPE (GossipGroupChat, gossip_group_chat, GOSSIP_TYPE_CHAT)

static void
gossip_group_chat_class_init (GossipGroupChatClass *klass)
{
	GObjectClass    *object_class;
	GossipChatClass *chat_class;

	object_class = G_OBJECT_CLASS (klass);
	chat_class = GOSSIP_CHAT_CLASS (klass);

	object_class->finalize           = group_chat_finalize;

	chat_class->get_name             = group_chat_get_name;
	chat_class->get_tooltip          = group_chat_get_tooltip;
	chat_class->get_status_icon_name = group_chat_get_status_icon_name;
	chat_class->get_widget           = group_chat_get_widget;
	chat_class->is_group_chat        = group_chat_is_group_chat;

	g_type_class_add_private (object_class, sizeof (GossipGroupChatPriv));
}

static void
gossip_group_chat_init (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatView      *chatview;

	priv = GET_PRIV (chat);

	priv->contacts_visible = TRUE;

	chatview = GOSSIP_CHAT_VIEW (GOSSIP_CHAT (chat)->view);
	gossip_chat_view_set_is_group_chat (chatview, TRUE);

	group_chat_create_ui (chat);
}

static void
group_chat_finalize (GObject *object)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;

	gossip_debug (DEBUG_DOMAIN, "Finalized:%p", object);

	chat = GOSSIP_GROUP_CHAT (object);
	priv = GET_PRIV (chat);
	
	g_free (priv->name);
	g_free (priv->topic);
	g_object_unref (priv->store);
	g_object_unref (priv->tp_chat);	
	g_completion_free (priv->completion);

	G_OBJECT_CLASS (gossip_group_chat_parent_class)->finalize (object);
}

GossipGroupChat *
gossip_group_chat_new (McAccount *account,
		       TpChan    *tp_chan)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	chat = g_object_new (GOSSIP_TYPE_GROUP_CHAT, NULL);

	priv = GET_PRIV (chat);

	GOSSIP_CHAT (chat)->account = g_object_ref (account);
	priv->tp_chat = empathy_tp_chatroom_new (account, tp_chan);
	gossip_chat_set_tp_chat (GOSSIP_CHAT (chat), EMPATHY_TP_CHAT (priv->tp_chat));

	/* FIXME: Ask the user before accepting */
	empathy_tp_chatroom_accept_invitation (priv->tp_chat);

	/* Create contact list */
	priv->store = gossip_contact_list_store_new (EMPATHY_CONTACT_LIST (priv->tp_chat));
	priv->view = gossip_contact_list_view_new (priv->store);
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_contacts),
			   GTK_WIDGET (priv->view));
	gtk_widget_show (GTK_WIDGET (priv->view));

	g_signal_connect (priv->tp_chat, "contact-added",
			  G_CALLBACK (group_chat_contact_added_cb),
			  chat);
	g_signal_connect (priv->tp_chat, "contact-removed",
			  G_CALLBACK (group_chat_contact_removed_cb),
			  chat);
/*	g_signal_connect (priv->tp_chat, "chatroom-topic-changed",
			  G_CALLBACK (group_chat_topic_changed_cb),
			  chat);
	g_signal_connect (priv->tp_chat, "contact-info-changed",
			  G_CALLBACK (group_chat_contact_info_changed_cb),
			  chat);*/

	return chat;
}

gboolean
gossip_group_chat_get_show_contacts (GossipGroupChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->contacts_visible;
}

void
gossip_group_chat_set_show_contacts (GossipGroupChat *chat,
				     gboolean         show)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_GROUP_CHAT (chat));

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	priv->contacts_visible = show;

	if (show) {
		gtk_widget_show (priv->scrolled_window_contacts);
		gtk_paned_set_position (GTK_PANED (priv->hpaned),
					priv->contacts_width);
	} else {
		priv->contacts_width = gtk_paned_get_position (GTK_PANED (priv->hpaned));
		gtk_widget_hide (priv->scrolled_window_contacts);
	}
}

void
gossip_group_chat_set_topic (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatWindow    *chat_window;
	GtkWidget           *chat_dialog;
	GtkWidget           *dialog;
	GtkWidget           *entry;
	GtkWidget           *hbox;
	const gchar         *topic;

	g_return_if_fail (GOSSIP_IS_GROUP_CHAT (chat));

	priv = GET_PRIV (chat);

	chat_window = gossip_chat_get_window (GOSSIP_CHAT (chat));
	chat_dialog = gossip_chat_window_get_dialog (chat_window);

	dialog = gtk_message_dialog_new (GTK_WINDOW (chat_dialog),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Enter the new topic you want to set for this room:"));

	topic = gtk_label_get_text (GTK_LABEL (priv->label_topic));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    hbox, FALSE, TRUE, 4);

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), topic);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
		    
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);

	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label, "use-markup", TRUE, NULL);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (group_chat_topic_entry_activate_cb),
			  dialog);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (group_chat_topic_response_cb),
			  chat);

	gtk_widget_show_all (dialog);
}

static void
group_chat_create_ui (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GladeXML            *glade;
 	GList               *list = NULL; 

	priv = GET_PRIV (chat);

	glade = gossip_glade_get_file ("gossip-group-chat.glade",
				       "group_chat_widget",
				       NULL,
				       "group_chat_widget", &priv->widget,
				       "hpaned", &priv->hpaned,
				       "vbox_left", &priv->vbox_left,
				       "scrolled_window_chat", &priv->scrolled_window_chat,
				       "scrolled_window_input", &priv->scrolled_window_input,
				       "hbox_topic", &priv->hbox_topic,
				       "label_topic", &priv->label_topic,
				       "scrolled_window_contacts", &priv->scrolled_window_contacts,
				       NULL);

	gossip_glade_connect (glade,
			      chat,
			      "group_chat_widget", "destroy", group_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", g_object_ref (chat));

	/* Add room GtkTextView. */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_chat),
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	/* Add input GtkTextView */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_input),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);

	/* Add nick name completion */
	priv->completion = g_completion_new (NULL);
	g_completion_set_compare (priv->completion,
				  group_chat_contacts_completion_func);

	/* Set widget focus order */
	list = g_list_append (NULL, priv->scrolled_window_input);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->vbox_left), list);
	g_list_free (list);

	list = g_list_append (NULL, priv->vbox_left);
	list = g_list_append (list, priv->scrolled_window_contacts);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->hpaned), list);
	g_list_free (list);

	list = g_list_append (NULL, priv->hpaned);
	list = g_list_append (list, priv->hbox_topic);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->widget), list);
	g_list_free (list);
}

static void
group_chat_widget_destroy_cb (GtkWidget       *widget,
			      GossipGroupChat *chat)
{
	gossip_debug (DEBUG_DOMAIN, "Destroyed");

	g_object_unref (chat);
}

static void
group_chat_contact_added_cb (EmpathyTpChatroom *tp_chat,
			     GossipContact     *contact,
			     GossipGroupChat   *chat)
{
	GossipGroupChatPriv *priv;
	gchar               *str;

	priv = GET_PRIV (chat);

	str = g_strdup_printf (_("%s has joined the room"),
			       gossip_contact_get_name (contact));
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
	g_free (str);
}

static void
group_chat_contact_removed_cb (EmpathyTpChatroom *tp_chat,
			       GossipContact     *contact,
			       GossipGroupChat   *chat)
{
	GossipGroupChatPriv *priv;
	gchar               *str;

	priv = GET_PRIV (chat);

	str = g_strdup_printf (_("%s has left the room"),
			       gossip_contact_get_name (contact));
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
	g_free (str);
}
/*
static void
group_chat_topic_changed_cb (EmpathyTpChatroom *tp_chat,
			     const gchar       *new_topic,
			     GossipGroupChat   *chat)
{
	GossipGroupChatPriv *priv;
	gchar               *str;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Topic changed by to:'%s'", new_topic);

	g_free (priv->topic);
	priv->topic = g_strdup (new_topic);
	
	gtk_label_set_text (GTK_LABEL (priv->label_topic), new_topic);

	str = g_strdup_printf (_("Topic set to: %s"), new_topic);
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
	g_free (str);
}
*/
static void
group_chat_topic_entry_activate_cb (GtkWidget *entry,
				    GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
group_chat_topic_response_cb (GtkWidget       *dialog,
			      gint             response,			      
			      GossipGroupChat *chat)
{
	if (response == GTK_RESPONSE_OK) {
		GtkWidget   *entry;
		const gchar *topic;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		topic = gtk_entry_get_text (GTK_ENTRY (entry));
		
		if (!G_STR_EMPTY (topic)) {
			GossipGroupChatPriv *priv;

			priv = GET_PRIV (chat);

			empathy_tp_chatroom_set_topic (priv->tp_chat, topic);
		}
	}

	gtk_widget_destroy (dialog);
}

static const gchar *
group_chat_get_name (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->name;
}

static gchar *
group_chat_get_tooltip (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	if (priv->topic) {
		gchar *topic, *tmp;

		topic = g_strdup_printf (_("Topic: %s"), priv->topic);
		tmp = g_strdup_printf ("%s\n%s", priv->name, topic);
		g_free (topic);

		return tmp;
	}

	return g_strdup (priv->name);
}

static const gchar *
group_chat_get_status_icon_name (GossipChat *chat)
{
	return EMPATHY_IMAGE_GROUP_MESSAGE;
}

static GtkWidget *
group_chat_get_widget (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->widget;
}

static gboolean
group_chat_is_group_chat (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);

	return TRUE;
}
#if 0
static gboolean
group_chat_key_press_event (GtkWidget       *widget,
			    GdkEventKey     *event,
			    GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkAdjustment       *adj;
	gdouble              val;
	GtkTextBuffer       *buffer;
	GtkTextIter          start, current;
	gchar               *nick, *completed;
	gint                 len;
	GList               *list, *l, *completed_list;
	gboolean             is_start_of_buffer;

	priv = GET_PRIV (chat);

	if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
	    (event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK &&
	    event->keyval == GDK_Tab) {
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &current, gtk_text_buffer_get_insert (buffer));

		/* Get the start of the nick to complete. */
		gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
		gtk_text_iter_backward_word_start (&start);
		is_start_of_buffer = gtk_text_iter_is_start (&start);

		nick = gtk_text_buffer_get_text (buffer, &start, &current, FALSE);

		g_completion_clear_items (priv->completion);

		len = strlen (nick);

		list = group_chat_get_nick_list (chat);

		g_completion_add_items (priv->completion, list);

		completed_list = g_completion_complete (priv->completion,
							nick,
							&completed);

		g_free (nick);

		if (completed) {
			int       len;
			gchar    *text;

			gtk_text_buffer_delete (buffer, &start, &current);

			len = g_list_length (completed_list);

			if (len == 1) {
				/* If we only have one hit, use that text
				 * instead of the text in completed since the
				 * completed text will use the typed string
				 * which might be cased all wrong.
				 * Fixes #120876
				 * */
				text = (gchar *) completed_list->data;
			} else {
				text = completed;
			}

			gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));

			if (len == 1) {
				if (is_start_of_buffer) {
					gtk_text_buffer_insert_at_cursor (buffer, ", ", 2);
				}
			}

			g_free (completed);
		}

		g_completion_clear_items (priv->completion);

		for (l = list; l; l = l->next) {
			g_free (l->data);
		}

		g_list_free (list);

		return TRUE;
	}

	return FALSE;
}
#endif

static gint
group_chat_contacts_completion_func (const gchar *s1,
				     const gchar *s2,
				     gsize        n)
{
	gchar *tmp, *nick1, *nick2;
	gint   ret;

	tmp = g_utf8_normalize (s1, -1, G_NORMALIZE_DEFAULT);
	nick1 = g_utf8_casefold (tmp, -1);
	g_free (tmp);

	tmp = g_utf8_normalize (s2, -1, G_NORMALIZE_DEFAULT);
	nick2 = g_utf8_casefold (tmp, -1);
	g_free (tmp);

	ret = strncmp (nick1, nick2, n);

	g_free (nick1);
	g_free (nick2);

	return ret;
}

