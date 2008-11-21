/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 */

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib/gi18n.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-props-iface.h>

#include <telepathy-glib/proxy-subclass.h>

#include "empathy-tp-file.h"
#include "empathy-contact-factory.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

/**
 * SECTION:empathy-tp-file
 * @short_description: File channel
 * @see_also: #EmpathyTpFile, #EmpathyContact, empathy_send_file()
 * @include: libempthy/empathy-tp-file.h
 *
 * The #EmpathyTpFile object represents a Telepathy file channel.
 */

/**
 * EMPATHY_TP_FILE_UNKNOWN_SIZE:
 *
 * Value used for the "size" or "estimated-size" properties when the size of
 * the transferred file is unknown.
 */

/* Functions to copy the content of a GInputStream to a GOutputStream */

#define N_BUFFERS 2
#define BUFFER_SIZE 4096

typedef struct {
  GInputStream *in;
  GOutputStream *out;
  GCancellable  *cancellable;
  char *buff[N_BUFFERS]; /* the temporary buffers */
  gsize count[N_BUFFERS]; /* how many bytes are used in the buffers */
  gboolean is_full[N_BUFFERS]; /* whether the buffers contain data */
  gint curr_read; /* index of the buffer used for reading */
  gint curr_write; /* index of the buffer used for writing */
  gboolean is_reading; /* we are reading */
  gboolean is_writing; /* we are writing */
  guint n_closed; /* number of streams that have been closed */
} CopyData;

static void schedule_next (CopyData *copy);

static void
free_copy_data_if_closed (CopyData *copy)
{
  gint i;

  /* Free the data only if both the input and output streams have
   * been closed. */
  copy->n_closed++;
  if (copy->n_closed < 2)
    return;

  if (copy->in != NULL)
    g_object_unref (copy->in);

  if (copy->out != NULL)
    g_object_unref (copy->out);

  for (i = 0; i < N_BUFFERS; i++)
    g_free (copy->buff[i]);

  g_object_unref (copy->cancellable);
  g_free (copy);
}

static void
io_error (CopyData *copy,
          GError *error)
{
  g_cancellable_cancel (copy->cancellable);

  if (error == NULL)
    g_warning ("I/O error");
  else if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
    ; /* Ignore cancellations */
  else
    g_warning ("I/O error: %d: %s\n", error->code, error->message);

  if (copy->in != NULL)
    g_input_stream_close (copy->in, NULL, NULL);

  if (copy->out != NULL)
    g_output_stream_close (copy->out, NULL, NULL);

  free_copy_data_if_closed (copy);
}

static void
close_done (GObject *source_object,
            GAsyncResult *res,
            gpointer user_data)
{
  CopyData *copy = user_data;

  g_object_unref (source_object);
  free_copy_data_if_closed (copy);
}

static void
write_done_cb (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
  CopyData *copy = user_data;
  gssize count_write;
  GError *error = NULL;

  count_write = g_output_stream_write_finish (copy->out, res, &error);

  if (count_write <= 0)
    {
      io_error (copy, error);
      g_error_free (error);
      return;
    }

  copy->is_full[copy->curr_write] = FALSE;
  copy->curr_write = (copy->curr_write + 1) % N_BUFFERS;
  copy->is_writing = FALSE;

  schedule_next (copy);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  CopyData *copy = user_data;
  gssize count_read;
  GError *error = NULL;

  count_read = g_input_stream_read_finish (copy->in, res, &error);

  if (count_read == 0)
    {
      g_input_stream_close_async (copy->in, 0, copy->cancellable,
          close_done, copy);
      copy->in = NULL;
    }
  else if (count_read < 0)
    {
      io_error (copy, error);
      g_error_free (error);
      return;
    }

  copy->count[copy->curr_read] = count_read;
  copy->is_full[copy->curr_read] = TRUE;
  copy->curr_read = (copy->curr_read + 1) % N_BUFFERS;
  copy->is_reading = FALSE;

  schedule_next (copy);
}

