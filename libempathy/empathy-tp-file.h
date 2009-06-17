/*
 * Copyright (C) 2007-2009 Collabora Ltd.
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
 *          Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#ifndef __EMPATHY_TP_FILE_H__
#define __EMPATHY_TP_FILE_H__

#include <gio/gio.h>
#include <glib.h>

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_FILE (empathy_tp_file_get_type ())
#define EMPATHY_TP_FILE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), \
    EMPATHY_TYPE_TP_FILE, EmpathyTpFile))
#define EMPATHY_TP_FILE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), \
    EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))
#define EMPATHY_IS_TP_FILE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
    EMPATHY_TYPE_TP_FILE))
#define EMPATHY_IS_TP_FILE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
    EMPATHY_TYPE_TP_FILE))
#define EMPATHY_TP_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), \
    EMPATHY_TYPE_TP_FILE, EmpathyTpFileClass))

#define EMPATHY_FT_ERROR_QUARK g_quark_from_static_string ("EmpathyFTError")

typedef enum {
	EMPATHY_FT_ERROR_FAILED,
	EMPATHY_FT_ERROR_HASH_MISMATCH,
	EMPATHY_FT_ERROR_TP_ERROR,
	EMPATHY_FT_ERROR_SOCKET,
	EMPATHY_FT_ERROR_NOT_SUPPORTED,
	EMPATHY_FT_ERROR_INVALID_SOURCE_FILE,
	EMPATHY_FT_ERROR_EMPTY_SOURCE_FILE
} EmpathyFTErrorEnum;

typedef struct _EmpathyTpFile EmpathyTpFile;
typedef struct _EmpathyTpFileClass EmpathyTpFileClass;

struct _EmpathyTpFile {
  GObject  parent;
  gpointer priv;
};

struct _EmpathyTpFileClass {
  GObjectClass parent_class;
};

/* prototypes for operation callbacks */

/**
 * EmpathyTpFileProgressCallback:
 * @tp_file: the #EmpathyTpFile being transferred
 * @current_bytes: the bytes currently transferred by the operation
 * @user_data: user data passed to the callback
 **/
typedef void (* EmpathyTpFileProgressCallback)
    (EmpathyTpFile *tp_file,
     guint64 current_bytes,
     gpointer user_data);

/**
 * EmpathyTpFileOperationCallback:
 * @tp_file: the #EmpathyTpFile that has been transferred
 * @error: a #GError if the operation didn't succeed, %NULL otherwise
 * @user_data: user data passed to the callback
 **/
typedef void (* EmpathyTpFileOperationCallback)
    (EmpathyTpFile *tp_file,
     const GError *error,
     gpointer user_data);

GType empathy_tp_file_get_type (void) G_GNUC_CONST;

/* public methods */

EmpathyTpFile * empathy_tp_file_new (TpChannel *channel,
    gboolean incoming);

void empathy_tp_file_accept (EmpathyTpFile *tp_file,
    guint64 offset,
    GFile *gfile,
    GCancellable *cancellable,
    EmpathyTpFileProgressCallback progress_callback,
    gpointer progress_user_data,
    EmpathyTpFileOperationCallback op_callback,
    gpointer op_user_data);

void empathy_tp_file_offer (EmpathyTpFile *tp_file,
    GFile *gfile,
    GCancellable *cancellable,
    EmpathyTpFileProgressCallback progress_callback,
    gpointer progress_user_data,
    EmpathyTpFileOperationCallback op_callback,
    gpointer op_user_data);

void empathy_tp_file_cancel (EmpathyTpFile *tp_file);
void empathy_tp_file_close (EmpathyTpFile *tp_file);

gboolean empathy_tp_file_is_incoming (EmpathyTpFile *tp_file);

G_END_DECLS

#endif /* __EMPATHY_TP_FILE_H__ */
