/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-account-manager.h>
#include <libempathy/empathy-log-manager.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-dispatcher.h>

#include "empathy-chat.h"
#include "empathy-conf.h"
#include "empathy-spell.h"
#include "empathy-spell-dialog.h"
#include "empathy-contact-list-store.h"
#include "empathy-contact-list-view.h"
#include "empathy-contact-menu.h"
#include "empathy-theme-manager.h"
#include "empathy-smiley-manager.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CHAT
#include <libempathy/empathy-debug.h>

#define CHAT_DIR_CREATE_MODE  (S_IRUSR | S_IWUSR | S_IXUSR)
#define CHAT_FILE_CREATE_MODE (S_IRUSR | S_IWUSR)
#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)
#define MAX_INPUT_HEIGHT 150
#define COMPOSING_STOP_TIMEOUT 5

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyChat)
typedef struct {
	EmpathyTpChat     *tp_chat;
	gulong            tp_chat_destroy_handler;
	McAccount         *account;
	gchar             *id;
	gchar             *name;
	gchar             *subject;
	EmpathyContact    *remote_contact;

	EmpathyLogManager *log_manager;
	EmpathyAccountManager *account_manager;
	GSList            *sent_messages;
	gint               sent_messages_index;
	GList             *compositors;
	GCompletion       *completion;
	guint              composing_stop_timeout_id;
	guint              block_events_timeout_id;
	TpHandleType       handle_type;
	gint               contacts_width;
	gboolean           has_input_vscroll;

	GtkWidget         *widget;
	GtkWidget         *hpaned;
	GtkWidget         *vbox_left;
	GtkWidget         *scrolled_window_chat;
	GtkWidget         *scrolled_window_input;
	GtkWidget         *scrolled_window_contacts;
	GtkWidget         *hbox_topic;
	GtkWidget         *label_topic;
	GtkWidget         *contact_list_view;
} EmpathyChatPriv;

enum {
	COMPOSING,
	NEW_MESSAGE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_TP_CHAT,
	PROP_ACCOUNT,
	PROP_ID,
	PROP_NAME,
	PROP_SUBJECT,
	PROP_REMOTE_CONTACT,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EmpathyChat, empathy_chat, GTK_TYPE_BIN);

static void
chat_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	EmpathyChat *chat = EMPATHY_CHAT (object);
	EmpathyChatPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TP_CHAT:
		g_value_set_object (value, priv->tp_chat);
		break;
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_NAME:
		g_value_set_string (value, empathy_chat_get_name (chat));
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_SUBJECT:
		g_value_set_string (value, priv->subject);
		break;
	case PROP_REMOTE_CONTACT:
		g_value_set_object (value, priv->remote_contact);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
