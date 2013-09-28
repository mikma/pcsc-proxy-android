/***************************************************************************
    begin       : Wed Jan 20 2010
    copyright   : (C) 2010 by Martin Preuss
    email       : martin@libchipcard.de

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "network.h"
#include "message.h"

#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <netdb.h>



int pp_listen(int family, const char *ip, int port) {
  union {
    struct sockaddr raw;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
  } addr;
  int s;
  int fl;

  memset(&addr, 0, sizeof(addr));
  addr.raw.sa_family=family;
  switch (family) {
  case AF_INET:
    if (!inet_aton(ip, &addr.in.sin_addr)) {
      DEBUGPE("ERROR: inet_aton(): %d=%s\n", errno, strerror(errno));
      return -1;
    }
    addr.in.sin_port=htons(port);
    break;
  case AF_INET6:
    if (!inet_pton(family, ip, &addr.in6.sin6_addr)) {
      DEBUGPE("ERROR: inet_pton(): %d=%s\n", errno, strerror(errno));
      return -1;
    }
    addr.in6.sin6_port=htons(port);
    break;
  default:
    DEBUGPE("ERROR: unknown family %d\n", family);
  }

  s=socket(family, SOCK_STREAM,0);
  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  fl=1;
  if (setsockopt(s,
		 SOL_SOCKET,
		 SO_REUSEADDR,
		 &fl,
		 sizeof(fl))) {
    DEBUGPE("ERROR: setsockopt(): %s", strerror(errno));
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



int pp_accept(int sk) {
  socklen_t addrLen;
  int newS;
  struct sockaddr peerAddr;

  addrLen=sizeof(peerAddr);
  newS=accept(sk, &peerAddr, &addrLen);
  if (newS!=-1)
    return newS;
  else {
    if (errno!=EINTR) {
      DEBUGPE("ERROR: accept(): %d=%s\n", errno, strerror(errno));
    }
    return -1;
  }
}



int pp_connect_by_ip(const char *ip, int port) {
  union {
    struct sockaddr raw;
    struct sockaddr_in in;
  } addr;
  int s;

  memset(&addr, 0, sizeof(addr));
#if defined(PF_INET)
  addr.raw.sa_family=PF_INET;
#elif defined (AF_INET)
  addr.raw.sa_family=AF_INET;
#endif
  if (!inet_aton(ip, &addr.in.sin_addr)) {
    struct hostent *he;

    he=gethostbyname(ip);
    if (!he) {
      DEBUGPE("ERROR: gethostbyname(%s): %d=%s\n", ip, errno, strerror(errno));
      return -1;
    }
    memcpy(&(addr.in.sin_addr),
	   he->h_addr_list[0],
	   sizeof(struct in_addr));
  }
  addr.in.sin_port=htons(port);

#if defined(PF_INET)
  s=socket(PF_INET, SOCK_STREAM,0);
#elif defined (AF_INET)
  s=socket(AF_INET, SOCK_STREAM,0);
#endif
  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (connect(s, &addr.raw, sizeof(struct sockaddr_in))) {
    DEBUGPE("ERROR: connect(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  return s;
}



int pp_recv(int sk, s_message *msg) {
  char *p;
  int bytesRead;

  assert(msg);

  bytesRead=0;
  p=(char*) msg;

  /* read header */
  while(bytesRead<sizeof(s_msg_header)) {
    ssize_t i;

    i=sizeof(s_msg_header)-bytesRead;
    i=read(sk, p, i);
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: read(): %d=%s\n", errno, strerror(errno));
	return -1;
      }
    }
    else if (i==0) {
      if (bytesRead==0) {
	DEBUGPI("INFO: peer disconnected (header)\n");
        return 0;
      }
      else {
	DEBUGPE("ERROR: eof met prematurely (header, %d bytes)\n", bytesRead);
      }
      return -1;
    }
    bytesRead+=i;
    p+=i;
    DEBUGPI("INFO: Received %d bytes (header)\n", bytesRead);
  }

  /* check length */
  if (msg->header.len>=PP_MAX_MESSAGE_LEN) {
    DEBUGPE("ERROR: Request too long (%d bytes)\n", msg->header.len);
    return -1;
  }
  DEBUGPI("INFO: Message has %d bytes (type: %s)\n",
	 msg->header.len,
	 pp_getMsgTypeText(msg->header.type));

  /* read payload */
  while(bytesRead<msg->header.len) {
    ssize_t i;

    i=msg->header.len-bytesRead;
    i=read(sk, p, i);
    if (i<0) {
      DEBUGPE("ERROR: read(): %d=%s\n", errno, strerror(errno));
      return -1;
    }
    else if (i==0) {
      DEBUGPE("ERROR: eof met prematurely (payload, %d bytes)\n",
	     bytesRead);
      return -1;
    }
    bytesRead+=i;
    p+=i;
    DEBUGPI("INFO: Received %d bytes (payload)\n", bytesRead);
  }

  return bytesRead;
}



int pp_send(int sk, const s_message *msg) {
  int bytesLeft;
  const uint8_t *p;

  /* send */
  DEBUGPI("INFO: Sending %d bytes (type %s)\n",
	 msg->header.len,
	 pp_getMsgTypeText(msg->header.type));

  bytesLeft=msg->header.len;
  p=(const uint8_t*) msg;

  while(bytesLeft) {
    ssize_t i;

    i=send(sk, (const void*)p, bytesLeft, 0);

    /* evaluate */
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: send(): %d=%s\n", errno, strerror(errno));
	return -1;
      }
    }
    else if (i==0) {
    }
    else {
      bytesLeft-=(int) i;
      p+=i;
    }
  }

  return 0;
}

static int pp_getport(int s) {
  union {
    struct sockaddr sa;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
  } addr;
  socklen_t len = sizeof(addr);

  memset(&addr, 0, sizeof(addr));

  if (getsockname(s, (struct sockaddr*)&addr, &len) == -1) {
    DEBUGPE("ERROR: getsockname %d\n", errno);
    return -1;
  }

  switch (addr.sa.sa_family) {
  case AF_INET:
    return ntohs(addr.in.sin_port);
  case AF_INET6:
    return ntohs(addr.in6.sin6_port);
  default:
    return -1;
  }
}


static netopts_t pp_network_opts = {
  .listen = pp_listen,
  .accept = pp_accept,
  .connect = pp_connect_by_ip,
  .recv = pp_recv,
  .send = pp_send,
  .getport = pp_getport,
};

int pp_network_init(netopts_t **opts) {
  *opts = &pp_network_opts;
  return 0;
}
