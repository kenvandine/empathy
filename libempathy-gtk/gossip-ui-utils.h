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
 * 
 *          Part of this file is copied from GtkSourceView (gtksourceiter.c):
 *          Paolo Maggi
 *          Jeroen Zwartepoorte
 */

#ifndef __GOSSIP_UI_UTILS_H__
#define __GOSSIP_UI_UTILS_H__

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-profile.h>

#include <libempathy/gossip-presence.h>
#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-avatar.h>

#include "gossip-chat-view.h"

G_BEGIN_DECLS

#define G_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

/* Glade */
void            gossip_glade_get_file_simple             (const gchar         *filename,
							  const gchar         *root,
							  const gchar         *domain,
							  const gchar         *first_required_widget,
							  ...);
GladeXML *      gossip_glade_get_file                    (const gchar         *filename,
							  const gchar         *root,
							  const gchar         *domain,
							  const gchar         *first_required_widget,
							  ...);
void            gossip_glade_connect                     (GladeXML            *gui,
							  gpointer             user_data,
							  gchar               *first_widget,
							  ...);
void            gossip_glade_setup_size_group            (GladeXML            *gui,
							  GtkSizeGroupMode     mode,
							  gchar               *first_widget,
							  ...);
/* Pixbufs */
GdkPixbuf *     gossip_pixbuf_from_icon_name             (const gchar         *icon_name,
							  GtkIconSize          icon_size);
GdkPixbuf *     gossip_pixbuf_from_smiley                (GossipSmiley         type,
							  GtkIconSize          icon_size);
const gchar *   gossip_icon_name_from_account            (McAccount           *account);
const gchar *   gossip_icon_name_for_presence_state      (McPresence           state);
const gchar *   gossip_icon_name_for_presence            (GossipPresence      *presence);
const gchar *   gossip_icon_name_for_contact             (GossipContact       *contact);
GdkPixbuf *     gossip_pixbuf_from_avatar_scaled         (GossipAvatar        *avatar,
							  gint                 width,
							  gint                 height);
GdkPixbuf *     gossip_pixbuf_avatar_from_contact        (GossipContact       *contact);
GdkPixbuf *     gossip_pixbuf_avatar_from_contact_scaled (GossipContact       *contact,
							  gint                 width,
							  gint                 height);
/* Text view */
gboolean   gossip_text_iter_forward_search          (const GtkTextIter   *iter,
						     const gchar         *str,
						     GtkTextIter         *match_start,
						     GtkTextIter         *match_end,
						     const GtkTextIter   *limit);
gboolean   gossip_text_iter_backward_search         (const GtkTextIter   *iter,
						     const gchar         *str,
						     GtkTextIter         *match_start,
						     GtkTextIter         *match_end,
						     const GtkTextIter   *limit);

/* Windows */
gboolean   gossip_window_get_is_visible             (GtkWindow           *window);
void       gossip_window_present                    (GtkWindow           *window,
						     gboolean             steal_focus);
GtkWindow *gossip_get_toplevel_window               (GtkWidget           *widget);
void       gossip_url_show                          (const char          *url);
void       gossip_toggle_button_set_state_quietly   (GtkWidget           *widget,
						     GCallback            callback,
						     gpointer             user_data,
						     gboolean             active);
GtkWidget *gossip_link_button_new                   (const gchar         *url,
						     const gchar         *title);


G_END_DECLS

#endif /*  __GOSSIP_UI_UTILS_H__ */