chat_set_property (GObject      *object,
		   guint         param_id,
		   const GValue *value,
		   GParamSpec   *pspec)
{
	EmpathyChat *chat = EMPATHY_CHAT (object);

	switch (param_id) {
	case PROP_TP_CHAT:
		empathy_chat_set_tp_chat (chat, EMPATHY_TP_CHAT (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
chat_connection_changed_cb (EmpathyAccountManager *manager,
			    McAccount *account,
			    TpConnectionStatusReason reason,
			    TpConnectionStatus current,
			    TpConnectionStatus previous,
			    EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	if (current == TP_CONNECTION_STATUS_CONNECTED && !priv->tp_chat &&
	    empathy_account_equal (account, priv->account) &&
	    priv->handle_type != TP_HANDLE_TYPE_NONE &&
	    !EMP_IS_EMPTY (priv->id)) {
		
		DEBUG ("Account reconnected, request a new Text channel");

		switch (priv->handle_type) {
			case TP_HANDLE_TYPE_CONTACT:
				empathy_dispatcher_chat_with_contact_id (account, priv->id,
					NULL, NULL);
				break;
			case TP_HANDLE_TYPE_ROOM:
				empathy_dispatcher_join_muc (account, priv->id, NULL, NULL);
				break;
			default:
				g_assert_not_reached ();
				break;
		}
	}
}

static void
chat_composing_remove_timeout (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->composing_stop_timeout_id) {
		g_source_remove (priv->composing_stop_timeout_id);
		priv->composing_stop_timeout_id = 0;
	}
}

static gboolean
chat_composing_stop_timeout_cb (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->composing_stop_timeout_id = 0;
	empathy_tp_chat_set_state (priv->tp_chat,
				   TP_CHANNEL_CHAT_STATE_PAUSED);

	return FALSE;
}

static void
chat_composing_start (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->composing_stop_timeout_id) {
		/* Just restart the timeout */
		chat_composing_remove_timeout (chat);
	} else {
		empathy_tp_chat_set_state (priv->tp_chat,
					   TP_CHANNEL_CHAT_STATE_COMPOSING);
	}

	priv->composing_stop_timeout_id = g_timeout_add_seconds (
		COMPOSING_STOP_TIMEOUT,
		(GSourceFunc) chat_composing_stop_timeout_cb,
		chat);
}

static void
chat_composing_stop (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	chat_composing_remove_timeout (chat);
	empathy_tp_chat_set_state (priv->tp_chat,
				   TP_CHANNEL_CHAT_STATE_ACTIVE);
}

static void 
chat_sent_message_add (EmpathyChat  *chat,
		       const gchar *str)
{
	EmpathyChatPriv *priv;
	GSList         *list;
	GSList         *item;

	priv = GET_PRIV (chat);

	/* Save the sent message in our repeat buffer */
	list = priv->sent_messages;
	
	/* Remove any other occurances of this msg */
	while ((item = g_slist_find_custom (list, str, (GCompareFunc) strcmp)) != NULL) {
		list = g_slist_remove_link (list, item);
		g_free (item->data);
		g_slist_free1 (item);
	}

	/* Trim the list to the last 10 items */
	while (g_slist_length (list) > 10) {
		item = g_slist_last (list);
		if (item) {
			list = g_slist_remove_link (list, item);
			g_free (item->data);
			g_slist_free1 (item);
		}
	}

	/* Add new message */
	list = g_slist_prepend (list, g_strdup (str));

	/* Set list and reset the index */
	priv->sent_messages = list;
	priv->sent_messages_index = -1;
}

static const gchar *
chat_sent_message_get_next (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;
	gint            max;
	
	priv = GET_PRIV (chat);

	if (!priv->sent_messages) {
		DEBUG ("No sent messages, next message is NULL");
		return NULL;
	}

	max = g_slist_length (priv->sent_messages) - 1;

	if (priv->sent_messages_index < max) {
		priv->sent_messages_index++;
	}
	
	DEBUG ("Returning next message index:%d", priv->sent_messages_index);

	return g_slist_nth_data (priv->sent_messages, priv->sent_messages_index);
}

static const gchar *
chat_sent_message_get_last (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	priv = GET_PRIV (chat);
	
	if (!priv->sent_messages) {
		DEBUG ("No sent messages, last message is NULL");
		return NULL;
	}

	if (priv->sent_messages_index >= 0) {
		priv->sent_messages_index--;
	}

	DEBUG ("Returning last message index:%d", priv->sent_messages_index);

	return g_slist_nth_data (priv->sent_messages, priv->sent_messages_index);
}

static void
chat_send (EmpathyChat  *chat,
	   const gchar *msg)
{
	EmpathyChatPriv *priv;
	EmpathyMessage  *message;

	priv = GET_PRIV (chat);

	if (EMP_STR_EMPTY (msg)) {
		return;
	}

	chat_sent_message_add (chat, msg);

	if (g_str_has_prefix (msg, "/clear")) {
		empathy_chat_view_clear (chat->view);
		return;
	}

	message = empathy_message_new (msg);

	empathy_tp_chat_send (priv->tp_chat, message);

	g_object_unref (message);
}

static void
chat_input_text_view_send (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;
	GtkTextBuffer  *buffer;
	GtkTextIter     start, end;
	gchar	       *msg;

	priv = GET_PRIV (chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* clear the input field */
	gtk_text_buffer_set_text (buffer, "", -1);

	chat_send (chat, msg);
	g_free (msg);
}

static void
chat_state_changed_cb (EmpathyTpChat      *tp_chat,
		       EmpathyContact     *contact,
		       TpChannelChatState  state,
		       EmpathyChat        *chat)
{
	EmpathyChatPriv *priv;
	GList          *l;
	gboolean        was_composing;

	priv = GET_PRIV (chat);

	if (empathy_contact_is_user (contact)) {
		/* We don't care about our own chat state */
		return;
	}

	was_composing = (priv->compositors != NULL);

	/* Find the contact in the list. After that l is the list elem or NULL */
	for (l = priv->compositors; l; l = l->next) {
		if (contact == l->data) {
			break;
		}
	}

	switch (state) {
	case TP_CHANNEL_CHAT_STATE_GONE:
	case TP_CHANNEL_CHAT_STATE_INACTIVE:
	case TP_CHANNEL_CHAT_STATE_PAUSED:
	case TP_CHANNEL_CHAT_STATE_ACTIVE:
		/* Contact is not composing */
		if (l) {
			priv->compositors = g_list_remove_link (priv->compositors, l);
			g_object_unref (l->data);
			g_list_free1 (l);
		}
		break;
	case TP_CHANNEL_CHAT_STATE_COMPOSING:
		/* Contact is composing */
		if (!l) {
			priv->compositors = g_list_prepend (priv->compositors,
							    g_object_ref (contact));
		}
		break;
	default:
		g_assert_not_reached ();
	}

	DEBUG ("Was composing: %s now composing: %s",
		was_composing ? "yes" : "no",
		priv->compositors ? "yes" : "no");

	if ((was_composing && !priv->compositors) ||
	    (!was_composing && priv->compositors)) {
		/* Composing state changed */
		g_signal_emit (chat, signals[COMPOSING], 0,
			       priv->compositors != NULL);
	}
}

static void
chat_message_received (EmpathyChat *chat, EmpathyMessage *message)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	EmpathyContact  *sender;

	sender = empathy_message_get_sender (message);

	DEBUG ("Appending new message from %s (%d)",
		empathy_contact_get_name (sender),
		empathy_contact_get_handle (sender));

	empathy_chat_view_append_message (chat->view, message);

	/* We received a message so the contact is no longer composing */
	chat_state_changed_cb (priv->tp_chat, sender,
			       TP_CHANNEL_CHAT_STATE_ACTIVE,
			       chat);

	g_signal_emit (chat, signals[NEW_MESSAGE], 0, message);
}

static void
chat_message_received_cb (EmpathyTpChat  *tp_chat,
			  EmpathyMessage *message,
			  EmpathyChat    *chat)
{
	chat_message_received (chat, message);
	empathy_tp_chat_acknowledge_message (tp_chat, message);
}

static void
chat_send_error_cb (EmpathyTpChat          *tp_chat,
		    EmpathyMessage         *message,
		    TpChannelTextSendError  error_code,
		    EmpathyChat            *chat)
{
	const gchar *error;
	gchar       *str;

	switch (error_code) {
	case TP_CHANNEL_TEXT_SEND_ERROR_OFFLINE:
		error = _("offline");
		break;
	case TP_CHANNEL_TEXT_SEND_ERROR_INVALID_CONTACT:
		error = _("invalid contact");
		break;
	case TP_CHANNEL_TEXT_SEND_ERROR_PERMISSION_DENIED:
		error = _("permission denied");
		break;
	case TP_CHANNEL_TEXT_SEND_ERROR_TOO_LONG:
		error = _("too long message");
		break;
	case TP_CHANNEL_TEXT_SEND_ERROR_NOT_IMPLEMENTED:
		error = _("not implemented");
		break;
	default:
		error = _("unknown");
		break;
	}

	str = g_strdup_printf (_("Error sending message '%s': %s"),
			       empathy_message_get_body (message),
			       error);
	empathy_chat_view_append_event (chat->view, str);
	g_free (str);
}

static void
chat_property_changed_cb (EmpathyTpChat *tp_chat,
			  const gchar   *name,
			  GValue        *value,
			  EmpathyChat   *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	if (!tp_strdiff (name, "subject")) {
		g_free (priv->subject);
		priv->subject = g_value_dup_string (value);
		g_object_notify (G_OBJECT (chat), "subject");

		if (EMP_STR_EMPTY (priv->subject)) {
			gtk_widget_hide (priv->hbox_topic);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->label_topic), priv->subject);
			gtk_widget_show (priv->hbox_topic);
		}
		if (priv->block_events_timeout_id == 0) {
			gchar *str;

			if (!EMP_STR_EMPTY (priv->subject)) {
				str = g_strdup_printf (_("Topic set to: %s"), priv->subject);
			} else {
				str = g_strdup (_("No topic defined"));
			}
			empathy_chat_view_append_event (EMPATHY_CHAT (chat)->view, str);
			g_free (str);
		}
	}
	else if (!tp_strdiff (name, "name")) {
		g_free (priv->name);
		priv->name = g_value_dup_string (value);
		g_object_notify (G_OBJECT (chat), "name");
	}
}

