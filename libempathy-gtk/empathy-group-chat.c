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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-chatroom.h>
#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-conf.h>

#include "empathy-group-chat.h"
#include "empathy-chat.h"
#include "empathy-chat-view.h"
#include "empathy-contact-list-store.h"
#include "empathy-contact-list-view.h"
//#include "empathy-chat-invite.h"
//#include "empathy-sound.h"
#include "empathy-images.h"
#include "empathy-ui-utils.h"
#include "empathy-preferences.h"

#define DEBUG_DOMAIN "GroupChat"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_GROUP_CHAT, EmpathyGroupChatPriv))

struct _EmpathyGroupChatPriv {
	EmpathyContactListStore *store;
	EmpathyContactListView  *view;
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
static void          group_chat_create_ui                (EmpathyGroupChat  *chat);
static void          group_chat_widget_destroy_cb        (GtkWidget         *widget,
							  EmpathyGroupChat  *chat);
static void          group_chat_members_changed_cb       (EmpathyTpChatroom *tp_chat,
							  EmpathyContact    *contact,
							  EmpathyContact    *actor,
							  guint              reason,
							  gchar             *message,
							  gboolean           is_member,
							  EmpathyGroupChat  *chat);
static void          group_chat_topic_entry_activate_cb  (GtkWidget         *entry,
							  GtkDialog         *dialog);
static void          group_chat_topic_response_cb        (GtkWidget         *dialog,
							  gint               response,			      
							  EmpathyGroupChat  *chat);
static const gchar * group_chat_get_name                 (EmpathyChat       *chat);
static gchar *       group_chat_get_tooltip              (EmpathyChat       *chat);
static const gchar * group_chat_get_status_icon_name     (EmpathyChat       *chat);
static GtkWidget *   group_chat_get_widget               (EmpathyChat       *chat);
static gboolean      group_chat_is_group_chat            (EmpathyChat       *chat);
static void          group_chat_set_tp_chat              (EmpathyChat       *chat,
							  EmpathyTpChat     *tp_chat);
static void          group_chat_subject_notify_cb        (EmpathyTpChat     *tp_chat,
							  GParamSpec        *param,
							  EmpathyGroupChat  *chat);
static void          group_chat_name_notify_cb           (EmpathyTpChat     *tp_chat,
							  GParamSpec        *param,
							  EmpathyGroupChat  *chat);
static gboolean      group_chat_key_press_event          (EmpathyChat       *chat,
							  GdkEventKey       *event);
static gint          group_chat_contacts_completion_func (const gchar       *s1,
							  const gchar       *s2,
							  gsize              n);

G_DEFINE_TYPE (EmpathyGroupChat, empathy_group_chat, EMPATHY_TYPE_CHAT)

static void
empathy_group_chat_class_init (EmpathyGroupChatClass *klass)
{
	GObjectClass    *object_class;
	EmpathyChatClass *chat_class;

	object_class = G_OBJECT_CLASS (klass);
	chat_class = EMPATHY_CHAT_CLASS (klass);

	object_class->finalize           = group_chat_finalize;

	chat_class->get_name             = group_chat_get_name;
	chat_class->get_tooltip          = group_chat_get_tooltip;
	chat_class->get_status_icon_name = group_chat_get_status_icon_name;
	chat_class->get_widget           = group_chat_get_widget;
	chat_class->is_group_chat        = group_chat_is_group_chat;
	chat_class->set_tp_chat          = group_chat_set_tp_chat;
	chat_class->key_press_event      = group_chat_key_press_event;

	g_type_class_add_private (object_class, sizeof (EmpathyGroupChatPriv));
}

static void
empathy_group_chat_init (EmpathyGroupChat *chat)
{
	EmpathyGroupChatPriv *priv;
	EmpathyChatView      *chatview;

	priv = GET_PRIV (chat);

	priv->contacts_visible = TRUE;

	chatview = EMPATHY_CHAT_VIEW (EMPATHY_CHAT (chat)->view);
	empathy_chat_view_set_is_group_chat (chatview, TRUE);

	group_chat_create_ui (chat);
}

static void
group_chat_finalize (GObject *object)
{
	EmpathyGroupChat     *chat;
	EmpathyGroupChatPriv *priv;

	empathy_debug (DEBUG_DOMAIN, "Finalized:%p", object);

	chat = EMPATHY_GROUP_CHAT (object);
	priv = GET_PRIV (chat);
	
	g_free (priv->name);
	g_free (priv->topic);
	g_object_unref (priv->store);
	g_object_unref (priv->tp_chat);	
	g_completion_free (priv->completion);

	G_OBJECT_CLASS (empathy_group_chat_parent_class)->finalize (object);
}

EmpathyGroupChat *
empathy_group_chat_new (McAccount *account,
			TpChan    *tp_chan)
{
	EmpathyGroupChat     *chat;
	EmpathyGroupChatPriv *priv;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	chat = g_object_new (EMPATHY_TYPE_GROUP_CHAT, NULL);

	priv = GET_PRIV (chat);

	EMPATHY_CHAT (chat)->account = g_object_ref (account);
	priv->tp_chat = empathy_tp_chatroom_new (account, tp_chan);
	empathy_chat_set_tp_chat (EMPATHY_CHAT (chat), EMPATHY_TP_CHAT (priv->tp_chat));

	return chat;
}

gboolean
empathy_group_chat_get_show_contacts (EmpathyGroupChat *chat)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_GROUP_CHAT (chat), FALSE);

	group_chat = EMPATHY_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->contacts_visible;
}

