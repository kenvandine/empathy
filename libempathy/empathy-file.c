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

#include "config.h"

#include "empathy-file.h"
#include "empathy-tp-file.h"
#include "empathy-tp-file-priv.h"
#include "empathy-utils.h"
#include "empathy-enum-types.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

/**
 * SECTION:empathy-file
 * @short_description: Transfer files with contacts
 * @see_also: #EmpathyTpFile, #EmpathyContact
 * @include: libempthy/empathy-file.h
 *
 * The #EmpathyFile object represents a single file transfer. Usually file
 * transfers are not created directly, but using empathy_tp_file_offer_file()
 * or an utility function such as empathy_send_file().
 * For incoming file transfers you can get notified of their creation
 * connecting to the "new-file-transfer" signal of #EmpathyTpFile.
 */

/**
 * EMPATHY_FILE_UNKNOWN_SIZE:
 *
 * Value used for the "size" property when the size of the transferred
 * file is unknown.
 */

/* Telepathy does not have file transfer objects, file transfers are identified
 * just with an ID (valid only with the corresponding channel).
 * So EmpathyFile is just a convenience class that contains only an EmpathyTpFile
 * and an ID. Every actual function is delegated to the EmpathyTpFile. */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_FILE, EmpathyFilePriv))

typedef struct _EmpathyFilePriv EmpathyFilePriv;

struct _EmpathyFilePriv
{
  EmpathyTpFile *tp_file;
  guint id;
};

static void empathy_file_class_init (EmpathyFileClass *klass);
static void empathy_file_init (EmpathyFile *file);
static void file_finalize (GObject *object);
static void file_get_property (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec);
static void file_set_property (GObject *object, guint param_id,
    const GValue *value, GParamSpec *pspec);
static void file_set_tp_file (EmpathyFile *file, EmpathyTpFile *tp_file);

enum
{
  PROP_0,
  PROP_TP_FILE,
  PROP_CONTACT,
  PROP_INPUT_STREAM,
  PROP_OUTPUT_STREAM,
  PROP_FILENAME,
  PROP_DIRECTION,
  PROP_STATE,
  PROP_SIZE,
  PROP_TRANSFERRED_BYTES,
  PROP_REMAINING_TIME,
  PROP_STATE_CHANGE_REASON,
};

G_DEFINE_TYPE (EmpathyFile, empathy_file, G_TYPE_OBJECT);