static gboolean
chat_get_is_command (const gchar *str)
{
	g_return_val_if_fail (str != NULL, FALSE);

	if (str[0] != '/') {
		return FALSE;
	}

	if (g_str_has_prefix (str, "/me")) {
		return TRUE;
	}
	else if (g_str_has_prefix (str, "/nick")) {
		return TRUE;
	}
	else if (g_str_has_prefix (str, "/topic")) {
		return TRUE;
	}

	return FALSE;
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
				   EmpathyChat    *chat)
{
	EmpathyChatPriv *priv;
	GtkTextIter     start, end;
	gchar          *str;
	gboolean        spell_checker = FALSE;

	priv = GET_PRIV (chat);

	if (gtk_text_buffer_get_char_count (buffer) == 0) {
		chat_composing_stop (chat);
	} else {
		chat_composing_start (chat);
	}

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_CHAT_SPELL_CHECKER_ENABLED,
			      &spell_checker);

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (!spell_checker) {
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_remove_tag_by_name (buffer, "misspelled", &start, &end);
		return;
	}

	if (!empathy_spell_supported ()) {
		return;
	}

	/* NOTE: this is really inefficient, we shouldn't have to
	   reiterate the whole buffer each time and check each work
	   every time. */
	while (TRUE) {
		gboolean correct = FALSE;

		/* if at start */
		if (gtk_text_iter_is_start (&start)) {
			end = start;

			if (!gtk_text_iter_forward_word_end (&end)) {
				/* no whole word yet */
				break;
			}
		} else {
			if (!gtk_text_iter_forward_word_end (&end)) {
				/* must be the end of the buffer */
				break;
			}

			start = end;
			gtk_text_iter_backward_word_start (&start);
		}

		str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

		/* spell check string */
		if (!chat_get_is_command (str)) {
			correct = empathy_spell_check (str);
		} else {
			correct = TRUE;
		}

		if (!correct) {
			gtk_text_buffer_apply_tag_by_name (buffer, "misspelled", &start, &end);
		} else {
			gtk_text_buffer_remove_tag_by_name (buffer, "misspelled", &start, &end);
		}

		g_free (str);

		/* set start iter to the end iters position */
		start = end;
	}
}

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
			       GdkEventKey *event,
			       EmpathyChat *chat)
{
	EmpathyChatPriv *priv;
	GtkAdjustment  *adj;
	gdouble         val;
	GtkWidget      *text_view_sw;

	priv = GET_PRIV (chat);

	/* Catch ctrl+up/down so we can traverse messages we sent */
	if ((event->state & GDK_CONTROL_MASK) && 
	    (event->keyval == GDK_Up || 
	     event->keyval == GDK_Down)) {
		GtkTextBuffer *buffer;
		const gchar   *str;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

		if (event->keyval == GDK_Up) {
			str = chat_sent_message_get_next (chat);
		} else {
			str = chat_sent_message_get_last (chat);
		}

		g_signal_handlers_block_by_func (buffer, 
						 chat_input_text_buffer_changed_cb,
						 chat);
		gtk_text_buffer_set_text (buffer, str ? str : "", -1);
		g_signal_handlers_unblock_by_func (buffer, 
						   chat_input_text_buffer_changed_cb,
						   chat);

		return TRUE;    
	}

	/* Catch enter but not ctrl/shift-enter */
	if (IS_ENTER (event->keyval) &&
	    !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))) {
		GtkTextView *view;

		/* This is to make sure that kinput2 gets the enter. And if
		 * it's handled there we shouldn't send on it. This is because
		 * kinput2 uses Enter to commit letters. See:
		 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=104299
		 */

		view = GTK_TEXT_VIEW (chat->input_text_view);
		if (gtk_im_context_filter_keypress (view->im_context, event)) {
			GTK_TEXT_VIEW (chat->input_text_view)->need_im_reset = TRUE;
			return TRUE;
		}

		chat_input_text_view_send (chat);
		return TRUE;
	}

	text_view_sw = gtk_widget_get_parent (GTK_WIDGET (chat->view));

	if (IS_ENTER (event->keyval) &&
	    (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))) {
		/* Newline for shift/control-enter. */
		return FALSE;
	}
	if (!(event->state & GDK_CONTROL_MASK) &&
	    event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (text_view_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);
		return TRUE;
	}
	if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
	    event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (text_view_sw));
		val = MIN (adj->value + adj->page_size, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, val);
		return TRUE;
	}
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

