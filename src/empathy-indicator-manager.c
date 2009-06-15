#include <config.h>

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-event-manager.h"
#include "empathy-indicator.h"
#include "empathy-indicator-manager.h"
#include "empathy-misc.h"

#include <libindicate/server.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIndicatorManager)

enum {
  SERVER_ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
  EmpathyEventManager *event_manager;
  IndicateServer      *indicate_server;
  GSList              *indicator_events;
  GHashTable          *login_timeouts;
} EmpathyIndicatorManagerPriv;

typedef struct {
  EmpathyIndicator   *indicator;
  EmpathyEvent        *event;
} IndicatorEvent;

typedef struct {
	EmpathyIndicatorManager *manager;
	EmpathyIndicator *indicator;
} LoginData;

G_DEFINE_TYPE (EmpathyIndicatorManager, empathy_indicator_manager, G_TYPE_OBJECT);

static EmpathyIndicatorManager * manager_singleton = NULL;

static void
free_indicator_event (IndicatorEvent *indicator_event)
{
  g_object_unref(indicator_event->indicator);
  g_free(indicator_event);
}

static void
indicate_server_activate (IndicateServer          *server,
                          EmpathyIndicatorManager *manager)
{
  g_signal_emit (manager, signals[SERVER_ACTIVATE], 0);
}

static void
indicate_show_cb (EmpathyIndicator *indicator,
                  EmpathyEvent     *event)
{
  empathy_event_activate (event);
}

static void
indicator_manager_event_added_cb (EmpathyEventManager *event_manager,
			    EmpathyEvent        *event,
			    EmpathyIndicatorManager   *manager)
{
  EmpathyIndicator *indicator;
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  IndicatorEvent *indicator_event;

  if (!event->contact)
      return;
  indicator = empathy_indicator_new (event->contact, event->message, "im");
  if (!indicator)
      return;
  empathy_indicator_show (indicator);
  g_signal_connect(G_OBJECT(indicator), "activate",
          G_CALLBACK(indicate_show_cb),
          event);
  indicator_event = (IndicatorEvent *) g_malloc(sizeof(IndicatorEvent));
  indicator_event->indicator = g_object_ref(indicator);
  indicator_event->event = event;
  priv->indicator_events = g_slist_prepend(priv->indicator_events,
          indicator_event);
}

static void
indicator_manager_event_removed_cb (EmpathyEventManager *event_manager,
			      EmpathyEvent        *event,
			      EmpathyIndicatorManager   *manager)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  GSList *l;
  IndicatorEvent *ret = NULL;

  for (l = priv->indicator_events; l; l = l->next)
  {
    IndicatorEvent *indicator_event = l->data;
    if (indicator_event->event == event) {
      ret = indicator_event;
      break;
    }
  }

  if (!ret)
    return;

  priv->indicator_events = g_slist_remove (priv->indicator_events, ret);

  empathy_indicator_hide (ret->indicator);
  free_indicator_event (ret);
}

static void
indicator_manager_event_updated_cb (EmpathyEventManager *event_manager,
			      EmpathyEvent        *event,
			      EmpathyIndicatorManager   *manager)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  GSList *l;
  IndicatorEvent *ret = NULL;

  for (l = priv->indicator_events; l; l = l->next)
  {
    IndicatorEvent *indicator_event = l->data;
    if (indicator_event->event == event) {
      ret = indicator_event;
      break;
    }
  }

  if (!ret)
    return;

  empathy_indicator_update (ret->indicator, event->message);
}

static gboolean
indicate_login_timeout (gpointer data)
{
  LoginData *login_data = (LoginData *)data;
  EmpathyIndicator *e_indicator = login_data->indicator;
  EmpathyIndicatorManager *manager = login_data->manager;
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  GValue *wrapped_timeout;
  guint login_timeout;

  wrapped_timeout = g_hash_table_lookup (priv->login_timeouts, e_indicator);
  if (wrapped_timeout) {
    login_timeout = g_value_get_uint (wrapped_timeout);
    g_hash_table_remove (priv->login_timeouts, e_indicator);
    empathy_indicator_hide (e_indicator);
    g_object_unref(e_indicator);
    g_slice_free (GValue, wrapped_timeout);
  }
  g_object_unref (e_indicator);
  g_object_unref (manager);
  g_slice_free (LoginData, data);

  return FALSE;
}

