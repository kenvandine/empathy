/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 */

#include <config.h>

#include <string.h>
#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-call-factory.h>

#include <extensions/extensions.h>

#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-event-manager.h"
#include "empathy-main-window.h"
#include "empathy-tube-dispatch.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyEventManager)

typedef struct {
  EmpathyEventManager *manager;
  EmpathyDispatchOperation *operation;
  gulong approved_handler;
  gulong claimed_handler;
  gulong invalidated_handler;
  /* Remove contact if applicable */
  EmpathyContact *contact;
  /* Tube dispatcher if applicable */
  EmpathyTubeDispatch *tube_dispatch;
  /* option signal handler and it's instance */
  gulong handler;
  GObject *handler_instance;
  /* optional accept widget */
  GtkWidget *dialog;
} EventManagerApproval;

typedef struct {
  EmpathyDispatcher *dispatcher;
  EmpathyContactManager *contact_manager;
  GSList *events;
  /* Ongoing approvals */
  GSList *approvals;

  /* voip ringing sound */
  guint voip_timeout;
  gint ringing;
} EmpathyEventManagerPriv;

typedef struct _EventPriv EventPriv;
typedef void (*EventFunc) (EventPriv *event);

struct _EventPriv {
  EmpathyEvent public;
  EmpathyEventManager *manager;
  EventManagerApproval *approval;
  EventFunc func;
  gboolean inhibit;
  gpointer user_data;
};

enum {
  EVENT_ADDED,
  EVENT_REMOVED,
  EVENT_UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyEventManager, empathy_event_manager, G_TYPE_OBJECT);

static EmpathyEventManager * manager_singleton = NULL;

static EventManagerApproval *
event_manager_approval_new (EmpathyEventManager *manager,
  EmpathyDispatchOperation *operation)
{
  EventManagerApproval *result = g_slice_new0 (EventManagerApproval);
  result->operation = g_object_ref (operation);
  result->manager = manager;

  return result;
}

static void
event_manager_approval_free (EventManagerApproval *approval)
{
  g_signal_handler_disconnect (approval->operation,
    approval->approved_handler);
  g_signal_handler_disconnect (approval->operation,
    approval->claimed_handler);
  g_signal_handler_disconnect (approval->operation,
    approval->invalidated_handler);
  g_object_unref (approval->operation);

  if (approval->handler != 0)
    g_signal_handler_disconnect (approval->handler_instance,
      approval->handler);

  if (approval->contact != NULL)
    g_object_unref (approval->contact);

  if (approval->tube_dispatch != NULL)
    g_object_unref (approval->tube_dispatch);

  if (approval->dialog != NULL)
    {
      gtk_widget_destroy (approval->dialog);
    }

  g_slice_free (EventManagerApproval, approval);
}

static void event_remove (EventPriv *event);

static void
event_free (EventPriv *event)
{
  g_free (event->public.icon_name);
  g_free (event->public.header);
  g_free (event->public.message);

  if (event->public.contact)
    {
      g_object_unref (event->public.contact);
    }

  g_slice_free (EventPriv, event);
}

static void event_manager_ringing_finished_cb (ca_context *c, guint id,
  int error_code, gpointer user_data);

static gboolean
event_manager_ringing_timeout_cb (gpointer data)
{
  EmpathyEventManager *manager = EMPATHY_EVENT_MANAGER (data);
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  priv->voip_timeout = 0;

  empathy_sound_play_full (empathy_main_window_get (),
      EMPATHY_SOUND_PHONE_INCOMING, event_manager_ringing_finished_cb,
      manager);

  return FALSE;
}

static gboolean
event_manager_ringing_idle_cb (gpointer data)
{
  EmpathyEventManager *manager = EMPATHY_EVENT_MANAGER (data);
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  if (priv->ringing > 0)
    priv->voip_timeout = g_timeout_add (500, event_manager_ringing_timeout_cb,
      data);

  return FALSE;
}

static void
event_manager_ringing_finished_cb (ca_context *c, guint id, int error_code,
  gpointer user_data)
{
  if (error_code == CA_ERROR_CANCELED)
    return;

  g_idle_add (event_manager_ringing_idle_cb, user_data);
}