static gboolean
chat_text_view_focus_in_event_cb (GtkWidget  *widget,
				  GdkEvent   *event,
				  EmpathyChat *chat)
{
	gtk_widget_grab_focus (chat->input_text_view);

	return TRUE;
}

static gboolean
chat_input_set_size_request_idle (gpointer sw)
{
	gtk_widget_set_size_request (sw, -1, MAX_INPUT_HEIGHT);

	return FALSE;
}

static void
chat_input_size_request_cb (GtkWidget      *widget,
			    GtkRequisition *requisition,
			    EmpathyChat    *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	GtkWidget       *sw;

	sw = gtk_widget_get_parent (widget);
	if (requisition->height >= MAX_INPUT_HEIGHT && !priv->has_input_vscroll) {
		g_idle_add (chat_input_set_size_request_idle, sw);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_ALWAYS);
		priv->has_input_vscroll = TRUE;
	}

	if (requisition->height < MAX_INPUT_HEIGHT && priv->has_input_vscroll) {
		gtk_widget_set_size_request (sw, -1, -1);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_NEVER);
		priv->has_input_vscroll = FALSE;
	}
}

static void
chat_input_realize_cb (GtkWidget   *widget,
		       EmpathyChat *chat)
{
	DEBUG ("Setting focus to the input text view");
	gtk_widget_grab_focus (widget);
}

static void
chat_insert_smiley_activate_cb (EmpathySmileyManager *manager,
				EmpathySmiley        *smiley,
				gpointer              user_data)
{
	EmpathyChat   *chat = EMPATHY_CHAT (user_data);
	GtkTextBuffer *buffer;
	GtkTextIter    iter;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, smiley->str, -1);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, " ", -1);
}

typedef struct {
	EmpathyChat  *chat;
	gchar       *word;

	GtkTextIter  start;
	GtkTextIter  end;
} EmpathyChatSpell;

static EmpathyChatSpell *
chat_spell_new (EmpathyChat  *chat,
		const gchar *word,
		GtkTextIter  start,
		GtkTextIter  end)
{
	EmpathyChatSpell *chat_spell;

	chat_spell = g_slice_new0 (EmpathyChatSpell);

	chat_spell->chat = g_object_ref (chat);
	chat_spell->word = g_strdup (word);
	chat_spell->start = start;
	chat_spell->end = end;

	return chat_spell;
}

static void
chat_spell_free (EmpathyChatSpell *chat_spell)
{
	g_object_unref (chat_spell->chat);
	g_free (chat_spell->word);
	g_slice_free (EmpathyChatSpell, chat_spell);
}

static void
chat_text_check_word_spelling_cb (GtkMenuItem     *menuitem,
				  EmpathyChatSpell *chat_spell)
{
	empathy_spell_dialog_show (chat_spell->chat,
				  &chat_spell->start,
				  &chat_spell->end,
				  chat_spell->word);
}

static void
chat_text_send_cb (GtkMenuItem *menuitem,
		   EmpathyChat *chat)
{
	chat_input_text_view_send (chat);
}