static void
indicate_login_cb (EmpathyIndicator *e_indicator,
                   EmpathyIndicatorManager *manager)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  GSList *events, *l;
  EmpathyContact *contact;

  g_hash_table_remove (priv->login_timeouts, e_indicator);
  empathy_indicator_hide (e_indicator);
  g_object_unref (e_indicator);

  contact = empathy_indicator_get_contact (e_indicator);
  /* If the contact has an event activate it, otherwise the
   * default handler of row-activated will be called. */
  events = empathy_event_manager_get_events (priv->event_manager);
  for (l = events; l; l = l->next) {
    EmpathyEvent *event = l->data;

    if (event->contact == contact) {
      empathy_event_activate (event);
      return;
    }
  }

  /* Else start a new conversation */
  empathy_dispatcher_chat_with_contact (contact, NULL, NULL);
}

EmpathyIndicatorManager *
empathy_indicator_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_INDICATOR_MANAGER, NULL);
}

static void
indicator_manager_finalize (GObject *object)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (object);

  g_slist_foreach (priv->indicator_events, (GFunc) free_indicator_event,
          NULL);
  g_object_unref (priv->event_manager);
  g_object_unref (priv->indicate_server);
  g_object_unref (priv->login_timeouts);
}

static GObject *
indicator_manager_constructor (GType type,
			   guint n_props,
			   GObjectConstructParam *props)
{
	GObject *retval;

	if (manager_singleton) {
		retval = g_object_ref (manager_singleton);
	} else {
		retval = G_OBJECT_CLASS (empathy_indicator_manager_parent_class)->constructor
			(type, n_props, props);

		manager_singleton = EMPATHY_INDICATOR_MANAGER (retval);
		g_object_add_weak_pointer (retval, (gpointer) &manager_singleton);
	}

	return retval;
}

static void
empathy_indicator_manager_class_init (EmpathyIndicatorManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = indicator_manager_finalize;
  object_class->constructor = indicator_manager_constructor;

  signals[SERVER_ACTIVATE] =
    g_signal_new ("server-activate",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE,
      0);

  g_type_class_add_private (object_class, sizeof (EmpathyIndicatorManagerPriv));
}

static void
empathy_indicator_manager_init (EmpathyIndicatorManager *manager)
{
  EmpathyIndicatorManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
    EMPATHY_TYPE_INDICATOR_MANAGER, EmpathyIndicatorManagerPriv);

  manager->priv = priv;

  priv->event_manager = empathy_event_manager_dup_singleton ();
  priv->login_timeouts = g_hash_table_new (NULL, NULL);
  priv->indicate_server = indicate_server_ref_default();
  indicate_server_set_type (priv->indicate_server, "message.instant");
  indicate_server_set_desktop_file (priv->indicate_server, "/usr/share/applications/empathy.desktop");

  g_signal_connect (priv->indicate_server,
        INDICATE_SERVER_SIGNAL_SERVER_DISPLAY,
        G_CALLBACK(indicate_server_activate),
        manager);

  g_signal_connect (priv->event_manager, "event-added",
        G_CALLBACK (indicator_manager_event_added_cb),
        manager);
  g_signal_connect (priv->event_manager, "event-removed",
        G_CALLBACK (indicator_manager_event_removed_cb),
        manager);
  g_signal_connect (priv->event_manager, "event-updated",
        G_CALLBACK (indicator_manager_event_updated_cb),
        manager);
}

void
empathy_indicator_manager_set_server_visible (EmpathyIndicatorManager *manager,
                        gboolean visible)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);

  if (visible) {
    indicate_server_show (priv->indicate_server);
  } else {
    /* Causes a crash on next show currently due to object not being
       unregistered from dbus
    indicate_server_hide (priv->indicate_server);
    */
  }
}

EmpathyIndicator *
empathy_indicator_manager_add_indicator (EmpathyIndicatorManager *manager,
                        EmpathyContact          *sender,
                        const gchar             *body)
{
  return empathy_indicator_new (sender, body, "im");
}

void
empathy_indicator_manager_add_login_indicator (EmpathyIndicatorManager *manager,
                        EmpathyContact          *contact)
{
  EmpathyIndicatorManagerPriv *priv = GET_PRIV (manager);
  guint login_timeout;
  GValue *wrapped_timeout;
  EmpathyIndicator *e_indicator = empathy_indicator_new (contact, NULL, "login");
  LoginData *login_data = g_slice_new0 (LoginData);
  if (!login_data) {
    return;
  }
  login_data->indicator = g_object_ref(e_indicator);
  login_data->manager = g_object_ref(manager);

  login_timeout = g_timeout_add_seconds(15, indicate_login_timeout, login_data);
  wrapped_timeout = g_slice_new0 (GValue);
  g_value_init (wrapped_timeout, G_TYPE_UINT);
  g_value_set_uint (wrapped_timeout, login_timeout);
  g_hash_table_insert (priv->login_timeouts, e_indicator, wrapped_timeout);
  g_signal_connect(e_indicator, "activate",
         G_CALLBACK(indicate_login_cb), manager);
  empathy_indicator_show (e_indicator);
}