static void
event_manager_start_ringing (EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  priv->ringing++;

  if (priv->ringing == 1)
    {
      empathy_sound_play_full (empathy_main_window_get (),
        EMPATHY_SOUND_PHONE_INCOMING, event_manager_ringing_finished_cb,
        manager);
    }
}

static void
event_manager_stop_ringing (EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  priv->ringing--;

  if (priv->ringing > 0)
    return;

  empathy_sound_stop (EMPATHY_SOUND_PHONE_INCOMING);

  if (priv->voip_timeout != 0)
    {
      g_source_remove (priv->voip_timeout);
      priv->voip_timeout = 0;
    }
}

static void
event_remove (EventPriv *event)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (event->manager);

  DEBUG ("Removing event %p", event);
  priv->events = g_slist_remove (priv->events, event);
  g_signal_emit (event->manager, signals[EVENT_REMOVED], 0, event);
  event_free (event);
}

static void
event_manager_add (EmpathyEventManager *manager, EmpathyContact *contact,
  const gchar *icon_name, const gchar *header, const gchar *message,
  EventManagerApproval *approval, EventFunc func, gpointer user_data)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);
  EventPriv               *event;

  event = g_slice_new0 (EventPriv);
  event->public.contact = contact ? g_object_ref (contact) : NULL;
  event->public.icon_name = g_strdup (icon_name);
  event->public.header = g_strdup (header);
  event->public.message = g_strdup (message);
  event->inhibit = FALSE;
  event->func = func;
  event->user_data = user_data;
  event->manager = manager;
  event->approval = approval;

  DEBUG ("Adding event %p", event);
  priv->events = g_slist_prepend (priv->events, event);
  g_signal_emit (event->manager, signals[EVENT_ADDED], 0, event);
}

static void
event_channel_process_func (EventPriv *event)
{
  empathy_dispatch_operation_approve (event->approval->operation);
}

static void
event_text_channel_process_func (EventPriv *event)
{
  EmpathyTpChat *tp_chat;

  if (event->approval->handler != 0)
    {
      tp_chat = EMPATHY_TP_CHAT
        (empathy_dispatch_operation_get_channel_wrapper (event->approval->operation));

      g_signal_handler_disconnect (tp_chat, event->approval->handler);
      event->approval->handler = 0;
    }

  empathy_dispatch_operation_approve (event->approval->operation);
}

static EventPriv *
event_lookup_by_approval (EmpathyEventManager *manager,
  EventManagerApproval *approval)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);
  GSList *l;
  EventPriv *retval = NULL;

  for (l = priv->events; l; l = l->next)
    {
      EventPriv *event = l->data;

      if (event->approval == approval)
        {
          retval = event;
          break;
        }
    }

  return retval;
}

static void
event_update (EmpathyEventManager *manager, EventPriv *event,
  const char *icon_name, const char *header, const char *msg)
{
  g_free (event->public.icon_name);
  g_free (event->public.header);
  g_free (event->public.message);

  event->public.icon_name = g_strdup (icon_name);
  event->public.header = g_strdup (header);
  event->public.message = g_strdup (msg);

  g_signal_emit (manager, signals[EVENT_UPDATED], 0, event);
}

static void
event_manager_call_window_confirmation_dialog_response_cb (GtkDialog *dialog,
  gint response, gpointer user_data)
{
  EventManagerApproval *approval = user_data;

  gtk_widget_destroy (approval->dialog);
  approval->dialog = NULL;

  if (response != GTK_RESPONSE_ACCEPT)
    {
      EmpathyTpCall *call =
        EMPATHY_TP_CALL (
          empathy_dispatch_operation_get_channel_wrapper (
            approval->operation));

      g_object_ref (call);
      if (empathy_dispatch_operation_claim (approval->operation))
        empathy_tp_call_close (call);
      g_object_unref (call);

    }
  else
    {
      EmpathyCallFactory *factory = empathy_call_factory_get ();
      empathy_call_factory_claim_channel (factory, approval->operation);
    }
}

