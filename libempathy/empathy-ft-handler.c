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

#include <extensions/extensions.h>
#include <glib.h>
#include <telepathy-glib/util.h>

#include "empathy-ft-handler.h"
#include "empathy-dispatcher.h"
#include "empathy-utils.h"

G_DEFINE_TYPE (EmpathyFTHandler, empathy_ft_handler, G_TYPE_OBJECT)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyFTHandler)

#define BUFFER_SIZE 4096

enum {
  PROP_TP_FILE = 1,
  PROP_G_FILE,
  PROP_CONTACT
};

typedef struct {
  EmpathyFTHandler *handler;
  GFile *gfile;
  GHashTable *request;
} RequestData;

typedef struct {
  RequestData *req_data;
  GInputStream *stream;
  gboolean done_reading;
  GError *error;
  guchar *buffer;
  GChecksum *checksum;
} HashingData;

/* private data */
typedef struct {
  gboolean dispose_run;
  EmpathyContact *contact;
  GFile *gfile;
  EmpathyTpFile *tpfile;
} EmpathyFTHandlerPriv;

/* prototypes */
static void schedule_hash_chunk (HashingData *hash_data);

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

  g_type_class_add_private (klass, sizeof (EmpathyFTHandlerPriv));

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

  g_slice_free (HashingData, data);
}

static void
request_data_free (RequestData *data)
{
  if (data->gfile != NULL)
    {
      g_object_unref (data->gfile);
      data->gfile = NULL;
    }

  if (data->request != NULL)
    {
      g_hash_table_unref (data->request);
      data->request = NULL;
    }

  g_slice_free (RequestData, data);
}

static RequestData *
request_data_new (EmpathyFTHandler *handler, GFile *gfile)
{
  RequestData *ret;

  ret = g_slice_new0 (RequestData);
  ret->request = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  ret->handler = g_object_ref (handler);
  ret->gfile = g_object_ref (gfile);

  return ret;
}

static void
ft_handler_create_channel_cb (EmpathyDispatchOperation *operation,
                              const GError *error,
                              gpointer user_data)
{
  RequestData *req_data = user_data;
  GError *myerr = NULL;
  EmpathyFTHandlerPriv *priv = GET_PRIV (req_data->handler);

  if (error != NULL)
    {
      /* TODO: error handling */
      goto out;
    }

  priv->tpfile = g_object_ref
      (empathy_dispatch_operation_get_channel_wrapper (operation));
  empathy_tp_file_offer (priv->tpfile, req_data->gfile, &myerr);
  empathy_dispatch_operation_claim (operation);

out:
  request_data_free (req_data);
}

static void
ft_handler_push_to_dispatcher (RequestData *req_data)
{
  EmpathyDispatcher *dispatcher;
  McAccount *account;
  EmpathyFTHandlerPriv *priv = GET_PRIV (req_data->handler);

  dispatcher = empathy_dispatcher_dup_singleton ();
  account = empathy_contact_get_account (priv->contact);

  empathy_dispatcher_create_channel (dispatcher, account, req_data->request,
      ft_handler_create_channel_cb, req_data);

  g_object_unref (dispatcher);
}

static gboolean
ft_handler_check_if_allowed (EmpathyFTHandler *handler)
{
  EmpathyDispatcher *dispatcher;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);
  McAccount *account;
  GStrv allowed;
  gboolean res = TRUE;

  dispatcher = empathy_dispatcher_dup_singleton ();
  account = empathy_contact_get_account (priv->contact);

  allowed = empathy_dispatcher_find_channel_class (dispatcher, account,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, TP_HANDLE_TYPE_CONTACT);

  if (!tp_strv_contains ((const gchar * const *) allowed,
      TP_IFACE_CHANNEL ".TargetHandle"))
    res = FALSE;

  g_object_unref (dispatcher);

  return res;
}

static void
ft_handler_populate_outgoing_request (RequestData *req_data,
                                      GFileInfo *file_info)
{
  guint contact_handle;
  const char *content_type;
  const char *display_name;
  goffset size;
  GTimeVal mtime;
  GValue *value;
  GHashTable *request = req_data->request;
  EmpathyFTHandlerPriv *priv = GET_PRIV (req_data->handler);

  /* gather all the information */
  contact_handle = empathy_contact_get_handle (priv->contact);

  content_type = g_file_info_get_content_type (file_info);
  display_name = g_file_info_get_display_name (file_info);
  size = g_file_info_get_size (file_info);
  g_file_info_get_modification_time (file_info, &mtime);

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
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
  g_value_set_string (value, content_type);
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, display_name);
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, (guint64) size);
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, (guint64) mtime.tv_sec);
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Date", value);
}

static void
hash_job_async_close_stream_cb (GObject *source,
                                GAsyncResult *res,
                                gpointer user_data)
{
  HashingData *hash_data = user_data;
  RequestData *req_data = hash_data->req_data;
  GError *error = NULL;
  GValue *value;
  GHashTable *request;

  /* if we're here we for sure have done reading, check if we stopped due
   * to an error.
   */
  g_input_stream_close_finish (hash_data->stream, res, &error);
  if (error != NULL)
    {
      if (hash_data->error != NULL)
        {
          /* if we already stopped due to an error, probably we're completely
           * hosed for some reason. just return the first read error
           * to the user.
           */
          g_clear_error (&error);
          error = hash_data->error;
        }

      goto cleanup;
    }

  if (hash_data->error != NULL)
    {
      error = hash_data->error;
      goto cleanup;
    }

  /* set the checksum in the request */
  request = req_data->request;

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentHash */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, g_checksum_get_string (hash_data->checksum));
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentHash", value);