void
empathy_group_chat_set_show_contacts (EmpathyGroupChat *chat,
				     gboolean         show)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_if_fail (EMPATHY_IS_GROUP_CHAT (chat));

	group_chat = EMPATHY_GROUP_CHAT (chat);
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
empathy_group_chat_set_topic (EmpathyGroupChat *chat)
{
	EmpathyGroupChatPriv *priv;
	EmpathyChatWindow    *chat_window;
	GtkWidget           *chat_dialog;
	GtkWidget           *dialog;
	GtkWidget           *entry;
	GtkWidget           *hbox;
	const gchar         *topic;

	g_return_if_fail (EMPATHY_IS_GROUP_CHAT (chat));

	priv = GET_PRIV (chat);

	chat_window = empathy_chat_get_window (EMPATHY_CHAT (chat));
	chat_dialog = empathy_chat_window_get_dialog (chat_window);

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
group_chat_create_ui (EmpathyGroupChat *chat)
{
	EmpathyGroupChatPriv *priv;
	GladeXML            *glade;
 	GList               *list = NULL; 

	priv = GET_PRIV (chat);

	glade = empathy_glade_get_file ("empathy-group-chat.glade",
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

	empathy_glade_connect (glade,
			      chat,
			      "group_chat_widget", "destroy", group_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", g_object_ref (chat));

	/* Add room GtkTextView. */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_chat),
			   GTK_WIDGET (EMPATHY_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (EMPATHY_CHAT (chat)->view));

	/* Add input GtkTextView */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_input),
			   EMPATHY_CHAT (chat)->input_text_view);
	gtk_widget_show (EMPATHY_CHAT (chat)->input_text_view);

	/* Add nick name completion */
	priv->completion = g_completion_new ((GCompletionFunc) empathy_contact_get_name);
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
			      EmpathyGroupChat *chat)
{
	empathy_debug (DEBUG_DOMAIN, "Destroyed");

	g_object_unref (chat);
}

static void
group_chat_members_changed_cb (EmpathyTpChatroom *tp_chat,
			       EmpathyContact     *contact,
			       EmpathyContact     *actor,
			       guint               reason,
			       gchar              *message,
			       gboolean            is_member,
			       EmpathyGroupChat   *chat)
{
	EmpathyGroupChatPriv *priv;
	gchar                *str;

	priv = GET_PRIV (chat);

	if (is_member) {
		str = g_strdup_printf (_("%s has joined the room"),
				       empathy_contact_get_name (contact));
	} else {
		str = g_strdup_printf (_("%s has left the room"),
				       empathy_contact_get_name (contact));
	}
	empathy_chat_view_append_event (EMPATHY_CHAT (chat)->view, str);
	g_free (str);
}