static void
chat_input_populate_popup_cb (GtkTextView *view,
			      GtkMenu     *menu,
			      EmpathyChat *chat)
{
	EmpathyChatPriv      *priv;
	GtkTextBuffer        *buffer;
	GtkTextTagTable      *table;
	GtkTextTag           *tag;
	gint                  x, y;
	GtkTextIter           iter, start, end;
	GtkWidget            *item;
	gchar                *str = NULL;
	EmpathyChatSpell      *chat_spell;
	EmpathySmileyManager *smiley_manager;
	GtkWidget            *smiley_menu;
	GtkWidget            *image;

	priv = GET_PRIV (chat);
	buffer = gtk_text_view_get_buffer (view);

	/* Add the emoticon menu. */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Insert Smiley"));
	image = gtk_image_new_from_icon_name ("face-smile",
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	smiley_manager = empathy_smiley_manager_dup_singleton ();
	smiley_menu = empathy_smiley_menu_new (smiley_manager,
					       chat_insert_smiley_activate_cb,
					       chat);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), smiley_menu);
	g_object_unref (smiley_manager);

	/* Add the Send menu item. */
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	if (!EMP_STR_EMPTY (str)) {
		item = gtk_menu_item_new_with_mnemonic (_("_Send"));
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (chat_text_send_cb), chat);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}
	str = NULL;

	/* Add the spell check menu item. */
	table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (table, "misspelled");
	gtk_widget_get_pointer (GTK_WIDGET (view), &x, &y);
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
					       GTK_TEXT_WINDOW_WIDGET,
					       x, y,
					       &x, &y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, x, y);
	start = end = iter;
	if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
	    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {

		str = gtk_text_buffer_get_text (buffer,
						&start, &end, FALSE);
	}
	if (!EMP_STR_EMPTY (str)) {
		chat_spell = chat_spell_new (chat, str, start, end);
		g_object_set_data_full (G_OBJECT (menu),
					"chat_spell", chat_spell,
					(GDestroyNotify) chat_spell_free);

		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		item = gtk_image_menu_item_new_with_mnemonic (_("_Check Word Spelling..."));
		image = gtk_image_new_from_icon_name (GTK_STOCK_SPELL_CHECK,
						      GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_signal_connect (item,
				  "activate",
				  G_CALLBACK (chat_text_check_word_spelling_cb),
				  chat_spell);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}
}

