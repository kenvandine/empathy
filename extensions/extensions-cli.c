#include "extensions.h"

#include <telepathy-glib/proxy-subclass.h>

static void _emp_ext_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/signals-marshal.h"
#include "_gen/cli-misc-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

void
emp_cli_init (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      _emp_ext_register_dbus_glib_marshallers ();

      tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_PROXY,
          emp_cli_misc_add_signals);
      initialized = TRUE;
    }
}
