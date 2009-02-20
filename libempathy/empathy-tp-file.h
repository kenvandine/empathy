/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
 * Copyright (C) 2007 Marco Barisione <marco@barisione.org>
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
 * Authors: Marco Barisione <marco@barisione.org>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#ifndef __EMPATHY_TP_FILE_H__
#define __EMPATHY_TP_FILE_H__

#include <gio/gio.h>
#include <glib.h>

#include <telepathy-glib/channel.h>

#include "empathy-contact.h"

#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

#define EMPATHY_TP_FILE_UNKNOWN_SIZE G_MAXUINT64

#define EMPATHY_TYPE_TP_FILE         (empathy_tp_file_get_type ())
#define EMPATHY_TP_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_FILE, EmpathyTpFile))
#define EMPATHY_TP_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))
#define EMPATHY_IS_TP_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_FILE))
#define EMPATHY_IS_TP_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_FILE))
#define EMPATHY_TP_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))

typedef struct _EmpathyTpFile EmpathyTpFile;
typedef struct _EmpathyTpFilePriv EmpathyTpFilePriv;
typedef struct _EmpathyTpFileClass EmpathyTpFileClass;

struct _EmpathyTpFile
{
  GObject      parent;

  EmpathyTpFilePriv *priv;
};

struct _EmpathyTpFileClass
{
  GObjectClass parent_class;
};

GType empathy_tp_file_get_type (void) G_GNUC_CONST;

EmpathyTpFile *empathy_tp_file_new (TpChannel *channel);

TpChannel *empathy_tp_file_get_channel (EmpathyTpFile *tp_file);
void empathy_tp_file_accept (EmpathyTpFile *tp_file, guint64 offset,
  GFile *gfile, GError **error);
void empathy_tp_file_cancel (EmpathyTpFile *tp_file);
void empathy_tp_file_close (EmpathyTpFile *tp_file);
void empathy_tp_file_offer (EmpathyTpFile *tp_file, GFile *gfile,
  GError **error);

EmpathyContact *empathy_tp_file_get_contact (EmpathyTpFile *tp_file);
const gchar *empathy_tp_file_get_filename (EmpathyTpFile *tp_file);
gboolean empathy_tp_file_is_incoming (EmpathyTpFile *tp_file);
TpFileTransferState empathy_tp_file_get_state (
  EmpathyTpFile *tp_file, TpFileTransferStateChangeReason *reason);
guint64 empathy_tp_file_get_size (EmpathyTpFile *tp_file);
guint64 empathy_tp_file_get_transferred_bytes (EmpathyTpFile *tp_file);
gint empathy_tp_file_get_remaining_time (EmpathyTpFile *tp_file);
const gchar *empathy_tp_file_get_content_type (EmpathyTpFile *tp_file);

G_END_DECLS

#endif /* __EMPATHY_TP_FILE_H__ */
