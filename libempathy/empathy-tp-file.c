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

#include "empathy-tp-file.h"
#include "empathy-file.h"
#include "empathy-tp-file-priv.h"
#include "empathy-contact-factory.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FT
#include "empathy-debug.h"

/**
 * SECTION:empathy-tp-file
 * @short_description: File channel
 * @see_also: #EmpathyFile, #EmpathyContact, empathy_send_file()
 * @include: libempthy/empathy-tp-file.h
 *
 * The #EmpathyTpFile object represents a Telepathy file channel.
 */

static void      empathy_tp_file_class_init           (EmpathyTpFileClass      *klass);
static void      empathy_tp_file_init                 (EmpathyTpFile           *tp_file);
static void      tp_file_finalize                     (GObject               *object);
static GObject * tp_file_constructor                  (GType                  type,
                 guint                  n_props,
                 GObjectConstructParam *props);
static void      tp_file_get_property                 (GObject               *object,
                 guint                  param_id,
                 GValue                *value,
                 GParamSpec            *pspec);
static void      tp_file_set_property                 (GObject               *object,
                 guint                  param_id,
                 const GValue          *value,
                 GParamSpec            *pspec);
static void      tp_file_destroy_cb                   (TpChannel             *file_chan,
                 EmpathyTpFile           *tp_file);
static void      tp_file_closed_cb                    (TpChannel             *file_chan,
                 EmpathyTpFile           *tp_file,
                 GObject                 *weak_object);
static void      tp_file_state_changed_cb             (DBusGProxy            *file_iface,
                 guint                  state,
                 guint                  reason,
                 EmpathyTpFile           *tp_file);
static void      tp_file_transferred_bytes_changed_cb (DBusGProxy            *file_iface,
                 guint64                transferred_bytes,
                 EmpathyTpFile           *tp_file);
static void      copy_stream                        (GInputStream          *in,
                 GOutputStream         *out,
                 GCancellable          *cancellable);

/* EmpathyTpFile object */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
           EMPATHY_TYPE_TP_FILE, EmpathyTpFilePriv))

typedef struct _EmpathyTpFilePriv  EmpathyTpFilePriv;

struct _EmpathyTpFilePriv {
  EmpathyContactFactory *factory;
  McAccount             *account;
  gchar                 *id;
  MissionControl        *mc;

  TpChannel             *channel;

  /* Previously in File/FT struct */
  EmpathyFile                            *cached_empathy_file;
  EmpathyContact                         *contact;
  GInputStream                           *in_stream;
  GOutputStream                          *out_stream;
  gchar                                  *filename;
  EmpFileTransferDirection                direction;
  EmpFileTransferState                    state;
  EmpFileTransferStateChangeReason        state_change_reason;
  guint64                                 size;
  guint64                                 transferred_bytes;
  gint64                                  start_time;
  gchar                                  *unix_socket_path;
  gchar                                  *content_md5;
  gchar                                  *content_type;
  gchar                                  *description;
  GCancellable                           *cancellable;
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
  PROP_CONTENT_MD5,
  PROP_IN_STREAM,
};

G_DEFINE_TYPE (EmpathyTpFile, empathy_tp_file, G_TYPE_OBJECT);

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
      PROP_IN_STREAM,
      g_param_spec_object ("in-stream",
          "transfer input stream",
          "The input stream for file transfer",
          G_TYPE_INPUT_STREAM,
          G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (EmpathyTpFilePriv));
}

static void
empathy_tp_file_init (EmpathyTpFile *tp_file)
{
}

static void
tp_file_finalize (GObject *object)
{
  EmpathyTpFilePriv *priv;
  EmpathyTpFile     *tp_file;

  tp_file = EMPATHY_TP_FILE (object);
  priv = GET_PRIV (tp_file);

  if (priv->channel)
    {
      DEBUG ("Closing channel..");
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_file_destroy_cb, object);
      tp_cli_channel_run_close (priv->channel, -1, NULL, NULL);
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

  G_OBJECT_CLASS (empathy_tp_file_parent_class)->finalize (object);
}

