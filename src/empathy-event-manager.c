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

#include <extensions/extensions.h>

#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>

#include "empathy-event-manager.h"
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
  /* option signal handler */
  gulong handler;
} EventManagerApproval;

typedef struct {
  EmpathyDispatcher *dispatcher;
  EmpathyContactManager *contact_manager;
  GSList *events;
  /* Ongoing approvals */
  GSList *approvals;
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

  if (approval->contact != NULL)
    g_object_unref (approval->contact);

  if (approval->tube_dispatch != NULL)
    g_object_unref (approval->tube_dispatch);

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

  if (event != NULL && event->inhibit)
    {
      g_signal_handlers_disconnect_by_func (tp_chat,
        event_manager_chat_message_received_cb, approval);
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
      msg, approval, event_channel_process_func, NULL);

  g_free (header);
}

static void
event_manager_approval_done (EventManagerApproval *approval)
{
  EmpathyEventManagerPriv *priv = GET_PRIV (approval->manager);
  GSList                  *l;

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
    approval, event_channel_process_func, NULL);

  g_free (header);
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
        break;
      case EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE:
        /* fallthrough */
      case EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE:
        event_manager_add_tube_approval (approval, dispatchability);
        break;
    }
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

      g_signal_connect (tp_chat, "message-received",
        G_CALLBACK (event_manager_chat_message_received_cb), approval);

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
  else if (!tp_strdiff (channel_type, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER))
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
		g_object_add_weak_pointer (retval, (gpointer *) &manager_singleton);
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