static gboolean
chat_log_filter (EmpathyMessage *message,
		 gpointer user_data)
{
	EmpathyChat *chat = (EmpathyChat *) user_data;
	EmpathyChatPriv *priv = GET_PRIV (chat);
	const GList *pending;

	pending = empathy_tp_chat_get_pending_messages (priv->tp_chat);

	for (; pending; pending = g_list_next (pending)) {
		if (empathy_message_equal (message, pending->data)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
chat_add_logs (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	gboolean         is_chatroom;
	GList           *messages, *l;

	if (!priv->id) {
		return;
	}

	/* Turn off scrolling temporarily */
	empathy_chat_view_scroll (chat->view, FALSE);

	/* Add messages from last conversation */
	is_chatroom = priv->handle_type == TP_HANDLE_TYPE_ROOM;

	messages = empathy_log_manager_get_filtered_messages (priv->log_manager,
							      priv->account,
							      priv->id,
							      is_chatroom,
							      5,
							      chat_log_filter,
							      chat);

	for (l = messages; l; l = g_list_next (l)) {
		empathy_chat_view_append_message (chat->view, l->data);
		g_object_unref (l->data);
	}

	g_list_free (messages);

	/* Turn back on scrolling */
	empathy_chat_view_scroll (chat->view, TRUE);
}

static gint
chat_contacts_completion_func (const gchar *s1,
			       const gchar *s2,
			       gsize        n)
{
	gchar *tmp, *nick1, *nick2;
	gint   ret;

	if (s1 == s2) {
		return 0;
	}
	if (!s1 || !s2) {
		return s1 ? -1 : +1;
	}

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

static void
chat_members_changed_cb (EmpathyTpChat  *tp_chat,
			 EmpathyContact *contact,
			 EmpathyContact *actor,
			 guint           reason,
			 gchar          *message,
			 gboolean        is_member,
			 EmpathyChat    *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	if (priv->block_events_timeout_id == 0) {
		gchar *str;

		empathy_contact_run_until_ready (contact,
						 EMPATHY_CONTACT_READY_NAME,
						 NULL);

		if (is_member) {
			str = g_strdup_printf (_("%s has joined the room"),
					       empathy_contact_get_name (contact));
		} else {
			str = g_strdup_printf (_("%s has left the room"),
					       empathy_contact_get_name (contact));
		}
		empathy_chat_view_append_event (chat->view, str);
		g_free (str);
	}
}

static gboolean
chat_reset_size_request (gpointer widget)
{
	gtk_widget_set_size_request (widget, -1, -1);

	return FALSE;
}

static void
chat_set_show_contacts (EmpathyChat *chat, gboolean show)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	if (!priv->scrolled_window_contacts) {
		return;
	}

	if (show) {
		EmpathyContactListStore *store;
		gint                     min_width;

		/* We are adding the contact list to the chat, we don't want the
		 * chat view to become too small. If the chat view is already
		 * smaller than 250 make sure that size won't change. If the
		 * chat view is bigger the contact list will take some space on
		 * it but we make sure the chat view don't become smaller than
		 * 250. Relax the size request once the resize is done */
		min_width = MIN (priv->vbox_left->allocation.width, 250);
		gtk_widget_set_size_request (priv->vbox_left, min_width, -1);
		g_idle_add (chat_reset_size_request, priv->vbox_left);

		if (priv->contacts_width > 0) {
			gtk_paned_set_position (GTK_PANED (priv->hpaned),
						priv->contacts_width);
		}

		store = empathy_contact_list_store_new (EMPATHY_CONTACT_LIST (priv->tp_chat));
		priv->contact_list_view = GTK_WIDGET (empathy_contact_list_view_new (store,
			EMPATHY_CONTACT_LIST_FEATURE_NONE,
			EMPATHY_CONTACT_FEATURE_CHAT |
			EMPATHY_CONTACT_FEATURE_CALL |
			EMPATHY_CONTACT_FEATURE_LOG |
			EMPATHY_CONTACT_FEATURE_INFO));
		gtk_container_add (GTK_CONTAINER (priv->scrolled_window_contacts),
				   priv->contact_list_view);
		gtk_widget_show (priv->contact_list_view);
		gtk_widget_show (priv->scrolled_window_contacts);
		g_object_unref (store);
	} else {
		priv->contacts_width = gtk_paned_get_position (GTK_PANED (priv->hpaned));
		gtk_widget_hide (priv->scrolled_window_contacts);
		if (priv->contact_list_view) {
			gtk_widget_destroy (priv->contact_list_view);
			priv->contact_list_view = NULL;
		}
	}
}

static void
chat_remote_contact_changed_cb (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	if (priv->remote_contact) {
		g_object_unref (priv->remote_contact);
		priv->remote_contact = NULL;
	}

	priv->remote_contact = empathy_tp_chat_get_remote_contact (priv->tp_chat);
	if (priv->remote_contact) {
		g_object_ref (priv->remote_contact);
		priv->handle_type = TP_HANDLE_TYPE_CONTACT;
		g_free (priv->id);
		priv->id = g_strdup (empathy_contact_get_id (priv->remote_contact));
	}
	else if (priv->tp_chat) {
		TpChannel *channel;

		channel = empathy_tp_chat_get_channel (priv->tp_chat);
		g_object_get (channel, "handle-type", &priv->handle_type, NULL);
		g_free (priv->id);
		priv->id = g_strdup (empathy_tp_chat_get_id (priv->tp_chat));
	}

	chat_set_show_contacts (chat, priv->remote_contact == NULL);

	g_object_notify (G_OBJECT (chat), "remote-contact");
	g_object_notify (G_OBJECT (chat), "id");
}

static void
chat_destroy_cb (EmpathyTpChat *tp_chat,
		 EmpathyChat   *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	if (!priv->tp_chat) {
		return;
	}

	chat_composing_remove_timeout (chat);
	g_object_unref (priv->tp_chat);
	priv->tp_chat = NULL;
	g_object_notify (G_OBJECT (chat), "tp-chat");

	empathy_chat_view_append_event (chat->view, _("Disconnected"));
	gtk_widget_set_sensitive (chat->input_text_view, FALSE);
	chat_set_show_contacts (chat, FALSE);
}

static void
show_pending_messages (EmpathyChat *chat) {
	EmpathyChatPriv *priv = GET_PRIV (chat);
	const GList *messages, *l;

	if (chat->view == NULL || priv->tp_chat == NULL)
		return;

	messages = empathy_tp_chat_get_pending_messages (priv->tp_chat);

	for (l = messages; l != NULL ; l = g_list_next (l)) {
		EmpathyMessage *message = EMPATHY_MESSAGE (l->data);
		chat_message_received (chat, message);
	}
	empathy_tp_chat_acknowledge_messages (priv->tp_chat, messages);
}

static void
chat_create_ui (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	GladeXML        *glade;
 	GList           *list = NULL; 
	gchar           *filename;
	GtkTextBuffer   *buffer;

	filename = empathy_file_lookup ("empathy-chat.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"chat_widget",
					NULL,
					"chat_widget", &priv->widget,
					"hpaned", &priv->hpaned,
					"vbox_left", &priv->vbox_left,
					"scrolled_window_chat", &priv->scrolled_window_chat,
					"scrolled_window_input", &priv->scrolled_window_input,
					"hbox_topic", &priv->hbox_topic,
					"label_topic", &priv->label_topic,
					"scrolled_window_contacts", &priv->scrolled_window_contacts,
					NULL);
	g_free (filename);
	g_object_unref (glade);

	/* Add message view. */
	chat->view = empathy_theme_manager_create_view (empathy_theme_manager_get ());
	g_signal_connect (chat->view, "focus_in_event",
			  G_CALLBACK (chat_text_view_focus_in_event_cb),
			  chat);
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_chat),
			   GTK_WIDGET (chat->view));
	gtk_widget_show (GTK_WIDGET (chat->view));

	/* Add input GtkTextView */
	chat->input_text_view = g_object_new (GTK_TYPE_TEXT_VIEW,
					      "pixels-above-lines", 2,
					      "pixels-below-lines", 2,
					      "pixels-inside-wrap", 1,
					      "right-margin", 2,
					      "left-margin", 2,
					      "wrap-mode", GTK_WRAP_WORD_CHAR,
					      NULL);
	g_signal_connect (chat->input_text_view, "key-press-event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);
	g_signal_connect (chat->input_text_view, "size-request",
			  G_CALLBACK (chat_input_size_request_cb),
			  chat);
	g_signal_connect (chat->input_text_view, "realize",
			  G_CALLBACK (chat_input_realize_cb),
			  chat);
	g_signal_connect (chat->input_text_view, "populate-popup",
			  G_CALLBACK (chat_input_populate_popup_cb),
			  chat);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer, "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);
	gtk_text_buffer_create_tag (buffer, "misspelled",
				    "underline", PANGO_UNDERLINE_ERROR,
				    NULL);
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_input),
			   chat->input_text_view);
	gtk_widget_show (chat->input_text_view);

	/* Create contact list */
	chat_set_show_contacts (chat, priv->remote_contact == NULL);

	/* Initialy hide the topic, will be shown if not empty */
	gtk_widget_hide (priv->hbox_topic);

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

	/* Add the main widget in the chat widget */
	gtk_container_add (GTK_CONTAINER (chat), priv->widget);
}

