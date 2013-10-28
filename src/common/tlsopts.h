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


#ifndef PP_TLSOPTS_H
#define PP_TLSOPTS_H



#include "message.h"
#include "tls.h"


struct tlsopts {
  /**
   * Call this once at the start of the server.
   */
  int (*init_server)(PP_TLS_SERVER_CONTEXT **ctx);
  int (*fini_server)(PP_TLS_SERVER_CONTEXT *ctx);

  /**
   * Call this for every session.
   */
  int (*init_server_session)(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION **session, int sock);
  int (*fini_server_session)(PP_TLS_SERVER_CONTEXT *ctx, PP_TLS_SESSION *session);

  int (*init_client)(PP_TLS_CLIENT_CONTEXT **ctx);
  int (*fini_client)(PP_TLS_CLIENT_CONTEXT *ctx);
  int (*client_set_srp_auth)(PP_TLS_CLIENT_CONTEXT *ctx, const char *name, const char *password);

  int (*init_client_session)(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION **session, int sock);
  int (*fini_client_session)(PP_TLS_CLIENT_CONTEXT *ctx, PP_TLS_SESSION *session);

  int (*recv)(PP_TLS_SESSION *session, s_message *msg);
  int (*send)(PP_TLS_SESSION *session, const s_message *msg);
  int (*get_socket)(PP_TLS_SESSION *session);
};

typedef struct tlsopts tlsopts_t;

int pp_tlsopts_init_any(int nullenc, tlsopts_t **opts);

#endif
