/*
 * Copyright (C) 2007-2009 Collabora Ltd.
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
 *
 * Authors: Marco Barisione <marco@barisione.org>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib/gi18n-lib.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#include "empathy-tp-file.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

/**
 * SECTION:empathy-tp-file
 * @title: EmpathyTpFile
 * @short_description: Object which represents a Telepathy file channel
 * @include: libempathy/empathy-tp-file.h
 *
 * #EmpathyTpFile is an object which represents a Telepathy file channel.
 */

/**
 * EmpathyTpFile:
 * @parent: parent object
 *
 * Object which represents a Telepathy file channel.
 */

/**
 * EMPATHY_TP_FILE_UNKNOWN_SIZE:
 *
 * Value used for the "size" or "estimated-size" properties when the size of
 * the transferred file is unknown.
 */

/* EmpathyTpFile object */

typedef struct {
  TpChannel *channel;
  gboolean ready;

  GInputStream *in_stream;
  GOutputStream *out_stream;

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer D-Bus properties */
  TpFileTransferState state;
  TpFileTransferStateChangeReason state_change_reason;

  /* transfer properties */
  gboolean incoming;
  time_t start_time;
  GArray *unix_socket_path;
  guint64 offset;

  /* GCancellable we're passed when offering/accepting the transfer */
  GCancellable *cancellable;

  /* callbacks for the operation */
  EmpathyTpFileProgressCallback progress_callback;
  gpointer progress_user_data;
  EmpathyTpFileOperationCallback op_callback;
  gpointer op_user_data;

  gboolean is_closed;

  gboolean dispose_run;
} EmpathyTpFilePriv;

enum {
  PROP_0,
  PROP_CHANNEL,
  PROP_INCOMING
};

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpFile)

G_DEFINE_TYPE (EmpathyTpFile, empathy_tp_file, G_TYPE_OBJECT);

/* private functions */

static void
tp_file_get_state_cb (TpProxy *proxy,
                      const GValue *value,
                      const GError *error,
                      gpointer user_data,
                      GObject *weak_object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (weak_object);

  if (error)
    {
      /* set a default value for the state */
      priv->state = TP_FILE_TRANSFER_STATE_NONE;
      return;
    }

  priv->state = g_value_get_uint (value);
}

static void
tp_file_invalidated_cb (TpProxy       *proxy,
			guint          domain,
			gint           code,
			gchar         *message,
			EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  DEBUG ("Channel invalidated: %s", message);

  if (priv->state != TP_FILE_TRANSFER_STATE_COMPLETED &&
      priv->state != TP_FILE_TRANSFER_STATE_CANCELLED)
    {
      /* The channel is not in a finished state, an error occured */
      priv->state = TP_FILE_TRANSFER_STATE_CANCELLED;
      priv->state_change_reason =
          TP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_ERROR;
    }
}

static void
ft_operation_close_clean (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  DEBUG ("FT operation close clean");

  if (priv->is_closed)
    return;
  else
    priv->is_closed = TRUE;

  if (priv->op_callback)
    priv->op_callback (tp_file, NULL, priv->op_user_data);
}

static void
ft_operation_close_with_error (EmpathyTpFile *tp_file,
                               GError *error)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  DEBUG ("FT operation close with error %s", error->message);

  if (priv->is_closed)
    return;
  else
    priv->is_closed = TRUE;

  /* close the channel if it's not cancelled already */
  if (priv->state != TP_FILE_TRANSFER_STATE_CANCELLED)
    empathy_tp_file_cancel (tp_file);

  if (priv->op_callback)
    priv->op_callback (tp_file, error, priv->op_user_data);
}

static void
splice_stream_ready_cb (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  EmpathyTpFile *tp_file;
  GError *error = NULL;

  tp_file = user_data;

  DEBUG ("Splice stream ready cb");

  g_output_stream_splice_finish (G_OUTPUT_STREAM (source), res, &error);

  if (error != NULL)
    {
      ft_operation_close_with_error (tp_file, error);
      g_clear_error (&error);
      return;
    }
}

