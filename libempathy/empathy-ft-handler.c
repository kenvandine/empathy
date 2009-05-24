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

/**
 * SECTION:empathy-ft-handler
 * @title: EmpathyFTHandler
 * @short_description: an object representing a File Transfer
 * @include: libempathy/empathy-ft-handler
 *
 * #EmpathyFTHandler is the object which represents a File Transfer with all
 * its properties.
 * The creation of an #EmpathyFTHandler is done with
 * empathy_ft_handler_new_outgoing() or empathy_ft_handler_new_incoming(),
 * even though clients should not need to call them directly, as
 * #EmpathyFTFactory does it for them. Remember that for the file transfer
 * to work with an incoming handler,
 * empathy_ft_handler_incoming_set_destination() should be called after
 * empathy_ft_handler_new_incoming(). #EmpathyFTFactory does this
 * automatically.
 * It's important to note that, as the creation of the handlers is async, once
 * an handler is created, it already has all the interesting properties set,
 * like filename, total bytes, content type and so on, making it useful
 * to be displayed in an UI.
 * The transfer API works like a state machine; it has three signals,
 * ::transfer-started, ::transfer-progress, ::transfer-done, which will be
 * emitted in the relevant phases.
 * In addition, if the handler is created with checksumming enabled,
 * other three signals (::hashing-started, ::hashing-progress, ::hashing-done)
 * will be emitted before or after the transfer, depending on the direction
 * (respectively outgoing and incoming) of the handler.
 * At any time between the call to empathy_ft_handler_start_transfer() and
 * the last signal, a ::transfer-error can be emitted, indicating that an
 * error has happened in the operation. The message of the error is localized
 * to use in an UI.
 */

G_DEFINE_TYPE (EmpathyFTHandler, empathy_ft_handler, G_TYPE_OBJECT)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyFTHandler)

#define BUFFER_SIZE 4096

enum {
  PROP_TP_FILE = 1,
  PROP_G_FILE,
  PROP_CONTACT,
  PROP_CONTENT_TYPE,
  PROP_DESCRIPTION,
  PROP_FILENAME,
  PROP_MODIFICATION_TIME,
  PROP_TOTAL_BYTES,
  PROP_TRANSFERRED_BYTES,
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
  GError *error /* comment to make the style checker happy */;
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

  EmpathyDispatcher *dispatcher;

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
      case PROP_CONTENT_TYPE:
        g_value_set_string (value, priv->content_type);
        break;
      case PROP_DESCRIPTION:
        g_value_set_string (value, priv->description);
        break;
      case PROP_FILENAME:
        g_value_set_string (value, priv->filename);
        break;
      case PROP_MODIFICATION_TIME:
        g_value_set_uint64 (value, priv->mtime);
        break;
      case PROP_TOTAL_BYTES:
        g_value_set_uint64 (value, priv->total_bytes);
        break;
      case PROP_TRANSFERRED_BYTES:
        g_value_set_uint64 (value, priv->transferred_bytes);
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
      case PROP_CONTENT_TYPE:
        priv->content_type = g_value_dup_string (value);
        break;
      case PROP_DESCRIPTION:
        priv->description = g_value_dup_string (value);
        break;
      case PROP_FILENAME:
        priv->filename = g_value_dup_string (value);
        break;
      case PROP_MODIFICATION_TIME:
        priv->mtime = g_value_get_uint64 (value);
        break;
      case PROP_TOTAL_BYTES:
        priv->total_bytes = g_value_get_uint64 (value);
        break;
      case PROP_TRANSFERRED_BYTES:
        priv->transferred_bytes = g_value_get_uint64 (value);
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

  if (priv->contact != NULL) {
    g_object_unref (priv->contact);
    priv->contact = NULL;
  }

  if (priv->gfile != NULL) {
    g_object_unref (priv->gfile);
    priv->gfile = NULL;
  }

  if (priv->tpfile != NULL) {
    empathy_tp_file_close (priv->tpfile);
    g_object_unref (priv->tpfile);
    priv->tpfile = NULL;
  }

  if (priv->cancellable != NULL) {
    g_object_unref (priv->cancellable);
    priv->cancellable = NULL;
  }

  if (priv->request != NULL)
    {
      g_hash_table_unref (priv->request);
      priv->request = NULL;
    }

