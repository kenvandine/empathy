/*
*  Copyright (C) 2009 Collabora Ltd.
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
*  Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
*/

#ifndef __EMPATHY_DEBUG_DIALOG_H__
#define __EMPATHY_DEBUG_DIALOG_H__

G_BEGIN_DECLS

#include <glib-object.h>
#include <gtk/gtk.h>

#define EMPATHY_TYPE_DEBUG_DIALOG (empathy_debug_dialog_get_type ())
#define EMPATHY_DEBUG_DIALOG(object) (G_TYPE_CHECK_INSTANCE_CAST \
        ((object), EMPATHY_TYPE_DEBUG_DIALOG, EmpathyDebugDialog))
#define EMPATHY_DEBUG_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        EMPATHY_TYPE_DEBUG_DIALOG, EmpathyDebugDialogClass))
#define EMPATHY_IS_DEBUG_DIALOG(object) (G_TYPE_CHECK_INSTANCE_TYPE \
    ((object), EMPATHY_TYPE_DEBUG_DIALOG))
#define EMPATHY_IS_DEBUG_DIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_DEBUG_DIALOG))
#define EMPATHY_DEBUG_DIALOG_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS \
    ((object), EMPATHY_TYPE_DEBUG_DIALOG, EmpathyDebugDialogClass))

typedef struct _EmpathyDebugDialog EmpathyDebugDialog;
typedef struct _EmpathyDebugDialogClass EmpathyDebugDialogClass;

struct _EmpathyDebugDialog
{
  GtkDialog parent;
  gpointer priv;
};

struct _EmpathyDebugDialogClass
{
  GtkDialogClass parent_class;
};

GType empathy_debug_dialog_get_type (void) G_GNUC_CONST;

GtkWidget * empathy_debug_dialog_new (GtkWindow *parent);

G_END_DECLS

#endif /* __EMPATHY_DEBUG_DIALOG_H__ */
