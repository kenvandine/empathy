/*
 * empathy-call-handler.h - Header for EmpathyCallHandler
 * Copyright (C) 2008-2009 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __EMPATHY_CALL_HANDLER_H__
#define __EMPATHY_CALL_HANDLER_H__

#include <glib-object.h>

#include <gst/gst.h>

#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

typedef struct _EmpathyCallHandler EmpathyCallHandler;
typedef struct _EmpathyCallHandlerClass EmpathyCallHandlerClass;

struct _EmpathyCallHandlerClass {
    GObjectClass parent_class;
};

struct _EmpathyCallHandler {
    GObject parent;
    gpointer priv;
};

GType empathy_call_handler_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_CALL_HANDLER \
  (empathy_call_handler_get_type ())
#define EMPATHY_CALL_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_CALL_HANDLER, \
    EmpathyCallHandler))
#define EMPATHY_CALL_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_CALL_HANDLER, \
  EmpathyCallHandlerClass))
#define EMPATHY_IS_CALL_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_CALL_HANDLER))
#define EMPATHY_IS_CALL_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_CALL_HANDLER))
#define EMPATHY_CALL_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_CALL_HANDLER, \
  EmpathyCallHandlerClass))

EmpathyCallHandler * empathy_call_handler_new_for_contact (
  EmpathyContact *contact);

EmpathyCallHandler * empathy_call_handler_new_for_contact_with_streams (
  EmpathyContact *contact, gboolean audio, gboolean video);

EmpathyCallHandler * empathy_call_handler_new_for_channel (
  EmpathyTpCall *call);

void empathy_call_handler_start_call (EmpathyCallHandler *handler);
void empathy_call_handler_stop_call (EmpathyCallHandler *handler);

gboolean empathy_call_handler_has_initial_video (EmpathyCallHandler *handler);

void empathy_call_handler_bus_message (EmpathyCallHandler *handler,
  GstBus *bus, GstMessage *message);

G_END_DECLS

#endif /* #ifndef __EMPATHY_CALL_HANDLER_H__*/
