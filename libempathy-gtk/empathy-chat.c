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
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-log-manager.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include "empathy-chat.h"
#include "empathy-chat-window.h"
#include "empathy-geometry.h"
#include "empathy-conf.h"
#include "empathy-preferences.h"
#include "empathy-spell.h"
#include "empathy-spell-dialog.h"
#include "empathy-ui-utils.h"
#include "empathy-gtk-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_CHAT, EmpathyChatPriv))

#define DEBUG_DOMAIN "Chat"

#define CHAT_DIR_CREATE_MODE  (S_IRUSR | S_IWUSR | S_IXUSR)
#define CHAT_FILE_CREATE_MODE (S_IRUSR | S_IWUSR)

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define MAX_INPUT_HEIGHT 150

#define COMPOSING_STOP_TIMEOUT 5

struct _EmpathyChatPriv {
	EmpathyLogManager     *log_manager;
	EmpathyTpChat         *tp_chat;
	EmpathyChatWindow     *window;
	McAccount             *account;
	MissionControl        *mc;
	guint                  composing_stop_timeout_id;
	gboolean               sensitive;
	gchar                 *id;
	GSList                *sent_messages;
	gint                   sent_messages_index;
	GList                 *compositors;
	guint                  scroll_idle_id;
	gboolean               first_tp_chat;
	time_t                 last_log_timestamp;
	gboolean               is_first_char;
	/* Used to automatically shrink a window that has temporarily
	 * grown due to long input. 
	 */
	gint                   padding_height;
	gint                   default_window_height;
	gint                   last_input_height;
	gboolean               vscroll_visible;
};

typedef struct {
	EmpathyChat  *chat;
	gchar       *word;

	GtkTextIter  start;
	GtkTextIter  end;
} EmpathyChatSpell;

static void             empathy_chat_class_init           (EmpathyChatClass       *klass);
static void             empathy_chat_init                 (EmpathyChat            *chat);
static void             chat_finalize                     (GObject                *object);
static void             chat_destroy_cb                   (EmpathyTpChat          *tp_chat,
							   EmpathyChat            *chat);
static void             chat_send                         (EmpathyChat            *chat,
							   const gchar            *msg);
static void             chat_input_text_view_send         (EmpathyChat            *chat);
static void             chat_message_received_cb          (EmpathyTpChat          *tp_chat,
							   EmpathyMessage         *message,
							   EmpathyChat            *chat);
static void             chat_send_error_cb                (EmpathyTpChat          *tp_chat,
							   EmpathyMessage         *message,
							   TpChannelTextSendError  error_code,
							   EmpathyChat            *chat);
static void             chat_sent_message_add             (EmpathyChat            *chat,
							   const gchar            *str);
static const gchar *    chat_sent_message_get_next        (EmpathyChat            *chat);
static const gchar *    chat_sent_message_get_last        (EmpathyChat            *chat);
static gboolean         chat_input_key_press_event_cb     (GtkWidget              *widget,
							   GdkEventKey            *event,
							   EmpathyChat            *chat);
static void             chat_input_text_buffer_changed_cb (GtkTextBuffer          *buffer,
							   EmpathyChat            *chat);
static gboolean         chat_text_view_focus_in_event_cb  (GtkWidget              *widget,
							   GdkEvent               *event,
							   EmpathyChat            *chat);
static void             chat_text_view_scroll_hide_cb     (GtkWidget              *widget,
							   EmpathyChat            *chat);
static void             chat_text_view_size_allocate_cb   (GtkWidget              *widget,
							   GtkAllocation          *allocation,
							   EmpathyChat            *chat);
static void             chat_text_view_realize_cb         (GtkWidget              *widget,
							   EmpathyChat            *chat);
static void             chat_text_populate_popup_cb       (GtkTextView            *view,
							   GtkMenu                *menu,
							   EmpathyChat            *chat);
static void             chat_text_check_word_spelling_cb  (GtkMenuItem            *menuitem,
							   EmpathyChatSpell       *chat_spell);
static EmpathyChatSpell *chat_spell_new                   (EmpathyChat            *chat,
							   const gchar            *word,
							   GtkTextIter             start,
							   GtkTextIter             end);
