/***************************************************************************
    begin       : Fri Aug 30 2013
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>



#define ENABLE_DEBUGE



struct PP_TLS_SERVER_CONTEXT {
  SSL_CTX *ssl_ctx;
};


struct PP_TLS_CLIENT_CONTEXT {
  SSL_CTX *ssl_ctx;
};

struct PP_TLS_SESSION {
  SSL *ssl;
  BIO *bio;
  int socket;
};


static DH* setup_dh()
{
  DH* dh = DH_new();
  if (!dh)
    return NULL;

  if (DH_generate_parameters_ex(dh, 2, DH_GENERATOR_2, 0) < 0) {
    DH_free(dh);
    return NULL;
  }

  int codes = 0;
  if (DH_check(dh, &codes) < 0) {
    DH_free(dh);
    return NULL;
  }

  if (DH_generate_key(dh) < 0) {
    DH_free(dh);
  }

  return dh;
}


int pp_init_server(PP_TLS_SERVER_CONTEXT **ctx_p){
  PP_TLS_SERVER_CONTEXT *ctx = malloc(sizeof(PP_TLS_SERVER_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  SSL_library_init();
  SSL_load_error_strings();
  ctx->ssl_ctx = SSL_CTX_new(TLSv1_1_server_method());

  if (ctx->ssl_ctx == NULL) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  DH *dh = setup_dh();
  SSL_CTX_set_tmp_dh(ctx->ssl_ctx, dh);

  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, "ADH-AES256-SHA") < 0){
    DH_free(dh);
    DEBUGPE("ERROR: SSL cipher list failed\n");
    ERR_print_errors_fp(stderr);
    free(ctx);
    return -1;
  }

  *ctx_p = ctx;
  return 0;
}



int pp_fini_server(PP_TLS_SERVER_CONTEXT *ctx){

  SSL_CTX_free(ctx->ssl_ctx);
  free(ctx);
  return 0;
}



int pp_init_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  int ret;
  int close_flag = 0;
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;

  session->ssl = SSL_new(ctx->ssl_ctx);
  if (session->ssl == NULL) {
    DEBUGPE("ERROR: SSL session failed\n");
    ERR_print_errors_fp(stderr);
    free(session);
    return -1;
  }

  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, "ADH-AES256-SHA") < 0){
    DEBUGPE("ERROR: SSL cipher list failed\n");
    ERR_print_errors_fp(stderr);
    free(session);
    return -1;
  }

  session->bio = BIO_new_socket(sock, close_flag);
  if (session->bio == NULL) {
    DEBUGPE("ERROR: BIO failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(session->ssl);
    free(session);
    return -1;
  }

  SSL_set_bio(session->ssl, session->bio, session->bio);

  ret=SSL_accept(session->ssl);
  if (ret<0) {
    DEBUGPE("ERROR: SSL accept failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(session->ssl);
    free(session);
    /* DEBUGPE("ERROR: Handshake failed (%s)\n", */
    /*         SSL_get_error(session->ssl, ret)); */
    return -1;
  }

  *session_p = session;
  return 0;
}



int pp_fini_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION *session){
  
  SSL_shutdown(session->ssl);
  SSL_free(session->ssl);
  free(session);
  return 0;
}






int pp_init_client(PP_TLS_CLIENT_CONTEXT **ctx_p){
  PP_TLS_CLIENT_CONTEXT *ctx = malloc(sizeof(PP_TLS_CLIENT_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

  SSL_library_init();
  SSL_load_error_strings();
  ctx->ssl_ctx = SSL_CTX_new(TLSv1_1_client_method());

  if (ctx->ssl_ctx == NULL) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, "ADH-AES256-SHA") < 0){
    DEBUGPE("ERROR: SSL cipher list failed\n");
    ERR_print_errors_fp(stderr);
    free(ctx);
    return -1;
  }

  *ctx_p = ctx;
  return 0;
}



int pp_fini_client(PP_TLS_CLIENT_CONTEXT *ctx){

  SSL_CTX_free(ctx->ssl_ctx);
  free(ctx);
  return 0;
}



int pp_init_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION **session_p, int sock){
  int ret;
  int close_flag = 0;
  PP_TLS_SESSION *session = malloc(sizeof(PP_TLS_SESSION));
  memset(session, 0, sizeof(*session));

  session->socket = sock;

  session->ssl = SSL_new(ctx->ssl_ctx);
  if (session->ssl == NULL) {
    DEBUGPE("ERROR: SSL session failed\n");
    ERR_print_errors_fp(stderr);
    free(session);
    return -1;
  }

  session->bio = BIO_new_socket(sock, close_flag);
  if (session->bio == NULL) {
    DEBUGPE("ERROR: BIO failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(session->ssl);
    free(session);
    return -1;
  }

  SSL_set_bio(session->ssl, session->bio, session->bio);

  ret=SSL_connect(session->ssl);
  if (ret<0) {
    DEBUGPE("ERROR: Handshake failed\n");
    /*         SSL_get_error(session->ssl, ret)); */
    ERR_print_errors_fp(stderr);
    SSL_free(session->ssl);
    free(session);
    return -1;
  }

  *session_p = session;
  return 0;
}



int pp_fini_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION *session){

  SSL_shutdown(session->ssl);
  SSL_free(session->ssl);
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
    i=SSL_read(session->ssl, p, i);
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
    i=SSL_read(session->ssl, p, i);
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

    i=SSL_write(session->ssl, (const void*)p, bytesLeft);

    /* evaluate */
    if (i<0) {
      if (errno!=EINTR) {
	DEBUGPE("ERROR: send(): %d\n", (int) i);
        ERR_print_errors_fp(stderr);
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











