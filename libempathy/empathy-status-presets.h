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

#ifndef __EMPATHY_STATUS_PRESETS_H__
#define __EMPATHY_STATUS_PRESETS_H__

#include <libmissioncontrol/mission-control.h>

G_BEGIN_DECLS

void          empathy_status_presets_get_all            (void);
GList *       empathy_status_presets_get                (McPresence   state,
							 gint         max_number);
void          empathy_status_presets_set_last           (McPresence   state,
							 const gchar *status);
void          empathy_status_presets_remove             (McPresence   state,
							 const gchar *status);
void          empathy_status_presets_reset              (void);
McPresence    empathy_status_presets_get_default_state  (void);
const gchar * empathy_status_presets_get_default_status (void);
void          empathy_status_presets_set_default        (McPresence   state,
							 const gchar *status);
void          empathy_status_presets_clear_default      (void);

G_END_DECLS

#endif /* __EMPATHY_STATUS_PRESETS_H__ */