static void             chat_spell_free                   (EmpathyChatSpell       *chat_spell);
static void             chat_composing_start              (EmpathyChat            *chat);
static void             chat_composing_stop               (EmpathyChat            *chat);
static void             chat_composing_remove_timeout     (EmpathyChat            *chat);
static gboolean         chat_composing_stop_timeout_cb    (EmpathyChat            *chat);
static void             chat_state_changed_cb             (EmpathyTpChat          *tp_chat,
							   EmpathyContact         *contact,
							   TpChannelChatState      state,
							   EmpathyChat            *chat);
static void             chat_add_logs                     (EmpathyChat            *chat);
static gboolean         chat_scroll_down_idle_func        (EmpathyChat            *chat);

enum {
	COMPOSING,
	NEW_MESSAGE,
	NAME_CHANGED,
	STATUS_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_TP_CHAT
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EmpathyChat, empathy_chat, G_TYPE_OBJECT);

static void
chat_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	EmpathyChatPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TP_CHAT:
		g_value_set_object (value, priv->tp_chat);
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
		empathy_chat_set_tp_chat (chat,
					  EMPATHY_TP_CHAT (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
chat_status_changed_cb (MissionControl           *mc,
			TpConnectionStatus        status,
			McPresence                presence,
			TpConnectionStatusReason  reason,
			const gchar              *unique_name,
			EmpathyChat              *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	McAccount       *account;

	account = mc_account_lookup (unique_name);

	if (status == TP_CONNECTION_STATUS_CONNECTED && !priv->tp_chat &&
	    empathy_account_equal (account, priv->account)) {
		TpHandleType handle_type;

		empathy_debug (DEBUG_DOMAIN,
			       "Account reconnected, request a new Text channel");

		if (empathy_chat_is_group_chat (chat)) {
			handle_type = TP_HANDLE_TYPE_ROOM;
		} else {
			handle_type = TP_HANDLE_TYPE_CONTACT;
		}

		mission_control_request_channel_with_string_handle (mc,
								    priv->account,
								    TP_IFACE_CHANNEL_TYPE_TEXT,
								    priv->id,
								    handle_type,
								    NULL, NULL);
	}

	g_object_unref (account);
}

static void
empathy_chat_class_init (EmpathyChatClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chat_finalize;
	object_class->get_property = chat_get_property;
	object_class->set_property = chat_set_property;

	g_object_class_install_property (object_class,
					 PROP_TP_CHAT,
					 g_param_spec_object ("tp-chat",
							      "Empathy tp chat",
							      "The tp chat object",
							      EMPATHY_TYPE_TP_CHAT,
							      G_PARAM_CONSTRUCT |
							      G_PARAM_READWRITE));

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
			      _empathy_gtk_marshal_VOID__OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_MESSAGE, G_TYPE_BOOLEAN);

	signals[NAME_CHANGED] =
		g_signal_new ("name-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyChatPriv));
}

static void
empathy_chat_init (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);
	GtkTextBuffer  *buffer;

	chat->view = empathy_chat_view_new ();
	chat->input_text_view = gtk_text_view_new ();

	priv->is_first_char = TRUE;

	g_object_set (chat->input_text_view,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      "pixels-inside-wrap", 1,
		      "right-margin", 2,
		      "left-margin", 2,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      NULL);

	priv->log_manager = empathy_log_manager_new ();
	priv->default_window_height = -1;
	priv->vscroll_visible = FALSE;
	priv->sensitive = TRUE;
	priv->sent_messages = NULL;
	priv->sent_messages_index = -1;
	priv->first_tp_chat = TRUE;
	priv->mc = empathy_mission_control_new ();

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc), "AccountStatusChanged",
				     G_CALLBACK (chat_status_changed_cb),
				     chat, NULL);

	g_signal_connect (chat->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);
	g_signal_connect (chat->view,
			  "focus_in_event",
			  G_CALLBACK (chat_text_view_focus_in_event_cb),
			  chat);

	g_signal_connect (chat->input_text_view,
			  "size_allocate",
			  G_CALLBACK (chat_text_view_size_allocate_cb),
			  chat);

	g_signal_connect (chat->input_text_view,
			  "realize",
			  G_CALLBACK (chat_text_view_realize_cb),
			  chat);

	g_signal_connect (GTK_TEXT_VIEW (chat->input_text_view),
			  "populate_popup",
			  G_CALLBACK (chat_text_populate_popup_cb),
			  chat);

	/* create misspelt words identification tag */
	gtk_text_buffer_create_tag (buffer,
				    "misspelled",
				    "underline", PANGO_UNDERLINE_ERROR,
				    NULL);
}

