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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mc-account.h>

#include <libempathy/gossip-debug.h>
#include <libempathy/empathy-tp-chat.h>
//#include <libgossip/gossip-log.h>

#include "gossip-private-chat.h"
#include "gossip-chat-view.h"
#include "gossip-chat.h"
#include "gossip-preferences.h"
//#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "PrivateChat"

#define COMPOSING_STOP_TIMEOUT 5

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_PRIVATE_CHAT, GossipPrivateChatPriv))

struct _GossipPrivateChatPriv {   
	GossipContact *contact;
	gchar         *name;

	guint          scroll_idle_id;
	gboolean       is_online;

	GtkWidget     *widget;
	GtkWidget     *text_view_sw;
};

static void           gossip_private_chat_class_init            (GossipPrivateChatClass *klass);
static void           gossip_private_chat_init                  (GossipPrivateChat      *chat);
static void           private_chat_finalize                     (GObject                *object);
static void           private_chat_create_ui                    (GossipPrivateChat      *chat);
static void           private_chat_contact_presence_updated_cb  (GossipContact          *contact,
								 GParamSpec             *param,
								 GossipPrivateChat      *chat);
static void           private_chat_contact_updated_cb           (GossipContact          *contact,
								 GParamSpec             *param,
								 GossipPrivateChat      *chat);
static void           private_chat_widget_destroy_cb            (GtkWidget              *widget,
								 GossipPrivateChat      *chat);
static const gchar *  private_chat_get_name                     (GossipChat             *chat);
static gchar *        private_chat_get_tooltip                  (GossipChat             *chat);
static GdkPixbuf *    private_chat_get_status_pixbuf            (GossipChat             *chat);
static GossipContact *private_chat_get_contact                  (GossipChat             *chat);
static GtkWidget *    private_chat_get_widget                   (GossipChat             *chat);
/*static GdkPixbuf *    private_chat_pad_to_size                  (GdkPixbuf              *pixbuf,
								 gint                    width,
								 gint                    height,
								 gint                    extra_padding_right);*/

G_DEFINE_TYPE (GossipPrivateChat, gossip_private_chat, GOSSIP_TYPE_CHAT);

static void
gossip_private_chat_class_init (GossipPrivateChatClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	GossipChatClass *chat_class = GOSSIP_CHAT_CLASS (klass);

	object_class->finalize = private_chat_finalize;

	chat_class->get_name          = private_chat_get_name;
	chat_class->get_tooltip       = private_chat_get_tooltip;
	chat_class->get_status_pixbuf = private_chat_get_status_pixbuf;
	chat_class->get_contact       = private_chat_get_contact;
	chat_class->get_widget        = private_chat_get_widget;
	chat_class->get_show_contacts = NULL;
	chat_class->set_show_contacts = NULL;
	chat_class->is_group_chat     = NULL;

	g_type_class_add_private (object_class, sizeof (GossipPrivateChatPriv));
}

static void
gossip_private_chat_init (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->is_online = FALSE;

	private_chat_create_ui (chat);

}

static void
private_chat_finalize (GObject *object)
{
	GossipPrivateChat     *chat;
	GossipPrivateChatPriv *priv;
	
	chat = GOSSIP_PRIVATE_CHAT (object);
	priv = GET_PRIV (chat);

	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_updated_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_presence_updated_cb,
					      chat);

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	if (priv->scroll_idle_id) {
		g_source_remove (priv->scroll_idle_id);
	}

	g_free (priv->name);

	G_OBJECT_CLASS (gossip_private_chat_parent_class)->finalize (object);
}

static void
private_chat_create_ui (GossipPrivateChat *chat)
{
	GladeXML              *glade;
	GossipPrivateChatPriv *priv;
	GtkWidget             *input_text_view_sw;

	priv = GET_PRIV (chat);

	glade = gossip_glade_get_file ("empathy-chat.glade",
				       "chat_widget",
				       NULL,
				      "chat_widget", &priv->widget,
				      "chat_view_sw", &priv->text_view_sw,
				      "input_text_view_sw", &input_text_view_sw,
				       NULL);

	gossip_glade_connect (glade,
			      chat,
			      "chat_widget", "destroy", private_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", g_object_ref (chat));

	gtk_container_add (GTK_CONTAINER (priv->text_view_sw),
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_text_view_sw),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);
}