static GObject *
tp_file_constructor (GType                  type,
       guint                  n_props,
       GObjectConstructParam *props)
{
  GObject         *tp_file;
  EmpathyTpFilePriv *priv;
  GError            *error = NULL;
  GHashTable        *properties;

  tp_file = G_OBJECT_CLASS (empathy_tp_file_parent_class)->constructor (type, n_props, props);

  priv = GET_PRIV (tp_file);

  priv->factory = empathy_contact_factory_new ();
  priv->mc = empathy_mission_control_new ();

  tp_cli_channel_connect_to_closed (priv->channel,
                                    (tp_cli_channel_signal_callback_closed) tp_file_closed_cb,
                                    tp_file,
                                    NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_file_transfer_state_changed (TP_PROXY (priv->channel),
                                                                    (emp_cli_channel_type_file_signal_callback_file_transfer_state_changed) tp_file_state_changed_cb,
                                                                    tp_file,
                                                                    NULL, NULL, NULL);

  emp_cli_channel_type_file_connect_to_transferred_bytes_changed (TP_PROXY (priv->channel),
                                                                  (emp_cli_channel_type_file_signal_callback_transferred_bytes_changed) tp_file_transferred_bytes_changed_cb,
                                                                  tp_file,
                                                                  NULL, NULL, NULL);

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

  priv->transferred_bytes = g_value_get_uint64 (g_hash_table_lookup (properties, "TransferredBytes"));

  priv->filename = g_value_dup_string (g_hash_table_lookup (properties, "Filename"));

  priv->content_md5 = g_value_dup_string (g_hash_table_lookup (properties, "ContentMD5"));

  priv->description = g_value_dup_string (g_hash_table_lookup (properties, "Description"));

  priv->unix_socket_path = g_value_dup_string (g_hash_table_lookup (properties, "SocketPath"));

  priv->direction = g_value_get_uint (g_hash_table_lookup (properties, "Direction"));

  g_hash_table_destroy (properties);

  return tp_file;
}

static void
tp_file_get_property (GObject    *object,
        guint       param_id,
        GValue     *value,
        GParamSpec *pspec)
{
  EmpathyTpFilePriv *priv;
  EmpathyTpFile     *tp_file;

  priv = GET_PRIV (object);
  tp_file = EMPATHY_TP_FILE (object);

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
tp_file_channel_set_dbus_property (gpointer proxy,
                                   const gchar *property,
                                   const GValue *value)
{
        DEBUG ("Setting %s property", property);
        tp_cli_dbus_properties_run_set (TP_PROXY (proxy),
                                        -1,
                                        EMP_IFACE_CHANNEL_TYPE_FILE,
                                        property,
                                        value,
                                        NULL,
                                        NULL);
        DEBUG ("done");
}


static void
tp_file_set_property (GObject      *object,
        guint         param_id,
        const GValue *value,
        GParamSpec   *pspec)
{
  EmpathyTpFilePriv *priv;

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
        tp_file_channel_set_dbus_property (priv->channel, "Filename", value);
        break;
      case PROP_SIZE:
        priv->size = g_value_get_uint64 (value);
        tp_file_channel_set_dbus_property (priv->channel, "Size", value);
        break;
      case PROP_CONTENT_TYPE:
        tp_file_channel_set_dbus_property (priv->channel, "ContentType", value);
        g_free (priv->content_type);
        priv->content_type = g_value_dup_string (value);
        break;
      case PROP_CONTENT_MD5:
        tp_file_channel_set_dbus_property (priv->channel, "ContentMD5", value);
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
  EmpathyTpFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  priv = GET_PRIV (tp_file);

  return priv->id;
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
  EmpathyTpFilePriv *priv;

  g_return_val_if_fail (EMPATHY_IS_TP_FILE (tp_file), NULL);

  priv = GET_PRIV (tp_file);

  return priv->channel;
}

static void
tp_file_destroy_cb (TpChannel *file_channel, EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  DEBUG ("Channel Closed or CM crashed");

  g_object_unref  (priv->channel);
  priv->channel = NULL;
}

static void
tp_file_closed_cb (TpChannel *file_channel, EmpathyTpFile *tp_file, GObject *weak_object)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  /* The channel is closed, do just like if the proxy was destroyed */
  g_signal_handlers_disconnect_by_func (priv->channel,
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
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  if (priv->unix_socket_path == NULL)
    return -1;

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  path_len = strlen (priv->unix_socket_path);
  strncpy (addr.sun_path, priv->unix_socket_path, path_len);

  if (connect (fd, (struct sockaddr*) &addr,
      G_STRUCT_OFFSET (struct sockaddr_un, sun_path) + path_len) < 0)
    {
      close (fd);
      return -1;
    }

  return fd;
}

static void
send_file (EmpathyTpFile *tp_file)
{
  gint           socket_fd;
  GOutputStream *socket_stream;
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  DEBUG ("Sending file content: filename=%s",
           priv->filename);

  g_return_if_fail (priv->in_stream);

  socket_fd = _get_local_socket (tp_file);
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
tp_file_state_changed_cb (DBusGProxy *file_iface,
    EmpFileTransferState state,
    EmpFileTransferStateChangeReason reason,
    EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  DEBUG ("File transfer state changed: filename=%s, "
           "old state=%u, state=%u, reason=%u",
           priv->filename, priv->state, state, reason);

  if (state == EMP_FILE_TRANSFER_STATE_OPEN)
    priv->start_time = get_time_msec ();

  DEBUG ("state = %u, direction = %u, in_stream = %s",
         state, priv->direction, priv->in_stream ? "present" : "not present");

  if (state == EMP_FILE_TRANSFER_STATE_OPEN &&
      priv->direction == EMP_FILE_TRANSFER_DIRECTION_OUTGOING &&
      priv->in_stream)
    send_file (tp_file);

  priv->state = state;
  priv->state_change_reason = reason;

  g_object_notify (G_OBJECT (tp_file), "state");
}

static void
tp_file_transferred_bytes_changed_cb (DBusGProxy *file_iface,
    guint64 transferred_bytes, EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  if (priv->transferred_bytes == transferred_bytes)
    return;

  priv->transferred_bytes = transferred_bytes;

  g_object_notify (G_OBJECT (tp_file), "transferred-bytes");
}

EmpathyContact *
_empathy_tp_file_get_contact (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->contact;
}

GInputStream *
_empathy_tp_file_get_input_stream (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->in_stream;
}

GOutputStream *
_empathy_tp_file_get_output_stream (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->out_stream;
}

const gchar *
_empathy_tp_file_get_filename (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->filename;
}

EmpFileTransferDirection
_empathy_tp_file_get_direction (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->direction;
}

EmpFileTransferState
_empathy_tp_file_get_state (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->state;
}

EmpFileTransferStateChangeReason
_empathy_tp_file_get_state_change_reason (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  g_return_val_if_fail (priv->state_change_reason >= 0,
            EMP_FILE_TRANSFER_STATE_CHANGE_REASON_NONE);

  return priv->state_change_reason;
}

guint64
_empathy_tp_file_get_size (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->size;
}

guint64
_empathy_tp_file_get_transferred_bytes (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  return priv->transferred_bytes;
}

gint
_empathy_tp_file_get_remaining_time (EmpathyTpFile *tp_file)
{
  EmpathyTpFilePriv *priv;
  gint64   curr_time, elapsed_time;
  gdouble  time_per_byte;
  gdouble  remaining_time;

  priv = GET_PRIV (tp_file);

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
_empathy_tp_file_set_input_stream (EmpathyTpFile *tp_file,
    GInputStream *in_stream)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

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

  g_object_notify (G_OBJECT (tp_file), "in-stream");
}

void
_empathy_tp_file_set_output_stream (EmpathyTpFile *tp_file,
    GOutputStream *out_stream)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

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

  g_object_notify (G_OBJECT (tp_file), "output-stream");
}

void
_empathy_tp_file_set_filename (EmpathyTpFile *tp_file,
    const gchar *filename)
{
  EmpathyTpFilePriv *priv;

  priv = GET_PRIV (tp_file);

  g_return_if_fail (filename != NULL);

  if (priv->filename && strcmp (filename, priv->filename) == 0) {
    return;
  }

  g_free (priv->filename);
  priv->filename = g_strdup (filename);

  g_object_notify (G_OBJECT (tp_file), "filename");
}

/* Functions to copy the content of a GInputStream to a GOutputStream */

#define N_BUFFERS 2
#define BUFFER_SIZE 4096

typedef struct {
  GInputStream  *in;
  GOutputStream *out;
  GCancellable  *cancellable;
  char          *buff[N_BUFFERS]; /* the temporary buffers */
  gsize          count[N_BUFFERS]; /* how many bytes are used in the buffers */
  gboolean       is_full[N_BUFFERS]; /* whether the buffers contain data */
  gint           curr_read; /* index of the buffer used for reading */
  gint           curr_write; /* index of the buffer used for writing */
  gboolean       is_reading; /* we are reading */
  gboolean       is_writing; /* we are writing */
  guint          n_closed; /* number of streams that have been closed */
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
  gssize    count_write;
  GError   *error = NULL;

  count_write = g_output_stream_write_finish (copy->out, res, &error);

  if (count_write <= 0) {
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
  gssize    count_read;
  GError   *error = NULL;

  count_read = g_input_stream_read_finish (copy->in, res, &error);

  if (count_read == 0) {
    g_input_stream_close_async (copy->in, 0, copy->cancellable,
              close_done, copy);
    copy->in = NULL;
  }
  else if (count_read < 0) {
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
      !copy->is_full[copy->curr_read]) {
    /* We are not reading and the current buffer is empty, so
     * start an async read. */
    copy->is_reading = TRUE;
    g_input_stream_read_async (copy->in,
             copy->buff[copy->curr_read],
             BUFFER_SIZE, 0, copy->cancellable,
             read_done_cb, copy);
  }

  if (!copy->is_writing &&
      copy->is_full[copy->curr_write]) {
    if (copy->count[copy->curr_write] == 0) {
      /* The last read on the buffer read 0 bytes, this
       * means that we got an EOF, so we can close
       * the output channel. */
      g_output_stream_close_async (copy->out, 0,
                 copy->cancellable,
                 close_done, copy);
      copy->out = NULL;
    } else {
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
  gint      i;

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
