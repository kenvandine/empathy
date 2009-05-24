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
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib/gi18n-lib.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <telepathy-glib/gtypes.h>
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
 * Usually, clients do not need to deal with #EmpathyTpFile objects directly,
 * and are supposed to use #EmpathyFTHandler and #EmpathyFTFactory for
 * transferring files using libempathy.
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
  TpSocketAddressType socket_address_type;
  TpSocketAccessControl socket_access_control;

  /* transfer properties */
  gboolean incoming;
  time_t start_time;
  GArray *socket_address;
  guint port;
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

  if (error != NULL)
    {
      /* set a default value for the state */
      priv->state = TP_FILE_TRANSFER_STATE_NONE;
      return;
    }

  priv->state = g_value_get_uint (value);
}

static gint
uint_compare (gconstpointer a, gconstpointer b)
{
  const guint *uinta = a;
  const guint *uintb = b;

  if (*uinta == *uintb)
    return 0;

  return (*uinta > *uintb) ? 1 : -1;
}

static void
tp_file_get_available_socket_types_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (weak_object);
  GHashTable *socket_types;
  GArray *access_controls;

  if (error != NULL ||
      !G_VALUE_HOLDS (value, TP_HASH_TYPE_SUPPORTED_SOCKET_MAP))
    {
      /* set a default value */
      priv->socket_address_type = TP_SOCKET_ADDRESS_TYPE_UNIX;
      priv->socket_access_control = TP_SOCKET_ACCESS_CONTROL_LOCALHOST;
      goto out;
    }

  socket_types = g_value_get_boxed (value);

  /* here UNIX is preferred to IPV4 */
  if ((access_controls = g_hash_table_lookup (socket_types,
      GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_UNIX))) != NULL)
    {
      priv->socket_address_type = TP_SOCKET_ADDRESS_TYPE_UNIX;
      priv->socket_access_control = TP_SOCKET_ACCESS_CONTROL_LOCALHOST;
      goto out;
    }

  if ((access_controls = g_hash_table_lookup (socket_types,
      GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_IPV4))) != NULL)
    {
      priv->socket_address_type = TP_SOCKET_ADDRESS_TYPE_IPV4;
      g_array_sort (access_controls, uint_compare);

      /* here port is preferred over localhost */
      if ((g_array_index (access_controls, guint, 0) ==
          TP_SOCKET_ACCESS_CONTROL_LOCALHOST) &&
          (g_array_index (access_controls, guint, 1) ==
          TP_SOCKET_ACCESS_CONTROL_PORT))
        priv->socket_access_control = TP_SOCKET_ACCESS_CONTROL_PORT;
      else
        priv->socket_access_control =
            g_array_index (access_controls, guint, 0);
    }

out:
  DEBUG ("Socket address type: %u, access control %u",
      priv->socket_address_type, priv->socket_access_control);  
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

  if (priv->is_closed)
    return;

  DEBUG ("FT operation close clean");

  priv->is_closed = TRUE;

  if (priv->op_callback != NULL)
    priv->op_callback (tp_file, NULL, priv->op_user_data);
}