static void
event_channel_process_voip_func (EventPriv *event)
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *image;

  if (event->approval->dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (event->approval->dialog));
      return;
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (empathy_main_window_get()),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Incoming call"));
  gtk_message_dialog_format_secondary_text (
    GTK_MESSAGE_DIALOG (dialog),
      _("%s is calling you, do you want to answer?"),
      empathy_contact_get_name (event->approval->contact));

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
      GTK_RESPONSE_OK);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog),
      _("_Reject"), GTK_RESPONSE_REJECT);
  image = gtk_image_new_from_icon_name (GTK_STOCK_CANCEL,
    GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog),
      _("_Answer"), GTK_RESPONSE_ACCEPT);

  image = gtk_image_new_from_icon_name (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  g_signal_connect (dialog, "response",
      G_CALLBACK (event_manager_call_window_confirmation_dialog_response_cb),
      event->approval);

  gtk_widget_show (dialog);

  event->approval->dialog = dialog;
}

static void
event_manager_chat_message_received_cb (EmpathyTpChat *tp_chat,
  EmpathyMessage *message, EventManagerApproval *approval)
{
  EmpathyContact  *sender;
  gchar           *header;
  const gchar     *msg;
  TpChannel       *channel;
  EventPriv       *event;

  /* try to update the event if it's referring to a chat which is already in the
   * queue. */
  event = event_lookup_by_approval (approval->manager, approval);

  if (event != NULL && event->inhibit && approval->handler != 0)
    {
      g_signal_handler_disconnect (tp_chat, approval->handler);
      approval->handler = 0;
      return;
    }

  sender = empathy_message_get_sender (message);
  header = g_strdup_printf (_("New message from %s"),
                            empathy_contact_get_name (sender));
  msg = empathy_message_get_body (message);

  channel = empathy_tp_chat_get_channel (tp_chat);

  if (event != NULL)
    event_update (approval->manager, event, EMPATHY_IMAGE_NEW_MESSAGE, header, msg);
  else
    event_manager_add (approval->manager, sender, EMPATHY_IMAGE_NEW_MESSAGE, header,
      msg, approval, event_text_channel_process_func, NULL);

  g_free (header);
  empathy_sound_play (empathy_main_window_get (),
    EMPATHY_SOUND_CONVERSATION_NEW);
}

static void
event_manager_approval_done (EventManagerApproval *approval)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (approval->manager);
  GSList                  *l;

  if (approval->operation != NULL)
    {
      GQuark channel_type;

      channel_type = empathy_dispatch_operation_get_channel_type_id (
          approval->operation);
      if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_STREAMED_MEDIA)
        {
          event_manager_stop_ringing (approval->manager);
        }
    }

  priv->approvals = g_slist_remove (priv->approvals, approval);

  for (l = priv->events; l; l = l->next)
    {
      EventPriv *event = l->data;

      if (event->approval == approval)
        {
          event_remove (event);
          break;
        }
    }

  event_manager_approval_free (approval);
}

static void
event_manager_operation_approved_cb (EmpathyDispatchOperation *operation,
  EventManagerApproval *approval)
{
  event_manager_approval_done (approval);
}

static void
event_manager_operation_claimed_cb (EmpathyDispatchOperation *operation,
  EventManagerApproval *approval)
{
  event_manager_approval_done (approval);
}

static void
event_manager_operation_invalidated_cb (EmpathyDispatchOperation *operation,
  guint domain, gint code, gchar *message,
  EventManagerApproval *approval)
{
  event_manager_approval_done (approval);
}

static void
event_manager_media_channel_got_name_cb (EmpathyContact *contact,
  const GError *error, gpointer user_data, GObject *object)
{
  EventManagerApproval *approval = user_data;
  gchar *header;

  if (error != NULL)
    {
      /* FIXME just returning assuming the operation will be invalidated as
       * well */
      return;
    }

  header = g_strdup_printf (_("Incoming call from %s"),
    empathy_contact_get_name (contact));

  event_manager_add (approval->manager,
    approval->contact, EMPATHY_IMAGE_VOIP, header, NULL,
    approval, event_channel_process_voip_func, NULL);

  g_free (header);
  event_manager_start_ringing (approval->manager);
}

static void
event_manager_media_channel_got_contact (EventManagerApproval *approval)
{
  empathy_contact_call_when_ready (approval->contact,
     EMPATHY_CONTACT_READY_NAME, event_manager_media_channel_got_name_cb,
        approval, NULL, G_OBJECT (approval->manager));
}