static void
tp_file_start_transfer (EmpathyTpFile *tp_file)
{
  gint fd;
  struct sockaddr_un addr;
  GError *error = NULL;
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    {
      int code = errno;

      error = g_error_new_literal (EMPATHY_FT_ERROR_QUARK,
          EMPATHY_FT_ERROR_SOCKET, g_strerror (code));

      DEBUG ("Failed to create socket, closing channel");

      ft_operation_close_with_error (tp_file, error);
      g_clear_error (&error);

      return;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, priv->unix_socket_path->data,
      priv->unix_socket_path->len);

  if (connect (fd, (struct sockaddr*) &addr, sizeof (addr)) < 0)
    {
      int code = errno;

      error = g_error_new_literal (EMPATHY_FT_ERROR_QUARK,
          EMPATHY_FT_ERROR_SOCKET, g_strerror (code));

      DEBUG ("Failed to connect socket, closing channel");

      ft_operation_close_with_error (tp_file, error);
      close (fd);
      g_clear_error (&error);

      return;
    }

  DEBUG ("Start the transfer");

  priv->start_time = empathy_time_get_current ();

  /* notify we're starting a transfer */
  if (priv->progress_callback)
    priv->progress_callback (tp_file, 0, priv->progress_user_data);

  if (priv->incoming)
    {
      GInputStream *socket_stream;

      socket_stream = g_unix_input_stream_new (fd, TRUE);

      g_output_stream_splice_async (priv->out_stream, socket_stream,
          G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
          G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
          G_PRIORITY_DEFAULT, priv->cancellable,
          splice_stream_ready_cb, tp_file);

      g_object_unref (socket_stream);
    }
  else
    {
      GOutputStream *socket_stream;

      socket_stream = g_unix_output_stream_new (fd, TRUE);

      g_output_stream_splice_async (socket_stream, priv->in_stream,
          G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
          G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
          G_PRIORITY_DEFAULT, priv->cancellable,
          splice_stream_ready_cb, tp_file);

      g_object_unref (socket_stream);
    }
}

static GError *
error_from_state_change_reason (TpFileTransferStateChangeReason reason)
{
  const char *string;
  GError *retval;

  string = NULL;

  switch (reason)
    {
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE:
        string = _("No reason was specified");
        break;
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_REQUESTED:
        string = _("The change in state was requested");
        break;
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_STOPPED:
        string = _("You canceled the file transfer");
        break;
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_REMOTE_STOPPED:
        string = _("The other participant canceled the file transfer");
        break;
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_LOCAL_ERROR:
        string = _("Error while trying to transfer the file");
        break;
      case TP_FILE_TRANSFER_STATE_CHANGE_REASON_REMOTE_ERROR:
        string = _("The other participant is unable to transfer the file");
        break;
      default:
        string = _("Unknown reason");
        break;
    }

  retval = g_error_new_literal (EMPATHY_FT_ERROR_QUARK,
      EMPATHY_FT_ERROR_TP_ERROR, string);

  return retval;
}

static void
tp_file_state_changed_cb (TpChannel *proxy,
                          guint state,
                          guint reason,
                          gpointer user_data,
                          GObject *weak_object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (weak_object);
  GError *error;

  if (state == priv->state)
    return;

  DEBUG ("File transfer state changed:\n"
      "old state = %u, state = %u, reason = %u\n"
      "\tincoming = %s, in_stream = %s, out_stream = %s",
      priv->state, state, reason,
      priv->incoming ? "yes" : "no",
      priv->in_stream ? "present" : "not present",
      priv->out_stream ? "present" : "not present");

  priv->state = state;
  priv->state_change_reason = reason;

  /* If the channel is open AND we have the socket path, we can start the
   * transfer. The socket path could be NULL if we are not doing the actual
   * data transfer but are just an observer for the channel.
   */
  if (state == TP_FILE_TRANSFER_STATE_OPEN &&
      priv->unix_socket_path != NULL)
    tp_file_start_transfer (EMPATHY_TP_FILE (weak_object));

  if (state == TP_FILE_TRANSFER_STATE_COMPLETED)
    ft_operation_close_clean (EMPATHY_TP_FILE (weak_object));

  if (state == TP_FILE_TRANSFER_STATE_CANCELLED)
    {
      error = error_from_state_change_reason (priv->state_change_reason);
      ft_operation_close_with_error (EMPATHY_TP_FILE (weak_object), error);
    }
}