static void
ft_operation_close_with_error (EmpathyTpFile *tp_file,
    GError *error)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  if (priv->is_closed)
    return;

  DEBUG ("FT operation close with error %s", error->message);

  priv->is_closed = TRUE;

  /* close the channel if it's not cancelled already */
  if (priv->state != TP_FILE_TRANSFER_STATE_CANCELLED)
    empathy_tp_file_cancel (tp_file);

  if (priv->op_callback != NULL)
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

  g_output_stream_splice_finish (G_OUTPUT_STREAM (source), res, &error);

  DEBUG ("Splice stream ready cb, error %p", error);

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
  gint fd, domain, res = 0;
  GError *error = NULL;
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  if (priv->socket_address_type == TP_SOCKET_ADDRESS_TYPE_UNIX)
    domain = AF_UNIX;

  if (priv->socket_address_type == TP_SOCKET_ADDRESS_TYPE_IPV4)
    domain = AF_INET;

  fd = socket (domain, SOCK_STREAM, 0);

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

  if (priv->socket_address_type == TP_SOCKET_ADDRESS_TYPE_UNIX)
    {
      struct sockaddr_un addr;

      memset (&addr, 0, sizeof (addr));
      addr.sun_family = domain;
      strncpy (addr.sun_path, priv->socket_address->data,
          priv->socket_address->len);

      res = connect (fd, (struct sockaddr*) &addr, sizeof (addr));
    }
  else if (priv->socket_address_type == TP_SOCKET_ADDRESS_TYPE_IPV4)
    {
      struct sockaddr_in addr;

      memset (&addr, 0, sizeof (addr));
      addr.sin_family = domain;
      inet_pton (AF_INET, priv->socket_address->data, &addr.sin_addr);
      addr.sin_port = htons (priv->port);

      res = connect (fd, (struct sockaddr*) &addr, sizeof (addr));
    }

  if (res < 0)
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
  if (priv->progress_callback != NULL)
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
  GError *retval = NULL;

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
  GError *error = NULL;

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
      priv->socket_address != NULL)
    tp_file_start_transfer (EMPATHY_TP_FILE (weak_object));

  if (state == TP_FILE_TRANSFER_STATE_COMPLETED)
    ft_operation_close_clean (EMPATHY_TP_FILE (weak_object));

  if (state == TP_FILE_TRANSFER_STATE_CANCELLED)
    {
      error = error_from_state_change_reason (priv->state_change_reason);
      ft_operation_close_with_error (EMPATHY_TP_FILE (weak_object), error);
      g_clear_error (&error);
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
  if (priv->progress_callback != NULL)
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

  if (error != NULL)
    {
      if (myerr != NULL)
        {
          /* if we were both cancelled and failed when calling the method,
          * report the method error.
          */
          g_clear_error (&myerr);
          myerr = g_error_copy (error);
        }
    }

  if (myerr != NULL)
    {
      DEBUG ("Error: %s", error->message);
      ft_operation_close_with_error (tp_file, myerr);
      g_clear_error (&myerr);
      return;
    }

  if (G_VALUE_TYPE (address) == DBUS_TYPE_G_UCHAR_ARRAY)
    {
      priv->socket_address = g_value_dup_boxed (address);
    }
  else if (G_VALUE_TYPE (address) == G_TYPE_STRING)
    {
      /* Old bugged version of telepathy-salut used to store the address
       * as a 's' instead of an 'ay' */
      const gchar *path;

      path = g_value_get_string (address);
      priv->socket_address = g_array_sized_new (TRUE, FALSE, sizeof (gchar),
          strlen (path));
      g_array_insert_vals (priv->socket_address, 0, path, strlen (path));
    }
  else if (G_VALUE_TYPE (address) == TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4)
    {
      GValueArray *val_array;
      GValue *v;
      const char *addr;

      val_array = g_value_get_boxed (address);

      /* IPV4 address */
      v = g_value_array_get_nth (val_array, 0);
      addr = g_value_get_string (v);
      priv->socket_address = g_array_sized_new (TRUE, FALSE, sizeof (gchar),
          strlen (addr));
      g_array_insert_vals (priv->socket_address, 0, addr, strlen (addr));

      /* port number */
      v = g_value_array_get_nth (val_array, 1);
      priv->port = g_value_get_uint (v);
    }

  DEBUG ("Got socket address: %s, port (not zero if IPV4): %d",
      priv->socket_address->data, priv->port);

  /* if the channel is already open, start the transfer now, otherwise,
   * wait for the state change signal.
   */
  if (priv->state == TP_FILE_TRANSFER_STATE_OPEN)
    tp_file_start_transfer (tp_file);
}

