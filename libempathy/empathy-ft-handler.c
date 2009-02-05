/*
 * empathy-ft-handler.c - Source for EmpathyFTHandler
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
 
/* empathy-ft-handler.c */

#include "empathy-ft-handler.h"

G_DEFINE_TYPE (EmpathyFTHandler, empathy_ft_handler, G_TYPE_OBJECT)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyFTHandlerPriv)

enum {
  PROP_TP_FILE = 1,
  PROP_G_FILE,
  PROP_CONTACT
};

typedef struct EmpathyFTHandlerPriv {
  gboolean dispose_run;
  EmpathyContact *contact;
  GFile *gfile;
  EmpathyTpFile *tpfile;
};

static void
do_get_property (GObject *object,
                 guint property_id,
                 GValue *value,
                 GParamSpec *pspec)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CONTACT:
        g_value_set_object (value, priv->contact);
        break;
      case PROP_G_FILE:
        g_value_set_object (value, priv->gfile);
        break;
      case PROP_TP_FILE:
        g_value_set_object (value, priv->tpfile);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
do_set_property (GObject *object,
                 guint property_id, 
                 const GValue *value,
                 GParamSpec *pspec)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CONTACT:
        priv->contact = g_value_dup_object (value);
        break;
      case PROP_G_FILE:
        priv->gfile = g_value_dup_object (value);
        break;
      case PROP_TP_FILE:
        priv->tpfile = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
do_dispose (GObject *object)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (object);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->contact) {
    g_object_unref (priv->contact);
    priv->contact = NULL;
  }

  if (priv->gfile) {
    g_object_unref (priv->gfile);
    priv->gfile = NULL;
  }

  if (priv->tpfile) {
    empathy_tp_file_close (priv->tpfile);
    g_object_unref (priv->tpfile);
    priv->tpfile = NULL;
  }

  G_OBJECT_CLASS (empathy_ft_handler_parent_class)->dispose (object);
}

static void
do_finalize (GObject *object)
{
  G_OBJECT_CLASS (empathy_ft_handler_parent_class)->finalize (object);
}

static void
empathy_ft_handler_class_init (EmpathyFTHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (EmpathyFTHandlerPrivate));

  object_class->get_property = do_get_property;
  object_class->set_property = do_set_property;
  object_class->dispose = do_dispose;
  object_class->finalize = do_finalize;

  param_spec = g_param_spec_object ("contact",
    "contact", "The remote contact",
    EMPATHY_TYPE_CONTACT,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT, param_spec);

  param_spec = g_param_spec_object ("gfile",
    "gfile", "The GFile we're handling",
    G_TYPE_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_G_FILE, param_spec);

  param_spec = g_param_spec_object ("tp-file",
    "tp-file", "The file's channel wrapper",
    EMPATHY_TYPE_TP_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TP_FILE, param_spec);
}

static void
empathy_ft_handler_init (EmpathyFTHandler *self)
{
  EmpathyFTHandlerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
    EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerPriv);

  self->priv = priv;
}

static void
empathy_ft_handler_contact_ready_cb (EmpathyContact *contact,
                                     const GError *error,
                                     gpointer user_data,
                                     GObject *weak_object);
{
  EmpathyFTHandler *handler = EMPATHY_FT_HANDLER (weak_object);
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  /* start collecting info about the file */
}

/* public methods */

EmpathyFTHandler*
empathy_ft_handler_new (EmpathyContact *contact,
                        GFile *file)
{
  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (EMPATHY_TYPE_FT_HANDLER,
                       "contact", contact, "gfile", file, NULL);
}

EmpathyFTHandler *
empathy_ft_handler_new_for_channel (EmpathyTpFile *file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (file), NULL);

  return g_object_new (EMPATHY_TYPE_FT_HANDLER,
                       "tp-file", file, NULL);
}

void
empathy_ft_handler_start_transfer (EmpathyFTHandler *handler)
{
  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));

  if (priv->tpfile == NULL)
    {
      empathy_contact_call_when_ready (priv->contact,
        EMPATHY_CONTACT_READY_HANDLE,
        empathy_ft_handler_contact_ready_cb, NULL, NULL, G_OBJECT (handler));
    }
  else
    {
      /* TODO: */
    }
}