static void
tp_file_transferred_bytes_changed_cb (TpChannel *proxy,
                                      guint64 count,
                                      gpointer user_data,
                                      GObject *weak_object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (weak_object);

  /* don't notify for 0 bytes count */
  if (count == 0)
    return;

  /* notify clients */
  if (priv->progress_callback)
    priv->progress_callback (EMPATHY_TP_FILE (weak_object),
        count, priv->progress_user_data);
}

static void
ft_operation_provide_or_accept_file_cb (TpChannel *proxy,
                                        const GValue *address,
                                        const GError *error,
                                        gpointer user_data,
                                        GObject *weak_object)
{
  EmpathyTpFile *tp_file = EMPATHY_TP_FILE (weak_object);
  GError *myerr = NULL;
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  g_cancellable_set_error_if_cancelled (priv->cancellable, &myerr);

  if (error)
    {
      if (myerr)
        {
          /* if we were both cancelled and failed when calling the method,
          * report the method error.
          */
          g_clear_error (&myerr);
          myerr = g_error_copy (error);
        }
    }

  if (myerr)
    {
      DEBUG ("Error: %s", error->message);
      ft_operation_close_with_error (tp_file, myerr);
      g_clear_error (&myerr);
      return;
    }

  if (G_VALUE_TYPE (address) == DBUS_TYPE_G_UCHAR_ARRAY)
    {
      priv->unix_socket_path = g_value_dup_boxed (address);
    }
  else if (G_VALUE_TYPE (address) == G_TYPE_STRING)
    {
      /* Old bugged version of telepathy-salut used to store the address
       * as a 's' instead of an 'ay' */
      const gchar *path;

      path = g_value_get_string (address);
      priv->unix_socket_path = g_array_sized_new (TRUE, FALSE, sizeof (gchar),
                                                  strlen (path));
      g_array_insert_vals (priv->unix_socket_path, 0, path, strlen (path));
    }

  DEBUG ("Got unix socket path: %s", priv->unix_socket_path->data);

  /* if the channel is already open, start the transfer now, otherwise,
   * wait for the state change signal.
   */
  if (priv->state == TP_FILE_TRANSFER_STATE_OPEN)
    tp_file_start_transfer (tp_file);
}

static void
file_read_async_cb (GObject *source,
                    GAsyncResult *res,
                    gpointer user_data)
{
  GValue nothing = { 0 };
  EmpathyTpFile *tp_file = user_data;
  EmpathyTpFilePriv *priv;
  GFileInputStream *in_stream;
  GError *error = NULL;

  priv = GET_PRIV (tp_file);

  in_stream = g_file_read_finish (G_FILE (source), res, &error);

  if (error != NULL)
    {
      ft_operation_close_with_error (tp_file, error);
      g_clear_error (&error);
      return;
    }

  priv->in_stream = G_INPUT_STREAM (in_stream);

  g_value_init (&nothing, G_TYPE_STRING);
  g_value_set_static_string (&nothing, "");

  tp_cli_channel_type_file_transfer_call_provide_file (
      priv->channel, -1,
      TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
      &nothing, ft_operation_provide_or_accept_file_cb, NULL, NULL, G_OBJECT (tp_file));
}