static void
initialize_empty_ac_variant (TpSocketAccessControl ac,
    GValue *val)
{
  if (ac == TP_SOCKET_ACCESS_CONTROL_LOCALHOST)
    {
      g_value_init (val, G_TYPE_STRING);
      g_value_set_static_string (val, "");
    }
  else if (ac == TP_SOCKET_ACCESS_CONTROL_PORT)
    {
      g_value_init (val, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4);
    }
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

  /* we don't impose specific interface/port requirements even
   * if we're not using UNIX sockets.
   */
  initialize_empty_ac_variant (priv->socket_access_control, &nothing);

  tp_cli_channel_type_file_transfer_call_provide_file (
      priv->channel, -1,
      priv->socket_address_type, priv->socket_access_control,
      &nothing, ft_operation_provide_or_accept_file_cb,
      NULL, NULL, G_OBJECT (tp_file));
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

  /* we don't impose specific interface/port requirements even
   * if we're not using UNIX sockets.
   */
  initialize_empty_ac_variant (priv->socket_access_control, &nothing);

  tp_cli_channel_type_file_transfer_call_accept_file (priv->channel,
      -1, priv->socket_address_type, priv->socket_access_control,
      &nothing, priv->offset,
      ft_operation_provide_or_accept_file_cb, NULL, NULL, G_OBJECT (tp_file));
}

static void
channel_closed_cb (TpChannel *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  EmpathyTpFile *tp_file = EMPATHY_TP_FILE (weak_object);
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);
  gboolean cancel = GPOINTER_TO_INT (user_data);

  DEBUG ("Channel is closed, should cancel %s", cancel ? "True" : "False");

  if (priv->cancellable != NULL &&
      !g_cancellable_is_cancelled (priv->cancellable) && cancel)
    g_cancellable_cancel (priv->cancellable);
}

static void
close_channel_internal (EmpathyTpFile *tp_file,
    gboolean cancel)
{
  EmpathyTpFilePriv *priv = GET_PRIV (tp_file);

  DEBUG ("Closing channel, should cancel %s", cancel ?
         "True" : "False");

  tp_cli_channel_call_close (priv->channel, -1,
    channel_closed_cb, GINT_TO_POINTER (cancel), NULL, G_OBJECT (tp_file));
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

  if (priv->channel != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_file_invalidated_cb, object);
      g_object_unref (priv->channel);
      priv->channel = NULL;
    }

  if (priv->in_stream != NULL)
    g_object_unref (priv->in_stream);

  if (priv->out_stream != NULL)
    g_object_unref (priv->out_stream);

  if (priv->cancellable != NULL)
    g_object_unref (priv->cancellable);

  G_OBJECT_CLASS (empathy_tp_file_parent_class)->dispose (object);
}

