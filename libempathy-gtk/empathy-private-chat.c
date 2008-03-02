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

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-contact-list.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-utils.h>

#include "empathy-private-chat.h"
#include "empathy-chat-view.h"
#include "empathy-chat.h"
#include "empathy-preferences.h"
//#include "empathy-sound.h"
#include "empathy-images.h"
#include "empathy-ui-utils.h"

#define DEBUG_DOMAIN "PrivateChat"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_PRIVATE_CHAT, EmpathyPrivateChatPriv))

struct _EmpathyPrivateChatPriv {   
	EmpathyContactFactory *factory;
	EmpathyContact        *contact;
	gchar                 *name;
	gboolean               is_online;
	GtkWidget             *widget;
	GtkWidget             *text_view_sw;
};

static void           empathy_private_chat_class_init            (EmpathyPrivateChatClass *klass);
static void           empathy_private_chat_init                  (EmpathyPrivateChat      *chat);
static void           private_chat_finalize                     (GObject                *object);
static void           private_chat_create_ui                    (EmpathyPrivateChat      *chat);
static void           private_chat_contact_presence_updated_cb  (EmpathyContact          *contact,
								 GParamSpec             *param,
								 EmpathyPrivateChat      *chat);
static void           private_chat_contact_updated_cb           (EmpathyContact          *contact,
								 GParamSpec             *param,
								 EmpathyPrivateChat      *chat);
static void           private_chat_widget_destroy_cb            (GtkWidget              *widget,
								 EmpathyPrivateChat      *chat);
static const gchar *  private_chat_get_name                     (EmpathyChat             *chat);
static gchar *        private_chat_get_tooltip                  (EmpathyChat             *chat);
static const gchar *  private_chat_get_status_icon_name         (EmpathyChat             *chat);
static GtkWidget *    private_chat_get_widget                   (EmpathyChat             *chat);

G_DEFINE_TYPE (EmpathyPrivateChat, empathy_private_chat, EMPATHY_TYPE_CHAT);