static void
file_replace_async_cb (GObject *source,
                       GAsyncResult *res,
                       gpointer user_data)
{
  GValue nothing = { 0 };
  EmpathyTpFile *tp_file = user_data;
  EmpathyTpFilePriv *priv;
  GError *error = NULL;
  GFileOutputStream *out_stream;

  priv = GET_PRIV (tp_file);

  out_stream = g_file_replace_finish (G_FILE (source), res, &error);

  if (error != NULL)
    {
      ft_operation_close_with_error (tp_file, error);
      g_clear_error (&error);

      return;
    }

  priv->out_stream = G_OUTPUT_STREAM (out_stream);

  g_value_init (&nothing, G_TYPE_STRING);
  g_value_set_static_string (&nothing, "");

  tp_cli_channel_type_file_transfer_call_accept_file (priv->channel,
      -1, TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
      &nothing, priv->offset,
      ft_operation_provide_or_accept_file_cb, NULL, NULL, G_OBJECT (tp_file));
}

static void
close_channel_internal (EmpathyTpFile *tp_file,
                        gboolean cancel)
{
  EmpathyTpFilePriv *priv;

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  
  priv = GET_PRIV (tp_file);

  DEBUG ("Closing channel..");
  tp_cli_channel_call_close (priv->channel, -1,
    NULL, NULL, NULL, NULL);

  if (priv->cancellable != NULL &&
      !g_cancellable_is_cancelled (priv->cancellable) && cancel)
    g_cancellable_cancel (priv->cancellable);
}

/* GObject methods */

static void
empathy_tp_file_init (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE ((tp_file),
      EMPATHY_TYPE_TP_FILE, EmpathyTpFilePriv);

  tp_file->priv = priv;
}

static void
do_dispose (GObject *object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (object);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->channel)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_file_invalidated_cb, object);
      g_object_unref (priv->channel);
      priv->channel = NULL;
    }

  if (priv->in_stream)
    g_object_unref (priv->in_stream);

  if (priv->out_stream)
    g_object_unref (priv->out_stream);

  if (priv->cancellable)
    g_object_unref (priv->cancellable);

  G_OBJECT_CLASS (empathy_tp_file_parent_class)->dispose (object);
}

static void
do_finalize (GObject *object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (object);

  DEBUG ("%p", object);

  if (priv->unix_socket_path != NULL)
    {
      g_array_free (priv->unix_socket_path, TRUE);
      priv->unix_socket_path = NULL;
    }

  G_OBJECT_CLASS (empathy_tp_file_parent_class)->finalize (object);
}

