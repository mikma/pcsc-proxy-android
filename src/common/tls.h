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


#ifndef PP_TLS_H
#define PP_TLS_H



#include "message.h"

#include <gnutls/gnutls.h>



#define DH_BITS 1024



struct PP_TLS_SERVER_CONTEXT {
  gnutls_session_t session;
  gnutls_dh_params_t dh_params;
  gnutls_anon_server_credentials_t anoncred;
  int socket;
};
typedef struct PP_TLS_SERVER_CONTEXT PP_TLS_SERVER_CONTEXT;


/**
 * Call this once at the start of the server.
 */
int pp_init_server(PP_TLS_SERVER_CONTEXT *ctx);
int pp_fini_server(PP_TLS_SERVER_CONTEXT *ctx);

/**
 * Call this for every session.
 */
int pp_init_server_session(PP_TLS_SERVER_CONTEXT *ctx);

int pp_fini_server_session(PP_TLS_SERVER_CONTEXT *ctx);



struct PP_TLS_CLIENT_CONTEXT {
  gnutls_session_t session;
  gnutls_anon_client_credentials_t anoncred;
  int socket;
};
typedef struct PP_TLS_CLIENT_CONTEXT PP_TLS_CLIENT_CONTEXT;


int pp_init_client(PP_TLS_CLIENT_CONTEXT *ctx);
int pp_fini_client(PP_TLS_CLIENT_CONTEXT *ctx);

int pp_init_client_session(PP_TLS_CLIENT_CONTEXT *ctx);
int pp_fini_client_session(PP_TLS_CLIENT_CONTEXT *ctx);





int pp_tls_recv(gnutls_session_t session, s_message *msg);
int pp_tls_send(gnutls_session_t session, const s_message *msg);




#endif


