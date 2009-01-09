/*
 * empathy-tube-dispatch.h - Header for EmpathyTubeDispatch
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

#ifndef __EMPATHY_TUBE_DISPATCH_H__
#define __EMPATHY_TUBE_DISPATCH_H__

#include <glib-object.h>

#include <libempathy/empathy-dispatch-operation.h>

G_BEGIN_DECLS

typedef enum {
  EMPATHY_TUBE_DISPATCHABILITY_UNKNOWN,
  EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE,
  EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE,
} EmpathyTubeDispatchAbility;

typedef struct _EmpathyTubeDispatch EmpathyTubeDispatch;
typedef struct _EmpathyTubeDispatchClass EmpathyTubeDispatchClass;

struct _EmpathyTubeDispatchClass {
    GObjectClass parent_class;
};

struct _EmpathyTubeDispatch {
    GObject parent;
};

GType empathy_tube_dispatch_get_type(void);

/* TYPE MACROS */
#define EMPATHY_TYPE_TUBE_DISPATCH \
  (empathy_tube_dispatch_get_type())
#define EMPATHY_TUBE_DISPATCH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_TUBE_DISPATCH, \
    EmpathyTubeDispatch))
#define EMPATHY_TUBE_DISPATCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_TUBE_DISPATCH, \
    EmpathyTubeDispatchClass))
#define EMPATHY_IS_TUBE_DISPATCH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_TUBE_DISPATCH))
#define EMPATHY_IS_TUBE_DISPATCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_TUBE_DISPATCH))
#define EMPATHY_TUBE_DISPATCH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_TUBE_DISPATCH, \
   EmpathyTubeDispatchClass))

EmpathyTubeDispatch * empathy_tube_dispatch_new (
  EmpathyDispatchOperation *operation);

EmpathyTubeDispatchAbility empathy_tube_dispatch_is_dispatchable (
  EmpathyTubeDispatch *tube_dispatch);
void empathy_tube_dispatch_handle (EmpathyTubeDispatch *tube_dispatch);

G_END_DECLS

#endif /* #ifndef __EMPATHY_TUBE_DISPATCH_H__*/
