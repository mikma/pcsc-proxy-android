/***************************************************************************
    begin       : Tue Oct 1 2013
    copyright   : (C) 2013 by Mikael Magnusson
    email       : mikma264@gmail.com

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


#include "tls.h"
#include "tlsopts.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>



#define ENABLE_DEBUGE

struct PP_TLS_SERVER_CONTEXT {
  int socket;
};


struct PP_TLS_CLIENT_CONTEXT {
  int socket;
};

struct PP_TLS_SESSION {
  int socket;
};


static int nullenc_init_server(PP_TLS_SERVER_CONTEXT **ctx_p){
  PP_TLS_SERVER_CONTEXT *ctx = malloc(sizeof(PP_TLS_SERVER_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  *ctx_p = ctx;
  return 0;
}



static int nullenc_fini_server(PP_TLS_SERVER_CONTEXT *ctx){

  free(ctx);
  return 0;
}


static int nullenc_init_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;

  *session_p = session;
  return 0;
}



static int nullenc_fini_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION *session){
  
  free(session);
  return 0;
}






static int nullenc_init_client(PP_TLS_CLIENT_CONTEXT **ctx_p){
  PP_TLS_CLIENT_CONTEXT *ctx = malloc(sizeof(PP_TLS_CLIENT_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  *ctx_p = ctx;
  return 0;
}



static int nullenc_fini_client(PP_TLS_CLIENT_CONTEXT *ctx){

  free(ctx);
  return 0;
}


static int nullenc_client_set_srp_auth(PP_TLS_CLIENT_CONTEXT *ctx,
                           const char *name, const char *password)
{
  return 0;
}


static int nullenc_init_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;

  *session_p = session;
  return 0;
}



static int nullenc_fini_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION *session){

  free(session);
  return 0;
}








static int nullenc_tls_recv(PP_TLS_SESSION *session, s_message *msg) {
  char *p;
  int bytesRead;

  assert(msg);

  bytesRead=0;
  p=(char*) msg;

  /* read header */
  while(bytesRead<sizeof(s_msg_header)) {
    ssize_t i;

    i=sizeof(s_msg_header)-bytesRead;
    i=recv(session->socket, p, i, 0);
    if (i<0) {
      if (errno!=EINTR) {
	/* DEBUGPE("ERROR: read(): %d=%s\n", (int) i, gnutls_strerror(i)); */
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
    i=recv(session->socket, p, i, 0);
    if (i<0) {
      /* DEBUGPE("ERROR: read(): %d=%s\n", (int) i, gnutls_strerror(i)); */
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



static int nullenc_tls_send(PP_TLS_SESSION *session, const s_message *msg) {
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

    i=send(session->socket, (const void*)p, bytesLeft, 0);

    /* evaluate */
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: send(): %d\n", (int) i);
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


static int nullenc_tls_get_socket(PP_TLS_SESSION *session)
{
  return session->socket;
}


static tlsopts_t pp_nullenc_opts = {
  .init_server = nullenc_init_server,
  .fini_server = nullenc_fini_server,
  .init_server_session = nullenc_init_server_session,
  .fini_server_session = nullenc_fini_server_session,
  .init_client = nullenc_init_client,
  .fini_client = nullenc_fini_client,
  .client_set_srp_auth = nullenc_client_set_srp_auth,
  .init_client_session = nullenc_init_client_session,
  .fini_client_session = nullenc_fini_client_session,
  .recv = nullenc_tls_recv,
  .send = nullenc_tls_send,
  .get_socket = nullenc_tls_get_socket,
};


int pp_nullenc_init(tlsopts_t **opts) {
  *opts = &pp_nullenc_opts;
  return 0;
}

void pp_nullenc_fini() {
}
