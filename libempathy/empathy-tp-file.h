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

#ifndef __EMPATHY_TP_FILE_H__
#define __EMPATHY_TP_FILE_H__

#include <glib.h>

#include <telepathy-glib/channel.h>
#include <libtelepathy/tp-constants.h>

#include "empathy-contact.h"

#include <libmissioncontrol/mc-account.h>

/* Forward-declaration to resolve cyclic dependencies */
typedef struct _EmpathyTpFile      EmpathyTpFile;

#include "empathy-file.h"

G_BEGIN_DECLS
#define EMPATHY_TYPE_TP_FILE         (empathy_tp_file_get_type ())
#define EMPATHY_TP_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_FILE, EmpathyTpFile))
#define EMPATHY_TP_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))
#define EMPATHY_IS_TP_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_FILE))
#define EMPATHY_IS_TP_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_FILE))
#define EMPATHY_TP_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))

typedef struct _EmpathyTpFileClass EmpathyTpFileClass;

struct _EmpathyTpFile
{
  GObject      parent;
};

struct _EmpathyTpFileClass
{
  GObjectClass parent_class;
};

GType empathy_tp_file_get_type (void) G_GNUC_CONST;
EmpathyTpFile *empathy_tp_file_new (McAccount       *account, TpChannel *channel);
const gchar *empathy_tp_file_get_id (EmpathyTpFile *tp_file);
TpChannel *empathy_tp_file_get_channel (EmpathyTpFile *tp_file);

G_END_DECLS

#endif /* __EMPATHY_TP_FILE_H__ */
