/*
*  Copyright (C) 2009 Collabora Ltd.
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
*  Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
*/

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-account-chooser.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/proxy-subclass.h>

#include "extensions/extensions.h"

#include "empathy-debug-dialog.h"

G_DEFINE_TYPE (EmpathyDebugDialog, empathy_debug_dialog,
    GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_PARENT
};

enum
{
  COL_TIMESTAMP = 0,
  COL_DOMAIN,
  COL_CATEGORY,
  COL_LEVEL,
  COL_MESSAGE,
  NUM_COLS
};

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyDebugDialog)
typedef struct
{
  GtkWidget *filter;
  GtkWindow *parent;
  GtkWidget *view;
  GtkWidget *cm_chooser;
  GtkListStore *store;
  TpProxy *proxy;
  TpProxySignalConnection *signal_connection;
  gboolean paused;
  GList *bus_names;
  gboolean dispose_run;
} EmpathyDebugDialogPriv;

static const gchar *
log_level_to_string (guint level)
{
  switch (level)
    {
    case EMP_DEBUG_LEVEL_ERROR:
      return _("Error");
      break;
    case EMP_DEBUG_LEVEL_CRITICAL:
      return _("Critical");
      break;
    case EMP_DEBUG_LEVEL_WARNING:
      return _("Warning");
      break;
    case EMP_DEBUG_LEVEL_MESSAGE:
      return _("Message");
      break;
    case EMP_DEBUG_LEVEL_INFO:
      return _("Info");
      break;
    case EMP_DEBUG_LEVEL_DEBUG:
      return _("Debug");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
debug_dialog_add_message (EmpathyDebugDialog *debug_dialog,
                          gdouble timestamp,
                          const gchar *domain_category,
                          guint level,
                          const gchar *message)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);
  gchar *domain, *category;
  GtkTreeIter iter;

  if (g_strrstr (domain_category, "/"))
    {
      gchar **parts = g_strsplit (domain_category, "/", 2);
      domain = g_strdup (parts[0]);
      category = g_strdup (parts[1]);
      g_strfreev (parts);
    }
  else
    {
      domain = g_strdup (domain_category);
      category = g_strdup ("");
    }

  gtk_list_store_append (priv->store, &iter);
  gtk_list_store_set (priv->store, &iter,
      COL_TIMESTAMP, timestamp,
      COL_DOMAIN, domain,
      COL_CATEGORY, category,
      COL_LEVEL, log_level_to_string (level),
      COL_MESSAGE, message,
      -1);

  g_free (domain);
  g_free (category);
}

static void
debug_dialog_new_debug_message_cb (TpProxy *proxy,
                                   gdouble timestamp,
                                   const gchar *domain,
                                   guint level,
                                   const gchar *message,
                                   gpointer user_data,
                                   GObject *weak_object)
{
  EmpathyDebugDialog *debug_dialog = (EmpathyDebugDialog *) user_data;

  debug_dialog_add_message (debug_dialog, timestamp, domain, level,
      message);
}

static void
debug_dialog_set_enabled (EmpathyDebugDialog *debug_dialog,
                          gboolean enabled)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);
  GValue *val;

  val = tp_g_value_slice_new_boolean (enabled);

  tp_cli_dbus_properties_call_set (priv->proxy, -1, EMP_IFACE_DEBUG,
      "Enabled", val, NULL, NULL, NULL, NULL);

  tp_g_value_slice_free (val);
}

static void
debug_dialog_get_messages_cb (TpProxy *proxy,
                              const GPtrArray *messages,
                              const GError *error,
                              gpointer user_data,
                              GObject *weak_object)
{
  EmpathyDebugDialog *debug_dialog = (EmpathyDebugDialog *) user_data;
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);
  gint i;

  if (error != NULL)
    {
      DEBUG ("GetMessages failed: %s", error->message);
      return;
    }

  for (i = 0; i < messages->len; i++)
    {
      GValueArray *values = g_ptr_array_index (messages, i);

      debug_dialog_add_message (debug_dialog,
          g_value_get_double (g_value_array_get_nth (values, 0)),
          g_value_get_string (g_value_array_get_nth (values, 1)),
          g_value_get_uint (g_value_array_get_nth (values, 2)),
          g_value_get_string (g_value_array_get_nth (values, 3)));
    }

  /* Connect to NewDebugMessage */
  priv->signal_connection = emp_cli_debug_connect_to_new_debug_message (
      proxy, debug_dialog_new_debug_message_cb, debug_dialog,
      NULL, NULL, NULL);

  /* Set Enabled as appropriate */
  debug_dialog_set_enabled (debug_dialog, !priv->paused);
}