static void
schedule_next (CopyData *copy)
{
  if (copy->in != NULL &&
      !copy->is_reading &&
      !copy->is_full[copy->curr_read])
    {
      /* We are not reading and the current buffer is empty, so
       * start an async read. */
      copy->is_reading = TRUE;
      g_input_stream_read_async (copy->in,
          copy->buff[copy->curr_read],
          BUFFER_SIZE, 0, copy->cancellable,
          read_done_cb, copy);
    }

  if (!copy->is_writing &&
      copy->is_full[copy->curr_write])
    {
      if (copy->count[copy->curr_write] == 0)
        {
          /* The last read on the buffer read 0 bytes, this
           * means that we got an EOF, so we can close
           * the output channel. */
          g_output_stream_close_async (copy->out, 0,
              copy->cancellable,
              close_done, copy);
      copy->out = NULL;
        }
      else
        {
          /* We are not writing and the current buffer contains
           * data, so start an async write. */
          copy->is_writing = TRUE;
          g_output_stream_write_async (copy->out,
              copy->buff[copy->curr_write],
              copy->count[copy->curr_write],
              0, copy->cancellable,
              write_done_cb, copy);
        }
    }
}

static void
copy_stream (GInputStream *in,
             GOutputStream *out,
             GCancellable *cancellable)
{
  CopyData *copy;
  gint i;

  g_return_if_fail (in != NULL);
  g_return_if_fail (out != NULL);

  copy = g_new0 (CopyData, 1);
  copy->in = g_object_ref (in);
  copy->out = g_object_ref (out);

  if (cancellable != NULL)
    copy->cancellable = g_object_ref (cancellable);
  else
    copy->cancellable = g_cancellable_new ();

  for (i = 0; i < N_BUFFERS; i++)
    copy->buff[i] = g_malloc (BUFFER_SIZE);

  schedule_next (copy);
}

/* EmpathyTpFile object */

struct _EmpathyTpFilePriv {
  EmpathyContactFactory *factory;
  McAccount *account;
  gchar *id;
  MissionControl *mc;
  TpChannel *channel;

  EmpathyTpFile *cached_empathy_file;
  EmpathyContact *contact;
  GInputStream *in_stream;
  GOutputStream *out_stream;
  gboolean incoming;
  gchar *filename;
  EmpFileTransferState state;
  EmpFileTransferStateChangeReason state_change_reason;
  guint64 size;
  guint64 transferred_bytes;
  gint64 start_time;
  gchar *unix_socket_path;
  gchar *content_hash;
  EmpFileHashType content_hash_type;
  gchar *content_type;
  gchar *description;
  GCancellable *cancellable;
};

enum {
  PROP_0,
  PROP_ACCOUNT,
  PROP_CHANNEL,
  PROP_STATE,
  PROP_INCOMING,
  PROP_FILENAME,
  PROP_SIZE,
  PROP_CONTENT_TYPE,
  PROP_TRANSFERRED_BYTES,
  PROP_CONTENT_HASH_TYPE,
  PROP_CONTENT_HASH,
  PROP_IN_STREAM,
};

G_DEFINE_TYPE (EmpathyTpFile, empathy_tp_file, G_TYPE_OBJECT);

static void
empathy_tp_file_init (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE ((tp_file),
      EMPATHY_TYPE_TP_FILE, EmpathyTpFilePriv);

  tp_file->priv = priv;
}

static void
tp_file_destroy_cb (TpChannel *file_channel,
                    EmpathyTpFile *tp_file)
{
  DEBUG ("Channel Closed or CM crashed");

  g_object_unref (tp_file->priv->channel);
  tp_file->priv->channel = NULL;
}