static void
chat_finalize (GObject *object)
{
	EmpathyChat     *chat;
	EmpathyChatPriv *priv;

	chat = EMPATHY_CHAT (object);
	priv = GET_PRIV (chat);

	empathy_debug (DEBUG_DOMAIN, "Finalized: %p", object);

	g_slist_foreach (priv->sent_messages, (GFunc) g_free, NULL);
	g_slist_free (priv->sent_messages);

	g_list_foreach (priv->compositors, (GFunc) g_object_unref, NULL);
	g_list_free (priv->compositors);

	chat_composing_remove_timeout (chat);
	g_object_unref (priv->log_manager);

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc), "AccountStatusChanged",
					G_CALLBACK (chat_status_changed_cb),
					chat);
	g_object_unref (priv->mc);


	if (priv->tp_chat) {
		g_object_unref (priv->tp_chat);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->scroll_idle_id) {
		g_source_remove (priv->scroll_idle_id);
	}

	g_free (priv->id);

	G_OBJECT_CLASS (empathy_chat_parent_class)->finalize (object);
}

static void
chat_destroy_cb (EmpathyTpChat *tp_chat,
		 EmpathyChat    *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->tp_chat) {
		g_object_unref (priv->tp_chat);
		priv->tp_chat = NULL;
	}
	priv->sensitive = FALSE;

	empathy_chat_view_append_event (chat->view, _("Disconnected"));
	gtk_widget_set_sensitive (chat->input_text_view, FALSE);

	if (EMPATHY_CHAT_GET_CLASS (chat)->set_tp_chat) {
		EMPATHY_CHAT_GET_CLASS (chat)->set_tp_chat (chat, NULL);
	}
}

static void
chat_send (EmpathyChat  *chat,
	   const gchar *msg)
{
	EmpathyChatPriv *priv;
	EmpathyMessage  *message;

	priv = GET_PRIV (chat);

	if (G_STR_EMPTY (msg)) {
		return;
	}

	chat_sent_message_add (chat, msg);

	if (g_str_has_prefix (msg, "/clear")) {
		empathy_chat_view_clear (chat->view);
		return;
	}

	/* FIXME: add here something to let group/privrate chat handle
	 *        some special messages */

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

	priv->is_first_char = TRUE;
}

static void
chat_message_received_cb (EmpathyTpChat  *tp_chat,
			  EmpathyMessage *message,
			  EmpathyChat    *chat)
{
	EmpathyChatPriv *priv;
	EmpathyContact  *sender;
	time_t           timestamp;

	priv = GET_PRIV (chat);

	timestamp = empathy_message_get_timestamp (message);
	if (timestamp <= priv->last_log_timestamp) {
		/* Do not take care of messages anterior of the last
		 * logged message. Some Jabber chatroom sends messages
		 * received before we joined the room, this avoid
		 * displaying those messages if we already logged them
		 * last time we joined that room. */
		empathy_debug (DEBUG_DOMAIN, "Skipping message because it is "
			       "anterior of last logged message.");
		return;
	}

	sender = empathy_message_get_sender (message);
	empathy_debug (DEBUG_DOMAIN, "Appending message ('%s')",
		      empathy_contact_get_name (sender));

	empathy_log_manager_add_message (priv->log_manager,
					 empathy_chat_get_id (chat),
					 empathy_chat_is_group_chat (chat),
					 message);

	empathy_chat_view_append_message (chat->view, message);

	if (empathy_chat_should_play_sound (chat)) {
		// FIXME: empathy_sound_play (EMPATHY_SOUND_CHAT);
	}

	/* We received a message so the contact is no more composing */
	chat_state_changed_cb (tp_chat, sender,
			       TP_CHANNEL_CHAT_STATE_ACTIVE,
			       chat);

	g_signal_emit (chat, signals[NEW_MESSAGE], 0, message, FALSE);
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
		empathy_debug (DEBUG_DOMAIN, 
			      "No sent messages, next message is NULL");
		return NULL;
	}

	max = g_slist_length (priv->sent_messages) - 1;

	if (priv->sent_messages_index < max) {
		priv->sent_messages_index++;
	}
	
	empathy_debug (DEBUG_DOMAIN, 
		      "Returning next message index:%d",
		      priv->sent_messages_index);

	return g_slist_nth_data (priv->sent_messages, priv->sent_messages_index);
}