  if (priv->dispatcher != NULL)
    {
      g_object_unref (priv->dispatcher);
      priv->dispatcher = NULL;
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

  /**
   * EmpathyFTHandler:contact:
   *
   * The remote #EmpathyContact for the transfer
   */
  param_spec = g_param_spec_object ("contact",
    "contact", "The remote contact",
    EMPATHY_TYPE_CONTACT,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_CONTACT, param_spec);

  /**
   * EmpathyFTHandler:content-type:
   *
   * The content type of the file being transferred
   */
  param_spec = g_param_spec_string ("content-type",
    "content-type", "The content type of the file", NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_CONTENT_TYPE, param_spec);

  /**
   * EmpathyFTHandler:description:
   *
   * The description of the file being transferred
   */
  param_spec = g_param_spec_string ("description",
    "description", "The description of the file", NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_DESCRIPTION, param_spec);

  /**
   * EmpathyFTHandler:filename:
   *
   * The name of the file being transferred
   */
  param_spec = g_param_spec_string ("filename",
    "filename", "The name of the file", NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_FILENAME, param_spec);

  /**
   * EmpathyFTHandler:modification-time:
   *
   * The modification time of the file being transferred
   */
  param_spec = g_param_spec_uint64 ("modification-time",
    "modification-time", "The mtime of the file", 0,
    G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_MODIFICATION_TIME, param_spec);

  /**
   * EmpathyFTHandler:total-bytes:
   *
   * The size (in bytes) of the file being transferred
   */
  param_spec = g_param_spec_uint64 ("total-bytes",
    "total-bytes", "The size of the file", 0,
    G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_TOTAL_BYTES, param_spec);

  /**
   * EmpathyFTHandler:transferred-bytes:
   *
   * The number of the bytes already transferred
   */
  param_spec = g_param_spec_uint64 ("transferred-bytes",
    "transferred-bytes", "The number of bytes already transferred", 0,
    G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_TRANSFERRED_BYTES, param_spec);

  /**
   * EmpathyFTHandler:gfile:
   *
   * The #GFile object where the transfer actually happens
   */
  param_spec = g_param_spec_object ("gfile",
    "gfile", "The GFile we're handling",
    G_TYPE_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_G_FILE, param_spec);

  /**
   * EmpathyFTHandler:tp-file:
   *
   * The underlying #EmpathyTpFile managing the transfer
   */
  param_spec = g_param_spec_object ("tp-file",
    "tp-file", "The file's channel wrapper",
    EMPATHY_TYPE_TP_FILE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_TP_FILE, param_spec);

  /**
   * EmpathyFTHandler:use-hash:
   *
   * %TRUE if checksumming is enabled for the handler, %FALSE otherwise
   */
  param_spec = g_param_spec_boolean ("use-hash",
    "use-hash", "Whether we should use checksum when sending or receiving",
    FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_USE_HASH, param_spec);

  /* signals */

  /**
   * EmpathyFTHandler::transfer-started
   * @handler: the object which has received the signal
   * @tp_file: the #EmpathyTpFile for which the transfer has started
   *
   * This signal is emitted when the actual transfer starts.
   */
  signals[TRANSFER_STARTED] =
    g_signal_new ("transfer-started", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, EMPATHY_TYPE_TP_FILE);

  /**
   * EmpathyFTHandler::transfer-done
   * @handler: the object which has received the signal
   * @tp_file: the #EmpathyTpFile for which the transfer has started
   *
   * This signal will be emitted when the actual transfer is completed
   * successfully.
   */
  signals[TRANSFER_DONE] =
    g_signal_new ("transfer-done", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, EMPATHY_TYPE_TP_FILE);

  /**
   * EmpathyFTHandler::transfer-error
   * @handler: the object which has received the signal
   * @error: a #GError
   *
   * This signal can be emitted anytime between the call to
   * empathy_ft_handler_start_transfer() and the last expected signal
   * (::transfer-done or ::hashing-done), and it's guaranteed to be the last
   * signal coming from the handler, meaning that no other operation will
   * take place after this signal.
   */
  signals[TRANSFER_ERROR] =
    g_signal_new ("transfer-error", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1, G_TYPE_POINTER);

  /**
   * EmpathyFTHandler::transfer-progress
   * @handler: the object which has received the signal
   * @current_bytes: the bytes currently transferred
   * @total_bytes: the total bytes of the handler
   * @remaining_time: the number of seconds remaining for the transfer
   * to be completed
   * @speed: the current speed of the transfer (in KB/s)
   *
   * This signal is emitted to notify clients of the progress of the
   * transfer.
   */
  signals[TRANSFER_PROGRESS] =
    g_signal_new ("transfer-progress", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _empathy_marshal_VOID__UINT64_UINT64_UINT_DOUBLE,
        G_TYPE_NONE,
        4, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT, G_TYPE_DOUBLE);

  /**
   * EmpathyFTHandler::hashing-started
   * @handler: the object which has received the signal
   *
   * This signal is emitted when the hashing operation of the handler
   * is started. Note that this only happens if the handler is created
   * with checksum enabled and, even if the option is set, is not
   * guaranteed to happen for incoming handlers, as the CM might not
   * support sending/receiving the file hash. You can use
   * empathy_ft_handler_get_use_hash() to find out whether the handler really
   * supports checksum.
   */
  signals[HASHING_STARTED] =
    g_signal_new ("hashing-started", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  /**
   * EmpathyFTHandler::hashing-progress
   * @handler: the object which has received the signal
   * @current_bytes: the bytes currently hashed
   * @total_bytes: the total bytes of the handler
   *
   * This signal is emitted to notify clients of the progress of the
   * hashing operation.
   */
  signals[HASHING_PROGRESS] =
    g_signal_new ("hashing-progress", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _empathy_marshal_VOID__UINT64_UINT64,
        G_TYPE_NONE,
        2, G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * EmpathyFTHandler::hashing-done
   * @handler: the object which has received the signal
   *
   * This signal is emitted when the hashing operation of the handler
   * is completed.
   */
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
  priv->dispatcher = empathy_dispatcher_dup_singleton ();
}

/* private functions */

static void
hash_data_free (HashingData *data)
{
  g_free (data->buffer);

  if (data->stream != NULL)
    g_object_unref (data->stream);

  if (data->checksum != NULL)
    g_checksum_free (data->checksum);

  if (data->error != NULL)
    g_error_free (data->error);

  if (data->handler != NULL)
    g_object_unref (data->handler);

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
      remaining_time = (priv->total_bytes - priv->transferred_bytes) / speed;
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
  TpConnection *connection;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

  DEBUG ("Pushing request to the dispatcher");

  connection = empathy_contact_get_connection (priv->contact);

  /* I want to own a reference to the request, and destroy it later */
  empathy_dispatcher_create_channel (priv->dispatcher, connection,
      g_hash_table_ref (priv->request), ft_handler_create_channel_cb, handler);
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
  value = tp_g_value_slice_new_string (TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new_uint (TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandle */
  value = tp_g_value_slice_new_uint (contact_handle);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandle", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentType */
  value = tp_g_value_slice_new_string (priv->content_type);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
  value = tp_g_value_slice_new_string (priv->filename);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
  value = tp_g_value_slice_new_uint64 (priv->total_bytes);
  g_hash_table_insert (request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
  value = tp_g_value_slice_new_uint64 ((guint64) priv->mtime);
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
              _("The hash of the received file and the "
                "sent one do not match"));
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
      value = tp_g_value_slice_new_string
          (g_checksum_get_string (hash_data->checksum));
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
  if (error != NULL)
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

  if (error != NULL)
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
  /* FIXME: MD5 is the only ContentHashType supported right now */
  hash_data->checksum = g_checksum_new (G_CHECKSUM_MD5);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentHashType */
  value = tp_g_value_slice_new_uint (TP_FILE_HASH_TYPE_MD5);
  g_hash_table_insert (priv->request,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentHashType", value);

  g_signal_emit (handler, signals[HASHING_STARTED], 0);

  g_io_scheduler_push_job (do_hash_job, hash_data, NULL,
      G_PRIORITY_DEFAULT, priv->cancellable);
}

static void
callbacks_data_free (gpointer user_data)
{
  CallbacksData *data = user_data;

  if (data->handler != NULL)
    g_object_unref (data->handler);

  g_slice_free (CallbacksData, data);
}

static void
find_ft_channel_class_cb (GStrv channel_class,
    gpointer user_data)
{
  CallbacksData *data = user_data;
  EmpathyFTHandler *handler = data->handler;  
  gboolean allowed = TRUE;
  GError *myerr = NULL;

  /* this takes care of channel_class == NULL as well */
  if (!tp_strv_contains ((const gchar * const *) channel_class,
      TP_IFACE_CHANNEL ".TargetHandle"))
    allowed = FALSE;

  DEBUG ("check if FT is allowed: %s", allowed ? "True" : "False");

  if (!allowed)
    {
      g_set_error_literal (&myerr, EMPATHY_FT_ERROR_QUARK,
          EMPATHY_FT_ERROR_NOT_SUPPORTED,
          _("File transfer not supported by remote contact"));

      data->callback (NULL, myerr, data->user_data);
      g_clear_error (&myerr);
    }
  else
    {
      data->callback (handler, NULL, data->user_data);
    }

  callbacks_data_free (data);
}

static void
find_hash_channel_class_cb (GStrv channel_class,
    gpointer user_data)
{
  CallbacksData *data = user_data;
  EmpathyFTHandler *handler = data->handler;
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);  
  gboolean allowed = TRUE;

  /* this takes care of channel_class == NULL as well */
  if (!tp_strv_contains ((const gchar * const *) channel_class,
      TP_IFACE_CHANNEL ".TargetHandle"))
    allowed = FALSE;

  DEBUG ("check if FT+hash is allowed: %s", allowed ? "True" : "False");

  if (!allowed)
    {
      priv->use_hash = FALSE;

      /* see if we support FT without hash instead */
      empathy_dispatcher_find_requestable_channel_classes_async
          (priv->dispatcher, empathy_contact_get_connection (priv->contact),
           TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, TP_HANDLE_TYPE_CONTACT,
           find_ft_channel_class_cb, data, NULL);

      return;
    }
  else
    {
      data->callback (handler, NULL, data->user_data);
      callbacks_data_free (data);
    }
}

static void
ft_handler_complete_request (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv = GET_PRIV (handler);

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
  if (error != NULL)
    {
      if (!g_cancellable_is_cancelled (priv->cancellable))
        g_cancellable_cancel (priv->cancellable);

      cb_data->callback (NULL, error, cb_data->user_data);
      g_error_free (error);
      g_object_unref (cb_data->handler);

      callbacks_data_free (cb_data);
    }
  else
    {
      /* see if FT/hashing are allowed */
      empathy_dispatcher_find_requestable_channel_classes_async
          (priv->dispatcher, empathy_contact_get_connection (priv->contact),
           TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, TP_HANDLE_TYPE_CONTACT,
           find_hash_channel_class_cb, cb_data, "ContentHashType", NULL);
    }
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

  c_factory = empathy_tp_contact_factory_dup_singleton
      (tp_channel_borrow_connection (TP_CHANNEL (proxy)));
  c_handle = tp_channel_get_handle (TP_CHANNEL (proxy), NULL);
  empathy_tp_contact_factory_get_from_handle (c_factory, c_handle,
      contact_factory_contact_cb, cb_data, callbacks_data_free,
      G_OBJECT (handler));

  g_object_unref (c_factory);
}

/* public methods */

/**
 * empathy_ft_handler_new_outgoing:
 * @contact: the #EmpathyContact to send @source to
 * @source: the #GFile to send
 * @use_hash: whether the handler should send a checksum of the file
 * @callback: callback to be called when the handler has been created
 * @user_data: user data to be passed to @callback
 *
 * Triggers the creation of a new #EmpathyFTHandler for an outgoing transfer.
 */
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

/**
 * empathy_ft_handler_new_incoming:
 * @tp_file: the #EmpathyTpFile wrapping the incoming channel
 * @callback: callback to be called when the handler has been created
 * @user_data: user data to be passed to @callback
 *
 * Triggers the creation of a new #EmpathyFTHandler for an incoming transfer.
 * Note that for the handler to be useful, you will have to set a destination
 * file with empathy_ft_handler_incoming_set_destination() after the handler
 * is ready.
 */
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

/**
 * empathy_ft_handler_start_transfer:
 * @handler: an #EmpathyFTHandler
 *
 * Starts the transfer machinery. After this call, the transfer and hashing
 * signals will be emitted by the handler.
 */
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

/**
 * empathy_ft_handler_cancel_transfer:
 * @handler: an #EmpathyFTHandler
 *
 * Cancels an ongoing handler operation. Note that this doesn't destroy
 * the object, which will keep all the properties, altough it won't be able
 * to do any more I/O.
 */
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

/**
 * empathy_ft_handler_incoming_set_destination:
 * @handler: an #EmpathyFTHandler
 * @destination: the #GFile where the transfer should be saved
 * @use_hash: whether the handler should, after the transfer, try to
 * validate it with checksum.
 *
 * Sets the destination of the incoming handler to be @destination.
 * Note that calling this method is mandatory before starting the transfer
 * for incoming handlers.
 */
void
empathy_ft_handler_incoming_set_destination (EmpathyFTHandler *handler,
    GFile *destination,
    gboolean use_hash)
{
  EmpathyFTHandlerPriv *priv;

  DEBUG ("Set incoming destination, use hash %s",
         use_hash ? "True" : "False");

  g_return_if_fail (EMPATHY_IS_FT_HANDLER (handler));
  g_return_if_fail (G_IS_FILE (destination));

  priv = GET_PRIV (handler);

  g_object_set (handler, "gfile", destination,
      "use-hash", use_hash, NULL);

  /* check if hash is really supported. if it isn't, set use_hash to FALSE
   * anyway, so that clients won't be expecting us to checksum.
   */
  if (EMP_STR_EMPTY (priv->content_hash) ||
      priv->content_hash_type == TP_FILE_HASH_TYPE_NONE)
    priv->use_hash = FALSE;
}

/**
 * empathy_ft_handler_get_filename:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the name of the file being transferred.
 *
 * Return value: the name of the file being transferred
 */
const char *
empathy_ft_handler_get_filename (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->filename;
}

/**
 * empathy_ft_handler_get_content_type:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the content type of the file being transferred.
 *
 * Return value: the content type of the file being transferred
 */
const char *
empathy_ft_handler_get_content_type (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->content_type;
}

/**
 * empathy_ft_handler_get_contact:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the remote #EmpathyContact at the other side of the transfer.
 *
 * Return value: the remote #EmpathyContact for @handler
 */
EmpathyContact *
empathy_ft_handler_get_contact (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->contact;
}

/**
 * empathy_ft_handler_get_gfile:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the #GFile where the transfer is being read/saved.
 *
 * Return value: the #GFile where the transfer is being read/saved
 */
GFile *
empathy_ft_handler_get_gfile (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), NULL);

  priv = GET_PRIV (handler);

  return priv->gfile;
}

/**
 * empathy_ft_handler_get_use_hash:
 * @handler: an #EmpathyFTHandler
 *
 * Returns whether @handler has checksumming enabled. Note that if the CM
 * doesn't support sending/receiving the checksum, this can return %FALSE even
 * if the handler was created with the use_hash parameter set to %TRUE.
 *
 * Return value: %TRUE if the handler has checksumming enabled,
 * %FALSE otherwise.
 */
gboolean
empathy_ft_handler_get_use_hash (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return priv->use_hash;
}

/**
 * empathy_ft_handler_is_incoming:
 * @handler: an #EmpathyFTHandler
 *
 * Returns whether @handler is incoming or outgoing.
 *
 * Return value: %TRUE if the handler is incoming, %FALSE otherwise.
 */
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

/**
 * empathy_ft_handler_get_transferred_bytes:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the number of bytes already transferred by the handler.
 *
 * Return value: the number of bytes already transferred by the handler.
 */
guint64
empathy_ft_handler_get_transferred_bytes (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), 0);

  priv = GET_PRIV (handler);

  return priv->transferred_bytes;
}

/**
 * empathy_ft_handler_get_total_bytes:
 * @handler: an #EmpathyFTHandler
 *
 * Returns the total size of the file being transferred by the handler.
 *
 * Return value: a number of bytes indicating the total size of the file being
 * transferred by the handler.
 */
guint64
empathy_ft_handler_get_total_bytes (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), 0);

  priv = GET_PRIV (handler);

  return priv->total_bytes;
}

/**
 * empathy_ft_handler_is_completed:
 * @handler: an #EmpathyFTHandler
 *
 * Returns whether the transfer for @handler has been completed succesfully.
 *
 * Return value: %TRUE if the handler has been transferred correctly, %FALSE
 * otherwise
 */
gboolean
empathy_ft_handler_is_completed (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return priv->is_completed;
}

/**
 * empathy_ft_handler_is_cancelled:
 * @handler: an #EmpathyFTHandler
 *
 * Returns whether the transfer for @handler has been cancelled or has stopped
 * due to an error.
 *
 * Return value: %TRUE if the transfer for @handler has been cancelled
 * or has stopped due to an error, %FALSE otherwise.
 */
gboolean
empathy_ft_handler_is_cancelled (EmpathyFTHandler *handler)
{
  EmpathyFTHandlerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FT_HANDLER (handler), FALSE);

  priv = GET_PRIV (handler);

  return g_cancellable_is_cancelled (priv->cancellable);
}
