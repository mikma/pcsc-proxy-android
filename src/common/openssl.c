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
#include <openssl/srp.h>

#include <assert.h>
#include <string.h>



#define ENABLE_DEBUGE

#define CIPHER_SRP "SRP"
#define CIPHER CIPHER_SRP

struct PP_TLS_SERVER_CONTEXT {
  SSL_CTX *ssl_ctx;
#ifndef OPENSSL_NO_SRP
  SRP_VBASE *srp_vbase;
  char *srp_vfile;
  void *srp_unknown_user_seed;
#endif
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


#ifndef OPENSSL_NO_SRP
static int srp_server_param_cb(SSL *ssl, int *ad, void *arg) {
  SRP_user_pwd *sup = NULL;
  PP_TLS_SERVER_CONTEXT *ctx = arg;

  if ((sup = SRP_VBASE_get_by_user(ctx->srp_vbase,
                                   SSL_get_srp_username(ssl))) == NULL) {
    DEBUGPE("User %s doesn't exist\n", SSL_get_srp_username(ssl));
    return SSL3_AL_FATAL;
  }

  if (SSL_set_srp_server_param(ssl, sup->N, sup->g,
                               sup->s, sup->v, sup->info) < 0) {
    DEBUGPE("User %s auth failed\n", SSL_get_srp_username(ssl));
    return SSL3_AL_FATAL;
  }
  return SSL_ERROR_NONE;
}


static int srp_init_server(struct PP_TLS_SERVER_CONTEXT *ctx) {
  if (ctx->srp_vfile != NULL) {
    int res;
    DEBUGPD("Use SRP verifier file %s\n", ctx->srp_vfile);

    if (!(ctx->srp_vbase = SRP_VBASE_new(ctx->srp_unknown_user_seed))) {
      DEBUGPE("Can't initialize SRP verifier %p\n",
              ctx->srp_unknown_user_seed);
      return -1;
    }

    res = SRP_VBASE_init(ctx->srp_vbase, ctx->srp_vfile);
    if (res != SRP_NO_ERROR) {
      DEBUGPE("Can't load SRP verifier file  %d\n", res);
      return -1;
    }
  }

  SSL_CTX_set_srp_username_callback(ctx->ssl_ctx, srp_server_param_cb);
  SSL_CTX_set_srp_cb_arg(ctx->ssl_ctx, ctx);
  return 0;
}
#endif /* OPENSSL_NO_SRP */



int pp_init_server(PP_TLS_SERVER_CONTEXT **ctx_p){
  PP_TLS_SERVER_CONTEXT *ctx = malloc(sizeof(PP_TLS_SERVER_CONTEXT));
  memset(ctx, 0, sizeof(*ctx));

#ifndef OPENSSL_NO_SRP
  ctx->srp_vfile = "pcsc-proxy.srp";
#endif

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  SSL_library_init();
  ctx->ssl_ctx = SSL_CTX_new(TLSv1_1_server_method());

  if (ctx->ssl_ctx == NULL) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, CIPHER) < 0){
    DEBUGPE("ERROR: SSL cipher list failed\n");
    ERR_print_errors_fp(stderr);
    free(ctx);
    return -1;
  }

  DH *dh = setup_dh();
  SSL_CTX_set_tmp_dh(ctx->ssl_ctx, dh);

#ifndef OPENSSL_NO_SRP
  if (srp_init_server(ctx) < 0) {
    free(ctx);
    return -1;
  }
#endif

  *ctx_p = ctx;
  return 0;
}



int pp_fini_server(PP_TLS_SERVER_CONTEXT *ctx){

#ifndef OPENSSL_NO_SRP
  if (ctx->srp_vbase != NULL) {
    SRP_VBASE_free(ctx->srp_vbase);
    ctx->srp_vbase = NULL;
  }
#endif
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
    char buf[256]="";
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    DEBUGPE("ERROR: %s", buf);
    return -1;
  }

  *ctx_p = ctx;
  return 0;
}



int pp_client_set_srp_auth(PP_TLS_CLIENT_CONTEXT *ctx,
                           const char *name, const char *password) {
  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, CIPHER_SRP) < 0){
    ERR_print_errors_fp(stderr);
    return -1;
  }

  if (SSL_CTX_set_srp_username(ctx->ssl_ctx, name) < 0) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  if (SSL_CTX_set_srp_password(ctx->ssl_ctx, password) < 0) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

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