static const gchar *
chat_sent_message_get_last (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	priv = GET_PRIV (chat);
	
	if (!priv->sent_messages) {
		empathy_debug (DEBUG_DOMAIN, 
			      "No sent messages, last message is NULL");
		return NULL;
	}

	if (priv->sent_messages_index >= 0) {
		priv->sent_messages_index--;
	}

	empathy_debug (DEBUG_DOMAIN, 
		      "Returning last message index:%d",
		      priv->sent_messages_index);

	return g_slist_nth_data (priv->sent_messages, priv->sent_messages_index);
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
	else if (!(event->state & GDK_CONTROL_MASK) &&
		 event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (text_view_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);

		return TRUE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (text_view_sw));
		val = MIN (adj->value + adj->page_size, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, val);

		return TRUE;
	}

	if (EMPATHY_CHAT_GET_CLASS (chat)->key_press_event) {
		return EMPATHY_CHAT_GET_CLASS (chat)->key_press_event (chat, event);
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

	if (priv->is_first_char) {
		GtkRequisition  req;
		gint            window_height;
		GtkWidget      *dialog;
		GtkAllocation  *allocation;

		/* Save the window's size */
		dialog = empathy_chat_window_get_dialog (priv->window);
		gtk_window_get_size (GTK_WINDOW (dialog),
				     NULL, &window_height);

		gtk_widget_size_request (chat->input_text_view, &req);

		allocation = &GTK_WIDGET (chat->view)->allocation;

		priv->default_window_height = window_height;
		priv->last_input_height = req.height;
		priv->padding_height = window_height - req.height - allocation->height;

		priv->is_first_char = FALSE;
	}

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
		if (!empathy_chat_get_is_command (str)) {
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

typedef struct {
	GtkWidget *window;
	gint       width;
	gint       height;
} ChangeSizeData;

static gboolean
chat_change_size_in_idle_cb (ChangeSizeData *data)
{
	gtk_window_resize (GTK_WINDOW (data->window),
			   data->width, data->height);

	return FALSE;
}

static void
chat_text_view_scroll_hide_cb (GtkWidget  *widget,
			       EmpathyChat *chat)
{
	EmpathyChatPriv *priv;
	GtkWidget      *sw;

	priv = GET_PRIV (chat);

	priv->vscroll_visible = FALSE;
	g_signal_handlers_disconnect_by_func (widget, chat_text_view_scroll_hide_cb, chat);

	sw = gtk_widget_get_parent (chat->input_text_view);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	g_object_set (sw, "height-request", -1, NULL);
}

static void
chat_text_view_size_allocate_cb (GtkWidget     *widget,
				 GtkAllocation *allocation,
				 EmpathyChat    *chat)
{
	EmpathyChatPriv *priv;
	gint            width;
	GtkWidget      *dialog;
	ChangeSizeData *data;
	gint            window_height;
	gint            new_height;
	GtkAllocation  *view_allocation;
	gint            current_height;
	gint            diff;
	GtkWidget      *sw;

	priv = GET_PRIV (chat);

	if (priv->default_window_height <= 0) {
		return;
	}

	sw = gtk_widget_get_parent (widget);
	if (sw->allocation.height >= MAX_INPUT_HEIGHT && !priv->vscroll_visible) {
		GtkWidget *vscroll;

		priv->vscroll_visible = TRUE;
		gtk_widget_set_size_request (sw, sw->allocation.width, MAX_INPUT_HEIGHT);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);
		vscroll = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (sw));
		g_signal_connect (vscroll, "hide",
				  G_CALLBACK (chat_text_view_scroll_hide_cb),
				  chat);
	}

	if (priv->last_input_height <= allocation->height) {
		priv->last_input_height = allocation->height;
		return;
	}

	diff = priv->last_input_height - allocation->height;
	priv->last_input_height = allocation->height;

	view_allocation = &GTK_WIDGET (chat->view)->allocation;

	dialog = empathy_chat_window_get_dialog (priv->window);
	gtk_window_get_size (GTK_WINDOW (dialog), NULL, &current_height);

	new_height = view_allocation->height + priv->padding_height + allocation->height - diff;

	if (new_height <= priv->default_window_height) {
		window_height = priv->default_window_height;
	} else {
		window_height = new_height;
	}

	if (current_height <= window_height) {
		return;
	}

	/* Restore the window's size */
	gtk_window_get_size (GTK_WINDOW (dialog), &width, NULL);

	data = g_new0 (ChangeSizeData, 1);
	data->window = dialog;
	data->width  = width;
	data->height = window_height;

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 (GSourceFunc) chat_change_size_in_idle_cb,
			 data, g_free);
}

