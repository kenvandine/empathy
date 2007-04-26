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
#include <glade/glade.h>

#include <libempathy/empathy-session.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/gossip-conf.h>

#include "gossip-chat.h"
#include "gossip-chat-window.h"
//#include "gossip-geometry.h"
#include "gossip-preferences.h"
#include "gossip-spell.h"
//#include "gossip-spell-dialog.h"
#include "gossip-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT, GossipChatPriv))

#define DEBUG_DOMAIN "Chat"

#define CHAT_DIR_CREATE_MODE  (S_IRUSR | S_IWUSR | S_IXUSR)
#define CHAT_FILE_CREATE_MODE (S_IRUSR | S_IWUSR)

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define MAX_INPUT_HEIGHT 150

#define COMPOSING_STOP_TIMEOUT 5

struct _GossipChatPriv {
	EmpathyTpChat    *tp_chat;
	GossipChatWindow *window;

	GtkTooltips      *tooltips;
	guint             composing_stop_timeout_id;
	gboolean          sensitive;
	/* Used to automatically shrink a window that has temporarily
	 * grown due to long input. 
	 */
	gint              padding_height;
	gint              default_window_height;
	gint              last_input_height;
	gboolean          vscroll_visible;
};

typedef struct {
	GossipChat  *chat;
	gchar       *word;

	GtkTextIter  start;
	GtkTextIter  end;
} GossipChatSpell;

static void             gossip_chat_class_init            (GossipChatClass *klass);
static void             gossip_chat_init                  (GossipChat      *chat);
static void             chat_finalize                     (GObject         *object);
static void             chat_destroy_cb                   (EmpathyTpChat   *tp_chat,
							   GossipChat      *chat);
static void             chat_send                         (GossipChat      *chat,
							   const gchar     *msg);
static void             chat_input_text_view_send         (GossipChat      *chat);
static void             chat_message_received_cb          (EmpathyTpChat   *tp_chat,
							   GossipMessage   *message,
							   GossipChat      *chat);
static gboolean         chat_input_key_press_event_cb     (GtkWidget       *widget,
							   GdkEventKey     *event,
							   GossipChat      *chat);
static void             chat_input_text_buffer_changed_cb (GtkTextBuffer   *buffer,
							   GossipChat      *chat);
static gboolean         chat_text_view_focus_in_event_cb  (GtkWidget       *widget,
							   GdkEvent        *event,
							   GossipChat      *chat);
static void             chat_text_view_scroll_hide_cb     (GtkWidget       *widget,
							   GossipChat      *chat);
static void             chat_text_view_size_allocate_cb   (GtkWidget       *widget,
							   GtkAllocation   *allocation,
							   GossipChat      *chat);
static void             chat_text_view_realize_cb         (GtkWidget       *widget,
							   GossipChat      *chat);
static void             chat_text_populate_popup_cb       (GtkTextView     *view,
							   GtkMenu         *menu,
							   GossipChat      *chat);
static void             chat_text_check_word_spelling_cb  (GtkMenuItem     *menuitem,
							   GossipChatSpell *chat_spell);
static GossipChatSpell *chat_spell_new                    (GossipChat      *chat,
							   const gchar     *word,
							   GtkTextIter      start,
							   GtkTextIter      end);
static void             chat_spell_free                   (GossipChatSpell *chat_spell);
static void             chat_composing_start              (GossipChat      *chat);
static void             chat_composing_stop               (GossipChat      *chat);
static void             chat_composing_remove_timeout     (GossipChat      *chat);
static gboolean         chat_composing_stop_timeout_cb    (GossipChat      *chat);

enum {
	COMPOSING,
	NEW_MESSAGE,
	NAME_CHANGED,
	STATUS_CHANGED,
	LAST_SIGNAL
};

static guint chat_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GossipChat, gossip_chat, G_TYPE_OBJECT);

