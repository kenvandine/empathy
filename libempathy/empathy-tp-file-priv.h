/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Marco Barisione <marco@barisione.org>
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
 */

#ifndef __EMPATHY_TP_FILE_PRIV_H__
#define __EMPATHY_TP_FILE_PRIV_H__

#include <glib.h>

#include <gio/gio.h>

#include "empathy-file.h"
#include "empathy-tp-file.h"

#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

guint64                                 _empathy_tp_file_get_transferred_bytes   (EmpathyTpFile   *tp_file);
EmpathyContact *                        _empathy_tp_file_get_contact             (EmpathyTpFile   *tp_file);
GInputStream *                          _empathy_tp_file_get_input_stream        (EmpathyTpFile   *tp_file);
GOutputStream *                         _empathy_tp_file_get_output_stream       (EmpathyTpFile   *tp_file);
const gchar *                           _empathy_tp_file_get_filename            (EmpathyTpFile   *tp_file);
EmpFileTransferDirection          _empathy_tp_file_get_direction           (EmpathyTpFile   *tp_file);
EmpFileTransferState              _empathy_tp_file_get_state               (EmpathyTpFile   *tp_file);
EmpFileTransferStateChangeReason  _empathy_tp_file_get_state_change_reason (EmpathyTpFile   *tp_file);
guint64                                 _empathy_tp_file_get_size                (EmpathyTpFile   *tp_file);
guint64                                 _empathy_tp_file_get_transferred_bytes   (EmpathyTpFile   *tp_file);
gint                                    _empathy_tp_file_get_remaining_time      (EmpathyTpFile   *tp_file);

void                                    _empathy_tp_file_set_input_stream        (EmpathyTpFile   *tp_file,
										GInputStream  *uri);
void                                    _empathy_tp_file_set_output_stream       (EmpathyTpFile   *tp_file,
										GOutputStream *uri);
void                                    _empathy_tp_file_set_filename            (EmpathyTpFile   *tp_file,
										const gchar   *filename);

G_END_DECLS

#endif /* __EMPATHY_TP_FILE_PRIV_H__ */