static void
chat_text_view_realize_cb (GtkWidget  *widget,
			   EmpathyChat *chat)
{
	empathy_debug (DEBUG_DOMAIN, "Setting focus to the input text view");
	gtk_widget_grab_focus (widget);
}

static void
chat_insert_smiley_activate_cb (GtkWidget   *menuitem,
				EmpathyChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	const gchar   *smiley;

	smiley = g_object_get_data (G_OBJECT (menuitem), "smiley_text");

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, smiley, -1);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, " ", -1);
}

static void
chat_text_populate_popup_cb (GtkTextView *view,
			     GtkMenu     *menu,
			     EmpathyChat  *chat)
{
	EmpathyChatPriv  *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;
	gint             x, y;
	GtkTextIter      iter, start, end;
	GtkWidget       *item;
	gchar           *str = NULL;
	EmpathyChatSpell *chat_spell;
	GtkWidget       *smiley_menu;

	priv = GET_PRIV (chat);

	/* Add the emoticon menu. */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("Insert Smiley"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	smiley_menu = empathy_chat_view_get_smiley_menu (
		G_CALLBACK (chat_insert_smiley_activate_cb),
		chat);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), smiley_menu);

	/* Add the spell check menu item. */
	buffer = gtk_text_view_get_buffer (view);
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

	if (G_STR_EMPTY (str)) {
		return;
	}

	chat_spell = chat_spell_new (chat, str, start, end);

	g_object_set_data_full (G_OBJECT (menu),
				"chat_spell", chat_spell,
				(GDestroyNotify) chat_spell_free);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Check Word Spelling..."));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_text_check_word_spelling_cb),
			  chat_spell);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
chat_text_check_word_spelling_cb (GtkMenuItem     *menuitem,
				  EmpathyChatSpell *chat_spell)
{
	empathy_spell_dialog_show (chat_spell->chat,
				  chat_spell->start,
				  chat_spell->end,
				  chat_spell->word);
}

static EmpathyChatSpell *
chat_spell_new (EmpathyChat  *chat,
		const gchar *word,
		GtkTextIter  start,
		GtkTextIter  end)
{
	EmpathyChatSpell *chat_spell;

	chat_spell = g_new0 (EmpathyChatSpell, 1);

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
	g_free (chat_spell);
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
		if (empathy_contact_equal (contact, l->data)) {
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

	empathy_debug (DEBUG_DOMAIN, "Was composing: %s now composing: %s",
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
chat_add_logs (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;
	GList          *messages, *l;
	guint           num_messages;
	guint           i;

	priv = GET_PRIV (chat);

	/* Turn off scrolling temporarily */
	empathy_chat_view_scroll (chat->view, FALSE);

	/* Add messages from last conversation */
	messages = empathy_log_manager_get_last_messages (priv->log_manager,
							  priv->account,
							  empathy_chat_get_id (chat),
							  empathy_chat_is_group_chat (chat));
	num_messages  = g_list_length (messages);

	for (l = messages, i = 0; l; l = l->next, i++) {
		EmpathyMessage *message;

		message = l->data;

		/* Only add 10 last messages */
		if (num_messages - i > 10) {
			g_object_unref (message);
			continue;
		}

		priv->last_log_timestamp = empathy_message_get_timestamp (message);
		empathy_chat_view_append_message (chat->view, message);

		g_object_unref (message);
	}
	g_list_free (messages);

	/* Turn back on scrolling */
	empathy_chat_view_scroll (chat->view, TRUE);

	/* Scroll to the most recent messages, we reference the chat
	 * for the duration of the scroll func.
	 */
	priv->scroll_idle_id = g_idle_add ((GSourceFunc) chat_scroll_down_idle_func, 
					   g_object_ref (chat));
}

/* Scroll down after the back-log has been received. */
static gboolean
chat_scroll_down_idle_func (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	empathy_chat_scroll_down (chat);
	g_object_unref (chat);

	priv->scroll_idle_id = 0;

	return FALSE;
}

gboolean
empathy_chat_get_is_command (const gchar *str)
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

void
empathy_chat_correct_word (EmpathyChat  *chat,
			  GtkTextIter  start,
			  GtkTextIter  end,
			  const gchar *new_word)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (chat != NULL);
	g_return_if_fail (new_word != NULL);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_insert (buffer, &start,
				new_word,
				-1);
}

const gchar *
empathy_chat_get_name (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	if (EMPATHY_CHAT_GET_CLASS (chat)->get_name) {
		return EMPATHY_CHAT_GET_CLASS (chat)->get_name (chat);
	}

	return NULL;
}

gchar *
empathy_chat_get_tooltip (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	if (EMPATHY_CHAT_GET_CLASS (chat)->get_tooltip) {
		return EMPATHY_CHAT_GET_CLASS (chat)->get_tooltip (chat);
	}

	return NULL;
}

const gchar *
empathy_chat_get_status_icon_name (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	if (EMPATHY_CHAT_GET_CLASS (chat)->get_status_icon_name) {
		return EMPATHY_CHAT_GET_CLASS (chat)->get_status_icon_name (chat);
	}

	return NULL;
}

GtkWidget *
empathy_chat_get_widget (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), NULL);

	if (EMPATHY_CHAT_GET_CLASS (chat)->get_widget) {
		return EMPATHY_CHAT_GET_CLASS (chat)->get_widget (chat);
	}

	return NULL;
}