static void
event_manager_media_channel_contact_changed_cb (EmpathyTpCall *call,
  GParamSpec *param, EventManagerApproval *approval)
{
  EmpathyContact *contact;

  g_object_get (G_OBJECT (call), "contact", &contact, NULL);

  if (contact == NULL)
    return;

  approval->contact = contact;
  event_manager_media_channel_got_contact (approval);
}

static void
event_manager_tube_approved_cb (EventPriv *event)
{
  empathy_tube_dispatch_handle (event->approval->tube_dispatch);
}

static void
event_manager_add_tube_approval (EventManagerApproval *approval,
  EmpathyTubeDispatchAbility ability)
{
  const gchar *icon_name;
  gchar       *header;
  const gchar *msg;

  header = g_strdup_printf (_("%s is offering you an invitation"),
    empathy_contact_get_name (approval->contact));

  if (ability == EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE)
    {
      icon_name = GTK_STOCK_EXECUTE;
      msg = _("An external application will be started to handle it.");
    }
  else
    {
      icon_name = GTK_STOCK_DIALOG_ERROR;
      msg = _("You don't have the needed external "
              "application to handle it.");
    }

  event_manager_add (approval->manager, approval->contact, icon_name, header,
    msg, approval, event_manager_tube_approved_cb, approval);

  g_free (header);
  /* FIXME better sound for incoming tubes ? */
  empathy_sound_play (empathy_main_window_get (),
    EMPATHY_SOUND_CONVERSATION_NEW);
}

static void
event_manager_tube_dispatch_ability_cb (GObject *object,
   GParamSpec *spec, gpointer user_data)
{
  EventManagerApproval *approval = (EventManagerApproval *)user_data;
  EmpathyTubeDispatchAbility dispatchability;

  dispatchability =
    empathy_tube_dispatch_is_dispatchable (approval->tube_dispatch);

  if (dispatchability != EMPATHY_TUBE_DISPATCHABILITY_UNKNOWN)
    {
      event_manager_add_tube_approval (approval, dispatchability);
      g_signal_handler_disconnect (object, approval->handler);
      approval->handler = 0;
    }
}

static void
event_manager_tube_got_contact_name_cb (EmpathyContact *contact,
  const GError *error, gpointer user_data, GObject *object)
{
  EventManagerApproval *approval = (EventManagerApproval *)user_data;
  EmpathyTubeDispatchAbility dispatchability;

  if (error != NULL)
    {
      /* FIXME?, we assume that the operation gets invalidated as well (if it
       * didn't already */
       return;
    }

  dispatchability = empathy_tube_dispatch_is_dispatchable
    (approval->tube_dispatch);


  switch (dispatchability)
    {
      case EMPATHY_TUBE_DISPATCHABILITY_UNKNOWN:
        approval->handler = g_signal_connect (approval->tube_dispatch,
          "notify::dispatchability",
          G_CALLBACK (event_manager_tube_dispatch_ability_cb), approval);
        approval->handler_instance = G_OBJECT (approval->tube_dispatch);
        break;
      case EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE:
        /* fallthrough */
      case EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE:
        event_manager_add_tube_approval (approval, dispatchability);
        break;
    }
}

static void
invite_dialog_response_cb (GtkDialog *dialog,
                           gint response,
                           EventManagerApproval *approval)
{
  EmpathyTpChat *tp_chat;
  TpChannel *channel;
  TpHandle self_handle;
  GArray *members;

  gtk_widget_destroy (GTK_WIDGET (approval->dialog));
  approval->dialog = NULL;

  tp_chat = EMPATHY_TP_CHAT (empathy_dispatch_operation_get_channel_wrapper (
        approval->operation));

  if (response != GTK_RESPONSE_OK)
    {
      /* close channel */
      DEBUG ("Muc invitation rejected");

      if (empathy_dispatch_operation_claim (approval->operation))
        empathy_tp_chat_close (tp_chat);
      return;
    }

  DEBUG ("Muc invitation accepted");

  /* join the room */
  channel = empathy_tp_chat_get_channel (tp_chat);

  self_handle = tp_channel_group_get_self_handle (channel);
  members = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
  g_array_append_val (members, self_handle);

  tp_cli_channel_interface_group_call_add_members (channel, -1, members,
      "", NULL, NULL, NULL, NULL);

  empathy_dispatch_operation_approve (approval->operation);

  g_array_free (members, TRUE);
}