cleanup:
  hash_data_free (hash_data);

  if (error != NULL)
    {
      /* TODO: error handling. */
    }
  else
    {
      /* the request is complete now, push it to the dispatcher */
      ft_handler_push_to_dispatcher (req_data);
    }
}

static void
hash_job_async_read_cb (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  HashingData *hash_data = user_data;
  gssize bytes_read;
  GError *error = NULL;

  bytes_read = g_input_stream_read_finish (hash_data->stream, res, &error);
  if (error != NULL)
    {
      hash_data->error = error;
      hash_data->done_reading = TRUE;
      goto out;
    }

  /* TODO: notify progress */

  /* we now have the chunk */
  if (bytes_read == 0)
    {
      hash_data->done_reading = TRUE;
      schedule_hash_chunk (hash_data);
      goto out;
    }
  else
    {
      g_checksum_update (hash_data->checksum, hash_data->buffer, bytes_read);
    }

out:
  schedule_hash_chunk (hash_data);
}

static void
schedule_hash_chunk (HashingData *hash_data)
{
  if (hash_data->done_reading)
    {
      g_input_stream_close_async (hash_data->stream, G_PRIORITY_DEFAULT,
          NULL, hash_job_async_close_stream_cb, hash_data);
    }
  else
    {
      if (hash_data->buffer != NULL)
        {
          g_free (hash_data->buffer);
          hash_data->buffer = g_malloc0 (BUFFER_SIZE);
        }

      g_input_stream_read_async (hash_data->stream, hash_data->buffer,
          BUFFER_SIZE, G_PRIORITY_DEFAULT, NULL,
          hash_job_async_read_cb, hash_data);
    }
}

static void
ft_handler_read_async_cb (GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
  GFileInputStream *stream;
  GError *error = NULL;
  HashingData *hash_data;
  GHashTable *request;
  GValue *value;
  RequestData *req_data = user_data;

  stream = g_file_read_finish (req_data->gfile, res, &error);
  if (error != NULL)
    {
      /* TODO: error handling. */
      return;
    }

  hash_data = g_slice_new0 (HashingData);
  hash_data->stream = G_INPUT_STREAM (stream);
  hash_data->done_reading = FALSE;
  hash_data->req_data = req_data;
  hash_data->error = NULL;
  /* FIXME: should look at the CM capabilities before setting the
   * checksum type?
   */
  hash_data->checksum = g_checksum_new (G_CHECKSUM_MD5);

  request = req_data->request;

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentHashType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, EMP_FILE_HASH_TYPE_MD5);
  g_hash_table_insert (request,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentHashType", value);

  schedule_hash_chunk (hash_data);
}

static void
ft_handler_gfile_ready_cb (GObject *source,
                           GAsyncResult *res,
                           RequestData *req_data)
{
  GFileInfo *info;
  GError *error = NULL;

  info = g_file_query_info_finish (req_data->gfile, res, &error);
  if (error != NULL)
    {
      /* TODO: error handling. */
      return;
    }

  ft_handler_populate_outgoing_request (req_data, info);

  /* now start hashing the file */
  g_file_read_async (req_data->gfile, G_PRIORITY_DEFAULT,
      NULL, ft_handler_read_async_cb, req_data);
}

static void
ft_handler_contact_ready_cb (EmpathyContact *contact,
                             const GError *error,
                             gpointer user_data,
                             GObject *weak_object)  
{
  RequestData *req_data = user_data;
  EmpathyFTHandlerPriv *priv = GET_PRIV (req_data->handler);

  g_assert (priv->contact != NULL);
  g_assert (priv->gfile != NULL);

  /* check if FT is allowed before firing up the I/O machinery */
  if (!ft_handler_check_if_allowed (req_data->handler))
    {
      /* TODO: error handling. */
      request_data_free (req_data);
      return;
    }

  /* start collecting info about the file */
  g_file_query_info_async (req_data->gfile,
      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
      G_FILE_ATTRIBUTE_STANDARD_SIZE ","
      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
      G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
      NULL, (GAsyncReadyCallback) ft_handler_gfile_ready_cb,
      req_data);
}

/* public methods */

EmpathyFTHandler*
empathy_ft_handler_new_outgoing (EmpathyContact *contact,
                                 GFile *source)
{
  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (G_IS_FILE (source), NULL);

  return g_object_new (EMPATHY_TYPE_FT_HANDLER,
      "contact", contact, "gfile", source, NULL);
}

EmpathyFTHandler *
empathy_ft_handler_new_incoming (EmpathyTpFile *tp_file,
                                 GFile *destination)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  g_return_val_if_fail (G_IS_FILE (destination), NULL);

  return g_object_new (EMPATHY_TYPE_FT_HANDLER,
      "tp-file", tp_file, "gfile", destination, NULL);
}

void
empathy_ft_handler_start_transfer (EmpathyFTHandler *handler)
{
  RequestData *data;
  EmpathyFTHandlerPriv *priv;
  GError *error = NULL;

  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));

  priv = GET_PRIV (handler);

  if (priv->tpfile == NULL)
    {
      data = request_data_new (handler, priv->gfile);
      empathy_contact_call_when_ready (priv->contact,
          EMPATHY_CONTACT_READY_HANDLE,
          ft_handler_contact_ready_cb, data, NULL, G_OBJECT (handler));
    }
  else
    {
      /* TODO: add support for resume. */
      empathy_tp_file_accept (priv->tpfile, 0, priv->gfile, &error);
    }
}