gboolean
empathy_chat_is_group_chat (EmpathyChat *chat)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), FALSE);

	if (EMPATHY_CHAT_GET_CLASS (chat)->is_group_chat) {
		return EMPATHY_CHAT_GET_CLASS (chat)->is_group_chat (chat);
	}

	return FALSE;
}

gboolean 
empathy_chat_is_connected (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), FALSE);

	priv = GET_PRIV (chat);

	return (priv->tp_chat != NULL);
}

void
empathy_chat_save_geometry (EmpathyChat *chat,
			   gint        x,
			   gint        y,
			   gint        w,
			   gint        h)
{
	empathy_geometry_save (empathy_chat_get_id (chat), x, y, w, h);
}

void
empathy_chat_load_geometry (EmpathyChat *chat,
			   gint       *x,
			   gint       *y,
			   gint       *w,
			   gint       *h)
{
	empathy_geometry_load (empathy_chat_get_id (chat), x, y, w, h);
}

void
empathy_chat_set_tp_chat (EmpathyChat   *chat,
			  EmpathyTpChat *tp_chat)
{
	EmpathyChatPriv *priv;
	GList           *messages, *l;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));
	g_return_if_fail (EMPATHY_IS_TP_CHAT (tp_chat));

	priv = GET_PRIV (chat);

	if (tp_chat == priv->tp_chat) {
		return;
	}

	if (priv->tp_chat) {
		g_signal_handlers_disconnect_by_func (priv->tp_chat,
						      chat_message_received_cb,
						      chat);
		g_signal_handlers_disconnect_by_func (priv->tp_chat,
						      chat_send_error_cb,
						      chat);
		g_signal_handlers_disconnect_by_func (priv->tp_chat,
						      chat_destroy_cb,
						      chat);
		g_object_unref (priv->tp_chat);
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}

	g_free (priv->id);
	priv->tp_chat = g_object_ref (tp_chat);
	priv->id = g_strdup (empathy_tp_chat_get_id (tp_chat));
	priv->account = g_object_ref (empathy_tp_chat_get_account (tp_chat));

	if (priv->first_tp_chat) {
		chat_add_logs (chat);
		priv->first_tp_chat = FALSE;
	}

	g_signal_connect (tp_chat, "message-received",
			  G_CALLBACK (chat_message_received_cb),
			  chat);
	g_signal_connect (tp_chat, "send-error",
			  G_CALLBACK (chat_send_error_cb),
			  chat);
	g_signal_connect (tp_chat, "chat-state-changed",
			  G_CALLBACK (chat_state_changed_cb),
			  chat);
	g_signal_connect (tp_chat, "destroy",
			  G_CALLBACK (chat_destroy_cb),
			  chat);

	/* Get pending messages */
	empathy_tp_chat_set_acknowledge (tp_chat, TRUE);
	messages = empathy_tp_chat_get_pendings (tp_chat);
	for (l = messages; l; l = l->next) {
		chat_message_received_cb (tp_chat, l->data, chat);
		g_object_unref (l->data);
	}
	g_list_free (messages);

	if (!priv->sensitive) {
		gtk_widget_set_sensitive (chat->input_text_view, TRUE);
		empathy_chat_view_append_event (chat->view, _("Connected"));
		priv->sensitive = TRUE;
	}

	if (EMPATHY_CHAT_GET_CLASS (chat)->set_tp_chat) {
		EMPATHY_CHAT_GET_CLASS (chat)->set_tp_chat (chat, tp_chat);
	}

	g_object_notify (G_OBJECT (chat), "tp-chat");
}

