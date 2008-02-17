#include <stdlib.h>
#include <string.h>

#include <check.h>
#include "check-helpers.h"

#include <libempathy/empathy-irc-server.h>
#include <libempathy/empathy-irc-network.h>
#include <libempathy/empathy-irc-network-manager.h>

#ifndef __CHECK_IRC_HELPER_H__
#define __CHECK_IRC_HELPER_H__

struct server_t
{
  gchar *address;
  guint port;
  gboolean ssl;
};

void check_server (EmpathyIrcServer *server, const gchar *_address,
    guint _port, gboolean _ssl);

void check_network (EmpathyIrcNetwork *network, const gchar *_name,
    const gchar *_charset, struct server_t *_servers, guint nb_servers);

#endif /* __CHECK_IRC_HELPER_H__ */