static void
event_room_channel_process_func (EventPriv *event)
{
  GtkWidget *dialog, *button, *image;
  TpChannel *channel = empathy_dispatch_operation_get_channel (
      event->approval->operation);

  if (event->approval->dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (event->approval->dialog));
      return;
    }

  /* create dialog */
  dialog = gtk_message_dialog_new (NULL, 0,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Room invitation"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
      _("%s is inviting you to join %s"),
      empathy_contact_get_name (event->approval->contact),
      tp_channel_get_identifier (channel));

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
      GTK_RESPONSE_OK);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog),
      _("_Decline"), GTK_RESPONSE_CANCEL);
  image = gtk_image_new_from_icon_name (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog),
      _("_Join"), GTK_RESPONSE_OK);
  image = gtk_image_new_from_icon_name (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  g_signal_connect (dialog, "response",
      G_CALLBACK (invite_dialog_response_cb), event->approval);

  gtk_widget_show (dialog);

  event->approval->dialog = dialog;
}

static void
event_manager_muc_invite_got_contact_name_cb (EmpathyContact *contact,
                                              const GError *error,
                                              gpointer user_data,
                                              GObject *object)
{
  EventManagerApproval *approval = (EventManagerApproval *) user_data;
  TpChannel *channel;
  const gchar *invite_msg;
  gchar *msg;
  TpHandle self_handle;

  channel = empathy_dispatch_operation_get_channel (approval->operation);

  self_handle = tp_channel_group_get_self_handle (channel);
  tp_channel_group_get_local_pending_info (channel, self_handle, NULL, NULL,
      &invite_msg);

  msg = g_strdup_printf (_("%s invited you to join %s"),
      empathy_contact_get_name (approval->contact),
      tp_channel_get_identifier (channel));

  event_manager_add (approval->manager,
    approval->contact, EMPATHY_IMAGE_GROUP_MESSAGE, msg, invite_msg,
    approval, event_room_channel_process_func, NULL);

  empathy_sound_play (empathy_main_window_get (),
    EMPATHY_SOUND_CONVERSATION_NEW);

  g_free (msg);
}

