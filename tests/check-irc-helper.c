#include "check-irc-helper.h"

void
check_server (EmpathyIrcServer *server,
              const gchar *_address,
              guint _port,
              gboolean _ssl)
{
  gchar *address;
  guint port;
  gboolean ssl;

  fail_if (server == NULL);

  g_object_get (server,
      "address", &address,
      "port", &port,
      "ssl", &ssl,
      NULL);

  fail_if (address == NULL || strcmp (address, _address) != 0);
  fail_if (port != _port);
  fail_if (ssl != _ssl);

  g_free (address);
}

void
check_network (EmpathyIrcNetwork *network,
              const gchar *_name,
              const gchar *_charset,
              struct server_t *_servers,
              guint nb_servers)
{
  gchar  *name, *charset;
  GSList *servers, *l;
  guint i;

  fail_if (network == NULL);

  g_object_get (network,
      "name", &name,
      "charset", &charset,
      NULL);

  fail_if (name == NULL || strcmp (name, _name) != 0);
  fail_if (charset == NULL || strcmp (charset, _charset) != 0);

  servers = empathy_irc_network_get_servers (network);
  fail_if (g_slist_length (servers) != nb_servers);

  /* Is that the right servers ? */
  for (l = servers, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      EmpathyIrcServer *server;
      gchar *address;
      guint port;
      gboolean ssl;

      server = l->data;

      g_object_get (server,
          "address", &address,
          "port", &port,
          "ssl", &ssl,
          NULL);

      fail_if (address == NULL || strcmp (address, _servers[i].address)
          != 0);
      fail_if (port != _servers[i].port);
      fail_if (ssl != _servers[i].ssl);

      g_free (address);
    }

  g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
  g_slist_free (servers);
  g_free (name);
  g_free (charset);
}
