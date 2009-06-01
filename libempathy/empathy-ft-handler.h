/*
 * empathy-ft-handler.h - Header for EmpathyFTHandler
 * Copyright (C) 2009 Collabora Ltd.
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
 * Author: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

/* empathy-ft-handler.h */

#ifndef __EMPATHY_FT_HANDLER_H__
#define __EMPATHY_FT_HANDLER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "empathy-tp-file.h"
#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_FT_HANDLER empathy_ft_handler_get_type()
#define EMPATHY_FT_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandler))
#define EMPATHY_FT_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerClass))
#define EMPATHY_IS_FT_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_FT_HANDLER))
#define EMPATHY_IS_FT_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_FT_HANDLER))
#define EMPATHY_FT_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerClass))

typedef struct {
  GObject parent;
  gpointer priv;
} EmpathyFTHandler;

typedef struct {
  GObjectClass parent_class;
} EmpathyFTHandlerClass;

/**
 * EmpathyFTHandlerReadyCallback:
 * @handler: the handler which is now ready
 * @error: a #GError if the operation failed, or %NULL
 * @user_data: user data passed to the callback
 */
typedef void (* EmpathyFTHandlerReadyCallback) (EmpathyFTHandler *handler,
    GError *error,
    gpointer user_data);

GType empathy_ft_handler_get_type (void);

/* public methods */
void empathy_ft_handler_new_outgoing (EmpathyContact *contact,
    GFile *source,
    EmpathyFTHandlerReadyCallback callback,
    gpointer user_data);

void empathy_ft_handler_new_incoming (EmpathyTpFile *tp_file,
    EmpathyFTHandlerReadyCallback callback,
    gpointer user_data);
void empathy_ft_handler_incoming_set_destination (EmpathyFTHandler *handler,
    GFile *destination);

void empathy_ft_handler_start_transfer (EmpathyFTHandler *handler);
void empathy_ft_handler_cancel_transfer (EmpathyFTHandler *handler);

/* properties of the transfer */
const char * empathy_ft_handler_get_filename (EmpathyFTHandler *handler);
const char * empathy_ft_handler_get_content_type (EmpathyFTHandler *handler);
EmpathyContact * empathy_ft_handler_get_contact (EmpathyFTHandler *handler);
GFile * empathy_ft_handler_get_gfile (EmpathyFTHandler *handler);
gboolean empathy_ft_handler_get_use_hash (EmpathyFTHandler *handler);
gboolean empathy_ft_handler_is_incoming (EmpathyFTHandler *handler);
guint64 empathy_ft_handler_get_transferred_bytes (EmpathyFTHandler *handler);
guint64 empathy_ft_handler_get_total_bytes (EmpathyFTHandler *handler);
gboolean empathy_ft_handler_is_completed (EmpathyFTHandler *handler);
gboolean empathy_ft_handler_is_cancelled (EmpathyFTHandler *handler);

G_END_DECLS

#endif /* __EMPATHY_FT_HANDLER_H__ */
