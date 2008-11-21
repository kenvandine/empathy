/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 */

#ifndef __EMPATHY_FILE_H__
#define __EMPATHY_FILE_H__

#include <gio/gio.h>
#include <glib.h>

#include <telepathy-glib/channel.h>
#include <libtelepathy/tp-constants.h>

#include <extensions/extensions.h>

#include "empathy-contact.h"

#include <libmissioncontrol/mc-account.h>

/* Forward-declaration to resolve cyclic dependencies */
typedef struct _EmpathyFile      EmpathyFile;

#include "empathy-file.h"

G_BEGIN_DECLS

#define EMPATHY_FILE_UNKNOWN_SIZE G_MAXUINT64

#define EMPATHY_TYPE_FILE         (empathy_file_get_type ())
#define EMPATHY_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_FILE, EmpathyFile))
#define EMPATHY_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_FILE, EmpathyFileClass))
#define EMPATHY_IS_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_FILE))
#define EMPATHY_IS_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_FILE))
#define EMPATHY_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_FILE, EmpathyFileClass))

typedef struct _EmpathyFileClass EmpathyFileClass;

struct _EmpathyFile
{
  GObject      parent;
};

struct _EmpathyFileClass
{
  GObjectClass parent_class;
};

GType empathy_file_get_type (void) G_GNUC_CONST;

EmpathyFile *empathy_file_new (McAccount *account, TpChannel *channel);

TpChannel *empathy_file_get_channel (EmpathyFile *file);
void empathy_file_accept (EmpathyFile *file);
void empathy_file_cancel (EmpathyFile *file);

const gchar *empathy_file_get_id (EmpathyFile *file);
guint64 empathy_file_get_transferred_bytes (EmpathyFile *file);
EmpathyContact *empathy_file_get_contact (EmpathyFile *file);
GInputStream *empathy_file_get_input_stream (EmpathyFile *file);
GOutputStream *empathy_file_get_output_stream (EmpathyFile *file);
const gchar *empathy_file_get_filename (EmpathyFile *file);
EmpFileTransferDirection empathy_file_get_direction (EmpathyFile *file);
EmpFileTransferState empathy_file_get_state (EmpathyFile *file);
EmpFileTransferStateChangeReason empathy_file_get_state_change_reason (EmpathyFile *file);
guint64 empathy_file_get_size (EmpathyFile *file);
guint64 empathy_file_get_transferred_bytes (EmpathyFile *file);
gint empathy_file_get_remaining_time (EmpathyFile *file);

void empathy_file_set_input_stream (EmpathyFile *file, GInputStream *uri);
void empathy_file_set_output_stream (EmpathyFile *file, GOutputStream *uri);
void empathy_file_set_filename (EmpathyFile *file, const gchar *filename);

G_END_DECLS

#endif /* __EMPATHY_FILE_H__ */