static void
debug_dialog_cm_chooser_changed_cb (GtkComboBox *cm_chooser,
                                    EmpathyDebugDialog *debug_dialog)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);
  MissionControl *mc;
  TpDBusDaemon *dbus;
  GError *error = NULL;
  const gchar *bus_name;
  TpConnection *connection;

  if (gtk_combo_box_get_active (cm_chooser) == -1)
    {
      DEBUG ("No CM is selected");
      return;
    }

  mc = empathy_mission_control_dup_singleton ();
  dbus = tp_dbus_daemon_dup (&error);

  if (error != NULL)
    {
      DEBUG ("Failed at duping the dbus daemon: %s", error->message);
      g_object_unref (mc);
    }

  bus_name = g_list_nth_data (priv->bus_names, gtk_combo_box_get_active (cm_chooser));
  connection = tp_connection_new (dbus, bus_name, DEBUG_OBJECT_PATH, &error);

  if (error != NULL)
    {
      DEBUG ("Getting a new TpConnection failed: %s", error->message);
      g_error_free (error);
      g_object_unref (dbus);
      g_object_unref (mc);
      return;
    }

  /* Disable debug signalling */
  if (priv->proxy != NULL)
    debug_dialog_set_enabled (debug_dialog, FALSE);

  /* Disconnect from previous NewDebugMessage signal */
  if (priv->signal_connection)
    {
      tp_proxy_signal_connection_disconnect (priv->signal_connection);
      priv->signal_connection = NULL;
    }

  if (priv->proxy)
    g_object_unref (priv->proxy);

  priv->proxy = TP_PROXY (connection);

  tp_proxy_add_interface_by_id (priv->proxy, emp_iface_quark_debug ());

  emp_cli_debug_call_get_messages (priv->proxy, -1,
      debug_dialog_get_messages_cb, debug_dialog, NULL, NULL);

  g_object_unref (dbus);
  g_object_unref (mc);
}

static void
debug_dialog_list_connection_names_cb (const gchar * const *names,
                                       gsize n,
                                       const gchar * const *cms,
                                       const gchar * const *protocols,
                                       const GError *error,
                                       gpointer user_data,
                                       GObject *weak_object)
{
  EmpathyDebugDialog *debug_dialog = (EmpathyDebugDialog *) user_data;
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);
  int i;

  if (error != NULL)
    {
      DEBUG ("list_connection_names failed: %s", error->message);
      return;
    }

  for (i = 0; cms[i] != NULL; i++)
    gtk_combo_box_append_text (GTK_COMBO_BOX (priv->cm_chooser), cms[i]);

  for (i = 0; names[i] != NULL; i++)
    priv->bus_names = g_list_append (priv->bus_names, g_strdup (names[i]));

  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->cm_chooser), 0);

  /* Fill treeview */
  debug_dialog_cm_chooser_changed_cb (GTK_COMBO_BOX (priv->cm_chooser), debug_dialog);
}

static void
debug_dialog_fill_cm_chooser (EmpathyDebugDialog *debug_dialog)
{
  TpDBusDaemon *dbus;
  GError *error = NULL;

  dbus = tp_dbus_daemon_dup (&error);

  if (error != NULL)
    {
      DEBUG ("Failed to dup dbus daemon: %s", error->message);
      g_error_free (error);
      return;
    }

  tp_list_connection_names (dbus, debug_dialog_list_connection_names_cb,
      debug_dialog, NULL, NULL);

  g_object_unref (dbus);
}

static void
debug_dialog_pause_toggled_cb (GtkToggleToolButton *pause,
                               EmpathyDebugDialog *debug_dialog)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (debug_dialog);

  priv->paused = gtk_toggle_tool_button_get_active (pause);

  debug_dialog_set_enabled (debug_dialog, !priv->paused);
}

