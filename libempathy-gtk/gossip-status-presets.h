/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Author: Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_STATUS_PRESETS_H__
#define __GOSSIP_STATUS_PRESETS_H__

#include <libempathy/gossip-presence.h>

G_BEGIN_DECLS

void          gossip_status_presets_get_all            (void);
GList *       gossip_status_presets_get                (McPresence   state,
							gint         max_number);
void          gossip_status_presets_set_last           (McPresence   state,
							const gchar *status);
void          gossip_status_presets_reset              (void);
McPresence    gossip_status_presets_get_default_state  (void);
const gchar * gossip_status_presets_get_default_status (void);
void          gossip_status_presets_set_default        (McPresence   state,
							const gchar *status);
void          gossip_status_presets_clear_default      (void);

G_END_DECLS

#endif /* __GOSSIP_STATUS_PRESETS_H__ */
