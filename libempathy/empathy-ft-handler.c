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

#include <glib.h>
#include <glib/gi18n.h>
#include <telepathy-glib/util.h>

#include "empathy-ft-handler.h"
#include "empathy-tp-contact-factory.h"
#include "empathy-dispatcher.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

G_DEFINE_TYPE (EmpathyFTHandler, empathy_ft_handler, G_TYPE_OBJECT)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyFTHandler)

#define BUFFER_SIZE 4096

enum {
  PROP_TP_FILE = 1,
  PROP_G_FILE,
  PROP_CONTACT,
  PROP_USE_HASH
};

enum {
  HASHING_STARTED,
  HASHING_PROGRESS,
  HASHING_DONE,
  TRANSFER_STARTED,
  TRANSFER_PROGRESS,
  TRANSFER_DONE,
  TRANSFER_ERROR,
  LAST_SIGNAL
};

typedef struct {
  GInputStream *stream;
  GError *error;
  guchar *buffer;
  GChecksum *checksum;
  gssize total_read;
  guint64 total_bytes;
  EmpathyFTHandler *handler;
} HashingData;

typedef struct {
  EmpathyFTHandlerReadyCallback callback;
  gpointer user_data;
  EmpathyFTHandler *handler;
} CallbacksData;

/* private data */
typedef struct {
  gboolean dispose_run;

  GFile *gfile;
  EmpathyTpFile *tpfile;
  GCancellable *cancellable;
  gboolean use_hash;

  /* request for the new transfer */
  GHashTable *request;

  /* transfer properties */
  EmpathyContact *contact;
  gchar *content_type;
  gchar *filename;
  gchar *description;
  guint64 total_bytes;
  guint64 transferred_bytes;
  guint64 mtime;
  gchar *content_hash;
  TpFileHashType content_hash_type;
  TpFileTransferState current_state;

  /* time and speed */
  gdouble speed;
  guint remaining_time;
  time_t last_update_time;

  gboolean is_completed;
} EmpathyFTHandlerPriv;

static guint signals[LAST_SIGNAL] = { 0 };

static gboolean do_hash_job_incoming (GIOSchedulerJob *job,
    GCancellable *cancellable, gpointer user_data);

/* GObject implementations */
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
      case PROP_USE_HASH:
        g_value_set_boolean (value, priv->use_hash);
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
  EmpathyFTHandlerPriv *priv = GET_PRIV (object);

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
      case PROP_USE_HASH:
        priv->use_hash = g_value_get_boolean (value);
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

  if (priv->cancellable) {
    g_object_unref (priv->cancellable);
    priv->cancellable = NULL;
  }

  if (priv->request != NULL)
    {
      g_hash_table_unref (priv->request);
      priv->request = NULL;
    }
  
  G_OBJECT_CLASS (empathy_ft_handler_parent_class)->dispose (object);
}

static void
do_finalize (GObject *object)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (object);

  DEBUG ("%p", object);

  g_free (priv->content_type);
  priv->content_type = NULL;

  g_free (priv->filename);
  priv->filename = NULL;

  g_free (priv->description);
  priv->description = NULL;

  g_free (priv->content_hash);
  priv->content_hash = NULL;

  G_OBJECT_CLASS (empathy_ft_handler_parent_class)->finalize (object);
}