static void
gossip_chat_class_init (GossipChatClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chat_finalize;

	chat_signals[COMPOSING] =
		g_signal_new ("composing",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	chat_signals[NEW_MESSAGE] =
		g_signal_new ("new-message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_MESSAGE);

	chat_signals[NAME_CHANGED] =
		g_signal_new ("name-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	chat_signals[STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (GossipChatPriv));
}

static void
gossip_chat_init (GossipChat *chat)
{
	GossipChatPriv *priv;
	GtkTextBuffer  *buffer;

	chat->view = gossip_chat_view_new ();
	chat->input_text_view = gtk_text_view_new ();

	chat->is_first_char = TRUE;

	g_object_set (chat->input_text_view,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      "pixels-inside-wrap", 1,
		      "right-margin", 2,
		      "left-margin", 2,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      NULL);

	priv = GET_PRIV (chat);

	priv->tooltips = gtk_tooltips_new ();

	priv->default_window_height = -1;
	priv->vscroll_visible = FALSE;
	priv->sensitive = TRUE;

	g_signal_connect (chat->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);
	g_signal_connect (GOSSIP_CHAT (chat)->view,
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
	GossipChat     *chat;
	GossipChatPriv *priv;

	chat = GOSSIP_CHAT (object);
	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Finalized: %p", object);

	chat_composing_remove_timeout (chat);
	g_object_unref (GOSSIP_CHAT (object)->account);

	if (priv->tp_chat) {
		g_object_unref (priv->tp_chat);
	}

	G_OBJECT_CLASS (gossip_chat_parent_class)->finalize (object);
}

static void
chat_destroy_cb (EmpathyTpChat *tp_chat,
		 GossipChat    *chat)
{
	GossipChatPriv *priv;
	GtkWidget      *widget;

	priv = GET_PRIV (chat);

	if (priv->tp_chat) {
		g_object_unref (priv->tp_chat);
		priv->tp_chat = NULL;
	}

	gossip_chat_view_append_event (chat->view, _("Disconnected"));

	widget = gossip_chat_get_widget (chat);
	gtk_widget_set_sensitive (widget, FALSE);
	priv->sensitive = FALSE;
}

static void
chat_send (GossipChat  *chat,
	   const gchar *msg)
{
	GossipChatPriv   *priv;
	//GossipLogManager *log_manager;
	GossipMessage    *message;
	GossipContact    *own_contact;

	priv = GET_PRIV (chat);

	if (msg == NULL || msg[0] == '\0') {
		return;
	}

	if (g_str_has_prefix (msg, "/clear")) {
		gossip_chat_view_clear (chat->view);
		return;
	}

	/* FIXME: gossip_app_set_not_away ();*/

	own_contact = gossip_chat_get_own_contact (chat);
	message = gossip_message_new (msg);
	gossip_message_set_sender (message, own_contact);

	//FIXME: log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	//gossip_log_message_for_contact (log_manager, message, FALSE);

	empathy_tp_chat_send (priv->tp_chat, message);

	g_object_unref (message);
}

static void
chat_input_text_view_send (GossipChat *chat)
{
	GossipChatPriv *priv;
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

	chat->is_first_char = TRUE;
}

static void
chat_message_received_cb (EmpathyTpChat *tp_chat,
			  GossipMessage *message,
			  GossipChat    *chat)
{
	GossipChatPriv *priv;
	//GossipLogManager      *log_manager;
	GossipContact         *sender;

	priv = GET_PRIV (chat);

	sender = gossip_message_get_sender (message);
	gossip_debug (DEBUG_DOMAIN, "Appending message ('%s')",
		      gossip_contact_get_name (sender));

/*FIXME:
	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	gossip_log_message_for_contact (log_manager, message, TRUE);
*/
	gossip_chat_view_append_message (chat->view, message);

	if (gossip_chat_should_play_sound (chat)) {
		// FIXME: gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	g_signal_emit_by_name (chat, "new-message", message);
}

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
			       GdkEventKey *event,
			       GossipChat  *chat)
{
	GossipChatPriv *priv;
	GtkAdjustment  *adj;
	gdouble         val;
	GtkWidget      *text_view_sw;

	priv = GET_PRIV (chat);

	if (event->keyval == GDK_Tab && !(event->state & GDK_CONTROL_MASK)) {
		return TRUE;
	}

	/* Catch enter but not ctrl/shift-enter */
	if (IS_ENTER (event->keyval) && !(event->state & GDK_SHIFT_MASK)) {
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
	if (IS_ENTER (event->keyval) && (event->state & GDK_SHIFT_MASK)) {
		/* Newline for shift-enter. */
		return FALSE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
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

	return FALSE;
}

static gboolean
chat_text_view_focus_in_event_cb (GtkWidget  *widget,
				  GdkEvent   *event,
				  GossipChat *chat)
{
	gtk_widget_grab_focus (chat->input_text_view);

	return TRUE;
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
				   GossipChat    *chat)
{
	GossipChatPriv *priv;
	GtkTextIter     start, end;
	gchar          *str;
	gboolean        spell_checker = FALSE;

	priv = GET_PRIV (chat);

	if (gtk_text_buffer_get_char_count (buffer) == 0) {
		chat_composing_stop (chat);
	} else {
		chat_composing_start (chat);
	}

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CHAT_SPELL_CHECKER_ENABLED,
			      &spell_checker);

	if (chat->is_first_char) {
		GtkRequisition  req;
		gint            window_height;
		GtkWidget      *dialog;
		GtkAllocation  *allocation;

		/* Save the window's size */
		dialog = gossip_chat_window_get_dialog (priv->window);
		gtk_window_get_size (GTK_WINDOW (dialog),
				     NULL, &window_height);

		gtk_widget_size_request (chat->input_text_view, &req);

		allocation = &GTK_WIDGET (chat->view)->allocation;

		priv->default_window_height = window_height;
		priv->last_input_height = req.height;
		priv->padding_height = window_height - req.height - allocation->height;

		chat->is_first_char = FALSE;
	}

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (!spell_checker) {
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_remove_tag_by_name (buffer, "misspelled", &start, &end);
		return;
	}

	if (!gossip_spell_supported ()) {
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
		if (!gossip_chat_get_is_command (str)) {
			correct = gossip_spell_check (str);
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
			       GossipChat *chat)
{
	GossipChatPriv *priv;
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
				 GossipChat    *chat)
{
	GossipChatPriv *priv;
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

	dialog = gossip_chat_window_get_dialog (priv->window);
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
			   GossipChat *chat)
{
	gossip_debug (DEBUG_DOMAIN, "Setting focus to the input text view");
	gtk_widget_grab_focus (widget);
}

static void
chat_insert_smiley_activate_cb (GtkWidget  *menuitem,
				GossipChat *chat)
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
			     GossipChat  *chat)
{
	GossipChatPriv  *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;
	gint             x, y;
	GtkTextIter      iter, start, end;
	GtkWidget       *item;
	gchar           *str = NULL;
	GossipChatSpell *chat_spell;
	GtkWidget       *smiley_menu;

	priv = GET_PRIV (chat);

	/* Add the emoticon menu. */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("Insert Smiley"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	smiley_menu = gossip_chat_view_get_smiley_menu (
		G_CALLBACK (chat_insert_smiley_activate_cb),
		chat,
		priv->tooltips);
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
				  GossipChatSpell *chat_spell)
{
/*FIXME:	gossip_spell_dialog_show (chat_spell->chat,
				  chat_spell->start,
				  chat_spell->end,
				  chat_spell->word);*/
}

static GossipChatSpell *
chat_spell_new (GossipChat  *chat,
		const gchar *word,
		GtkTextIter  start,
		GtkTextIter  end)
{
	GossipChatSpell *chat_spell;

	chat_spell = g_new0 (GossipChatSpell, 1);

	chat_spell->chat = g_object_ref (chat);
	chat_spell->word = g_strdup (word);
	chat_spell->start = start;
	chat_spell->end = end;

	return chat_spell;
}

static void
chat_spell_free (GossipChatSpell *chat_spell)
{
	g_object_unref (chat_spell->chat);
	g_free (chat_spell->word);
	g_free (chat_spell);
}

static void
chat_composing_start (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->composing_stop_timeout_id) {
		/* Just restart the timeout */
		chat_composing_remove_timeout (chat);
	} else {
	/* FIXME:
		gossip_session_send_composing (gossip_app_get_session (),
					       priv->contact, TRUE);
					      */
	}

	priv->composing_stop_timeout_id = g_timeout_add (
		1000 * COMPOSING_STOP_TIMEOUT,
		(GSourceFunc) chat_composing_stop_timeout_cb,
		chat);
}

static void
chat_composing_stop (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);

	chat_composing_remove_timeout (chat);
	/* FIXME:
	gossip_session_send_composing (gossip_app_get_session (),
				       priv->contact, FALSE);*/
}

static void
chat_composing_remove_timeout (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->composing_stop_timeout_id) {
		g_source_remove (priv->composing_stop_timeout_id);
		priv->composing_stop_timeout_id = 0;
	}
}

static gboolean
chat_composing_stop_timeout_cb (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->composing_stop_timeout_id = 0;
	/* FIXME:
	gossip_session_send_composing (gossip_app_get_session (),
				       priv->contact, FALSE);*/

	return FALSE;
}

gboolean
gossip_chat_get_is_command (const gchar *str)
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
gossip_chat_correct_word (GossipChat  *chat,
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
gossip_chat_get_name (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_name) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_name (chat);
	}

	return NULL;
}

gchar *
gossip_chat_get_tooltip (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_tooltip) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_tooltip (chat);
	}

	return NULL;
}

GdkPixbuf *
gossip_chat_get_status_pixbuf (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_status_pixbuf) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_status_pixbuf (chat);
	}

	return NULL;
}