const gchar *
empathy_chat_get_id (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->id;
}

McAccount *
empathy_chat_get_account (EmpathyChat *chat)
{
	EmpathyChatPriv *priv = GET_PRIV (chat);

	return priv->account;
}

void
empathy_chat_clear (EmpathyChat *chat)
{
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	empathy_chat_view_clear (chat->view);
}

void
empathy_chat_set_window (EmpathyChat       *chat,
			EmpathyChatWindow *window)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);
	priv->window = window;
}

EmpathyChatWindow *
empathy_chat_get_window (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->window;
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
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
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

	if (empathy_chat_view_get_selection_bounds (chat->view, NULL, NULL)) {
		empathy_chat_view_copy_clipboard (chat->view);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
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
empathy_chat_present (EmpathyChat *chat)
{
	EmpathyChatPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	priv = GET_PRIV (chat);

	if (priv->window == NULL) {
		EmpathyChatWindow *window;

		window = empathy_chat_window_get_default ();
		if (!window) {
			window = empathy_chat_window_new ();
		}

		empathy_chat_window_add_chat (window, chat);
	}

	empathy_chat_window_switch_to_chat (priv->window, chat);
	empathy_window_present (
		GTK_WINDOW (empathy_chat_window_get_dialog (priv->window)),
		TRUE);

 	gtk_widget_grab_focus (chat->input_text_view); 
}

gboolean
empathy_chat_should_play_sound (EmpathyChat *chat)
{
	EmpathyChatWindow *window;
	gboolean          play = TRUE;

	g_return_val_if_fail (EMPATHY_IS_CHAT (chat), FALSE);

	window = empathy_chat_get_window (chat);
	if (!window) {
		return TRUE;
	}

	play = !empathy_chat_window_has_focus (window);

	return play;
}

gboolean
empathy_chat_should_highlight_nick (EmpathyMessage *message)
{
	EmpathyContact *contact;
	const gchar   *msg, *to;
	gchar         *cf_msg, *cf_to;
	gchar         *ch;
	gboolean       ret_val;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), FALSE);

	empathy_debug (DEBUG_DOMAIN, "Highlighting nickname");

	ret_val = FALSE;

	msg = empathy_message_get_body (message);
	if (!msg) {
		return FALSE;
	}

	contact = empathy_message_get_receiver (message);
	if (!contact || !empathy_contact_is_user (contact)) {
		return FALSE;
	}

	to = empathy_contact_get_name (contact);
	if (!to) {
		return FALSE;
	}

	cf_msg = g_utf8_casefold (msg, -1);
	cf_to = g_utf8_casefold (to, -1);

	ch = strstr (cf_msg, cf_to);
	if (ch == NULL) {
		goto finished;
	}

	if (ch != cf_msg) {
		/* Not first in the message */
		if ((*(ch - 1) != ' ') &&
		    (*(ch - 1) != ',') &&
		    (*(ch - 1) != '.')) {
			goto finished;
		}
	}

	ch = ch + strlen (cf_to);
	if (ch >= cf_msg + strlen (cf_msg)) {
		ret_val = TRUE;
		goto finished;
	}

	if ((*ch == ' ') ||
	    (*ch == ',') ||
	    (*ch == '.') ||
	    (*ch == ':')) {
		ret_val = TRUE;
		goto finished;
	}

finished:
	g_free (cf_msg);
	g_free (cf_to);

	return ret_val;
}