static void
empathy_ft_handler_class_init (EmpathyFTHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (EmpathyFTHandlerPriv));

  object_class->get_property = do_get_property;
  object_class->set_property = do_set_property;
  object_class->dispose = do_dispose;
  object_class->finalize = do_finalize;

  /* properties */
  param_spec = g_param_spec_object ("contact",
    "contact", "The remote contact",
    EMPATHY_TYPE_CONTACT,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_CONTACT, param_spec);

  param_spec = g_param_spec_object ("gfile",
    "gfile", "The GFile we're handling",
    G_TYPE_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_G_FILE, param_spec);

  param_spec = g_param_spec_object ("tp-file",
    "tp-file", "The file's channel wrapper",
    EMPATHY_TYPE_TP_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_TP_FILE, param_spec);

  param_spec = g_param_spec_boolean ("use-hash",
    "use-hash", "Whether we should use checksum when sending or receiving",
    FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_USE_HASH, param_spec);

  /* signals */
  signals[TRANSFER_STARTED] =
    g_signal_new ("transfer-started", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, EMPATHY_TYPE_TP_FILE);

  signals[TRANSFER_DONE] =
    g_signal_new ("transfer-done", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, EMPATHY_TYPE_TP_FILE);

  signals[TRANSFER_ERROR] =
    g_signal_new ("transfer-error", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1, G_TYPE_POINTER);

  signals[TRANSFER_PROGRESS] =
    g_signal_new ("transfer-progress", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _empathy_marshal_VOID__UINT64_UINT64_UINT_DOUBLE,
        G_TYPE_NONE,
        4, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT, G_TYPE_DOUBLE);

  signals[HASHING_STARTED] =
    g_signal_new ("hashing-started", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  signals[HASHING_PROGRESS] =
    g_signal_new ("hashing-progress", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _empathy_marshal_VOID__UINT64_UINT64,
        G_TYPE_NONE,
        2, G_TYPE_UINT64, G_TYPE_UINT64);

  signals[HASHING_DONE] =
    g_signal_new ("hashing-done", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
empathy_ft_handler_init (EmpathyFTHandler *self)
{
  EmpathyFTHandlerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
    EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerPriv);

  self->priv = priv;
  priv->cancellable = g_cancellable_new ();
}

/* private functions */

static void
hash_data_free (HashingData *data)
{
  if (data->buffer != NULL)
    {
      g_free (data->buffer);
      data->buffer = NULL;
    }

  if (data->stream != NULL)
    {
      g_object_unref (data->stream);
      data->stream = NULL;
    }

  if (data->checksum != NULL)
    {
      g_checksum_free (data->checksum);
      data->checksum = NULL;
    }

  if (data->error != NULL)
    {
      g_error_free (data->error);
      data->error = NULL;
    }
  if (data->handler != NULL)
    {
      g_object_unref (data->handler);
      data->handler = NULL;
    }

  g_slice_free (HashingData, data);
}

static GChecksumType
tp_file_hash_to_g_checksum (TpFileHashType type)
{
  GChecksumType retval;

  switch (type)
    {
      case TP_FILE_HASH_TYPE_MD5:
        retval = G_CHECKSUM_MD5;
        break;
      case TP_FILE_HASH_TYPE_SHA1:
        retval = G_CHECKSUM_SHA1;
        break;
      case TP_FILE_HASH_TYPE_SHA256:
        retval = G_CHECKSUM_SHA256;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  return retval;
}

static void
check_hash_incoming (EmpathyFTHandler *handler)
{
  HashingData *hash_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  if (!EMP_STR_EMPTY (priv->content_hash))
    {
      hash_data = g_slice_new0 (HashingData);
      hash_data->total_bytes = priv->total_bytes;
      hash_data->handler = g_object_ref (handler);
      hash_data->checksum = g_checksum_new
        (tp_file_hash_to_g_checksum (priv->content_hash_type));

      g_signal_emit (handler, signals[HASHING_STARTED], 0);

      g_io_scheduler_push_job (do_hash_job_incoming, hash_data, NULL,
                               G_PRIORITY_DEFAULT, priv->cancellable);
    }
}

static void
emit_error_signal (EmpathyFTHandler *handler,
                   const GError *error)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  if (!g_cancellable_is_cancelled (priv->cancellable))
    g_cancellable_cancel (priv->cancellable);

  g_signal_emit (handler, signals[TRANSFER_ERROR], 0, error);
}

static void
ft_transfer_operation_callback (EmpathyTpFile *tp_file,
                                const GError *error,
                                gpointer user_data)
{
  EmpathyFTHandler *handler = user_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  DEBUG ("Transfer operation callback, error %p", error);

  if (error != NULL)
    {
      emit_error_signal (handler, error);
    }
  else 
    {
      priv->is_completed = TRUE;
      g_signal_emit (handler, signals[TRANSFER_DONE], 0, tp_file);

      empathy_tp_file_close (tp_file);

      if (empathy_ft_handler_is_incoming (handler) && priv->use_hash)
        {
          check_hash_incoming (handler);
        }
    }
}

static void
update_remaining_time_and_speed (EmpathyFTHandler *handler,
                                 guint64 transferred_bytes)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  time_t elapsed_time, current_time;
  guint64 transferred, last_transferred_bytes;
  gdouble speed;
  gint remaining_time;

  last_transferred_bytes = priv->transferred_bytes;
  priv->transferred_bytes = transferred_bytes;

  current_time = empathy_time_get_current ();
  elapsed_time = current_time - priv->last_update_time;

  if (elapsed_time >= 1)
    {
      transferred = transferred_bytes - last_transferred_bytes;
      speed = (gdouble) transferred / (gdouble) elapsed_time;
      remaining_time = (priv->total_bytes - transferred) / speed;
      priv->speed = speed;
      priv->remaining_time = remaining_time;
      priv->last_update_time = current_time;
    }
}

