#include "extensions.h"

#include <telepathy-glib/proxy-subclass.h>

static void _emp_ext_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/signals-marshal.h"
#include "_gen/cli-misc-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

static gpointer
emp_cli_once (gpointer data)
{
  _emp_ext_register_dbus_glib_marshallers ();

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_PROXY,
      emp_cli_misc_add_signals);

  return NULL;
}

void
emp_cli_init (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, emp_cli_once, NULL);
}
