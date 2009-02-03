/*
 * empathy-call-window.c - Source for EmpathyCallWindow
 * Copyright (C) 2008 Collabora Ltd.
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


#include <stdio.h>
#include <stdlib.h>

#include "empathy-call-window.h"

G_DEFINE_TYPE(EmpathyCallWindow, empathy_call_window, GTK_TYPE_WINDOW)

/* signal enum */
#if 0
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
#endif

/* private structure */
typedef struct _EmpathyCallWindowPrivate EmpathyCallWindowPrivate;

struct _EmpathyCallWindowPrivate
{
  gboolean dispose_has_run;
};

#define EMPATHY_CALL_WINDOW_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_CALL_WINDOW, \
    EmpathyCallWindowPrivate))

static void
empathy_call_window_init (EmpathyCallWindow *obj)
{
  //EmpathyCallWindowPrivate *priv = EMPATHY_CALL_WINDOW_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void empathy_call_window_dispose (GObject *object);
static void empathy_call_window_finalize (GObject *object);

static void
empathy_call_window_class_init (EmpathyCallWindowClass *empathy_call_window_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_call_window_class);

  g_type_class_add_private (empathy_call_window_class,
    sizeof (EmpathyCallWindowPrivate));

  object_class->dispose = empathy_call_window_dispose;
  object_class->finalize = empathy_call_window_finalize;

}

void
empathy_call_window_dispose (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPrivate *priv = EMPATHY_CALL_WINDOW_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose (object);
}

void
empathy_call_window_finalize (GObject *object)
{
  //EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  //EmpathyCallWindowPrivate *priv = EMPATHY_CALL_WINDOW_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (empathy_call_window_parent_class)->finalize (object);
}


EmpathyCallWindow *
empathy_call_window_new (EmpathyCallHandler *handler)
{
  return EMPATHY_CALL_WINDOW (g_object_new (EMPATHY_TYPE_CALL_WINDOW, NULL));
}
