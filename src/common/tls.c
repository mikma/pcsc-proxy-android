/***************************************************************************
    begin       : Tue Feb 18 2010
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


#include "tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <gnutls/gnutls.h>

#include <assert.h>
#include <string.h>



#define ENABLE_DEBUGE



struct PP_TLS_SERVER_CONTEXT {
  gnutls_dh_params_t dh_params;
  gnutls_anon_server_credentials_t anoncred;
};


struct PP_TLS_CLIENT_CONTEXT {
  gnutls_anon_client_credentials_t anoncred;
};

struct PP_TLS_SESSION {
  gnutls_session_t gnutls;
  int socket;
};


int pp_init_server(PP_TLS_SERVER_CONTEXT **ctx_p){
  PP_TLS_SERVER_CONTEXT *ctx = malloc(sizeof(PP_TLS_SERVER_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  gnutls_global_init ();

  gnutls_dh_params_init (&(ctx->dh_params));
  gnutls_dh_params_generate2(ctx->dh_params, DH_BITS);

  gnutls_anon_allocate_server_credentials (&(ctx->anoncred));
  gnutls_anon_set_server_dh_params(ctx->anoncred, ctx->dh_params);

  *ctx_p = ctx;
  return 0;
}



int pp_fini_server(PP_TLS_SERVER_CONTEXT *ctx){
  gnutls_anon_free_server_credentials (ctx->anoncred);
  gnutls_global_deinit();

  free(ctx);
  return 0;
}



int pp_init_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  int ret;
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;
  gnutls_init (&(session->gnutls), GNUTLS_SERVER);
  gnutls_priority_set_direct(session->gnutls, "NORMAL:+ANON-DH", NULL);
  gnutls_credentials_set(session->gnutls, GNUTLS_CRD_ANON, ctx->anoncred);
  gnutls_dh_set_prime_bits(session->gnutls, DH_BITS);

  gnutls_transport_set_ptr(session->gnutls, (gnutls_transport_ptr_t) ((unsigned long int)(session->socket)));
  ret=gnutls_handshake(session->gnutls);
  if (ret < 0) {
    free(session);
    DEBUGPE("ERROR: Handshake failed (%s)\n",
	   gnutls_strerror(ret));
    return -1;
  }

  *session_p = session;
  return 0;
}



int pp_fini_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION *session){
  gnutls_deinit(session->gnutls);
  return 0;
}






int pp_init_client(PP_TLS_CLIENT_CONTEXT **ctx_p){
  PP_TLS_CLIENT_CONTEXT *ctx = malloc(sizeof(PP_TLS_CLIENT_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  gnutls_global_init ();
  gnutls_anon_allocate_client_credentials (&(ctx->anoncred));
  *ctx_p = ctx;
  return 0;
}



int pp_fini_client(PP_TLS_CLIENT_CONTEXT *ctx){
  gnutls_anon_free_client_credentials (ctx->anoncred);
  gnutls_global_deinit();

  free(ctx);
  return 0;
}



int pp_init_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  int ret;
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;
  gnutls_init(&(session->gnutls), GNUTLS_CLIENT);
  gnutls_priority_set_direct(session->gnutls, "NORMAL:+ANON-DH", NULL);
  gnutls_credentials_set(session->gnutls, GNUTLS_CRD_ANON, ctx->anoncred);
  gnutls_dh_set_prime_bits(session->gnutls, DH_BITS);

  gnutls_transport_set_ptr(session->gnutls, (gnutls_transport_ptr_t) ((unsigned long int)(session->socket)));
  ret=gnutls_handshake(session->gnutls);
  if (ret<0) {
    free(session);
    DEBUGPE("ERROR: Handshake failed (%s)\n",
	    gnutls_strerror(ret));
    return -1;
  }

  *session_p = session;
  return 0;
}



int pp_fini_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION *session){
  gnutls_deinit(session->gnutls);
  free(session);
  return 0;
}








int pp_tls_recv(PP_TLS_SESSION *session, s_message *msg) {
  char *p;
  int bytesRead;

  assert(msg);

  bytesRead=0;
  p=(char*) msg;

  /* read header */
  while(bytesRead<sizeof(s_msg_header)) {
    ssize_t i;

    i=sizeof(s_msg_header)-bytesRead;
    i=gnutls_record_recv(session->gnutls, p, i);
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: read(): %d=%s\n", (int) i, gnutls_strerror(i));
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
    i=gnutls_record_recv(session->gnutls, p, i);
    if (i<0) {
      DEBUGPE("ERROR: read(): %d=%s\n", (int) i, gnutls_strerror(i));
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



int pp_tls_send(PP_TLS_SESSION *session, const s_message *msg) {
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

    i=gnutls_record_send(session->gnutls, (const void*)p, bytesLeft);

    /* evaluate */
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: send(): %d=%s\n", (int) i, gnutls_strerror(i));
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


int pp_tls_get_socket(PP_TLS_SESSION *session)
{
  return session->socket;
}