static void
ft_transfer_progress_callback (EmpathyTpFile *tp_file,
                               guint64 transferred_bytes,
                               gpointer user_data)
{
  EmpathyFTHandler *handler = user_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  if (empathy_ft_handler_is_cancelled (handler))
    return;

  if (transferred_bytes == 0)
    {
      priv->last_update_time = empathy_time_get_current ();
      g_signal_emit (handler, signals[TRANSFER_STARTED], 0, tp_file);
    }

  if (priv->transferred_bytes != transferred_bytes)
    {
      update_remaining_time_and_speed (handler, transferred_bytes);

      g_signal_emit (handler, signals[TRANSFER_PROGRESS], 0,
          transferred_bytes, priv->total_bytes, priv->remaining_time,
          priv->speed);
    }
}

static void
ft_handler_create_channel_cb (EmpathyDispatchOperation *operation,
                              const GError *error,
                              gpointer user_data)
{
  EmpathyFTHandler *handler = user_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  GError *my_error = (GError *) error;

  DEBUG ("Dispatcher create channel CB");

  if (my_error == NULL)
    {
      g_cancellable_set_error_if_cancelled (priv->cancellable, &my_error);
    }

  if (my_error != NULL)
    {
      emit_error_signal (handler, my_error);

      if (my_error != error)
        g_clear_error (&my_error);

      return;
    }

  priv->tpfile = g_object_ref
      (empathy_dispatch_operation_get_channel_wrapper (operation));

  empathy_tp_file_offer (priv->tpfile, priv->gfile, priv->cancellable,
      ft_transfer_progress_callback, handler,
      ft_transfer_operation_callback, handler);

  empathy_dispatch_operation_claim (operation);
}

static void
ft_handler_push_to_dispatcher (EmpathyFTHandler *handler)
{
  EmpathyDispatcher *dispatcher;
  TpConnection *connection;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  DEBUG ("Pushing request to the dispatcher");

  dispatcher = empathy_dispatcher_dup_singleton ();
  connection = empathy_contact_get_connection (priv->contact);

  /* I want to own a reference to the request, and destroy it later */
  empathy_dispatcher_create_channel (dispatcher, connection,
      g_hash_table_ref (priv->request), ft_handler_create_channel_cb, handler);

  g_object_unref (dispatcher);
}

static gboolean
ft_handler_check_if_allowed (EmpathyFTHandler *handler)
{
  EmpathyDispatcher *dispatcher;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  TpConnection *connection;
  GStrv allowed;
  gboolean res = TRUE;

  dispatcher = empathy_dispatcher_dup_singleton ();
  connection = empathy_contact_get_connection (priv->contact);

  allowed = empathy_dispatcher_find_channel_class (dispatcher, connection,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, TP_HANDLE_TYPE_CONTACT);

  if (!tp_strv_contains ((const gchar * const *) allowed,
      TP_IFACE_CHANNEL ".TargetHandle"))
    res = FALSE;

  g_object_unref (dispatcher);

  return res;
}