static void
private_chat_contact_presence_updated_cb (GossipContact     *contact,
					  GParamSpec        *param,
					  GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Presence update for contact: %s",
		      gossip_contact_get_id (contact));

	if (!gossip_contact_is_online (contact)) {
		if (priv->is_online) {
			gchar *msg;

			msg = g_strdup_printf (_("%s went offline"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = FALSE;

		g_signal_emit_by_name (chat, "composing", FALSE);

	} else {
		if (!priv->is_online) {
			gchar *msg;

			msg = g_strdup_printf (_("%s has come online"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = TRUE;
	}

	g_signal_emit_by_name (chat, "status-changed");
}

static void
private_chat_contact_updated_cb (GossipContact     *contact,
				 GParamSpec        *param,
				 GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (strcmp (priv->name, gossip_contact_get_name (contact)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (gossip_contact_get_name (contact));
		g_signal_emit_by_name (chat, "name-changed", priv->name);
	}
}

static void
private_chat_widget_destroy_cb (GtkWidget         *widget,
				GossipPrivateChat *chat)
{
	gossip_debug (DEBUG_DOMAIN, "Destroyed");

	g_object_unref (chat);
}

static const gchar *
private_chat_get_name (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->name;
}

static gchar *
private_chat_get_tooltip (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;
	const gchar           *status;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	contact = gossip_chat_get_contact (chat);
	status = gossip_contact_get_status (contact);

	return g_strdup_printf ("%s\n%s",
				gossip_contact_get_id (contact),
				status);
}

static GdkPixbuf *
private_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	contact = gossip_chat_get_contact (chat);

	return gossip_pixbuf_for_contact (contact);
}

static GossipContact *
private_chat_get_contact (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->contact;
}

static GtkWidget *
private_chat_get_widget (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->widget;
}

/* Scroll down after the back-log has been received. */
static gboolean
private_chat_scroll_down_idle_func (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_chat_scroll_down (chat);
	g_object_unref (chat);

	priv->scroll_idle_id = 0;

	return FALSE;
}

static void
private_chat_setup (GossipPrivateChat *chat,
		    GossipContact     *contact,
		    EmpathyTpChat     *tp_chat)
{
	GossipPrivateChatPriv *priv;
	//GossipLogManager      *log_manager;
	GossipChatView        *view;
/*	GossipContact         *sender;
	GossipMessage         *message;
	GList                 *messages, *l;
	gint                   num_messages, i;*/

	priv = GET_PRIV (chat);

	gossip_chat_set_tp_chat (GOSSIP_CHAT (chat), tp_chat);

	priv->contact = g_object_ref (contact);
	GOSSIP_CHAT (chat)->account = g_object_ref (gossip_contact_get_account (contact));

	priv->name = g_strdup (gossip_contact_get_name (contact));

	g_signal_connect (priv->contact, 
			  "notify::name",
			  G_CALLBACK (private_chat_contact_updated_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::presence",
			  G_CALLBACK (private_chat_contact_presence_updated_cb),
			  chat);

	view = GOSSIP_CHAT (chat)->view;

	/* Turn off scrolling temporarily */
	gossip_chat_view_scroll (view, FALSE);
#if 0
FIXME:
	/* Add messages from last conversation */
	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	messages = gossip_log_get_last_for_contact (log_manager, priv->contact);
	num_messages  = g_list_length (messages);

	for (l = messages, i = 0; l; l = l->next, i++) {
		message = l->data;

		if (num_messages - i > 10) {
			continue;
		}

		sender = gossip_message_get_sender (message);
		if (gossip_contact_equal (priv->own_contact, sender)) {
			gossip_chat_view_append_message_from_self (view,
								   message,
								   priv->own_contact,
								   priv->own_avatar);
		} else {
			gossip_chat_view_append_message_from_other (view,
								    message,
								    sender,
								    priv->other_avatar);
		}
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);
#endif
	/* Turn back on scrolling */
	gossip_chat_view_scroll (view, TRUE);

	/* Scroll to the most recent messages, we reference the chat
	 * for the duration of the scroll func.
	 */
	priv->scroll_idle_id = g_idle_add ((GSourceFunc) 
					   private_chat_scroll_down_idle_func, 
					   g_object_ref (chat));

	priv->is_online = gossip_contact_is_online (priv->contact);
}

GossipPrivateChat *
gossip_private_chat_new (GossipContact *contact)
{
	GossipPrivateChat *chat;
	EmpathyTpChat     *tp_chat;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);
	tp_chat = empathy_tp_chat_new_with_contact (contact);

	private_chat_setup (chat, contact, tp_chat);
	g_object_unref (tp_chat);

	return chat;
}

GossipPrivateChat *
gossip_private_chat_new_with_channel (GossipContact *contact,
				      TpChan        *tp_chan)
{
	GossipPrivateChat *chat;
	EmpathyTpChat     *tp_chat;
	McAccount         *account;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	account = gossip_contact_get_account (contact);
	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);
	tp_chat = empathy_tp_chat_new (account, tp_chan);

	private_chat_setup (chat, contact, tp_chat);
	g_object_unref (tp_chat);

	return chat;
}

/* Pads a pixbuf to the specified size, by centering it in a larger transparent
 * pixbuf. Returns a new ref.
 */
#if 0
FIXME:
static GdkPixbuf *
private_chat_pad_to_size (GdkPixbuf *pixbuf,
			  gint       width,
			  gint       height,
			  gint       extra_padding_right)
{
	gint       src_width, src_height;
	GdkPixbuf *padded;
	gint       x_offset, y_offset;

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);

	x_offset = (width - src_width) / 2;
	y_offset = (height - src_height) / 2;

	padded = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
				 TRUE, /* alpha */
				 gdk_pixbuf_get_bits_per_sample (pixbuf),
				 width + extra_padding_right,
				 height);

	gdk_pixbuf_fill (padded, 0);

	gdk_pixbuf_copy_area (pixbuf,
			      0, /* source coords */
			      0,
			      src_width,
			      src_height,
			      padded,
			      x_offset, /* dest coords */
			      y_offset);

	return padded;
}
#endif