static void
chat_size_request (GtkWidget      *widget,
		   GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
chat_size_allocate (GtkWidget     *widget,
		    GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation child_allocation;
  
  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width;
      child_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width;
      child_allocation.width = MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0);
      child_allocation.height = MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0);

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
chat_finalize (GObject *object)
{
	EmpathyChat     *chat;
	EmpathyChatPriv *priv;

	chat = EMPATHY_CHAT (object);
	priv = GET_PRIV (chat);

	DEBUG ("Finalized: %p", object);

	g_slist_foreach (priv->sent_messages, (GFunc) g_free, NULL);
	g_slist_free (priv->sent_messages);

	g_list_foreach (priv->compositors, (GFunc) g_object_unref, NULL);
	g_list_free (priv->compositors);

	chat_composing_remove_timeout (chat);

	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      chat_connection_changed_cb, object);

	g_object_unref (priv->account_manager);
	g_object_unref (priv->log_manager);

	if (priv->tp_chat) {
		g_signal_handler_disconnect (priv->tp_chat, priv->tp_chat_destroy_handler);
		empathy_tp_chat_close (priv->tp_chat);
		g_object_unref (priv->tp_chat);
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}
	if (priv->remote_contact) {
		g_object_unref (priv->remote_contact);
	}

	if (priv->block_events_timeout_id) {
		g_source_remove (priv->block_events_timeout_id);
	}

	g_free (priv->id);
	g_free (priv->name);
	g_free (priv->subject);

	G_OBJECT_CLASS (empathy_chat_parent_class)->finalize (object);
}

static void
chat_constructed (GObject *object)
{
	EmpathyChat *chat = EMPATHY_CHAT (object);

	chat_create_ui (chat);
	chat_add_logs (chat);
	show_pending_messages (chat);
}