static void
empathy_file_class_init (EmpathyFileClass *klass)
  {
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize     = file_finalize;
    object_class->get_property = file_get_property;
    object_class->set_property = file_set_property;

    g_object_class_install_property (object_class,
        PROP_TP_FILE,
        g_param_spec_object ("tp-file",
            "EmpathyTpFile",
            "The associated EmpathyTpFile",
            EMPATHY_TYPE_TP_FILE,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
        PROP_CONTACT,
        g_param_spec_object ("contact",
            "EmpathyContact",
            "The contact with whom we are transferring the file",
            EMPATHY_TYPE_CONTACT,
            G_PARAM_READABLE));

    g_object_class_install_property (object_class,
        PROP_INPUT_STREAM,
        g_param_spec_object ("input-stream",
            "Input stream",
            "The stream from which to read the data to send",
            G_TYPE_INPUT_STREAM,
            G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_OUTPUT_STREAM,
        g_param_spec_object ("output-stream",
            "Output stream",
            "The stream to which to write the received file",
            G_TYPE_OUTPUT_STREAM,
            G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_FILENAME,
        g_param_spec_string ("filename",
            "File name",
            "The file's name, for displaying",
            NULL,
            G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_DIRECTION,
        g_param_spec_uint ("direction",
            "Transfer direction",
             "The direction of the file transfer",
             0,
             G_MAXUINT,
             G_MAXUINT,
             G_PARAM_READABLE));

    g_object_class_install_property (object_class,
       PROP_STATE,
       g_param_spec_uint ("state",
           "Transfer state",
           "The file transfer state",
           0,
           G_MAXUINT,
           G_MAXUINT,
           G_PARAM_READABLE));

    g_object_class_install_property (object_class,
        PROP_SIZE,
        g_param_spec_uint64 ("size",
            "Size",
            "The file size in bytes",
            0,
            G_MAXUINT64,
            EMPATHY_FILE_UNKNOWN_SIZE,
            G_PARAM_READABLE));

    g_object_class_install_property (object_class,
        PROP_TRANSFERRED_BYTES,
        g_param_spec_uint64 ("transferred-bytes",
            "Transferred bytes",
            "The number of already transferred bytes",
            0,
            G_MAXUINT64,
            0,
            G_PARAM_READABLE));

    g_object_class_install_property (object_class,
        PROP_REMAINING_TIME,
        g_param_spec_int ("remaining-time",
            "Remaining time",
            "The estimated number of remaining seconds before completing the transfer",
            -1,
            G_MAXINT,
            -1,
            G_PARAM_READABLE));

    g_object_class_install_property (object_class,
        PROP_STATE_CHANGE_REASON,
        g_param_spec_uint ("state-change-reason",
            "State change reason",
            "The reason why the file transfer changed state",
            0,
            G_MAXUINT,
            G_MAXUINT,
            G_PARAM_READABLE));

    g_type_class_add_private (object_class, sizeof (EmpathyFilePriv));
}

static void
empathy_file_init (EmpathyFile *file)
{
}

static void
file_finalize (GObject *object)
{
  EmpathyFilePriv *priv;
  EmpathyTpFile *tp_file;

  priv = GET_PRIV (object);
  tp_file = priv->tp_file;

  DEBUG ("finalize: %p", object);

  (G_OBJECT_CLASS (empathy_file_parent_class)->finalize) (object);

  if (tp_file)
    g_object_unref (tp_file);
}

static void
file_get_property (GObject    *object,
     guint       param_id,
     GValue     *value,
     GParamSpec *pspec)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_TP_FILE:
        g_value_set_object (value,
            empathy_file_get_tp_file (EMPATHY_FILE (object)));
        break;
      case PROP_CONTACT:
        g_value_set_object (value,
            empathy_file_get_contact (EMPATHY_FILE (object)));
        break;
      case PROP_INPUT_STREAM:
        g_value_set_object (value,
            empathy_file_get_input_stream (EMPATHY_FILE (object)));
        break;
      case PROP_OUTPUT_STREAM:
        g_value_set_object (value,
            empathy_file_get_output_stream (EMPATHY_FILE (object)));
        break;
      case PROP_FILENAME:
        g_value_set_string (value,
            empathy_file_get_filename (EMPATHY_FILE (object)));
        break;
      case PROP_DIRECTION:
        g_value_set_uint (value,
          empathy_file_get_direction (EMPATHY_FILE (object)));
        break;
      case PROP_STATE:
        g_value_set_uint (value,
          empathy_file_get_state (EMPATHY_FILE (object)));
        break;
      case PROP_SIZE:
        g_value_set_uint64 (value,
            empathy_file_get_size (EMPATHY_FILE (object)));
        break;
      case PROP_TRANSFERRED_BYTES:
        g_value_set_uint64 (value,
            empathy_file_get_transferred_bytes (EMPATHY_FILE (object)));
        break;
      case PROP_REMAINING_TIME:
        g_value_set_int (value,
         empathy_file_get_remaining_time (EMPATHY_FILE (object)));
        break;
      case PROP_STATE_CHANGE_REASON:
        g_value_set_uint (value,
          empathy_file_get_state_change_reason (EMPATHY_FILE (object)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
file_set_property (GObject *object, guint param_id, const GValue *value,
     GParamSpec   *pspec)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_TP_FILE:
        file_set_tp_file (EMPATHY_FILE (object),
            g_value_get_object (value));
        break;
      case PROP_INPUT_STREAM:
        empathy_file_set_input_stream (EMPATHY_FILE (object),
            g_value_get_object (value));
        break;
      case PROP_OUTPUT_STREAM:
        empathy_file_set_output_stream (EMPATHY_FILE (object),
            g_value_get_object (value));
        break;
      case PROP_FILENAME:
        empathy_file_set_filename (EMPATHY_FILE (object),
            g_value_get_string (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

/**
 * empathy_file_new:
 * @tp_file: an EmpathyTpFile
 *
 * Returns a new #EmpathyFile object associated with #EmpathyTpFile.
 *
 * Returns: the associated #EmpathyFile
 */
EmpathyFile *
empathy_file_new (EmpathyTpFile *tp_file)
{
  return g_object_new (EMPATHY_TYPE_FILE,
                       "tp_file", tp_file,
                       NULL);
}

/**
 * empathy_file_get_tp_file:
 * @file: an #EmpathyFile
 *
 * Returns the #EmpathyTpFile associated to this file transfer.
 *
 * Returns: the associated #EmpathyTpFile
 */
EmpathyTpFile *
empathy_file_get_tp_file (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);

  return priv->tp_file;
}

/**
 * empathy_file_get_contact:
 * @file: an #EmpathyFile
 *
 * Returns the contact representing the other participant to the file
 * transfer.
 *
 * Returns: the other contact participating to the file transfer
 */
EmpathyContact *
empathy_file_get_contact (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_contact (priv->tp_file);
}

/**
 * empathy_file_get_input_stream:
 * @file: an #EmpathyFile
 *
 * Returns the #GInputStream from which to read the data to send.
 *
 * Returns: a #GInputStream
 */
GInputStream *
empathy_file_get_input_stream (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_input_stream (priv->tp_file);
}

/**
 * empathy_file_get_output_stream:
 * @file: an #EmpathyFile
 *
 * Returns the #GOutputStream to which to write the received file.
 *
 * Returns: a #GOutputStream
 */
GOutputStream *
empathy_file_get_output_stream (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_output_stream (priv->tp_file);
}

/**
 * empathy_file_get_filename:
 * @file: an #EmpathyFile
 *
 * Returns the UTF-8 encoded file's friendly name without path, for
 * displaying it to the user.
 *
 * Returns: the file name
 */
const gchar *
empathy_file_get_filename (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_filename (priv->tp_file);
}

/**
 * empathy_file_get_direction:
 * @file: an #EmpathyFile
 *
 * Returns the direction of the file transfer, i.e. whether the transfer is
 * incoming or outgoing.
 *
 * Returns: the direction of the file transfer
 */
EmpFileTransferDirection
empathy_file_get_direction (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), 0);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_direction (priv->tp_file);
}

/**
 * empathy_file_get_state:
 * @file: an #EmpathyFile
 *
 * Returns the file transfer current state.
 *
 * Returns: the file transfer state
 */
EmpFileTransferState
empathy_file_get_state (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file),
            EMP_FILE_TRANSFER_STATE_CANCELED);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_state (priv->tp_file);
}

/**
 * empathy_file_get_state_change_reason:
 * @file: an #EmpathyFile
 *
 * Returns the reason of the last file transfer state change.
 *
 * Returns: why the file transfer changed state
 */
EmpFileTransferStateChangeReason
empathy_file_get_state_change_reason (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file),
            EMP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_state_change_reason (priv->tp_file);
}

/**
 * empathy_file_get_size:
 * @file: an #EmpathyFile
 *
 * Returns the file size in bytes or #EMPATHY_FILE_UNKNOWN_SIZE if the size is
 * not known.
 *
 * Returns: the file size in bytes or #EMPATHY_FILE_UNKNOWN_SIZE
 */
guint64
empathy_file_get_size (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), EMPATHY_FILE_UNKNOWN_SIZE);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_size (priv->tp_file);
}