static void
group_chat_topic_entry_activate_cb (GtkWidget *entry,
				    GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
group_chat_topic_response_cb (GtkWidget       *dialog,
			      gint             response,			      
			      EmpathyGroupChat *chat)
{
	if (response == GTK_RESPONSE_OK) {
		GtkWidget   *entry;
		const gchar *topic;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		topic = gtk_entry_get_text (GTK_ENTRY (entry));
		
		if (!G_STR_EMPTY (topic)) {
			EmpathyGroupChatPriv *priv;

			priv = GET_PRIV (chat);

			empathy_tp_chatroom_set_topic (priv->tp_chat, topic);
		}
	}

	gtk_widget_destroy (dialog);
}

static const gchar *
group_chat_get_name (EmpathyChat *chat)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_GROUP_CHAT (chat), NULL);

	group_chat = EMPATHY_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	if (!priv->name) {
		const gchar *id;
		const gchar *server;

		id = empathy_chat_get_id (chat);
		server = strstr (id, "@");

		if (server) {
			priv->name = g_strndup (id, server - id);
		} else {
			priv->name = g_strdup (id);
		} 
	}

	return priv->name;
}

static gchar *
group_chat_get_tooltip (EmpathyChat *chat)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_GROUP_CHAT (chat), NULL);

	group_chat = EMPATHY_GROUP_CHAT (chat);
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
group_chat_get_status_icon_name (EmpathyChat *chat)
{
	return EMPATHY_IMAGE_GROUP_MESSAGE;
}

static GtkWidget *
group_chat_get_widget (EmpathyChat *chat)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_GROUP_CHAT (chat), NULL);

	group_chat = EMPATHY_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->widget;
}

static gboolean
group_chat_is_group_chat (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_GROUP_CHAT (chat), FALSE);

	return TRUE;
}

static void
group_chat_set_tp_chat (EmpathyChat    *chat,
			EmpathyTpChat *tp_chat)
{
	EmpathyGroupChat     *group_chat;
	EmpathyGroupChatPriv *priv;

	g_return_if_fail (EMPATHY_IS_GROUP_CHAT (chat));

	group_chat = EMPATHY_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	/* Free all resources related to tp_chat */
	if (priv->tp_chat) {
		g_object_unref (priv->tp_chat);
		priv->tp_chat = NULL;
	}
	if (priv->view) {
		gtk_widget_destroy (GTK_WIDGET (priv->view));
		g_object_unref (priv->store);
	}
	g_free (priv->name);
	g_free (priv->topic);
	priv->name = NULL;
	priv->topic = NULL;

	if (!tp_chat) {
		/* We are no more connected */
		gtk_widget_set_sensitive (priv->hbox_topic, FALSE);
		gtk_widget_set_sensitive (priv->scrolled_window_contacts, FALSE);
		return;
	}

	/* We are connected */
	gtk_widget_set_sensitive (priv->hbox_topic, TRUE);
	gtk_widget_set_sensitive (priv->scrolled_window_contacts, TRUE);

	priv->tp_chat = g_object_ref (tp_chat);

	if (empathy_tp_chatroom_get_invitation (priv->tp_chat, NULL, NULL)) {
		empathy_tp_chatroom_accept_invitation (priv->tp_chat);
	}

	/* Create contact list */
	priv->store = empathy_contact_list_store_new (EMPATHY_CONTACT_LIST (priv->tp_chat));
	priv->view = empathy_contact_list_view_new (priv->store);
	empathy_contact_list_view_set_interactive (priv->view, TRUE);
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_contacts),
			   GTK_WIDGET (priv->view));
	gtk_widget_show (GTK_WIDGET (priv->view));

	/* Connect signals */
	g_signal_connect (priv->tp_chat, "members-changed",
			  G_CALLBACK (group_chat_members_changed_cb),
			  chat);
	g_signal_connect (priv->tp_chat, "notify::subject",
			  G_CALLBACK (group_chat_subject_notify_cb),
			  chat);
	g_signal_connect (priv->tp_chat, "notify::name",
			  G_CALLBACK (group_chat_name_notify_cb),
			  chat);
}