static void
tp_file_finalize (GObject *object)
{
  EmpathyTpFile *tp_file;

  tp_file = EMPATHY_TP_FILE (object);

  if (tp_file->priv->channel)
    {
      DEBUG ("Closing channel..");
      g_signal_handlers_disconnect_by_func (tp_file->priv->channel,
          tp_file_destroy_cb, object);
      tp_cli_channel_call_close (tp_file->priv->channel, -1, NULL, NULL,
          NULL, NULL);
      if (G_IS_OBJECT (tp_file->priv->channel))
        g_object_unref (tp_file->priv->channel);
    }

  if (tp_file->priv->factory)
    {
      g_object_unref (tp_file->priv->factory);
    }
  if (tp_file->priv->account)
    {
      g_object_unref (tp_file->priv->account);
    }
  if (tp_file->priv->mc)
    {
      g_object_unref (tp_file->priv->mc);
    }

  g_free (tp_file->priv->id);
  g_free (tp_file->priv->filename);
  g_free (tp_file->priv->unix_socket_path);
  g_free (tp_file->priv->description);
  g_free (tp_file->priv->content_hash);
  g_free (tp_file->priv->content_type);

  if (tp_file->priv->in_stream)
    g_object_unref (tp_file->priv->in_stream);

  if (tp_file->priv->out_stream)
    g_object_unref (tp_file->priv->out_stream);

  if (tp_file->priv->contact)
    g_object_unref (tp_file->priv->contact);

  if (tp_file->priv->cancellable)
    g_object_unref (tp_file->priv->cancellable);

  G_OBJECT_CLASS (empathy_tp_file_parent_class)->finalize (object);
}

static void
tp_file_get_all_cb (TpProxy *proxy,
                    GHashTable *properties,
                    const GError *error,
                    gpointer user_data,
                    GObject *weak_object)
{
  EmpathyTpFile *tp_file = (EmpathyTpFile *) user_data;

  if (error)
    {
      DEBUG ("Failed to get properties: %s", error->message);
      return;
    }

  tp_file->priv->size = g_value_get_uint64 (
      g_hash_table_lookup (properties, "Size"));

  tp_file->priv->state = g_value_get_uint (
      g_hash_table_lookup (properties, "State"));

  /* Invalid reason, so empathy_file_get_state_change_reason() can give
   * a warning if called for a not closed file transfer. */
  tp_file->priv->state_change_reason = -1;

  tp_file->priv->transferred_bytes = g_value_get_uint64 (
      g_hash_table_lookup (properties, "TransferredBytes"));

  tp_file->priv->filename = g_value_dup_string (
      g_hash_table_lookup (properties, "Filename"));

  tp_file->priv->content_hash = g_value_dup_string (
      g_hash_table_lookup (properties, "ContentHash"));

  tp_file->priv->description = g_value_dup_string (
      g_hash_table_lookup (properties, "Description"));

  if (tp_file->priv->state == EMP_FILE_TRANSFER_STATE_LOCAL_PENDING)
    tp_file->priv->incoming = TRUE;

  g_hash_table_destroy (properties);
}

static void
tp_file_closed_cb (TpChannel *file_channel,
                   EmpathyTpFile *tp_file,
                   GObject *weak_object)
{
  /* The channel is closed, do just like if the proxy was destroyed */
  g_signal_handlers_disconnect_by_func (tp_file->priv->channel,
      tp_file_destroy_cb,
      tp_file);
  tp_file_destroy_cb (file_channel, tp_file);
}

