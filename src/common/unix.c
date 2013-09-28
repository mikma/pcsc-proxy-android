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
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


static int pp_uxlisten(int family, const char *path, int channel) {
  union {
    struct sockaddr raw;
    struct sockaddr_un sun;
  } addr;
  int s;

  memset(&addr, 0, sizeof(addr));

  addr.raw.sa_family=AF_UNIX;
  addr.sun.sun_path[0] = 0;
  snprintf(addr.sun.sun_path + 1, sizeof(addr.sun.sun_path)-1, "%s", path);

  s=socket(AF_UNIX, SOCK_STREAM, 0);
  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (bind(s, &addr.raw, sizeof(addr))) {
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

  memset(&addr, 0, sizeof(addr));
  addr.raw.sa_family=AF_UNIX;
  addr.sun.sun_path[0] = 0;
  snprintf(addr.sun.sun_path + 1, sizeof(addr.sun.sun_path)-1, "%s", path);

  s=socket(AF_UNIX, SOCK_STREAM,0);

  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (connect(s, &addr.raw, sizeof(struct sockaddr_un))) {
    DEBUGPE("ERROR: connect(): %d=%s\n", errno, strerror(errno));
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
  *opts = &pp_unix_opts;
  return 0;
}

void pp_unix_fini() {
}
