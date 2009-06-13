/*
 * Copyright (C) 2007-2008 Guillaume Desmottes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Guillaume Desmottes <gdesmott@gnome.org>
 */

#ifndef __EMPATHY_IRC_NETWORK_DIALOG_H__
#define __EMPATHY_IRC_NETWORK_DIALOG_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-irc-network.h>

G_BEGIN_DECLS

GtkWidget * empathy_irc_network_dialog_show (EmpathyIrcNetwork *network,
    GtkWidget *parent);

G_END_DECLS

#endif /* __EMPATHY_IRC_NETWORK_DIALOG_H__ */