static void
do_get_property (GObject *object,
                 guint param_id,
                 GValue *value,
                 GParamSpec *pspec)
{
  EmpathyTpFilePriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;
      case PROP_INCOMING:
        g_value_set_boolean (value, priv->incoming);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
do_set_property (GObject *object,
                 guint param_id,
                 const GValue *value,
                 GParamSpec *pspec)
{
  EmpathyTpFilePriv *priv = GET_PRIV (object);
  switch (param_id)
    {
      case PROP_CHANNEL:
        priv->channel = g_object_ref (g_value_get_object (value));
        break;
      case PROP_INCOMING:
        priv->incoming = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static GObject *
do_constructor (GType type,
                guint n_props,
                GObjectConstructParam *props)
{
  GObject *file_obj;
  EmpathyTpFile *tp_file;
  EmpathyTpFilePriv *priv;

  file_obj = G_OBJECT_CLASS (empathy_tp_file_parent_class)->constructor (type,
      n_props, props);
  
  tp_file = EMPATHY_TP_FILE (file_obj);
  priv = GET_PRIV (tp_file);

  g_signal_connect (priv->channel, "invalidated",
    G_CALLBACK (tp_file_invalidated_cb), tp_file);

  tp_cli_channel_type_file_transfer_connect_to_file_transfer_state_changed (
      priv->channel, tp_file_state_changed_cb, NULL, NULL,
      G_OBJECT (tp_file), NULL);

  tp_cli_channel_type_file_transfer_connect_to_transferred_bytes_changed (
      priv->channel, tp_file_transferred_bytes_changed_cb,
      NULL, NULL, G_OBJECT (tp_file), NULL);

  tp_cli_dbus_properties_call_get (priv->channel,
      -1, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, "State", tp_file_get_state_cb,
      NULL, NULL, file_obj);

  priv->state_change_reason =
      TP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE;

  return file_obj;
}

static void
empathy_tp_file_class_init (EmpathyTpFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = do_finalize;
  object_class->dispose = do_dispose;
  object_class->constructor = do_constructor;
  object_class->get_property = do_get_property;
  object_class->set_property = do_set_property;

  /* Construct-only properties */
  g_object_class_install_property (object_class,
      PROP_CHANNEL,
      g_param_spec_object ("channel",
          "telepathy channel",
          "The file transfer channel",
          TP_TYPE_CHANNEL,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
      PROP_INCOMING,
      g_param_spec_boolean ("incoming",
          "direction of transfer",
          "The direction of the file being transferred",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (EmpathyTpFilePriv));
}

/* public methods */

/**
 * empathy_tp_file_new:
 * @channel: a #TpChannel
 *
 * Creates a new #EmpathyTpFile wrapping @channel, or return a new ref to an
 * existing #EmpathyTpFile for that channel. The returned #EmpathyTpFile
 * should be unrefed with g_object_unref() when finished with.
 *
 * Return value: a new #EmpathyTpFile
 */
EmpathyTpFile *
empathy_tp_file_new (TpChannel *channel, gboolean incoming)
{
  EmpathyTpFile *tp_file;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  tp_file = g_object_new (EMPATHY_TYPE_TP_FILE,
      "channel", channel, "incoming", incoming,
      NULL);

  return tp_file;
}

void
empathy_tp_file_accept (EmpathyTpFile *tp_file,
                        guint64 offset,
                        GFile *gfile,
                        GCancellable *cancellable,
                        EmpathyTpFileProgressCallback progress_callback,
                        gpointer progress_user_data,
                        EmpathyTpFileOperationCallback op_callback,
                        gpointer op_user_data)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  g_return_if_fail (G_IS_FILE (gfile));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable));

  priv->cancellable = g_object_ref (cancellable);
  priv->progress_callback = progress_callback;
  priv->progress_user_data = progress_user_data;
  priv->op_callback = op_callback;
  priv->op_user_data = op_user_data;
  priv->offset = offset;

  g_file_replace_async (gfile, NULL, FALSE, G_FILE_CREATE_NONE,
      G_PRIORITY_DEFAULT, cancellable, file_replace_async_cb, tp_file);
}

void
empathy_tp_file_offer (EmpathyTpFile *tp_file,
                       GFile *gfile,
                       GCancellable *cancellable,
                       EmpathyTpFileProgressCallback progress_callback,
                       gpointer progress_user_data,
                       EmpathyTpFileOperationCallback op_callback,
                       gpointer op_user_data)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  g_return_if_fail (G_IS_FILE (gfile));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable));

  priv->cancellable = g_object_ref (cancellable);
  priv->progress_callback = progress_callback;
  priv->progress_user_data = progress_user_data;
  priv->op_callback = op_callback;
  priv->op_user_data = op_user_data;

  g_file_read_async (gfile, G_PRIORITY_DEFAULT, cancellable,
      file_read_async_cb, tp_file);
}

/**
 * empathy_tp_file_is_incoming:
 * @tp_file: an #EmpathyTpFile
 *
 * Returns whether @tp_file is incoming.
 *
 * Return value: %TRUE if the @tp_file is incoming, otherwise %FALSE
 */
gboolean
empathy_tp_file_is_incoming (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), FALSE);
  
  priv = GET_PRIV (tp_file);

  return priv->incoming;
}

void
empathy_tp_file_cancel (EmpathyTpFile *tp_file)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  close_channel_internal (tp_file, TRUE);
}

void
empathy_tp_file_close (EmpathyTpFile *tp_file)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  close_channel_internal (tp_file, FALSE);
}