static GObject *
debug_dialog_constructor (GType type,
                          guint n_construct_params,
                          GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyDebugDialogPriv *priv;
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkWidget *image;
  GtkWidget *label;
  GtkToolItem *item;
  GtkCellRenderer *renderer;
  GtkWidget *scrolled_win;

  object = G_OBJECT_CLASS (empathy_debug_dialog_parent_class)->constructor
    (type, n_construct_params, construct_params);
  priv = GET_PRIV (object);

  gtk_window_set_title (GTK_WINDOW (object), _("Debug Window"));
  gtk_window_set_default_size (GTK_WINDOW (object), 800, 400);
  gtk_window_set_transient_for (GTK_WINDOW (object), priv->parent);

  vbox = GTK_DIALOG (object)->vbox;

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (toolbar);

  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

  /* CM */
  priv->cm_chooser = gtk_combo_box_new_text ();
  gtk_widget_show (priv->cm_chooser);

  item = gtk_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (item), priv->cm_chooser);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  debug_dialog_fill_cm_chooser (EMPATHY_DEBUG_DIALOG (object));
  g_signal_connect (priv->cm_chooser, "changed",
      G_CALLBACK (debug_dialog_cm_chooser_changed_cb), object);
  gtk_widget_show (GTK_WIDGET (priv->cm_chooser));

  item = gtk_separator_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Save */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_SAVE);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Clear */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_separator_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Pause */
  priv->paused = FALSE;
  image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  item = gtk_toggle_tool_button_new ();
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (item), priv->paused);
  g_signal_connect (item, "toggled", G_CALLBACK (debug_dialog_pause_toggled_cb),
      object);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Pause"));
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (item), image);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_separator_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Level */
  item = gtk_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  label = gtk_label_new (_("Level "));
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (item), label);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  priv->filter = gtk_combo_box_new_text ();
  gtk_widget_show (priv->filter);

  item = gtk_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (item), priv->filter);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("All"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Debug"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Info"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Message"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Warning"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Critical"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Error"));

  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter), 0);
  gtk_widget_show (GTK_WIDGET (priv->filter));

  /* Debug treeview */
  priv->view = gtk_tree_view_new ();
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->view), TRUE);

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Time"), renderer, "text", COL_TIMESTAMP, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Domain"), renderer, "text", COL_DOMAIN, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Category"), renderer, "text", COL_CATEGORY, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Level"), renderer, "text", COL_LEVEL, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Message"), renderer, "text", COL_MESSAGE, NULL);

  priv->store = gtk_list_store_new (NUM_COLS, G_TYPE_DOUBLE,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view),
      GTK_TREE_MODEL (priv->store));

  /* Scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_widget_show (priv->view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->view);

  gtk_widget_show (scrolled_win);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (object));

  return object;
}

static void
empathy_debug_dialog_init (EmpathyDebugDialog *empathy_debug_dialog)
{
  EmpathyDebugDialogPriv *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (empathy_debug_dialog,
      EMPATHY_TYPE_DEBUG_DIALOG, EmpathyDebugDialogPriv);

  empathy_debug_dialog->priv = priv;

  priv->dispose_run = FALSE;
}

static void
debug_dialog_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_PARENT:
        priv->parent = GTK_WINDOW (g_value_dup_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
debug_dialog_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_PARENT:
        g_value_set_object (value, priv->parent);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
debug_dialog_dispose (GObject *object)
{
  EmpathyDebugDialog *selector = EMPATHY_DEBUG_DIALOG (object);
  EmpathyDebugDialogPriv *priv = GET_PRIV (selector);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->parent)
    g_object_unref (priv->parent);

  if (priv->store)
    g_object_unref (priv->store);

  if (priv->proxy)
    {
      debug_dialog_set_enabled (EMPATHY_DEBUG_DIALOG (object), FALSE);
      g_object_unref (priv->proxy);
    }

  if (priv->signal_connection)
    tp_proxy_signal_connection_disconnect (priv->signal_connection);

  if (priv->bus_names)
    {
      g_list_foreach (priv->bus_names, (GFunc) g_free, NULL);
      g_list_free (priv->bus_names);
    }

  (G_OBJECT_CLASS (empathy_debug_dialog_parent_class)->dispose) (object);
}

static void
empathy_debug_dialog_class_init (EmpathyDebugDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructor = debug_dialog_constructor;
  object_class->dispose = debug_dialog_dispose;
  object_class->set_property = debug_dialog_set_property;
  object_class->get_property = debug_dialog_get_property;
  g_type_class_add_private (klass, sizeof (EmpathyDebugDialogPriv));

  g_object_class_install_property (object_class, PROP_PARENT,
      g_param_spec_object ("parent", "parent", "parent",
      GTK_TYPE_WINDOW, G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

/* public methods */

GtkWidget *
empathy_debug_dialog_new (GtkWindow *parent)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return GTK_WIDGET (g_object_new (EMPATHY_TYPE_DEBUG_DIALOG,
      "parent", parent, NULL));
}