/**
 * empathy_file_get_transferred_bytes:
 * @file: an #EmpathyFile
 *
 * Returns the number of already transferred bytes.
 *
 * Returns: the number of transferred bytes
 */
guint64
empathy_file_get_transferred_bytes (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), 0);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_transferred_bytes (priv->tp_file);
}

/**
 * empathy_file_get_remaining_time:
 * @file: an #EmpathyFile
 *
 * Returns the estimated number of remaining seconds before completing the
 * file transfer, or -1 if it cannot be estimated.
 *
 * Returns: the estimated number of remaining seconds or -1
 */
gint
empathy_file_get_remaining_time (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), -1);

  priv = GET_PRIV (file);
  return _empathy_tp_file_get_remaining_time (priv->tp_file);
}

static void
file_set_tp_file (EmpathyFile   *file,
        EmpathyTpFile *tp_file)
{
  EmpathyFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_FILE (file));
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  priv = GET_PRIV (file);

  priv->tp_file = g_object_ref (tp_file);
}

/**
 * empathy_file_set_input_stream:
 * @file: an #EmpathyFile
 * @in_stream: the #GInputStream
 *
 * Sets the #GInputStream from which to read the data to send.
 */
void
empathy_file_set_input_stream (EmpathyFile    *file,
           GInputStream *in_stream)
{
  EmpathyFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_FILE (file));

  priv = GET_PRIV (file);

  _empathy_tp_file_set_input_stream (priv->tp_file, in_stream);
}

/**
 * empathy_file_set_output_stream:
 * @file: an #EmpathyFile
 * @out_stream: the #GOutputStream
 *
 * Sets the #GOutputStream to which to write the received file.
 */
void
empathy_file_set_output_stream (EmpathyFile     *file,
            GOutputStream *out_stream)
{
  EmpathyFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_FILE (file));

  priv = GET_PRIV (file);

  _empathy_tp_file_set_output_stream (priv->tp_file, out_stream);
}

/**
 * empathy_file_set_filename:
 * @file: an #EmpathyFile
 * @filename: the file name
 *
 * Sets the UTF-8 encoded file's friendly name without path, for
 * displaying it to the user.
 */
void
empathy_file_set_filename (EmpathyFile   *file,
       const gchar *filename)
{
  EmpathyFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_FILE (file));
  g_return_if_fail (filename != NULL);

  priv = GET_PRIV (file);

  _empathy_tp_file_set_filename (priv->tp_file, filename);
}

/**
 * empathy_file_accept:
 * @file: an #EmpathyFile
 *
 * Accepts a file transfer that's in the "local pending" state (i.e.
 * EMP_FILE_TRANSFER_STATE_LOCAL_PENDING).
 */
void
empathy_file_accept (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_FILE (file));

  priv = GET_PRIV (file);

/*  _empathy_tp_file_accept (priv->tp_file);*/
  if (priv) ;
}