static void
empathy_chat_class_init (EmpathyChatClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chat_finalize;
	object_class->get_property = chat_get_property;
	object_class->set_property = chat_set_property;
	object_class->constructed = chat_constructed;

	widget_class->size_request = chat_size_request;
	widget_class->size_allocate = chat_size_allocate;

	g_object_class_install_property (object_class,
					 PROP_TP_CHAT,
					 g_param_spec_object ("tp-chat",
							      "Empathy tp chat",
							      "The tp chat object",
							      EMPATHY_TYPE_TP_CHAT,
							      G_PARAM_CONSTRUCT |
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Account of the chat",
							      "The account of the chat",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Chat's id",
							      "The id of the chat",
							      NULL,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Chat's name",
							      "The name of the chat",
							      NULL,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_SUBJECT,
					 g_param_spec_string ("subject",
							      "Chat's subject",
							      "The subject or topic of the chat",
							      NULL,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_REMOTE_CONTACT,
					 g_param_spec_object ("remote-contact",
							      "The remote contact",
							      "The remote contact is any",
							      EMPATHY_TYPE_CONTACT,
							      G_PARAM_READABLE));

	signals[COMPOSING] =
		g_signal_new ("composing",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	signals[NEW_MESSAGE] =
		g_signal_new ("new-message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_MESSAGE);

	g_type_class_add_private (object_class, sizeof (EmpathyChatPriv));
}

static gboolean
chat_block_events_timeout_cb (gpointer data)
{
	EmpathyChatPriv *priv = GET_PRIV (data);

	priv->block_events_timeout_id = 0;

	return FALSE;
}

static void
empathy_chat_init (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chat,
		EMPATHY_TYPE_CHAT, EmpathyChatPriv);

	chat->priv = priv;
	priv->log_manager = empathy_log_manager_dup_singleton ();
	priv->contacts_width = -1;
	priv->sent_messages = NULL;
	priv->sent_messages_index = -1;
	priv->account_manager = empathy_account_manager_dup_singleton ();

	g_signal_connect (priv->account_manager,
			  "account-connection-changed",
			  G_CALLBACK (chat_connection_changed_cb),
			  chat);

	/* Block events for some time to avoid having "has come online" or
	 * "joined" messages. */
	priv->block_events_timeout_id =
		g_timeout_add_seconds (1, chat_block_events_timeout_cb, chat);

	/* Add nick name completion */
	priv->completion = g_completion_new ((GCompletionFunc) empathy_contact_get_name);
	g_completion_set_compare (priv->completion, chat_contacts_completion_func);
}

EmpathyChat *
empathy_chat_new (EmpathyTpChat *tp_chat)
{
	return g_object_new (EMPATHY_TYPE_CHAT, "tp-chat", tp_chat, NULL);
}

EmpathyTpChat *
empathy_chat_get_tp_chat (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	return priv->tp_chat;
}

void
empathy_chat_set_tp_chat (EmpathyChat   *chat,
			  EmpathyTpChat *tp_chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_if_fail (EMPATHY_IS_CHAT (chat));
	g_return_if_fail (EMPATHY_IS_TP_CHAT (tp_chat));
	g_return_if_fail (empathy_tp_chat_is_ready (tp_chat));

	if (priv->tp_chat) {
		return;
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	priv->tp_chat = g_object_ref (tp_chat);
	priv->account = g_object_ref (empathy_tp_chat_get_account (tp_chat));

	g_signal_connect (tp_chat, "message-received",
			  G_CALLBACK (chat_message_received_cb),
			  chat);
	g_signal_connect (tp_chat, "send-error",
			  G_CALLBACK (chat_send_error_cb),
			  chat);
	g_signal_connect (tp_chat, "chat-state-changed",
			  G_CALLBACK (chat_state_changed_cb),
			  chat);
	g_signal_connect (tp_chat, "property-changed",
			  G_CALLBACK (chat_property_changed_cb),
			  chat);
	g_signal_connect (tp_chat, "members-changed",
			  G_CALLBACK (chat_members_changed_cb),
			  chat);
	g_signal_connect_swapped (tp_chat, "notify::remote-contact",
				  G_CALLBACK (chat_remote_contact_changed_cb),
				  chat);
	priv->tp_chat_destroy_handler =
		g_signal_connect (tp_chat, "destroy",
			  G_CALLBACK (chat_destroy_cb),
			  chat);

	chat_remote_contact_changed_cb (chat);

	if (chat->input_text_view) {
		gtk_widget_set_sensitive (chat->input_text_view, TRUE);
		if (priv->block_events_timeout_id == 0) {
			empathy_chat_view_append_event (chat->view, _("Connected"));
		}
	}

	g_object_notify (G_OBJECT (chat), "tp-chat");
	g_object_notify (G_OBJECT (chat), "id");
	g_object_notify (G_OBJECT (chat), "account");

	/* This is a noop when tp-chat is set at object construction time and causes
	 * the pending messages to be show when it's set on the object after it has
	 * been created */
	show_pending_messages (chat);
}

McAccount *
empathy_chat_get_account (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	return priv->account;
}

const gchar *
empathy_chat_get_id (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	return priv->id;
}

const gchar *
empathy_chat_get_name (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	const gchar *ret;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	ret = priv->name;
	if (!ret && priv->remote_contact) {
		ret = empathy_contact_get_name (priv->remote_contact);
	}

	if (!ret)
		ret = priv->id;

	return ret ? ret : _("Conversation");
}

const gchar *
empathy_chat_get_subject (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	return priv->subject;
}

EmpathyContact *
empathy_chat_get_remote_contact (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	return priv->remote_contact;
}

guint
empathy_chat_get_members_count (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), 0);

	if (priv->tp_chat) {
		return empathy_tp_chat_get_members_count (priv->tp_chat);
	}

	return 0;
}

GtkWidget *
empathy_chat_get_contact_menu (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	GtkWidget       *menu = NULL;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	if (priv->remote_contact) {
		menu = empathy_contact_menu_new (priv->remote_contact,
						 EMPATHY_CONTACT_FEATURE_CALL |
						 EMPATHY_CONTACT_FEATURE_LOG |
						 EMPATHY_CONTACT_FEATURE_INFO);
	}
	else if (priv->contact_list_view) {
		EmpathyContactListView *view;

		view = EMPATHY_CONTACT_LIST_VIEW (priv->contact_list_view);
		menu = empathy_contact_list_view_get_contact_menu (view);
	}

	return menu;
}

void
empathy_chat_clear (EmpathyChat *chat)
{
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	empathy_chat_view_clear (chat->view);
}

void
empathy_chat_scroll_down (EmpathyChat *chat)
{
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	empathy_chat_view_scroll_down (chat->view);
}

void
empathy_chat_cut (EmpathyChat *chat)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_has_selection (buffer)) {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
	}
}

void
empathy_chat_copy (EmpathyChat *chat)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	if (empathy_chat_view_get_has_selection (chat->view)) {
		empathy_chat_view_copy_clipboard (chat->view);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_has_selection (buffer)) {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_copy_clipboard (buffer, clipboard);
	}
}

void
empathy_chat_paste (EmpathyChat *chat)
{
	GtkTextBuffer *buffer;
	GtkClipboard  *clipboard;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
}

void
empathy_chat_correct_word (EmpathyChat  *chat,
			  GtkTextIter *start,
			  GtkTextIter *end,
			  const gchar *new_word)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (chat != NULL);
	g_return_if_fail (new_word != NULL);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_delete (buffer, start, end);
	gtk_text_buffer_insert (buffer, start,
				new_word,
				-1);
}

gboolean
empathy_chat_is_room (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), FALSE);

	return (priv->handle_type == TP_HANDLE_TYPE_ROOM);
}