GossipContact *
gossip_chat_get_contact (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_contact) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_contact (chat);
	}

	return NULL;
}
GossipContact *
gossip_chat_get_own_contact (GossipChat *chat)
{
	EmpathyContactManager *manager;

	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	manager = empathy_session_get_contact_manager ();

	return empathy_contact_manager_get_own (manager, chat->account);
}

GtkWidget *
gossip_chat_get_widget (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_widget) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_widget (chat);
	}

	return NULL;
}

gboolean
gossip_chat_is_group_chat (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	if (GOSSIP_CHAT_GET_CLASS (chat)->is_group_chat) {
		return GOSSIP_CHAT_GET_CLASS (chat)->is_group_chat (chat);
	}

	return FALSE;
}

gboolean 
gossip_chat_is_connected (GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	priv = GET_PRIV (chat);

	return (priv->tp_chat != NULL);
}

gboolean
gossip_chat_get_show_contacts (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_show_contacts) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_show_contacts (chat);
	}

	return FALSE;
}

void
gossip_chat_set_show_contacts (GossipChat *chat,
			       gboolean    show)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	if (GOSSIP_CHAT_GET_CLASS (chat)->set_show_contacts) {
		GOSSIP_CHAT_GET_CLASS (chat)->set_show_contacts (chat, show);
	}
}