static void
event_manager_approve_channel_cb (EmpathyDispatcher *dispatcher,
  EmpathyDispatchOperation  *operation, EmpathyEventManager *manager)
{
  const gchar *channel_type;
  EventManagerApproval *approval;
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  channel_type = empathy_dispatch_operation_get_channel_type (operation);

  approval = event_manager_approval_new (manager, operation);
  priv->approvals = g_slist_prepend (priv->approvals, approval);

  approval->approved_handler = g_signal_connect (operation, "approved",
    G_CALLBACK (event_manager_operation_approved_cb), approval);

  approval->claimed_handler = g_signal_connect (operation, "claimed",
     G_CALLBACK (event_manager_operation_claimed_cb), approval);

  approval->invalidated_handler = g_signal_connect (operation, "invalidated",
     G_CALLBACK (event_manager_operation_invalidated_cb), approval);

  if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT))
    {
      EmpathyTpChat *tp_chat =
        EMPATHY_TP_CHAT (
          empathy_dispatch_operation_get_channel_wrapper (operation));
      TpChannel *channel = empathy_tp_chat_get_channel (tp_chat);

      if (tp_proxy_has_interface (channel, TP_IFACE_CHANNEL_INTERFACE_GROUP))
        {
          /* Are we in local-pending ? */
          TpHandle self_handle, inviter;

          self_handle = tp_channel_group_get_self_handle (channel);

          if (self_handle != 0 && tp_channel_group_get_local_pending_info (
                channel, self_handle, &inviter, NULL, NULL))
            {
              /* We are invited to a room */
              EmpathyContactFactory *contact_factory;
              McAccount *account;

              DEBUG ("Have been invited to %s. Ask user if he wants to accept",
                  tp_channel_get_identifier (channel));

              account = empathy_tp_chat_get_account (tp_chat);
              contact_factory = empathy_contact_factory_dup_singleton ();

              approval->contact = empathy_contact_factory_get_from_handle (
                  contact_factory, account, inviter);

              empathy_contact_call_when_ready (approval->contact,
                EMPATHY_CONTACT_READY_NAME,
                event_manager_muc_invite_got_contact_name_cb, approval, NULL,
                G_OBJECT (manager));

              g_object_unref (contact_factory);
              return;
            }

          /* if we are not invited, let's wait for the first message */
        }

      /* 1-1 text channel, wait for the first message */
      approval->handler = g_signal_connect (tp_chat, "message-received",
        G_CALLBACK (event_manager_chat_message_received_cb), approval);
      approval->handler_instance = G_OBJECT (tp_chat);
    }
  else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA))
    {
      EmpathyContact *contact;
      EmpathyTpCall *call = EMPATHY_TP_CALL (
          empathy_dispatch_operation_get_channel_wrapper (operation));

      g_object_get (G_OBJECT (call), "contact", &contact, NULL);

      if (contact == NULL)
        {
          g_signal_connect (call, "notify::contact",
            G_CALLBACK (event_manager_media_channel_contact_changed_cb),
            approval);
        }
      else
        {
          approval->contact = contact;
          event_manager_media_channel_got_contact (approval);
        }

    }
  else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER))
    {
      EmpathyContact        *contact;
      gchar                 *header;
      TpHandle               handle;
      McAccount             *account;
      EmpathyContactFactory *factory;
      TpChannel *channel = empathy_dispatch_operation_get_channel (operation);

      factory = empathy_contact_factory_dup_singleton ();
      handle = tp_channel_get_handle (channel, NULL);
      account = empathy_channel_get_account (channel);

      contact = empathy_contact_factory_get_from_handle (factory, account,
        handle);

      empathy_contact_run_until_ready (contact,
        EMPATHY_CONTACT_READY_NAME, NULL);

      header = g_strdup_printf (_("Incoming file transfer from %s"),
        empathy_contact_get_name (contact));

      event_manager_add (manager, contact, EMPATHY_IMAGE_DOCUMENT_SEND,
        header, NULL, approval, event_channel_process_func, NULL);

      /* FIXME better sound for incoming file transfers ?*/
      empathy_sound_play (empathy_main_window_get (),
        EMPATHY_SOUND_CONVERSATION_NEW);

      g_object_unref (factory);
      g_object_unref (account);
      g_free (header);
    }
  else if (!tp_strdiff (channel_type, EMP_IFACE_CHANNEL_TYPE_STREAM_TUBE) ||
      !tp_strdiff (channel_type, EMP_IFACE_CHANNEL_TYPE_DBUS_TUBE))
    {
      EmpathyContact        *contact;
      TpHandle               handle;
      TpHandleType           handle_type;
      McAccount             *account;
      EmpathyContactFactory *factory;
      EmpathyTubeDispatch *tube_dispatch;
      TpChannel *channel;

      channel = empathy_dispatch_operation_get_channel (operation);

      handle = tp_channel_get_handle (channel, &handle_type);

      /* Only understand p2p tubes */
      if (handle_type != TP_HANDLE_TYPE_CONTACT)
        return;

      factory = empathy_contact_factory_dup_singleton ();
      account = empathy_channel_get_account (channel);

      contact = empathy_contact_factory_get_from_handle (factory, account,
        handle);

      tube_dispatch = empathy_tube_dispatch_new (operation);

      approval->contact = contact;
      approval->tube_dispatch = tube_dispatch;

      empathy_contact_call_when_ready (contact,
        EMPATHY_CONTACT_READY_NAME, event_manager_tube_got_contact_name_cb,
        approval, NULL, G_OBJECT (manager));

      g_object_unref (factory);
      g_object_unref (account);
    }
  else
    {
      DEBUG ("Unknown channel type, ignoring..");
    }
}

static void
event_pending_subscribe_func (EventPriv *event)
{
  empathy_subscription_dialog_show (event->public.contact, NULL);
  event_remove (event);
}

