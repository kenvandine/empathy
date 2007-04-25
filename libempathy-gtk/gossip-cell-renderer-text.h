/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 */

#ifndef __GOSSIP_CELL_RENDERER_TEXT_H__
#define __GOSSIP_CELL_RENDERER_TEXT_H__

#include <gtk/gtkcellrenderertext.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_CELL_RENDERER_TEXT         (gossip_cell_renderer_text_get_type ())
#define GOSSIP_CELL_RENDERER_TEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CELL_RENDERER_TEXT, GossipCellRendererText))
#define GOSSIP_CELL_RENDERER_TEXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CELL_RENDERER_TEXT, GossipCellRendererTextClass))
#define GOSSIP_IS_CELL_RENDERER_TEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CELL_RENDERER_TEXT))
#define GOSSIP_IS_CELL_RENDERER_TEXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CELL_RENDERER_TEXT))
#define GOSSIP_CELL_RENDERER_TEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CELL_RENDERER_TEXT, GossipCellRendererTextClass))

typedef struct _GossipCellRendererText      GossipCellRendererText;
typedef struct _GossipCellRendererTextClass GossipCellRendererTextClass;
typedef struct _GossipCellRendererTextPriv  GossipCellRendererTextPriv;

struct _GossipCellRendererText {
	GtkCellRendererText         parent;

	GossipCellRendererTextPriv *priv;
};

struct _GossipCellRendererTextClass {
	GtkCellRendererTextClass    parent_class;
};

GType             gossip_cell_renderer_text_get_type (void) G_GNUC_CONST;
GtkCellRenderer * gossip_cell_renderer_text_new      (void);

G_END_DECLS

#endif /* __GOSSIP_CELL_RENDERER_TEXT_H__ */