static void
group_chat_subject_notify_cb (EmpathyTpChat   *tp_chat,
			      GParamSpec      *param,
			      EmpathyGroupChat *chat)
{
	EmpathyGroupChatPriv *priv;
	gchar                *str = NULL;

	priv = GET_PRIV (chat);

	g_object_get (priv->tp_chat, "subject", &str, NULL);
	if (!tp_strdiff (priv->topic, str)) {
		g_free (str);
		return;
	}

	g_free (priv->topic);
	priv->topic = str;
	gtk_label_set_text (GTK_LABEL (priv->label_topic), priv->topic);

	if (!G_STR_EMPTY (priv->topic)) {
		str = g_strdup_printf (_("Topic set to: %s"), priv->topic);
	} else {
		str = g_strdup (_("No topic defined"));
	}
	empathy_chat_view_append_event (EMPATHY_CHAT (chat)->view, str);
	g_free (str);
}

static void
group_chat_name_notify_cb (EmpathyTpChat   *tp_chat,
			   GParamSpec      *param,
			   EmpathyGroupChat *chat)
{
	EmpathyGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	g_free (priv->name);
	g_object_get (priv->tp_chat, "name", &priv->name, NULL);
}

static gboolean
group_chat_key_press_event (EmpathyChat *chat,
			    GdkEventKey *event)
{
	EmpathyGroupChatPriv *priv = GET_PRIV (chat);

	if (!(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
	    event->keyval == GDK_Tab) {
		GtkTextBuffer *buffer;
		GtkTextIter    start, current;
		gchar         *nick, *completed;
		GList         *list, *completed_list;
		gboolean       is_start_of_buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (EMPATHY_CHAT (chat)->input_text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &current, gtk_text_buffer_get_insert (buffer));

		/* Get the start of the nick to complete. */
		gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
		gtk_text_iter_backward_word_start (&start);
		is_start_of_buffer = gtk_text_iter_is_start (&start);

		list = empathy_contact_list_get_members (EMPATHY_CONTACT_LIST (priv->tp_chat));
		g_completion_add_items (priv->completion, list);

		nick = gtk_text_buffer_get_text (buffer, &start, &current, FALSE);
		completed_list = g_completion_complete (priv->completion,
							nick,
							&completed);

		g_free (nick);

		if (completed) {
			guint        len;
			const gchar *text;
			gchar       *complete_char = NULL;

			gtk_text_buffer_delete (buffer, &start, &current);

			len = g_list_length (completed_list);

			if (len == 1) {
				/* If we only have one hit, use that text
				 * instead of the text in completed since the
				 * completed text will use the typed string
				 * which might be cased all wrong.
				 * Fixes #120876
				 * */
				text = empathy_contact_get_name (completed_list->data);
			} else {
				text = completed;
			}

			gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));

			if (len == 1 && is_start_of_buffer &&
			    empathy_conf_get_string (empathy_conf_get (),
						     EMPATHY_PREFS_CHAT_NICK_COMPLETION_CHAR,
						     &complete_char) &&
			    complete_char != NULL) {
				gtk_text_buffer_insert_at_cursor (buffer,
								  complete_char,
								  strlen (complete_char));
				gtk_text_buffer_insert_at_cursor (buffer, " ", 1);
				g_free (complete_char);
			}

			g_free (completed);
		}

		g_completion_clear_items (priv->completion);

		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);

		return TRUE;
	}

	return FALSE;
}

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