void
gossip_chat_save_geometry (GossipChat *chat,
			   gint        x,
			   gint        y,
			   gint        w,
			   gint        h)
{
	//FIXME: gossip_geometry_save_for_chat (chat, x, y, w, h);
}

void
gossip_chat_load_geometry (GossipChat *chat,
			   gint       *x,
			   gint       *y,
			   gint       *w,
			   gint       *h)
{
	//FIXME: gossip_geometry_load_for_chat (chat, x, y, w, h);
}

void
gossip_chat_set_tp_chat (GossipChat    *chat,
			 EmpathyTpChat *tp_chat)
{
	GossipChatPriv *priv;
	GtkWidget      *widget;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));
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
						      chat_destroy_cb,
						      chat);
		g_object_unref (priv->tp_chat);
	}

	priv->tp_chat = g_object_ref (tp_chat);

	g_signal_connect (tp_chat, "message-received",
			  G_CALLBACK (chat_message_received_cb),
			  chat);
	g_signal_connect (tp_chat, "destroy",
			  G_CALLBACK (chat_destroy_cb),
			  chat);

	empathy_tp_chat_request_pending (tp_chat);

	if (!priv->sensitive) {
		widget = gossip_chat_get_widget (chat);
		gtk_widget_set_sensitive (widget, TRUE);
		gossip_chat_view_append_event (chat->view, _("Connected"));
		priv->sensitive = TRUE;
	}
}

void
gossip_chat_clear (GossipChat *chat)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	gossip_chat_view_clear (chat->view);
}

void
gossip_chat_set_window (GossipChat       *chat,
			GossipChatWindow *window)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);
	priv->window = window;
}

GossipChatWindow *
gossip_chat_get_window (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->window;
}

void
gossip_chat_scroll_down (GossipChat *chat)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	gossip_chat_view_scroll_down (chat->view);
}

void
gossip_chat_cut (GossipChat *chat)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
	}
}

void
gossip_chat_copy (GossipChat *chat)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	if (gossip_chat_view_get_selection_bounds (chat->view, NULL, NULL)) {
		gossip_chat_view_copy_clipboard (chat->view);
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
gossip_chat_paste (GossipChat *chat)
{
	GtkTextBuffer *buffer;
	GtkClipboard  *clipboard;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
}

void
gossip_chat_present (GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = GET_PRIV (chat);

	if (priv->window == NULL) {
		GossipChatWindow *window;

		window = gossip_chat_window_get_default ();
		if (!window) {
			window = gossip_chat_window_new ();
		}

		gossip_chat_window_add_chat (window, chat);
	}

	gossip_chat_window_switch_to_chat (priv->window, chat);
	gossip_window_present (
		GTK_WINDOW (gossip_chat_window_get_dialog (priv->window)),
		TRUE);

 	gtk_widget_grab_focus (chat->input_text_view); 
}

gboolean
gossip_chat_should_play_sound (GossipChat *chat)
{
	GossipChatWindow *window;
	gboolean          play = TRUE;

	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	window = gossip_chat_get_window (GOSSIP_CHAT (chat));
	if (!window) {
		return TRUE;
	}

	play = !gossip_chat_window_has_focus (window);

	return play;
}

gboolean
gossip_chat_should_highlight_nick (GossipMessage *message)
{
	GossipContact *my_contact;
	const gchar   *msg, *to;
	gchar         *cf_msg, *cf_to;
	gchar         *ch;
	gboolean       ret_val;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Highlighting nickname");

	ret_val = FALSE;

	msg = gossip_message_get_body (message);
	if (!msg) {
		return FALSE;
	}

	my_contact = gossip_get_own_contact_from_contact (gossip_message_get_sender (message));
	to = gossip_contact_get_name (my_contact);
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

