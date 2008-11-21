/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 *   @author: Xavier Claessens <xclaesse@gmail.com>
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

#include "empathy-file.h"
#include "empathy-contact-factory.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

/**
 * SECTION:empathy-file
 * @short_description: File channel
 * @see_also: #EmpathyFile, #EmpathyContact, empathy_send_file()
 * @include: libempthy/empathy-file.h
 *
 * The #EmpathyFile object represents a Telepathy file channel.
 */

/**
 * EMPATHY_FILE_UNKNOWN_SIZE:
 *
 * Value used for the "size" or "estimated-size" properties when the size of
 * the transferred file is unknown.
 */

static void empathy_file_class_init (EmpathyFileClass *klass);
static void empathy_file_init (EmpathyFile *file);
static void file_finalize (GObject *object);
static GObject *file_constructor (GType type, guint n_props,
    GObjectConstructParam *props);
static void file_get_property (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec);
static void file_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec);
static void file_destroy_cb (TpChannel *file_chan, EmpathyFile *file);
static void file_closed_cb (TpChannel *file_chan, EmpathyFile *file,
    GObject *weak_object);
static void file_state_changed_cb (DBusGProxy *file_iface, guint state,
    guint reason, EmpathyFile *file);
static void file_transferred_bytes_changed_cb (TpProxy *proxy, guint64 count,
    EmpathyFile *file, GObject *weak_object);
static void copy_stream (GInputStream *in, GOutputStream *out,
    GCancellable *cancellable);

/* EmpathyFile object */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
           EMPATHY_TYPE_FILE, EmpathyFilePriv))

typedef struct _EmpathyFilePriv  EmpathyFilePriv;

struct _EmpathyFilePriv {
  EmpathyContactFactory *factory;
  McAccount *account;
  gchar *id;
  MissionControl *mc;
  TpChannel *channel;

  EmpathyFile *cached_empathy_file;
  EmpathyContact *contact;
  GInputStream *in_stream;
  GOutputStream *out_stream;
  gchar *filename;
  EmpFileTransferDirection direction;
  EmpFileTransferState state;
  EmpFileTransferStateChangeReason state_change_reason;
  guint64 size;
  guint64 transferred_bytes;
  gint64 start_time;
  gchar *unix_socket_path;
  gchar *content_md5;
  gchar *content_type;
  gchar *description;
  GCancellable *cancellable;
};

enum {
  PROP_0,
  PROP_ACCOUNT,
  PROP_CHANNEL,
  PROP_STATE,
  PROP_DIRECTION,
  PROP_FILENAME,
  PROP_SIZE,
  PROP_CONTENT_TYPE,
  PROP_TRANSFERRED_BYTES,
  PROP_CONTENT_MD5,
  PROP_IN_STREAM,
};

G_DEFINE_TYPE (EmpathyFile, empathy_file, G_TYPE_OBJECT);