static void
event_manager_pendings_changed_cb (EmpathyContactList  *list,
  EmpathyContact *contact, EmpathyContact *actor,
  guint reason, gchar *message, gboolean is_pending,
  EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);
  gchar                   *header, *event_msg;

  if (!is_pending)
    {
      GSList *l;

      for (l = priv->events; l; l = l->next)
        {
          EventPriv *event = l->data;

      if (event->public.contact == contact &&
          event->func == event_pending_subscribe_func)
        {
          event_remove (event);
          break;
        }
      }

      return;
    }

  empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_NAME, NULL);

  header = g_strdup_printf (_("Subscription requested by %s"),
    empathy_contact_get_name (contact));

  if (!EMP_STR_EMPTY (message))
    event_msg = g_strdup_printf (_("\nMessage: %s"), message);
  else
    event_msg = NULL;

  event_manager_add (manager, contact, GTK_STOCK_DIALOG_QUESTION, header,
    event_msg, NULL, event_pending_subscribe_func, NULL);

  g_free (event_msg);
  g_free (header);
}

static GObject *
event_manager_constructor (GType type,
			   guint n_props,
			   GObjectConstructParam *props)
{
	GObject *retval;

	if (manager_singleton) {
		retval = g_object_ref (manager_singleton);
	} else {
		retval = G_OBJECT_CLASS (empathy_event_manager_parent_class)->constructor
			(type, n_props, props);

		manager_singleton = EMPATHY_EVENT_MANAGER (retval);
		g_object_add_weak_pointer (retval, (gpointer) &manager_singleton);
	}

	return retval;
}

static void
event_manager_finalize (GObject *object)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (object);

  g_slist_foreach (priv->events, (GFunc) event_free, NULL);
  g_slist_free (priv->events);
  g_slist_foreach (priv->approvals, (GFunc) event_manager_approval_free, NULL);
  g_slist_free (priv->approvals);
  g_object_unref (priv->contact_manager);
  g_object_unref (priv->dispatcher);
}

static void
empathy_event_manager_class_init (EmpathyEventManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = event_manager_finalize;
  object_class->constructor = event_manager_constructor;

  signals[EVENT_ADDED] =
    g_signal_new ("event-added",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__POINTER,
      G_TYPE_NONE,
      1, G_TYPE_POINTER);

  signals[EVENT_REMOVED] =
  g_signal_new ("event-removed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_POINTER);

  signals[EVENT_UPDATED] =
  g_signal_new ("event-updated",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_POINTER);


  g_type_class_add_private (object_class, sizeof (EmpathyEventManagerPriv));
}

static void
empathy_event_manager_init (EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
    EMPATHY_TYPE_EVENT_MANAGER, EmpathyEventManagerPriv);

  manager->priv = priv;

  priv->dispatcher = empathy_dispatcher_dup_singleton ();
  priv->contact_manager = empathy_contact_manager_dup_singleton ();
  g_signal_connect (priv->dispatcher, "approve",
    G_CALLBACK (event_manager_approve_channel_cb), manager);
  g_signal_connect (priv->contact_manager, "pendings-changed",
    G_CALLBACK (event_manager_pendings_changed_cb), manager);
}

EmpathyEventManager *
empathy_event_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_EVENT_MANAGER, NULL);
}

GSList *
empathy_event_manager_get_events (EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  g_return_val_if_fail (EMPATHY_IS_EVENT_MANAGER (manager), NULL);

  return priv->events;
}

EmpathyEvent *
empathy_event_manager_get_top_event (EmpathyEventManager *manager)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (manager);

  g_return_val_if_fail (EMPATHY_IS_EVENT_MANAGER (manager), NULL);

  return priv->events ? priv->events->data : NULL;
}

void
empathy_event_activate (EmpathyEvent *event_public)
{
  EventPriv *event = (EventPriv*) event_public;

  g_return_if_fail (event_public != NULL);

  if (event->func)
    event->func (event);
  else
    event_remove (event);
}

void
empathy_event_inhibit_updates (EmpathyEvent *event_public)
{
  EventPriv *event = (EventPriv *) event_public;

  g_return_if_fail (event_public != NULL);

  event->inhibit = TRUE;
}