static void
do_finalize (GObject *object)
{
  EmpathyTpFilePriv *priv = GET_PRIV (object);

  DEBUG ("%p", object);

  if (priv->socket_address != NULL)
    {
      g_array_free (priv->socket_address, TRUE);
      priv->socket_address = NULL;
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

static void
do_constructed (GObject *object)
{
  EmpathyTpFile *tp_file;
  EmpathyTpFilePriv *priv;

  tp_file = EMPATHY_TP_FILE (object);
  priv = GET_PRIV (tp_file);

  g_signal_connect (priv->channel, "invalidated",
    G_CALLBACK (tp_file_invalidated_cb), tp_file);

  tp_cli_channel_type_file_transfer_connect_to_file_transfer_state_changed (
      priv->channel, tp_file_state_changed_cb, NULL, NULL, object, NULL);

  tp_cli_channel_type_file_transfer_connect_to_transferred_bytes_changed (
      priv->channel, tp_file_transferred_bytes_changed_cb,
      NULL, NULL, object, NULL);

  tp_cli_dbus_properties_call_get (priv->channel,
      -1, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, "State", tp_file_get_state_cb,
      NULL, NULL, object);

  tp_cli_dbus_properties_call_get (priv->channel,
      -1, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, "AvailableSocketTypes",
      tp_file_get_available_socket_types_cb, NULL, NULL, object);

  priv->state_change_reason =
      TP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE;
}

static void
empathy_tp_file_class_init (EmpathyTpFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = do_finalize;
  object_class->dispose = do_dispose;
  object_class->constructed = do_constructed;
  object_class->get_property = do_get_property;
  object_class->set_property = do_set_property;

  /* Construct-only properties */

  /**
   * EmpathyTpFile:channel:
   *
   * The #TpChannel requested for the file transfer.
   */
  g_object_class_install_property (object_class,
      PROP_CHANNEL,
      g_param_spec_object ("channel",
          "telepathy channel",
          "The file transfer channel",
          TP_TYPE_CHANNEL,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY));

  /**
   * EmpathyTpFile:incoming:
   *
   * %TRUE if the transfer is incoming, %FALSE if it's outgoing.
   */
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
 * @incoming: whether the file transfer is incoming or not
 *
 * Creates a new #EmpathyTpFile wrapping @channel, with the direction
 * specified by @incoming. The returned #EmpathyTpFile should be unrefed
 * with g_object_unref() when finished with.
 *
 * Return value: a new #EmpathyTpFile
 */
EmpathyTpFile *
empathy_tp_file_new (TpChannel *channel,
    gboolean incoming)
{
  EmpathyTpFile *tp_file;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  tp_file = g_object_new (EMPATHY_TYPE_TP_FILE,
      "channel", channel, "incoming", incoming,
      NULL);

  return tp_file;
}

/**
 * empathy_tp_file_accept:
 * @tp_file: an incoming #EmpathyTpFile
 * @offset: the offset of @gfile where we should start writing
 * @gfile: the destination #GFile for the transfer
 * @cancellable: a #GCancellable
 * @progress_callback: function to callback with progress information
 * @progress_user_data: user_data to pass to @progress_callback
 * @op_callback: function to callback when the transfer ends
 * @op_user_data: user_data to pass to @op_callback
 *
 * Accepts an incoming file transfer, saving the result into @gfile.
 * The callback @op_callback will be called both when the transfer is
 * successful and in case of an error. Note that cancelling @cancellable,
 * closes the socket of the file operation in progress, but doesn't
 * guarantee that the transfer channel will be closed as well. Thus,
 * empathy_tp_file_cancel() or empathy_tp_file_close() should be used to
 * actually cancel an ongoing #EmpathyTpFile.
 */
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


/**
 * empathy_tp_file_offer:
 * @tp_file: an outgoing #EmpathyTpFile
 * @gfile: the source #GFile for the transfer
 * @cancellable: a #GCancellable
 * @progress_callback: function to callback with progress information
 * @progress_user_data: user_data to pass to @progress_callback
 * @op_callback: function to callback when the transfer ends
 * @op_user_data: user_data to pass to @op_callback
 *
 * Offers an outgoing file transfer, reading data from @gfile.
 * The callback @op_callback will be called both when the transfer is
 * successful and in case of an error. Note that cancelling @cancellable,
 * closes the socket of the file operation in progress, but doesn't
 * guarantee that the transfer channel will be closed as well. Thus,
 * empathy_tp_file_cancel() or empathy_tp_file_close() should be used to
 * actually cancel an ongoing #EmpathyTpFile.
 */
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

/**
 * empathy_tp_file_cancel:
 * @tp_file: an #EmpathyTpFile
 *
 * Cancels an ongoing #EmpathyTpFile, first closing the channel and then
 * cancelling any I/O operation and closing the socket.
 */
void
empathy_tp_file_cancel (EmpathyTpFile *tp_file)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  close_channel_internal (tp_file, TRUE);
}

/**
 * empathy_tp_file_close:
 * @tp_file: an #EmpathyTpFile
 *
 * Closes the channel for an ongoing #EmpathyTpFile. It's safe to call this
 * method after the transfer has ended.
 */
void
empathy_tp_file_close (EmpathyTpFile *tp_file)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  close_channel_internal (tp_file, FALSE);
}