static void
ft_handler_populate_outgoing_request (EmpathyFTHandler *handler)
{
  guint contact_handle;
  GHashTable *request;
  GValue *value;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  request = priv->request = g_hash_table_new_full (g_str_hash, g_str_equal,
	    NULL, (GDestroyNotify) tp_g_value_slice_free);

  contact_handle = empathy_contact_get_handle (priv->contact);

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandle */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, contact_handle);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandle", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, priv->content_type);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, priv->filename);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, (guint64) priv->total_bytes);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, (guint64) priv->mtime);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Date", value);
}

static gboolean
hash_job_done (gpointer user_data)
{
  HashingData *hash_data = user_data;
  EmpathyFTHandler *handler = hash_data->handler;
  EmpathyFTHandlerPriv *priv;
  GError *error = NULL;
  GValue *value;

  DEBUG ("Closing stream after hashing.");

  priv = GET_PRIV (handler);

  if (hash_data->error != NULL)
    {
      error = hash_data->error;
      goto cleanup;
    }

  DEBUG ("Got file hash %s", g_checksum_get_string (hash_data->checksum));

  if (empathy_ft_handler_is_incoming (handler))
    {
      if (g_strcmp0 (g_checksum_get_string (hash_data->checksum),
                     priv->content_hash))
        {
          DEBUG ("Hash mismatch when checking incoming handler: "
                 "received %s, calculated %s", priv->content_hash,
                 g_checksum_get_string (hash_data->checksum));

          error = g_error_new_literal (EMPATHY_FT_ERROR_QUARK,
              EMPATHY_FT_ERROR_HASH_MISMATCH,
              _("The hash of the received file and the sent one do not match"));
          goto cleanup;
        }
      else
        {
          DEBUG ("Hash verification matched, received %s, calculated %s",
                 priv->content_hash,
                 g_checksum_get_string (hash_data->checksum));
        }
    }
  else
    {
      /* set the checksum in the request...
       * org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentHash
       */
      value = tp_g_value_slice_new (G_TYPE_STRING);
      g_value_set_string (value, g_checksum_get_string (hash_data->checksum));
      g_hash_table_insert (priv->request,
          TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentHash", value);      
    }

cleanup:

  if (error != NULL)
    {
      emit_error_signal (handler, error);
      g_clear_error (&error);
    }
  else
    {
      g_signal_emit (handler, signals[HASHING_DONE], 0);

      if (!empathy_ft_handler_is_incoming (handler))
        /* the request is complete now, push it to the dispatcher */
        ft_handler_push_to_dispatcher (handler);
    }

  hash_data_free (hash_data);

  return FALSE;
}

static gboolean
emit_hashing_progress (gpointer user_data)
{
  HashingData *hash_data = user_data;

  g_signal_emit (hash_data->handler, signals[HASHING_PROGRESS], 0,
      (guint64) hash_data->total_read, (guint64) hash_data->total_bytes);

  return FALSE;
}

static gboolean
do_hash_job (GIOSchedulerJob *job,
             GCancellable *cancellable,
             gpointer user_data)
{
  HashingData *hash_data = user_data;
  gssize bytes_read;
  EmpathyFTHandlerPriv *priv;
  GError *error = NULL;

  priv = GET_PRIV (hash_data->handler);

again:
  if (hash_data->buffer == NULL)
    hash_data->buffer = g_malloc0 (BUFFER_SIZE);

  bytes_read = g_input_stream_read (hash_data->stream, hash_data->buffer,
                                    BUFFER_SIZE, cancellable, &error);
  if (error != NULL)
    goto out;

  hash_data->total_read += bytes_read;

  /* we now have the chunk */
  if (bytes_read > 0)
    {
      g_checksum_update (hash_data->checksum, hash_data->buffer, bytes_read);
      g_io_scheduler_job_send_to_mainloop_async (job, emit_hashing_progress,
          hash_data, NULL);

      g_free (hash_data->buffer);
      hash_data->buffer = NULL;

      goto again;
    }
  else
  {
    g_input_stream_close (hash_data->stream, cancellable, &error);
  }

out:
  if (error)
    hash_data->error = error;

  g_io_scheduler_job_send_to_mainloop_async (job, hash_job_done,
      hash_data, NULL);

  return FALSE;
}

static gboolean
do_hash_job_incoming (GIOSchedulerJob *job,
                      GCancellable *cancellable,
                      gpointer user_data)
{
  HashingData *hash_data = user_data;
  EmpathyFTHandler *handler = hash_data->handler;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  GError *error = NULL;

  DEBUG ("checking integrity for incoming handler");

  /* need to get the stream first */
  hash_data->stream =
    G_INPUT_STREAM (g_file_read (priv->gfile, cancellable, &error));

  if (error)
    {
      hash_data->error = error;
      g_io_scheduler_job_send_to_mainloop_async (job, hash_job_done,
          hash_data, NULL);
      return FALSE;
    }

  return do_hash_job (job, cancellable, user_data);
}

static void
ft_handler_read_async_cb (GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
  GFileInputStream *stream;
  GError *error = NULL;
  HashingData *hash_data;
  GValue *value;
  EmpathyFTHandler *handler = user_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  DEBUG ("GFile read async CB.");

  stream = g_file_read_finish (priv->gfile, res, &error);
  if (error != NULL)
    {
      emit_error_signal (handler, error);
      g_clear_error (&error);

      return;
    }

  hash_data = g_slice_new0 (HashingData);
  hash_data->stream = G_INPUT_STREAM (stream);
  hash_data->total_bytes = priv->total_bytes;
  hash_data->handler = g_object_ref (handler);
  /* FIXME: should look at the CM capabilities before setting the
   * checksum type?
   */
  hash_data->checksum = g_checksum_new (G_CHECKSUM_MD5);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentHashType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_FILE_HASH_TYPE_MD5);
  g_hash_table_insert (priv->request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentHashType", value);

  g_signal_emit (handler, signals[HASHING_STARTED], 0);

  g_io_scheduler_push_job (do_hash_job, hash_data, NULL,
      G_PRIORITY_DEFAULT, priv->cancellable);
}

static void
ft_handler_complete_request (EmpathyFTHandler *handler)
{ 
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  GError *myerr = NULL;

  /* check if FT is allowed before firing up the I/O machinery */
  if (!ft_handler_check_if_allowed (handler))
    {
      g_set_error_literal (&myerr, EMPATHY_FT_ERROR_QUARK,
          EMPATHY_FT_ERROR_NOT_SUPPORTED,
          _("File transfer not supported by remote contact"));

      emit_error_signal (handler, myerr);
      g_clear_error (&myerr);

      return;
    }

  /* populate the request table with all the known properties */
  ft_handler_populate_outgoing_request (handler);

  if (priv->use_hash)
    /* start hashing the file */
    g_file_read_async (priv->gfile, G_PRIORITY_DEFAULT,
        priv->cancellable, ft_handler_read_async_cb, handler);
  else
    /* push directly the handler to the dispatcher */
    ft_handler_push_to_dispatcher (handler);
}

static void
callbacks_data_free (gpointer user_data)
{
  CallbacksData *data = user_data;

  if (data->handler)
    g_object_unref (data->handler);

  g_slice_free (CallbacksData, data);
}

static void
ft_handler_gfile_ready_cb (GObject *source,
                           GAsyncResult *res,
                           CallbacksData *cb_data)
{
  GFileInfo *info;
  GError *error = NULL;
  GTimeVal mtime;
  EmpathyFTHandlerPriv *priv = GET_PRIV (cb_data->handler);

  DEBUG ("Got GFileInfo.");

  info = g_file_query_info_finish (priv->gfile, res, &error);

  if (error != NULL)
    goto out;

  priv->content_type = g_strdup (g_file_info_get_content_type (info));
  priv->filename = g_strdup (g_file_info_get_display_name (info));
  priv->total_bytes = g_file_info_get_size (info);
  g_file_info_get_modification_time (info, &mtime);
  priv->mtime = mtime.tv_sec;
  priv->transferred_bytes = 0;
  priv->description = NULL;

  g_object_unref (info);

out:
  if (error == NULL)
    {
      cb_data->callback (cb_data->handler, NULL, cb_data->user_data);
    }
  else
    {
      if (!g_cancellable_is_cancelled (priv->cancellable))
        g_cancellable_cancel (priv->cancellable);

      cb_data->callback (NULL, error, cb_data->user_data);
      g_error_free (error);
      g_object_unref (cb_data->handler);
    }

  callbacks_data_free (cb_data);
}

static void 
contact_factory_contact_cb (EmpathyTpContactFactory *factory,
                            EmpathyContact *contact,
                            const GError *error,
                            gpointer user_data,
                            GObject *weak_object)
{
  CallbacksData *cb_data = user_data;
  EmpathyFTHandler *handler = EMPATHY_FT_HANDLER (weak_object);
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  if (error != NULL)
    {
      if (!g_cancellable_is_cancelled (priv->cancellable))
        g_cancellable_cancel (priv->cancellable);

      cb_data->callback (NULL, (GError *) error, cb_data->user_data);
      g_object_unref (handler);
      return;
    }

  priv->contact = contact;

  cb_data->callback (handler, NULL, cb_data->user_data);
}

static void
channel_get_all_properties_cb (TpProxy *proxy,
                               GHashTable *properties,
                               const GError *error,
                               gpointer user_data,
                               GObject *weak_object)
{
  CallbacksData *cb_data = user_data;
  EmpathyFTHandler *handler = EMPATHY_FT_HANDLER (weak_object);
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  EmpathyTpContactFactory *c_factory;
  TpHandle c_handle;

  if (error != NULL)
    {
      if (!g_cancellable_is_cancelled (priv->cancellable))
        g_cancellable_cancel (priv->cancellable);

      cb_data->callback (NULL, (GError *) error, cb_data->user_data);
      g_object_unref (handler);
      return;
    }

  priv->total_bytes = g_value_get_uint64 (
      g_hash_table_lookup (properties, "Size"));

  priv->transferred_bytes = g_value_get_uint64 (
      g_hash_table_lookup (properties, "TransferredBytes"));

  priv->filename = g_value_dup_string (
      g_hash_table_lookup (properties, "Filename"));

  priv->content_hash = g_value_dup_string (
      g_hash_table_lookup (properties, "ContentHash"));

  priv->content_hash_type = g_value_get_uint (
      g_hash_table_lookup (properties, "ContentHashType"));

  priv->content_type = g_value_dup_string (
      g_hash_table_lookup (properties, "ContentType"));

  priv->description = g_value_dup_string (
      g_hash_table_lookup (properties, "Description"));

  g_hash_table_destroy (properties);

  c_factory = empathy_tp_contact_factory_dup_singleton
      (tp_channel_borrow_connection (TP_CHANNEL (proxy)));
  c_handle = tp_channel_get_handle (TP_CHANNEL (proxy), NULL);
  empathy_tp_contact_factory_get_from_handle (c_factory, c_handle,
      contact_factory_contact_cb, cb_data, callbacks_data_free,
      G_OBJECT (handler));
}

/* public methods */

void
empathy_ft_handler_new_outgoing (EmpathyContact *contact,
                                 GFile *source,
                                 gboolean use_hash,
                                 EmpathyFTHandlerReadyCallback callback,
                                 gpointer user_data)
{
  EmpathyFTHandler *handler;
  CallbacksData *data;
  EmpathyFTHandlerPriv *priv;

  DEBUG ("New handler outgoing, use hash %s",
         use_hash ? "True" : "False");

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (G_IS_FILE (source));

  handler = g_object_new (EMPATHY_TYPE_FT_HANDLER,
      "contact", contact, "gfile", source, "use-hash", use_hash, NULL);

  priv = GET_PRIV (handler);

  data = g_slice_new0 (CallbacksData);
  data->callback = callback;
  data->user_data = user_data;
  data->handler = g_object_ref (handler);

  /* start collecting info about the file */
  g_file_query_info_async (priv->gfile,
      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
      G_FILE_ATTRIBUTE_STANDARD_SIZE ","
      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
      G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
      NULL, (GAsyncReadyCallback) ft_handler_gfile_ready_cb, data);
}

void
empathy_ft_handler_new_incoming (EmpathyTpFile *tp_file,
                                 EmpathyFTHandlerReadyCallback callback,
                                 gpointer user_data)
{
  EmpathyFTHandler *handler;
  TpChannel *channel;
  CallbacksData *data;

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  handler = g_object_new (EMPATHY_TYPE_FT_HANDLER,
      "tp-file", tp_file, NULL);

  g_object_get (tp_file, "channel", &channel, NULL);

  data = g_slice_new0 (CallbacksData);
  data->callback = callback;
  data->user_data = user_data;
  data->handler = g_object_ref (handler);

  tp_cli_dbus_properties_call_get_all (channel,
      -1, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
      channel_get_all_properties_cb, data, NULL, G_OBJECT (handler));
}

void
empathy_ft_handler_start_transfer (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));

  priv = GET_PRIV (handler);

  if (priv->tpfile == NULL)
    {
      ft_handler_complete_request (handler);
    }
  else
    {
      /* TODO: add support for resume. */
      empathy_tp_file_accept (priv->tpfile, 0, priv->gfile, priv->cancellable,
          ft_transfer_progress_callback, handler,
          ft_transfer_operation_callback, handler);
    }
}

