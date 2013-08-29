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




#define DH_BITS 1024


typedef struct PP_TLS_SERVER_CONTEXT PP_TLS_SERVER_CONTEXT;
typedef struct PP_TLS_CLIENT_CONTEXT PP_TLS_CLIENT_CONTEXT;
typedef struct PP_TLS_SESSION PP_TLS_SESSION;


/**
 * Call this once at the start of the server.
 */
int pp_init_server(PP_TLS_SERVER_CONTEXT **ctx);
int pp_fini_server(PP_TLS_SERVER_CONTEXT *ctx);

/**
 * Call this for every session.
 */
int pp_init_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION **session, int sock);

int pp_fini_server_session(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION *session);





int pp_init_client(PP_TLS_CLIENT_CONTEXT **ctx);
int pp_fini_client(PP_TLS_CLIENT_CONTEXT *ctx);

int pp_init_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION **session, int sock);
int pp_fini_client_session(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION *session);





int pp_tls_recv(PP_TLS_SESSION *session, s_message *msg);
int pp_tls_send(PP_TLS_SESSION *session, const s_message *msg);
int pp_tls_get_socket(PP_TLS_SESSION *session);




#endif