static void
empathy_file_class_init (EmpathyFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = file_finalize;
  object_class->constructor = file_constructor;
  object_class->get_property = file_get_property;
  object_class->set_property = file_set_property;

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
      PROP_DIRECTION,
      g_param_spec_uint ("direction",
          "direction of the transfer",
          "The file transfer direction",
          0,
          G_MAXUINT,
          G_MAXUINT,
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
      PROP_CONTENT_MD5,
      g_param_spec_string ("content-md5",
          "file transfer md5sum",
          "The md5 sum of the transfer's contents",
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
  EmpathyFile *file;

  file = EMPATHY_FILE (object);
  priv = GET_PRIV (file);

  if (priv->channel)
    {
      DEBUG ("Closing channel..");
      g_signal_handlers_disconnect_by_func (priv->channel,
          file_destroy_cb, object);
      tp_cli_channel_run_close (priv->channel, -1, NULL, NULL);
      if (G_IS_OBJECT (priv->channel))
        g_object_unref (priv->channel);
    }

  if (priv->factory)
    {
      g_object_unref (priv->factory);
    }
  if (priv->account)
    {
      g_object_unref (priv->account);
    }
  if (priv->mc)
    {
      g_object_unref (priv->mc);
    }

  g_free (priv->id);
  g_free (priv->filename);
  g_free (priv->unix_socket_path);
  g_free (priv->description);
  g_free (priv->content_md5);
  g_free (priv->content_type);

  if (priv->in_stream)
    g_object_unref (priv->in_stream);

  if (priv->out_stream)
    g_object_unref (priv->out_stream);

  if (priv->contact)
    g_object_unref (priv->contact);

  if (priv->cancellable)
    g_object_unref (priv->cancellable);

  G_OBJECT_CLASS (empathy_file_parent_class)->finalize (object);
}

static GObject *
file_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
  GObject *file;
  EmpathyFilePriv *priv;
  GError *error = NULL;
  GHashTable *properties;
  TpHandle handle;

  file = G_OBJECT_CLASS (empathy_file_parent_class)->constructor (type, n_props,
      props);

  priv = GET_PRIV (file);

  priv->factory = empathy_contact_factory_new ();
  priv->mc = empathy_mission_control_new ();

  tp_cli_channel_connect_to_closed (priv->channel,
      (tp_cli_channel_signal_callback_closed) file_closed_cb,
      file,
      NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_file_transfer_state_changed (
      TP_PROXY (priv->channel),
      (emp_cli_channel_type_file_signal_callback_file_transfer_state_changed)
          file_state_changed_cb,
      file,
      NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_transferred_bytes_changed (
      TP_PROXY (priv->channel),
      (emp_cli_channel_type_file_signal_callback_transferred_bytes_changed)
          file_transferred_bytes_changed_cb,
      file,
      NULL, NULL, NULL);


  handle = tp_channel_get_handle (priv->channel, NULL);
  priv->contact = empathy_contact_factory_get_from_handle (priv->factory,
      priv->account,
      (guint) handle);

  if (!tp_cli_dbus_properties_run_get_all (priv->channel,
      -1, EMP_IFACE_CHANNEL_TYPE_FILE, &properties, &error, NULL))
    {
      DEBUG ("Failed to get properties: %s",
          error ? error->message : "No error given");
      g_clear_error (&error);
      return NULL;
    }

  priv->size = g_value_get_uint64 (g_hash_table_lookup (properties, "Size"));

  priv->state = g_value_get_uint (g_hash_table_lookup (properties, "State"));

  /* Invalid reason, so empathy_file_get_state_change_reason() can give
   * a warning if called for a not closed file transfer. */
  priv->state_change_reason = -1;

  priv->transferred_bytes = g_value_get_uint64 (g_hash_table_lookup (
      properties, "TransferredBytes"));

  priv->filename = g_value_dup_string (g_hash_table_lookup (properties,
      "Filename"));

  priv->content_md5 = g_value_dup_string (g_hash_table_lookup (properties,
      "ContentMD5"));

  priv->description = g_value_dup_string (g_hash_table_lookup (properties,
      "Description"));

  priv->unix_socket_path = g_value_dup_string (g_hash_table_lookup (properties,
      "SocketPath"));

  priv->direction = g_value_get_uint (g_hash_table_lookup (properties,
      "Direction"));

  g_hash_table_destroy (properties);

  return file;
}

static void
file_get_property (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec)
{
  EmpathyFilePriv *priv;
  EmpathyFile *file;

  priv = GET_PRIV (object);
  file = EMPATHY_FILE (object);

  switch (param_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, priv->account);
        break;
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
file_channel_set_dbus_property (gpointer proxy, const gchar *property,
    const GValue *value)
{
        DEBUG ("Setting %s property", property);
        tp_cli_dbus_properties_run_set (TP_PROXY (proxy),
            -1,
            EMP_IFACE_CHANNEL_TYPE_FILE,
            property,
            value,
            NULL, NULL);
        DEBUG ("done");
}


static void
file_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_ACCOUNT:
        priv->account = g_object_ref (g_value_get_object (value));
        break;
      case PROP_CHANNEL:
        priv->channel = g_object_ref (g_value_get_object (value));
        break;
      case PROP_STATE:
        priv->state = g_value_get_uint (value);
        break;
      case PROP_DIRECTION:
        priv->direction = g_value_get_uint (value);
        break;
      case PROP_FILENAME:
        g_free (priv->filename);
        priv->filename = g_value_dup_string (value);
        file_channel_set_dbus_property (priv->channel, "Filename", value);
        break;
      case PROP_SIZE:
        priv->size = g_value_get_uint64 (value);
        file_channel_set_dbus_property (priv->channel, "Size", value);
        break;
      case PROP_CONTENT_TYPE:
        file_channel_set_dbus_property (priv->channel, "ContentType", value);
        g_free (priv->content_type);
        priv->content_type = g_value_dup_string (value);
        break;
      case PROP_CONTENT_MD5:
        file_channel_set_dbus_property (priv->channel, "ContentMD5", value);
        g_free (priv->content_md5);
        priv->content_md5 = g_value_dup_string (value);
        break;
      case PROP_IN_STREAM:
        if (priv->in_stream)
          g_object_unref (priv->in_stream);
        priv->in_stream = g_object_ref (g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

/**
 * empathy_file_new:
 * @account: the #McAccount for the channel
 * @channel: a Telepathy channel
 *
 * Creates a new #EmpathyFile wrapping @channel.
 *
 * Returns: a new #EmpathyFile
 */
EmpathyFile *
empathy_file_new (McAccount *account, TpChannel *channel)
{
  return g_object_new (EMPATHY_TYPE_FILE,
      "account", account,
      "channel", channel,
      NULL);
}

/**
 * empathy_file_get_id:
 * @tp_file: an #EmpathyFile
 *
 * Returns the ID of @file.
 *
 * Returns: the ID
 */
const gchar *
empathy_file_get_id (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);

  return priv->id;
}

/**
 * empathy_file_get_channel
 * @file: an #EmpathyFile
 *
 * Returns the Telepathy file transfer channel
 *
 * Returns: the #TpChannel
 */
TpChannel *
empathy_file_get_channel (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_FILE (file), NULL);

  priv = GET_PRIV (file);

  return priv->channel;
}

static void
file_destroy_cb (TpChannel *file_channel, EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  DEBUG ("Channel Closed or CM crashed");

  g_object_unref (priv->channel);
  priv->channel = NULL;
}

static void
file_closed_cb (TpChannel *file_channel, EmpathyFile *file, GObject *weak_object)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  /* The channel is closed, do just like if the proxy was destroyed */
  g_signal_handlers_disconnect_by_func (priv->channel,
      file_destroy_cb,
      file);
  file_destroy_cb (file_channel, file);
}

static gint64
get_time_msec (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);
  return ((gint64) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

static gint
_get_local_socket (EmpathyFile *file)
{
  gint fd;
  size_t path_len;
  struct sockaddr_un addr;
  EmpathyFilePriv *priv;
  GValue *socket_path;

  priv = GET_PRIV (file);

  /* TODO: This could probably be a little nicer. */
  tp_cli_dbus_properties_run_get (priv->channel,
      -1,
      EMP_IFACE_CHANNEL_TYPE_FILE,
      "SocketPath",
      &socket_path,
      NULL,
      NULL);

  if (priv->unix_socket_path)
    g_free (priv->unix_socket_path);

  priv->unix_socket_path = g_value_dup_string (socket_path);
  g_value_unset (socket_path);

  if (G_STR_EMPTY (priv->unix_socket_path))
    return -1;

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  path_len = strlen (priv->unix_socket_path);
  strncpy (addr.sun_path, priv->unix_socket_path, path_len);

  if (connect (fd, (struct sockaddr*) &addr,
      sizeof (addr)) < 0)
    {
      close (fd);
      return -1;
    }

  return fd;
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
  GValue *address;
  GValue nothing = { 0 };
  GError *error = NULL;

  g_return_if_fail (EMPATHY_IS_FILE (file));

  priv = GET_PRIV (file);

  g_return_if_fail (priv->out_stream != NULL);

  DEBUG ("Accepting file: filename=%s", priv->filename);

  g_value_init (&nothing, G_TYPE_STRING);
  g_value_set_string (&nothing, "");

  if (!emp_cli_channel_type_file_run_accept_file (TP_PROXY (priv->channel),
      -1, TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
      &nothing, &address, &error, NULL))
    {
      DEBUG ("Accept error: %s",
          error ? error->message : "No message given");
      g_clear_error (&error);
    }

  if (priv->unix_socket_path)
    g_free (priv->unix_socket_path);

  priv->unix_socket_path = g_value_dup_string (address);
  g_value_unset (address);

  DEBUG ("Got unix socket path: %s", priv->unix_socket_path);
}

static void
receive_file (EmpathyFile *file)
{
  EmpathyFilePriv *priv;
  GInputStream *socket_stream;
  gint socket_fd;

  priv = GET_PRIV (file);

  socket_fd = _get_local_socket (file);

  if (socket_fd < 0)
    return;

  socket_stream = g_unix_input_stream_new (socket_fd, TRUE);

  priv->cancellable = g_cancellable_new ();

  copy_stream (socket_stream, priv->out_stream, priv->cancellable);

  g_object_unref (socket_stream);
}


static void
send_file (EmpathyFile *file)
{
  gint socket_fd;
  GOutputStream *socket_stream;
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  DEBUG ("Sending file content: filename=%s",
           priv->filename);

  g_return_if_fail (priv->in_stream);

  socket_fd = _get_local_socket (file);
  if (socket_fd < 0)
    {
      DEBUG ("failed to get local socket fd");
      return;
    }
  DEBUG ("got local socket fd");
  socket_stream = g_unix_output_stream_new (socket_fd, TRUE);

  priv->cancellable = g_cancellable_new ();

  copy_stream (priv->in_stream, socket_stream, priv->cancellable);

  g_object_unref (socket_stream);
}

static void
file_state_changed_cb (DBusGProxy *file_iface, EmpFileTransferState state,
    EmpFileTransferStateChangeReason reason, EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  DEBUG ("File transfer state changed: filename=%s, "
      "old state=%u, state=%u, reason=%u",
      priv->filename, priv->state, state, reason);

  if (state == EMP_FILE_TRANSFER_STATE_OPEN)
    priv->start_time = get_time_msec ();

  DEBUG ("state = %u, direction = %u, in_stream = %s, out_stream = %s",
      state, priv->direction,
      priv->in_stream ? "present" : "not present",
      priv->out_stream ? "present" : "not present");

  if (state == EMP_FILE_TRANSFER_STATE_OPEN &&
      priv->direction == EMP_FILE_TRANSFER_DIRECTION_OUTGOING &&
      priv->in_stream)
    send_file (file);
  else if (state == EMP_FILE_TRANSFER_STATE_OPEN &&
      priv->direction == EMP_FILE_TRANSFER_DIRECTION_INCOMING &&
      priv->out_stream)
      receive_file (file);

  priv->state = state;
  priv->state_change_reason = reason;

  g_object_notify (G_OBJECT (file), "state");
}

static void
file_transferred_bytes_changed_cb (TpProxy *proxy,
    guint64 count, EmpathyFile *file, GObject *weak_object)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  if (priv->transferred_bytes == count)
    return;

  priv->transferred_bytes = count;

  g_object_notify (G_OBJECT (file), "transferred-bytes");
}

EmpathyContact *
empathy_file_get_contact (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->contact;
}

GInputStream *
empathy_file_get_input_stream (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->in_stream;
}

GOutputStream *
empathy_file_get_output_stream (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->out_stream;
}

const gchar *
empathy_file_get_filename (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->filename;
}

EmpFileTransferDirection
empathy_file_get_direction (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->direction;
}

EmpFileTransferState
empathy_file_get_state (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->state;
}

EmpFileTransferStateChangeReason
empathy_file_get_state_change_reason (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  g_return_val_if_fail (priv->state_change_reason >= 0,
      EMP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE);

  return priv->state_change_reason;
}

guint64
empathy_file_get_size (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->size;
}

guint64
empathy_file_get_transferred_bytes (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  return priv->transferred_bytes;
}

gint
empathy_file_get_remaining_time (EmpathyFile *file)
{
  EmpathyFilePriv *priv;
  gint64 curr_time, elapsed_time;
  gdouble time_per_byte;
  gdouble remaining_time;

  priv = GET_PRIV (file);

  if (priv->size == EMPATHY_FILE_UNKNOWN_SIZE)
    return -1;

  if (priv->transferred_bytes == priv->size)
    return 0;

  curr_time = get_time_msec ();
  elapsed_time = curr_time - priv->start_time;
  time_per_byte = (gdouble) elapsed_time / (gdouble) priv->transferred_bytes;
  remaining_time = (time_per_byte * (priv->size - priv->transferred_bytes)) / 1000;

  return (gint) (remaining_time + 0.5);
}

void
empathy_file_cancel (EmpathyFile *file)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  tp_cli_channel_run_close (priv->channel, -1, NULL, NULL);

  g_cancellable_cancel (priv->cancellable);
}

void
empathy_file_set_input_stream (EmpathyFile *file,
    GInputStream *in_stream)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  if (priv->in_stream == in_stream)
    return;

  if (priv->direction == EMP_FILE_TRANSFER_DIRECTION_INCOMING)
    g_warning ("Setting an input stream for incoming file "
         "transfers is useless");

  if (priv->in_stream)
    g_object_unref (priv->in_stream);

  if (in_stream)
    g_object_ref (in_stream);

  priv->in_stream = in_stream;

  g_object_notify (G_OBJECT (file), "in-stream");
}

void
empathy_file_set_output_stream (EmpathyFile *file, GOutputStream *out_stream)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  if (priv->out_stream == out_stream)
    return;

  if (priv->direction == EMP_FILE_TRANSFER_DIRECTION_OUTGOING)
    g_warning ("Setting an output stream for outgoing file "
         "transfers is useless");

  if (priv->out_stream)
    g_object_unref (priv->out_stream);

  if (out_stream)
    g_object_ref (out_stream);

  priv->out_stream = out_stream;
}

void
empathy_file_set_filename (EmpathyFile *file,
    const gchar *filename)
{
  EmpathyFilePriv *priv;

  priv = GET_PRIV (file);

  g_return_if_fail (filename != NULL);

  if (priv->filename && strcmp (filename, priv->filename) == 0)
    return;

  g_free (priv->filename);
  priv->filename = g_strdup (filename);

  g_object_notify (G_OBJECT (file), "filename");
}

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
io_error (CopyData *copy, GError *error)
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
close_done (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CopyData *copy = user_data;

  g_object_unref (source_object);
  free_copy_data_if_closed (copy);
}

static void
write_done_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
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
read_done_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
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
copy_stream (GInputStream *in, GOutputStream *out, GCancellable *cancellable)
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