static GObject *
private_chat_constructor (GType                  type,
			  guint                  n_props,
			  GObjectConstructParam *props)
{
	GObject                *chat;
	EmpathyPrivateChatPriv *priv;
	EmpathyTpChat          *tp_chat;
	TpChan                 *tp_chan;
	McAccount              *account;

	chat = G_OBJECT_CLASS (empathy_private_chat_parent_class)->constructor (type, n_props, props);

	priv = GET_PRIV (chat);

	g_object_get (chat, "tp-chat", &tp_chat, NULL);
	tp_chan = empathy_tp_chat_get_channel (tp_chat);
	account = empathy_tp_chat_get_account (tp_chat);

	priv->factory = empathy_contact_factory_new ();
	priv->contact = empathy_contact_factory_get_from_handle (priv->factory,
								 account,
								 tp_chan->handle);

	priv->name = g_strdup (empathy_contact_get_name (priv->contact));

	g_signal_connect (priv->contact, 
			  "notify::name",
			  G_CALLBACK (private_chat_contact_updated_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::presence",
			  G_CALLBACK (private_chat_contact_presence_updated_cb),
			  chat);

	priv->is_online = empathy_contact_is_online (priv->contact);

	g_object_unref (tp_chat);

	return chat;
}

static void
empathy_private_chat_class_init (EmpathyPrivateChatClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	EmpathyChatClass *chat_class = EMPATHY_CHAT_CLASS (klass);

	object_class->finalize = private_chat_finalize;
	object_class->constructor = private_chat_constructor;

	chat_class->get_name             = private_chat_get_name;
	chat_class->get_tooltip          = private_chat_get_tooltip;
	chat_class->get_status_icon_name = private_chat_get_status_icon_name;
	chat_class->get_widget           = private_chat_get_widget;
	chat_class->set_tp_chat          = NULL;

	g_type_class_add_private (object_class, sizeof (EmpathyPrivateChatPriv));
}

static void
empathy_private_chat_init (EmpathyPrivateChat *chat)
{
	private_chat_create_ui (chat);
}

static void
private_chat_finalize (GObject *object)
{
	EmpathyPrivateChat     *chat;
	EmpathyPrivateChatPriv *priv;
	
	chat = EMPATHY_PRIVATE_CHAT (object);
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
	if (priv->factory) {
		g_object_unref (priv->factory);
	}
	g_free (priv->name);

	G_OBJECT_CLASS (empathy_private_chat_parent_class)->finalize (object);
}

static void
private_chat_create_ui (EmpathyPrivateChat *chat)
{
	GladeXML              *glade;
	EmpathyPrivateChatPriv *priv;
	GtkWidget             *input_text_view_sw;

	priv = GET_PRIV (chat);

	glade = empathy_glade_get_file ("empathy-chat.glade",
				       "chat_widget",
				       NULL,
				      "chat_widget", &priv->widget,
				      "chat_view_sw", &priv->text_view_sw,
				      "input_text_view_sw", &input_text_view_sw,
				       NULL);

	empathy_glade_connect (glade,
			      chat,
			      "chat_widget", "destroy", private_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", g_object_ref (chat));

	gtk_container_add (GTK_CONTAINER (priv->text_view_sw),
			   GTK_WIDGET (EMPATHY_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (EMPATHY_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_text_view_sw),
			   EMPATHY_CHAT (chat)->input_text_view);
	gtk_widget_show (EMPATHY_CHAT (chat)->input_text_view);
}

static void
private_chat_contact_presence_updated_cb (EmpathyContact     *contact,
					  GParamSpec         *param,
					  EmpathyPrivateChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	empathy_debug (DEBUG_DOMAIN, "Presence update for contact: %s",
		      empathy_contact_get_id (contact));

	if (!empathy_contact_is_online (contact)) {
		if (priv->is_online && !EMPATHY_CHAT (chat)->block_events) {
			gchar *msg;

			msg = g_strdup_printf (_("%s went offline"),
					       empathy_contact_get_name (priv->contact));
			empathy_chat_view_append_event (EMPATHY_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = FALSE;

		g_signal_emit_by_name (chat, "composing", FALSE);

	} else {
		if (!priv->is_online && !EMPATHY_CHAT (chat)->block_events) {
			gchar *msg;

			msg = g_strdup_printf (_("%s has come online"),
					       empathy_contact_get_name (priv->contact));
			empathy_chat_view_append_event (EMPATHY_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = TRUE;

		/* If offline message is not supported by CM we need to
		 * request a new Text Channel. */
		if (!empathy_chat_is_connected (EMPATHY_CHAT (chat))) {
			empathy_chat_with_contact (contact);
		}
	}

	g_signal_emit_by_name (chat, "status-changed");
}

static void
private_chat_contact_updated_cb (EmpathyContact     *contact,
				 GParamSpec        *param,
				 EmpathyPrivateChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (strcmp (priv->name, empathy_contact_get_name (contact)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (empathy_contact_get_name (contact));
		g_signal_emit_by_name (chat, "name-changed", priv->name);
	}
}

static void
private_chat_widget_destroy_cb (GtkWidget         *widget,
				EmpathyPrivateChat *chat)
{
	empathy_debug (DEBUG_DOMAIN, "Destroyed");

	g_object_unref (chat);
}

static const gchar *
private_chat_get_name (EmpathyChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->name;
}

static gchar *
private_chat_get_tooltip (EmpathyChat *chat)
{
	EmpathyPrivateChatPriv *priv;
	const gchar           *status;

	g_return_val_if_fail (EMPATHY_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	status = empathy_contact_get_status (priv->contact);

	return g_strdup_printf ("%s\n%s",
				empathy_contact_get_id (priv->contact),
				status);
}

static const gchar *
private_chat_get_status_icon_name (EmpathyChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return empathy_icon_name_for_contact (priv->contact);
}

EmpathyContact *
empathy_private_chat_get_contact (EmpathyPrivateChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->contact;
}

static GtkWidget *
private_chat_get_widget (EmpathyChat *chat)
{
	EmpathyPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->widget;
}

EmpathyPrivateChat *
empathy_private_chat_new (EmpathyTpChat *tp_chat)
{
	EmpathyPrivateChat *chat;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (tp_chat), NULL);

	chat = g_object_new (EMPATHY_TYPE_PRIVATE_CHAT,
			     "tp-chat", tp_chat,
			     NULL);

	return chat;
}

