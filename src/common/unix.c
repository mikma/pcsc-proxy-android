/***************************************************************************
    begin       : Sat Sep 28 2013
    copyright   : (C) 2013 by Mikael Magnusson

 ***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 *                                                                         *
 ***************************************************************************/

/*
 * Abstract Unix domain sockets
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "unix.h"
#include "message.h"
#include "network.h"

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


static int init_sockaddr_un(struct sockaddr_un *sun, socklen_t *sunlen,
                            const char *path) {
  int pathlen = strlen(path);

  if ((pathlen + 1) > sizeof(sun->sun_path))
    return -1;

  sun->sun_family=AF_UNIX;
  sun->sun_path[0] = 0;
  memcpy(sun->sun_path + 1, path, pathlen);
  *sunlen = pathlen + offsetof(struct sockaddr_un, sun_path) + 1;
  return 0;
}

static int pp_uxlisten(int family, const char *path, int channel) {
  union {
    struct sockaddr raw;
    struct sockaddr_un sun;
  } addr;
  socklen_t len = sizeof(addr);
  int s;

  memset(&addr, 0, sizeof(addr));

  if (init_sockaddr_un(&addr.sun, &len, path)) {
    DEBUGPE("ERROR: init_sockaddr_un()\n");
    return -1;
  }

  s=socket(AF_UNIX, SOCK_STREAM, 0);
  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (bind(s, &addr.raw, len)) {
    DEBUGPE("ERROR: bind(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  if (listen(s, 10)) {
    DEBUGPE("ERROR: listen(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  return s;
}



static int pp_uxconnect(const char *path, int channel) {
  union {
    struct sockaddr raw;
    struct sockaddr_un sun;
  } addr;
  int s;
  socklen_t len = sizeof(addr);

  memset(&addr, 0, sizeof(addr));

  if (init_sockaddr_un(&addr.sun, &len, path)) {
    DEBUGPE("ERROR: init_sockaddr_un()\n");
    return -1;
  }

  s=socket(AF_UNIX, SOCK_STREAM,0);

  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (connect(s, &addr.raw, len)) {
    DEBUGPE("ERROR: connect(%s): %d=%s\n", path, errno, strerror(errno));
    close(s);
    return -1;
  }

  return s;
}


static netopts_t pp_unix_opts = {
  .listen = pp_uxlisten,
  .accept = pp_accept,
  .connect = pp_uxconnect,
  .recv = pp_recv,
  .send = pp_send,
  .getport = NULL,
};


int pp_unix_init(netopts_t **opts) {
  DEBUGPI("Init Unix domain socket\n");
  *opts = &pp_unix_opts;
  return 0;
}

void pp_unix_fini() {
}