void
empathy_ft_handler_cancel_transfer (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));

  priv = GET_PRIV (handler);

  /* if we don't have an EmpathyTpFile, we are hashing, so
   * we can just cancel the GCancellable to stop it.
   */
  if (priv->tpfile == NULL)
    g_cancellable_cancel (priv->cancellable);
  else
    empathy_tp_file_cancel (priv->tpfile);
}

void
empathy_ft_handler_incoming_set_destination (EmpathyFTHandler *handler,
                                             GFile *destination,
                                             gboolean use_hash)
{
  DEBUG ("Set incoming destination, use hash %s",
         use_hash ? "True" : "False");

  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));
  g_return_if_fail (G_IS_FILE (destination));

  g_object_set (handler, "gfile", destination,
      "use-hash", use_hash, NULL);
}

const char *
empathy_ft_handler_get_filename (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->filename;
}

const char *
empathy_ft_handler_get_content_type (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->content_type;
}

EmpathyContact *
empathy_ft_handler_get_contact (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->contact;
}

GFile *
empathy_ft_handler_get_gfile (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->gfile;
}

gboolean
empathy_ft_handler_get_use_hash (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return priv->use_hash;
}

gboolean
empathy_ft_handler_is_incoming (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  if (priv->tpfile == NULL)
    return FALSE;

  return empathy_tp_file_is_incoming (priv->tpfile);  
}

guint64
empathy_ft_handler_get_transferred_bytes (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), 0);

  priv = GET_PRIV (handler);

  return priv->transferred_bytes;
}

guint64
empathy_ft_handler_get_total_bytes (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), 0);

  priv = GET_PRIV (handler);

  return priv->total_bytes;
}

gboolean
empathy_ft_handler_is_completed (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return priv->is_completed;
}

gboolean
empathy_ft_handler_is_cancelled (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return g_cancellable_is_cancelled (priv->cancellable);
}