static gint64
get_time_msec (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);
  return ((gint64) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

static gint
_get_local_socket (EmpathyTpFile *tp_file)
{
  gint fd;
  size_t path_len;
  struct sockaddr_un addr;

  if (G_STR_EMPTY (tp_file->priv->unix_socket_path))
    return -1;

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  path_len = strlen (tp_file->priv->unix_socket_path);
  strncpy (addr.sun_path, tp_file->priv->unix_socket_path, path_len);

  if (connect (fd, (struct sockaddr*) &addr,
      sizeof (addr)) < 0)
    {
      close (fd);
      return -1;
    }

  return fd;
}

static void
send_tp_file (EmpathyTpFile *tp_file)
{
  gint socket_fd;
  GOutputStream *socket_stream;

  DEBUG ("Sending file content: filename=%s",
      tp_file->priv->filename);

  g_return_if_fail (tp_file->priv->in_stream);

  socket_fd = _get_local_socket (tp_file);
  if (socket_fd < 0)
    {
      DEBUG ("failed to get local socket fd");
      return;
    }
  DEBUG ("got local socket fd");
  socket_stream = g_unix_output_stream_new (socket_fd, TRUE);

  tp_file->priv->cancellable = g_cancellable_new ();

  copy_stream (tp_file->priv->in_stream, socket_stream,
      tp_file->priv->cancellable);

  g_object_unref (socket_stream);
}

static void
receive_tp_file (EmpathyTpFile *tp_file)
{
  GInputStream *socket_stream;
  gint socket_fd;

  socket_fd = _get_local_socket (tp_file);

  if (socket_fd < 0)
    return;

  socket_stream = g_unix_input_stream_new (socket_fd, TRUE);

  tp_file->priv->cancellable = g_cancellable_new ();

  copy_stream (socket_stream, tp_file->priv->out_stream,
      tp_file->priv->cancellable);

  g_object_unref (socket_stream);
}

static void
tp_file_state_changed_cb (DBusGProxy *tp_file_iface,
                          EmpFileTransferState state,
                          EmpFileTransferStateChangeReason reason,
                          EmpathyTpFile *tp_file)
{
  DEBUG ("File transfer state changed: filename=%s, "
      "old state=%u, state=%u, reason=%u",
      tp_file->priv->filename, tp_file->priv->state, state, reason);

  if (state == EMP_FILE_TRANSFER_STATE_OPEN)
    tp_file->priv->start_time = get_time_msec ();

  DEBUG ("state = %u, incoming = %s, in_stream = %s, out_stream = %s",
      state, tp_file->priv->incoming ? "yes" : "no",
      tp_file->priv->in_stream ? "present" : "not present",
      tp_file->priv->out_stream ? "present" : "not present");

  if (state == EMP_FILE_TRANSFER_STATE_OPEN && !tp_file->priv->incoming &&
      tp_file->priv->in_stream)
    send_tp_file (tp_file);
  else if (state == EMP_FILE_TRANSFER_STATE_OPEN && tp_file->priv->incoming &&
      tp_file->priv->out_stream)
    receive_tp_file (tp_file);

  tp_file->priv->state = state;
  tp_file->priv->state_change_reason = reason;

  g_object_notify (G_OBJECT (tp_file), "state");
}

static void
tp_file_transferred_bytes_changed_cb (TpProxy *proxy,
                                      guint64 count,
                                      EmpathyTpFile *tp_file,
                                      GObject *weak_object)
{
  if (tp_file->priv->transferred_bytes == count)
    return;

  tp_file->priv->transferred_bytes = count;

  g_object_notify (G_OBJECT (tp_file), "transferred-bytes");
}

static GObject *
tp_file_constructor (GType type,
                     guint n_props,
                     GObjectConstructParam *props)
{
  GObject *file_obj;
  EmpathyTpFile *tp_file;
  TpHandle handle;

  file_obj = G_OBJECT_CLASS (empathy_tp_file_parent_class)->constructor (type,
      n_props, props);

  tp_file = EMPATHY_TP_FILE (file_obj);

  tp_file->priv->factory = empathy_contact_factory_new ();
  tp_file->priv->mc = empathy_mission_control_new ();

  tp_cli_channel_connect_to_closed (tp_file->priv->channel,
      (tp_cli_channel_signal_callback_closed) tp_file_closed_cb,
      tp_file,
      NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_file_transfer_state_changed (
      TP_PROXY (tp_file->priv->channel),
      (emp_cli_channel_type_file_signal_callback_file_transfer_state_changed)
          tp_file_state_changed_cb,
      tp_file,
      NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_transferred_bytes_changed (
      TP_PROXY (tp_file->priv->channel),
      (emp_cli_channel_type_file_signal_callback_transferred_bytes_changed)
          tp_file_transferred_bytes_changed_cb,
      tp_file,
      NULL, NULL, NULL);

  handle = tp_channel_get_handle (tp_file->priv->channel, NULL);
  tp_file->priv->contact = empathy_contact_factory_get_from_handle (
      tp_file->priv->factory, tp_file->priv->account, (guint) handle);

  tp_cli_dbus_properties_call_get_all (tp_file->priv->channel,
      -1, EMP_IFACE_CHANNEL_TYPE_FILE, tp_file_get_all_cb, tp_file, NULL, NULL);

  return file_obj;
}

static void
tp_file_get_property (GObject *object,
                      guint param_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpFile *tp_file;

  tp_file = EMPATHY_TP_FILE (object);

  switch (param_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, tp_file->priv->account);
        break;
      case PROP_CHANNEL:
        g_value_set_object (value, tp_file->priv->channel);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
tp_file_channel_set_dbus_property (gpointer proxy,
                                   const gchar *property,
                                   const GValue *value)
{
        DEBUG ("Setting %s property", property);
        tp_cli_dbus_properties_call_set (TP_PROXY (proxy), -1,
            EMP_IFACE_CHANNEL_TYPE_FILE, property, value,
            NULL, NULL, NULL, NULL);
}


static void
tp_file_set_property (GObject *object,
                      guint param_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpFile *tp_file = (EmpathyTpFile *) object;
  switch (param_id)
    {
      case PROP_ACCOUNT:
        tp_file->priv->account = g_object_ref (g_value_get_object (value));
        break;
      case PROP_CHANNEL:
        tp_file->priv->channel = g_object_ref (g_value_get_object (value));
        break;
      case PROP_STATE:
        tp_file->priv->state = g_value_get_uint (value);
        break;
      case PROP_INCOMING:
        tp_file->priv->incoming = g_value_get_boolean (value);
        break;
      case PROP_FILENAME:
        g_free (tp_file->priv->filename);
        tp_file->priv->filename = g_value_dup_string (value);
        tp_file_channel_set_dbus_property (tp_file->priv->channel,
            "Filename", value);
        break;
      case PROP_SIZE:
        tp_file->priv->size = g_value_get_uint64 (value);
        tp_file_channel_set_dbus_property (tp_file->priv->channel,
            "Size", value);
        break;
      case PROP_CONTENT_TYPE:
        tp_file_channel_set_dbus_property (tp_file->priv->channel,
            "ContentType", value);
        g_free (tp_file->priv->content_type);
        tp_file->priv->content_type = g_value_dup_string (value);
        break;
      case PROP_CONTENT_HASH:
        tp_file_channel_set_dbus_property (tp_file->priv->channel,
            "ContentHash", value);
        g_free (tp_file->priv->content_hash);
        tp_file->priv->content_hash = g_value_dup_string (value);
        break;
      case PROP_IN_STREAM:
        if (tp_file->priv->in_stream)
          g_object_unref (tp_file->priv->in_stream);
        tp_file->priv->in_stream = g_object_ref (g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

/**
 * empathy_tp_file_new:
 * @account: the #McAccount for the channel
 * @channel: a Telepathy channel
 *
 * Creates a new #EmpathyTpFile wrapping @channel.
 *
 * Returns: a new #EmpathyTpFile
 */
EmpathyTpFile *
empathy_tp_file_new (McAccount *account,
                     TpChannel *channel)
{
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TP_IS_CHANNEL (chnanel), NULL);

  return g_object_new (EMPATHY_TYPE_TP_FILE,
      "account", account,
      "channel", channel,
      NULL);
}

/**
 * empathy_tp_file_get_id:
 * @tp_file: an #EmpathyTpFile
 *
 * Returns the ID of @tp_file.
 *
 * Returns: the ID
 */
const gchar *
empathy_tp_file_get_id (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  return tp_file->priv->id;
}

/**
 * empathy_tp_file_get_channel
 * @tp_file: an #EmpathyTpFile
 *
 * Returns the Telepathy file transfer channel
 *
 * Returns: the #TpChannel
 */
TpChannel *
empathy_tp_file_get_channel (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  return tp_file->priv->channel;
}

static void
tp_file_method_cb (TpProxy *proxy,
                   const GValue *address,
                   const GError *error,
                   gpointer user_data,
                   GObject *weak_object)
{
  EmpathyTpFile *tp_file = (EmpathyTpFile *) user_data;

  if (error)
    {
      DEBUG ("Error: %s", error->message);
      return;
    }

  if (tp_file->priv->unix_socket_path)
    g_free (tp_file->priv->unix_socket_path);

  tp_file->priv->unix_socket_path = g_value_dup_string (address);

  DEBUG ("Got unix socket path: %s", tp_file->priv->unix_socket_path);
}


/**
 * empathy_tp_file_accept:
 * @tp_file: an #EmpathyTpFile
 *
 * Accepts a file transfer that's in the "local pending" state (i.e.
 * EMP_FILE_TRANSFER_STATE_LOCAL_PENDING).
 */
void
empathy_tp_file_accept (EmpathyTpFile *tp_file,
                        guint64 offset)
{
  GValue nothing = { 0 };

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  g_return_if_fail (tp_file->priv->out_stream != NULL);

  DEBUG ("Accepting file: filename=%s", tp_file->priv->filename);

  g_value_init (&nothing, G_TYPE_STRING);
  g_value_set_string (&nothing, "");

  emp_cli_channel_type_file_call_accept_file (TP_PROXY (tp_file->priv->channel),
      -1, TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
      &nothing, offset, tp_file_method_cb, tp_file, NULL, NULL);
}

/**
 * empathy_tp_file_offer:
 * @tp_file: an #EmpathyTpFile
 *
 * Offers a file transfer that's in the "not offered" state (i.e.
 * EMP_FILE_TRANSFER_STATE_NOT_OFFERED).
 */
void
empathy_tp_file_offer (EmpathyTpFile *tp_file)
{
  GValue nothing = { 0 };

  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));

  g_value_init (&nothing, G_TYPE_STRING);
  g_value_set_string (&nothing, "");

  emp_cli_channel_type_file_call_offer_file (
      TP_PROXY (tp_file->priv->channel), -1,
      TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
      &nothing, tp_file_method_cb, tp_file, NULL, NULL);
}

EmpathyContact *
empathy_tp_file_get_contact (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->contact;
}

GInputStream *
empathy_tp_file_get_input_stream (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->in_stream;
}

GOutputStream *
empathy_tp_file_get_output_stream (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->out_stream;
}

const gchar *
empathy_tp_file_get_filename (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->filename;
}

gboolean
empathy_tp_file_get_incoming (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->incoming;
}

EmpFileTransferState
empathy_tp_file_get_state (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->state;
}

EmpFileTransferStateChangeReason
empathy_tp_file_get_state_change_reason (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  g_return_val_if_fail (tp_file->priv->state_change_reason >= 0,
      EMP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE);

  return tp_file->priv->state_change_reason;
}

guint64
empathy_tp_file_get_size (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->size;
}

guint64
empathy_tp_file_get_transferred_bytes (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);
  return tp_file->priv->transferred_bytes;
}

gint
empathy_tp_file_get_remaining_time (EmpathyTpFile *tp_file)
{
  gint64 curr_time, elapsed_time;
  gdouble time_per_byte;
  gdouble remaining_time;

  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  if (tp_file->priv->size == EMPATHY_TP_FILE_UNKNOWN_SIZE)
    return -1;

  if (tp_file->priv->transferred_bytes == tp_file->priv->size)
    return 0;

  curr_time = get_time_msec ();
  elapsed_time = curr_time - tp_file->priv->start_time;
  time_per_byte = (gdouble) elapsed_time /
      (gdouble) tp_file->priv->transferred_bytes;
  remaining_time = (time_per_byte * (tp_file->priv->size -
      tp_file->priv->transferred_bytes)) / 1000;

  return (gint) (remaining_time + 0.5);
}

void
empathy_tp_file_cancel (EmpathyTpFile *tp_file)
{
  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  tp_cli_channel_call_close (tp_file->priv->channel, -1, NULL, NULL, NULL, NULL);

  g_cancellable_cancel (tp_file->priv->cancellable);
}

void
empathy_tp_file_set_input_stream (EmpathyTpFile *tp_file,
                                  GInputStream *in_stream)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  g_return_if_fail (G_IS_INPUT_STREAM (in_stream));

  if (tp_file->priv->in_stream == in_stream)
    return;

  if (tp_file->priv->incoming)
    g_warning ("Setting an input stream for incoming file "
         "transfers is useless");

  if (tp_file->priv->in_stream)
    g_object_unref (tp_file->priv->in_stream);

  if (in_stream)
    g_object_ref (in_stream);

  tp_file->priv->in_stream = in_stream;

  g_object_notify (G_OBJECT (tp_file), "in-stream");
}

void
empathy_tp_file_set_output_stream (EmpathyTpFile *tp_file,
                                   GOutputStream *out_stream)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  g_return_if_fail (G_IS_INPUT_STREAM (in_stream));

  if (tp_file->priv->out_stream == out_stream)
    return;

  if (!tp_file->priv->incoming)
    g_warning ("Setting an output stream for outgoing file "
         "transfers is useless");

  if (tp_file->priv->out_stream)
    g_object_unref (tp_file->priv->out_stream);

  if (out_stream)
    g_object_ref (out_stream);

  tp_file->priv->out_stream = out_stream;
}

void
empathy_tp_file_set_filename (EmpathyTpFile *tp_file,
                              const gchar *filename)
{
  g_return_if_fail (EMPATHY_IS_TP_FILE (tp_file));
  g_return_if_fail (filename != NULL);

  if (tp_file->priv->filename && strcmp (filename,
      tp_file->priv->filename) == 0)
    return;

  g_free (tp_file->priv->filename);
  tp_file->priv->filename = g_strdup (filename);

  g_object_notify (G_OBJECT (tp_file), "filename");
}

static void
empathy_tp_file_class_init (EmpathyTpFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tp_file_finalize;
  object_class->constructor = tp_file_constructor;
  object_class->get_property = tp_file_get_property;
  object_class->set_property = tp_file_set_property;

  /* Construct-only properties */
  g_object_class_install_property (object_class,
      PROP_ACCOUNT,
      g_param_spec_object ("account",
          "channel Account",
          "The account associated with the channel",
          MC_TYPE_ACCOUNT,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
      PROP_CHANNEL,
      g_param_spec_object ("channel",
          "telepathy channel",
          "The file transfer channel",
          TP_TYPE_CHANNEL,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
      PROP_STATE,
      g_param_spec_uint ("state",
          "state of the transfer",
          "The file transfer state",
          0,
          G_MAXUINT,
          G_MAXUINT,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
      PROP_INCOMING,
      g_param_spec_boolean ("incoming",
          "incoming",
          "Whether the transfer is incoming",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
      PROP_FILENAME,
      g_param_spec_string ("filename",
          "name of the transfer",
          "The file transfer filename",
          "",
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_SIZE,
      g_param_spec_uint64 ("size",
          "size of the file",
          "The file transfer size",
          0,
          G_MAXUINT64,
          G_MAXUINT64,
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_CONTENT_TYPE,
      g_param_spec_string ("content-type",
          "file transfer content-type",
          "The file transfer content-type",
          "",
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_CONTENT_HASH_TYPE,
      g_param_spec_uint ("content-hash-type",
          "file transfer hash type",
          "The type of the file transfer hash",
          0,
          G_MAXUINT,
          0,
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_CONTENT_HASH,
      g_param_spec_string ("content-hash",
          "file transfer hash",
          "The hash of the transfer's contents",
          "",
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_TRANSFERRED_BYTES,
      g_param_spec_uint64 ("transferred-bytes",
          "bytes transferred",
          "The number of bytes transferred",
          0,
          G_MAXUINT64,
          0,
          G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_IN_STREAM,
      g_param_spec_object ("in-stream",
          "transfer input stream",
          "The input stream for file transfer",
          G_TYPE_INPUT_STREAM,
          G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (EmpathyTpFilePriv));
}
