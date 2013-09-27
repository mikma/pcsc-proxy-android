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


#include "client.h"
#include "message.h"
#include "network.h"
#include "tls.h"

#include <stdio.h>
#include <time.h>
#include <paths.h>
#include <utmp.h>


#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <stdlib.h>

#include <pthread.h>




PP_EXPORT const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, sizeof(SCARD_IO_REQUEST) };	/**< Protocol Control Information for T=0 */
PP_EXPORT const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };	/**< Protocol Control Information for T=1 */
PP_EXPORT const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, sizeof(SCARD_IO_REQUEST) };	/**< Protocol Control Information for raw access */


struct PP_CLIENT_CONTEXT {
  uint32_t id;
  uint32_t pcScContext;
  PP_TLS_CLIENT_CONTEXT *tlsContext;
  PP_TLS_SESSION *tlsSession;
};
typedef struct PP_CLIENT_CONTEXT PP_CLIENT_CONTEXT;


struct PP_CARD_CONTEXT {
  uint32_t id;
  uint32_t pcScHandle;
  PP_CLIENT_CONTEXT *clientContext;
};
typedef struct PP_CARD_CONTEXT PP_CARD_CONTEXT;


#define PP_MAX_CONTEXT   64
#define PP_CONTEXT_MAGIC 0x88345289

#define PP_MAX_CARD      64
#define PP_CARD_MAGIC    0x38345289


static PP_CLIENT_CONTEXT *pp_context_array=NULL;
static PP_CARD_CONTEXT *pp_card_array=NULL;

static pthread_mutex_t pp_context_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pp_card_mutex=PTHREAD_MUTEX_INITIALIZER;

static netopts_t *g_opts;


static PP_CLIENT_CONTEXT *pp_findContext(uint32_t id) {
  uint32_t rid;
  int rv;

  DEBUGPI("INFO: Waiting for Context Mutex\n");
  rv=pthread_mutex_lock(&pp_context_mutex);
  DEBUGPI("INFO: Got Context Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    return NULL;
  }

  if (pp_context_array==NULL) {
    DEBUGPE("ERROR: Context %08x not found", id);
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  rid=id-PP_CONTEXT_MAGIC;
  if (rid>PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: Context id %08x out of range (%08x)", id, rid);
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }
  if (pp_context_array[rid].id==0) {
    DEBUGPE("ERROR: Accessing a context which isn't in use (%08x, %08x)\n", id, rid);
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  pthread_mutex_unlock(&pp_context_mutex);
  return &(pp_context_array[rid]);
}



static PP_CLIENT_CONTEXT *pp_createContext() {
  PP_CLIENT_CONTEXT *p;
  uint32_t i;
  uint32_t id;
  int rv;
  char *user;
  char *passwd;

  DEBUGPI("INFO: Waiting for Context Mutex");
  rv=pthread_mutex_lock(&pp_context_mutex);
  DEBUGPI("INFO: Got Context Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  if (pp_context_array==NULL) {
    /* create array */
    pp_context_array=(PP_CLIENT_CONTEXT*) malloc(sizeof(PP_CLIENT_CONTEXT)*PP_MAX_CONTEXT);
    if (pp_context_array==NULL) {
      DEBUGPE("ERROR: Out of memory\n");
      pthread_mutex_unlock(&pp_context_mutex);
      return NULL;
    }
    memset(pp_context_array, 0, sizeof(PP_CLIENT_CONTEXT)*PP_MAX_CONTEXT);
  }

  for (i=0; i<PP_MAX_CONTEXT; i++) {
    if (pp_context_array[i].id==0)
      break;
  }

  if (i>=PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: All contexts in use.\n");
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  p=&(pp_context_array[i]);
  id=i+PP_CONTEXT_MAGIC;
  p->id=id;

  rv=pp_init_client(&(p->tlsContext));
  if (rv<0) {
    DEBUGPE("ERROR: Unable to init TLS context\n");
    p->id=0;
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  user=getenv("PCSC_USER");
  passwd=getenv("PCSC_PASSWD");

  if (user==NULL || *user==0 || passwd==NULL || *passwd==0) {
    DEBUGPE("ERROR: PCSC_USER or PCSC_PASSWD undefined\n");
    p->id=0;
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  if (pp_client_set_srp_auth(p->tlsContext, user, passwd) < 0) {
    DEBUGPE("ERROR: Could not use SRP auth\n");
    p->id=0;
    pthread_mutex_unlock(&pp_context_mutex);
    return NULL;
  }

  pthread_mutex_unlock(&pp_context_mutex);
  DEBUGPI("INFO: Created context %08x (%u)\n", p->id, i);
  return p;
}



static void pp_releaseContext(uint32_t id) {
  uint32_t rid;
  int rv;

  DEBUGPI("INFO: Waiting for Context Mutex");
  rv=pthread_mutex_lock(&pp_context_mutex);
  DEBUGPI("INFO: Got Context Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    return;
  }

  if (pp_context_array==NULL) {
    DEBUGPE("ERROR: Context %08x not found", id);
    pthread_mutex_unlock(&pp_context_mutex);
    return;
  }

  rid=id-PP_CONTEXT_MAGIC;
  if (rid>PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: Context id %08x out of range (%08x)", id, rid);
    pthread_mutex_unlock(&pp_context_mutex);
    return;
  }

  pp_context_array[rid].id=0;
  pthread_mutex_unlock(&pp_context_mutex);
  DEBUGPI("INFO: Destroyed context %08x (%u)\n", id, rid);
}




static PP_CARD_CONTEXT *pp_findCard(uint32_t id) {
  uint32_t rid;
  int rv;

  DEBUGPI("INFO: Waiting for Card Mutex");
  rv=pthread_mutex_lock(&pp_card_mutex);
  DEBUGPI("INFO: Got Card Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    return NULL;
  }

  if (pp_card_array==NULL) {
    DEBUGPE("ERROR: Context %08x not found", id);
    pthread_mutex_unlock(&pp_card_mutex);
    return NULL;
  }

  rid=id-PP_CARD_MAGIC;
  if (rid>PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: Context id %08x out of range (%08x)", id, rid);
    pthread_mutex_unlock(&pp_card_mutex);
    return NULL;
  }

  if (pp_card_array[rid].id==0) {
    DEBUGPE("ERROR: Accessing a card which isn't in use (%08x, %08x)\n", id, rid);
    pthread_mutex_unlock(&pp_card_mutex);
    return NULL;
  }

  pthread_mutex_unlock(&pp_card_mutex);
  return &(pp_card_array[rid]);
}



static PP_CARD_CONTEXT *pp_createCard(PP_CLIENT_CONTEXT *ctx) {
  PP_CARD_CONTEXT *p;
  uint32_t i;
  uint32_t id;
  int rv;

  DEBUGPI("INFO: Waiting for Card Mutex");
  rv=pthread_mutex_lock(&pp_card_mutex);
  DEBUGPI("INFO: Got Card Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    return NULL;
  }

  if (pp_card_array==NULL) {
    /* create array */
    pp_card_array=(PP_CARD_CONTEXT*) malloc(sizeof(PP_CARD_CONTEXT)*PP_MAX_CONTEXT);
    if (pp_card_array==NULL) {
      DEBUGPE("ERROR: Out of memory\n");
      pthread_mutex_unlock(&pp_card_mutex);
      return NULL;
    }
    memset(pp_card_array, 0, sizeof(PP_CARD_CONTEXT)*PP_MAX_CONTEXT);
  }

  for (i=0; i<PP_MAX_CONTEXT; i++) {
    if (pp_card_array[i].id==0)
      break;
  }

  if (i>=PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: All cards in use.\n");
    pthread_mutex_unlock(&pp_card_mutex);
    return NULL;
  }

  p=&(pp_card_array[i]);
  id=i+PP_CARD_MAGIC;
  p->id=id;
  p->clientContext=ctx;

  pthread_mutex_unlock(&pp_card_mutex);
  DEBUGPI("INFO: Created card %08x (%u)\n", p->id, i);
  return p;
}



static void pp_releaseCard(uint32_t id) {
  uint32_t rid;
  int rv;

  DEBUGPI("INFO: Waiting for Card Mutex");
  rv=pthread_mutex_lock(&pp_card_mutex);
  DEBUGPI("INFO: Got Card Mutex (%d)\n", rv);
  if (rv) {
    DEBUGPE("ERROR: Could not lock mutex (%d)\n", rv);
    return;
  }

  if (pp_card_array==NULL) {
    DEBUGPE("ERROR: Context %08x not found", id);
    pthread_mutex_unlock(&pp_card_mutex);
    return;
  }

  rid=id-PP_CARD_MAGIC;
  if (rid>PP_MAX_CONTEXT) {
    DEBUGPE("ERROR: Context id %08x out of range (%08x)", id, rid);
    pthread_mutex_unlock(&pp_card_mutex);
    return;
  }

  pp_card_array[rid].id=0;
  pthread_mutex_unlock(&pp_card_mutex);
  DEBUGPI("INFO: Destroyed context %08x (%u)\n", id, rid);
}





static int pp_exchangeMsg(PP_CLIENT_CONTEXT *ctx, s_message *msg) {
  int rv;

  if (ctx->tlsSession == NULL) {
    if (msg->header.type!=SCARD_ESTABLISH_CONTEXT) {
	DEBUGPE("ERROR: Socket not connected when trying to send [%s]\n",
	       pp_getMsgTypeText(msg->header.type));
      return -1;
    }
    else {
      char hostname[256];
      const char *s;
      int sock;

      s=getenv("PCSC_SERVER");
      if (s && *s) {
	strncpy(hostname, s, sizeof(hostname)-1);
	hostname[sizeof(hostname)-1]=0;
      }
      else {
	char terminal[256];
	struct utmp *u_tmp_p;
	const char *ttyName=NULL;
    
	ttyName=ttyname(0);
	if (ttyName==NULL)
	  ttyName=ttyname(1);
	if (ttyName==NULL)
	  ttyName=ttyname(2);
	if (ttyName==NULL) {
	  DEBUGPE("ERROR: Unable to determine the controlling terminal");
	  return -1;
	}
	if (strlen(ttyName)>4)
	  strncpy(terminal, ttyName+5, sizeof(terminal)-1);
	terminal[sizeof(terminal)-1]=0;
    
	/* determine remote ip using getutent */
	//utmpname(_PATH_UTMP);
	setutent();  /* rewind file */
    
	DEBUGPI("INFO: Looking for UT for terminal [%s]\n", terminal);
	while ((u_tmp_p = getutent()) != NULL) {
	  if (u_tmp_p->ut_type==USER_PROCESS &&
	      u_tmp_p->ut_line[0] &&
	      strcasecmp(u_tmp_p->ut_line, terminal)==0)
	    break;
	}
    
	if (u_tmp_p==NULL) {
	  DEBUGPE("ERROR: getutent(): %d=%s\n", errno, strerror(errno));
	  endutent();
	  return -1;
	}
	if (u_tmp_p->ut_host[0]==0) {
	  DEBUGPE("ERROR: Unable to determine remote host from utmp\n");
	  endutent();
	  return -1;
	}
	DEBUGPI("INFO: User is logged in from [%s]\n", u_tmp_p->ut_host);
	strncpy(hostname, u_tmp_p->ut_host, sizeof(hostname)-1);
	hostname[sizeof(hostname)-1]=0;
	endutent();
      }

      pp_network_init(&g_opts);

      /* connect */
      rv=g_opts->connect(hostname, PP_TCP_PORT);
      if (rv<0) {
	DEBUGPE("ERROR: Could not connect\n");
	return rv;
      }
      DEBUGPI("INFO: Connected.\n");
      sock=rv;

      /* setup tls */
      rv=pp_init_client_session(ctx->tlsContext, &(ctx->tlsSession), sock);
      if (rv<0) {
	DEBUGPE("ERROR: Unable to establish TLS connection.\n");
	close(sock);
	return rv;
      }
    }
  }

  rv=pp_tls_send(ctx->tlsSession, msg);
  if (rv<0) {
    DEBUGPE("ERROR: Could not send message\n");
    return rv;
  }

  rv=pp_tls_recv(ctx->tlsSession, msg);
  if (rv<0) {
    DEBUGPE("ERROR: Could not receive message\n");
    return rv;
  }

  return 0;
}




PP_EXPORT LONG SCardEstablishContext(DWORD dwScope,
				     LPCVOID pvReserved1,
				     LPCVOID pvReserved2,
				     LPSCARDCONTEXT phContext) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);
  ctx=pp_createContext();
  if (ctx==NULL) {
    DEBUGPI("INFO: Could not create context\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_ESTABLISH_CONTEXT;
  m.msg.header.len=PP_MSG_SIZE(s_establish);

  /* payload */
  m.msg.establishStruct.dwScope=dwScope;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    pp_releaseContext(ctx->id);
    return SCARD_F_INTERNAL_ERROR;
  }

  if (m.msg.establishStruct.rv==SCARD_S_SUCCESS) {
    DEBUGPI("INFO: Mapping SCardContext %08x to %08x\n",
	   ctx->id, m.msg.establishStruct.hContext);
    ctx->pcScContext=m.msg.establishStruct.hContext;
    *phContext=ctx->id;
  }
  else
    pp_releaseContext(ctx->id);
  return (LONG) m.msg.establishStruct.rv;
}



PP_EXPORT LONG SCardReleaseContext(SCARDCONTEXT hContext) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;
  int sock;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_RELEASE_CONTEXT;
  m.msg.header.len=PP_MSG_SIZE(s_release);

  /* payload */
  m.msg.releaseStruct.hContext=ctx->pcScContext;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  DEBUGPI("INFO: Closing connection\n");
  sock = pp_tls_get_socket(ctx->tlsSession);
  pp_fini_client_session(ctx->tlsContext, ctx->tlsSession);
  ctx->tlsSession = NULL;
  close(sock);
  pp_fini_client(ctx->tlsContext);
  ctx->tlsContext = NULL;
  pp_releaseContext(ctx->id);
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.releaseStruct.rv;
}




PP_EXPORT LONG SCardIsValidContext(SCARDCONTEXT hContext) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_ISVALIDCONTEXT;
  m.msg.header.len=PP_MSG_SIZE(s_isvalid);

  /* payload */
  m.msg.isValidStruct.hContext=ctx->pcScContext;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.isValidStruct.rv;
}



PP_EXPORT LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_SETTIMEOUT;
  m.msg.header.len=PP_MSG_SIZE(s_settimeout);

  /* payload */
  m.msg.setTimeoutStruct.hContext=ctx->pcScContext;
  m.msg.setTimeoutStruct.dwTimeout=dwTimeout;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.setTimeoutStruct.rv;
}



PP_EXPORT LONG SCardConnect(SCARDCONTEXT hContext,
			    LPCSTR szReader,
			    DWORD dwShareMode,
			    DWORD dwPreferredProtocols,
			    LPSCARDHANDLE phCard,
			    LPDWORD pdwActiveProtocol) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;
  PP_CARD_CONTEXT *card;

  if (strlen(szReader)>=sizeof(m.msg.connectStruct.szReader)) {
    DEBUGPE("ERROR: Reader name too long\n");
    return SCARD_E_INVALID_VALUE;
  }

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  card=pp_createCard(ctx);
  if (card==NULL) {
    DEBUGPE("ERROR: All card contexts in use\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_CONNECT;
  m.msg.header.len=PP_MSG_SIZE(s_connect);

  /* payload */
  m.msg.connectStruct.hContext=ctx->pcScContext;
  strncpy(m.msg.connectStruct.szReader,
	  szReader,
	  sizeof(m.msg.connectStruct.szReader)-1);
  m.msg.connectStruct.szReader[sizeof(m.msg.connectStruct.szReader)-1]=0;
  m.msg.connectStruct.dwShareMode=dwShareMode;
  m.msg.connectStruct.dwPreferredProtocols=dwPreferredProtocols;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    pp_releaseCard(card->id);
    return SCARD_F_INTERNAL_ERROR;
  }

  if (m.msg.connectStruct.rv==SCARD_S_SUCCESS) {
    DEBUGPI("INFO: Mapping SCardHandle %08x to %08x\n",
	   card->id, m.msg.connectStruct.hCard);
    card->pcScHandle=m.msg.connectStruct.hCard;
    *phCard=card->id;
    *pdwActiveProtocol=m.msg.connectStruct.dwActiveProtocol;
  }
  else
    pp_releaseCard(card->id);

  return (LONG) m.msg.connectStruct.rv;
}



PP_EXPORT LONG SCardReconnect(SCARDHANDLE hCard,
			      DWORD dwShareMode,
			      DWORD dwPreferredProtocols,
			      DWORD dwInitialization,
			      LPDWORD pdwActiveProtocol) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_RECONNECT;
  m.msg.header.len=PP_MSG_SIZE(s_reconnect);

  /* payload */
  m.msg.reconnectStruct.hCard=card->pcScHandle;
  m.msg.reconnectStruct.dwShareMode=dwShareMode;
  m.msg.reconnectStruct.dwPreferredProtocols=dwPreferredProtocols;
  m.msg.reconnectStruct.dwInitialization=dwInitialization;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  *pdwActiveProtocol=m.msg.reconnectStruct.dwActiveProtocol;

  return (LONG) m.msg.reconnectStruct.rv;
}



PP_EXPORT LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_DISCONNECT;
  m.msg.header.len=PP_MSG_SIZE(s_disconnect);

  /* payload */
  m.msg.disconnectStruct.hCard=card->pcScHandle;
  m.msg.disconnectStruct.dwDisposition=dwDisposition;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.disconnectStruct.rv;
}



PP_EXPORT LONG SCardBeginTransaction(SCARDHANDLE hCard) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_BEGIN_TRANSACTION;
  m.msg.header.len=PP_MSG_SIZE(s_begin);

  /* payload */
  m.msg.beginStruct.hCard=card->pcScHandle;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.beginStruct.rv;
}



PP_EXPORT LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_END_TRANSACTION;
  m.msg.header.len=PP_MSG_SIZE(s_end);

  /* payload */
  m.msg.endStruct.hCard=card->pcScHandle;
  m.msg.endStruct.dwDisposition=dwDisposition;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.endStruct.rv;
}



PP_EXPORT LONG SCardCancelTransaction(SCARDHANDLE hCard) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_CANCEL_TRANSACTION;
  m.msg.header.len=PP_MSG_SIZE(s_cancel);

  /* payload */
  m.msg.cancelStruct.hCard=card->pcScHandle;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.cancelStruct.rv;
}



PP_EXPORT LONG SCardStatus(SCARDHANDLE hCard,
			   LPSTR mszReaderName,
			   LPDWORD pcchReaderLen,
			   LPDWORD pdwState,
			   LPDWORD pdwProtocol,
			   LPBYTE pbAtr,
			   LPDWORD pcbAtrLen) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  if (pcchReaderLen==NULL) {
    DEBUGPE("ERROR: Nullpointer given for pcchReaderLen\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (*pcchReaderLen==SCARD_AUTOALLOCATE) {
    DEBUGPE("ERROR: SCARD_AUTOALLOCATE not yet supported\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  if (pcbAtrLen==NULL) {
    DEBUGPE("ERROR: Nullpointer given for pcbAtrLen\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (*pcbAtrLen==SCARD_AUTOALLOCATE) {
    DEBUGPE("ERROR: SCARD_AUTOALLOCATE not yet supported\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_STATUS;
  m.msg.header.len=PP_MSG_SIZE(s_status);

  /* payload */
  m.msg.statusStruct.hCard=card->pcScHandle;

  m.msg.statusStruct.pcchReaderLen=*pcchReaderLen;
  if (m.msg.statusStruct.pcchReaderLen>sizeof(m.msg.statusStruct.mszReaderName))
    m.msg.statusStruct.pcchReaderLen=sizeof(m.msg.statusStruct.mszReaderName);

  m.msg.statusStruct.pcbAtrLen=*pcbAtrLen;
  if (m.msg.statusStruct.pcbAtrLen>sizeof(m.msg.statusStruct.pbAtr))
    m.msg.statusStruct.pcbAtrLen=sizeof(m.msg.statusStruct.pbAtr);

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  if (pdwState)
    *pdwState=m.msg.statusStruct.dwState;
  if (pdwProtocol)
    *pdwProtocol=m.msg.statusStruct.dwProtocol;

  /* copy reader name */
  if (m.msg.statusStruct.pcchReaderLen>sizeof(m.msg.statusStruct.mszReaderName)) {
    DEBUGPE("ERROR: Server returned a too long reader name\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  if (m.msg.statusStruct.pcchReaderLen &&
      m.msg.statusStruct.pcchReaderLen<=*pcchReaderLen)
    memmove(mszReaderName, m.msg.statusStruct.mszReaderName, m.msg.statusStruct.pcchReaderLen);
  *pcchReaderLen=m.msg.statusStruct.pcchReaderLen;

  /* copy ATR */
  if (m.msg.statusStruct.pcbAtrLen>sizeof(m.msg.statusStruct.pbAtr)) {
    DEBUGPE("ERROR: Server returned a too long ATR\n");
    return SCARD_F_INTERNAL_ERROR;
  }
  if (m.msg.statusStruct.pcbAtrLen &&
      m.msg.statusStruct.pcbAtrLen<=*pcbAtrLen)
    memmove(pbAtr, m.msg.statusStruct.pbAtr, m.msg.statusStruct.pcbAtrLen);
  *pcbAtrLen=m.msg.statusStruct.pcbAtrLen;

  return (LONG) m.msg.statusStruct.rv;
}



PP_EXPORT LONG SCardGetStatusChange(SCARDCONTEXT hContext,
				    DWORD dwTimeout,
				    LPSCARD_READERSTATE rgReaderStates,
				    DWORD cReaders) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CLIENT_CONTEXT *ctx;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_GET_STATUS_CHANGE;
  m.msg.header.len=PP_MSG_SIZE(s_getstatuschange)-sizeof(s_readerstate);

  /* payload */
  m.msg.getStatusChangeStruct.hContext=ctx->pcScContext;
  m.msg.getStatusChangeStruct.dwTimeout=dwTimeout;
  m.msg.getStatusChangeStruct.cReaders=cReaders;
  DEBUGPI("INFO: Will ask for changes on %d readers (sizeof SCARD_READERSTATE: %d)\n",
	 (unsigned int) cReaders,
	 (int)(sizeof(SCARD_READERSTATE)));

  if (cReaders) {
    uint32_t i;
    s_readerstate *pDst;
    LPSCARD_READERSTATE pSrc;

    if (m.msg.header.len+sizeof(s_readerstate)>PP_MAX_MESSAGE_LEN) {
      DEBUGPE("ERROR: Too many reader states.\n");
      return SCARD_F_INTERNAL_ERROR;
    }

    pSrc=rgReaderStates;
    pDst=m.msg.getStatusChangeStruct.readerStates;
    for (i=0; i<cReaders; i++) {
      /* copy reader name */
      if (pSrc->szReader)
	strncpy(pDst->mszReaderName,
		pSrc->szReader,
		sizeof(pDst->mszReaderName)-1);
      /* copy states */
      pDst->dwCurrentState=pSrc->dwCurrentState;
      pDst->dwEventState=pSrc->dwEventState;

      /* copy ATR */
      pDst->cbAtrLen=pSrc->cbAtr;
      if (pDst->cbAtrLen>sizeof(pDst->pbAtr))
	pDst->cbAtrLen=sizeof(pDst->pbAtr);
      memmove(pDst->pbAtr, pSrc->rgbAtr, pDst->cbAtrLen);

      /* adjust message size */
      m.msg.header.len+=sizeof(s_readerstate);

      DEBUGPI("INFO: Query reader [%s]: %08lx %08lx\n",
	     (pSrc->szReader)?(pSrc->szReader):"(unnamend)",
	     pSrc->dwCurrentState,
             pSrc->dwEventState);

      /* next */
      pSrc++;
      pDst++;
    }
  }

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  if (cReaders) {
    uint32_t i;
    LPSCARD_READERSTATE pDst;
    s_readerstate *pSrc;

    pDst=rgReaderStates;
    pSrc=m.msg.getStatusChangeStruct.readerStates;
    for (i=0; i<cReaders; i++) {
      /* copy states */
      pDst->dwCurrentState=pSrc->dwCurrentState;
      pDst->dwEventState=pSrc->dwEventState;

      /* copy ATR */
      pDst->cbAtr=pSrc->cbAtrLen;
      if (pDst->cbAtr>sizeof(pDst->rgbAtr))
	pDst->cbAtr=sizeof(pDst->rgbAtr);
      memmove(pDst->rgbAtr, pSrc->pbAtr, pDst->cbAtr);

      DEBUGPI("INFO: Status reader [%s]: %08lx %08lx\n",
	     (pDst->szReader)?(pDst->szReader):"(unnamend)",
	     pDst->dwCurrentState,
	     pDst->dwEventState);

      /* next */
      pSrc++;
      pDst++;
    }
  }

  return (LONG) m.msg.getStatusChangeStruct.rv;
}



PP_EXPORT LONG SCardCancel(SCARDHANDLE hCard) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_CANCEL;
  m.msg.header.len=PP_MSG_SIZE(s_cancel);

  /* payload */
  m.msg.cancelStruct.hCard=hCard;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.cancelStruct.rv;
}



PP_EXPORT LONG SCardControl(SCARDHANDLE hCard,
			    DWORD dwControlCode,
			    LPCVOID pbSendBuffer,
			    DWORD cbSendLength,
			    LPVOID pbRecvBuffer,
			    DWORD cbRecvLength,
			    LPDWORD lpBytesReturned) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  if (cbSendLength>sizeof(m.msg.controlStruct.pbSendBuffer)) {
    DEBUGPE("ERROR: SendBuffer too long\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (cbRecvLength>sizeof(m.msg.controlStruct.pbRecvBuffer))
    cbRecvLength=sizeof(m.msg.controlStruct.pbRecvBuffer);

  /* header */
  m.msg.header.type=SCARD_CONTROL;
  m.msg.header.len=PP_MSG_SIZE(s_control);

  /* payload */
  m.msg.controlStruct.hCard=card->pcScHandle;
  m.msg.controlStruct.dwControlCode=dwControlCode;
  m.msg.controlStruct.cbSendLength=cbSendLength;
  m.msg.controlStruct.cbRecvLength=cbRecvLength;
  if (cbSendLength && pbSendBuffer)
    memmove(m.msg.controlStruct.pbSendBuffer,
	    pbSendBuffer,
	    cbSendLength);

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  /* store return values */
  if (m.msg.controlStruct.dwBytesReturned) {
    memmove(pbRecvBuffer,
	    m.msg.controlStruct.pbRecvBuffer,
	    m.msg.controlStruct.dwBytesReturned);
  }

  if (lpBytesReturned)
    *lpBytesReturned=m.msg.controlStruct.dwBytesReturned;

  return (LONG) m.msg.controlStruct.rv;
}



PP_EXPORT LONG SCardTransmit(SCARDHANDLE hCard,
			     LPCSCARD_IO_REQUEST pioSendPci,
			     LPCBYTE pbSendBuffer,
			     DWORD cbSendLength,
			     LPSCARD_IO_REQUEST pioRecvPci,
			     LPBYTE pbRecvBuffer,
			     LPDWORD pcbRecvLength) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  int maxDataLength;
  PP_CARD_CONTEXT *card;

  maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_transmit)-1);

  fprintf(stderr,
	  "SCardTransmit:\n"
	  "  hCard          %08x\n"
	  "  pioSendPci     %p\n"
	  "  pbSendBuffer   %p\n"
	  "  cbSendLength   %d\n"
	  "  pioRecvPci     %p\n"
	  "  pbRecvBuffer   %p\n"
	  "  pcbRecvLength  %p\n"
	  "  *pcbRecvLength %d\n",
	  (uint32_t) hCard,
	  pioSendPci,
	  pbSendBuffer,
	  (int) cbSendLength,
	  pioRecvPci,
	  pbRecvBuffer,
	  pcbRecvLength,
	  (int)((pcbRecvLength)?(*pcbRecvLength):-1));

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  /* sanity checks */
  if (cbSendLength>maxDataLength) {
    DEBUGPE("ERROR: SendBuffer too long\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (cbSendLength<4) {
    DEBUGPE("ERROR: SendBuffer too short\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (pcbRecvLength==NULL) {
    DEBUGPE("ERROR: NULL pointer for pcbRecvLength");
    return SCARD_E_INVALID_VALUE;
  }

  fprintf(stderr, "Sending this:\n");
  pp_dumpString((const char*) pbSendBuffer, cbSendLength, stderr, 2);

  /* header */
  m.msg.header.type=SCARD_TRANSMIT;
  m.msg.header.len=PP_MSG_SIZE(s_transmit)+cbSendLength-1;

  /* payload */
  m.msg.transmitStruct.hCard=card->pcScHandle;
  m.msg.transmitStruct.ioSendPciProtocol=pioSendPci->dwProtocol;
  m.msg.transmitStruct.ioSendPciLength=pioSendPci->cbPciLength;

  m.msg.transmitStruct.cbBufferLength=cbSendLength;
  if (cbSendLength)
    memmove(m.msg.transmitStruct.pcbBuffer,
	    pbSendBuffer,
	    cbSendLength);

  if (pioRecvPci) {
    m.msg.transmitStruct.ioRecvPciProtocol=pioRecvPci->dwProtocol;
    m.msg.transmitStruct.ioRecvPciLength=pioRecvPci->cbPciLength;
  }

  m.msg.transmitStruct.pcbRecvLength=*pcbRecvLength;

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  /* sanity checks */
  if (m.msg.transmitStruct.cbBufferLength>maxDataLength) {
    DEBUGPE("ERROR: Server sent a too long response (%d>%d)\n",
	   m.msg.transmitStruct.cbBufferLength, maxDataLength);
    return SCARD_F_INTERNAL_ERROR;
  }

  if (m.msg.transmitStruct.cbBufferLength>*pcbRecvLength) {
    DEBUGPE("ERROR: Server sent a too long response\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  /* store return values */
  if (pioRecvPci) {
    pioRecvPci->dwProtocol=m.msg.transmitStruct.ioRecvPciProtocol;
    pioRecvPci->cbPciLength=m.msg.transmitStruct.ioRecvPciLength;
  }

  DEBUGPD("DEBUG: Moving these %d bytes\n", m.msg.transmitStruct.cbBufferLength);
  pp_dumpString((const char*) (m.msg.transmitStruct.pcbBuffer), m.msg.transmitStruct.cbBufferLength, stderr, 2);
  if (m.msg.transmitStruct.cbBufferLength && pbRecvBuffer)
    memmove(pbRecvBuffer, m.msg.transmitStruct.pcbBuffer, m.msg.transmitStruct.cbBufferLength);
  *pcbRecvLength=m.msg.transmitStruct.cbBufferLength;

  return (LONG) m.msg.transmitStruct.rv;
}



PP_EXPORT LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
			      LPBYTE pbAttr, LPDWORD pcbAttrLen) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  if (pcbAttrLen && *pcbAttrLen==SCARD_AUTOALLOCATE) {
    DEBUGPE("ERROR: SCARD_AUTOALLOCATE not yet supported\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  /* header */
  m.msg.header.type=SCARD_GET_ATTRIB;
  m.msg.header.len=PP_MSG_SIZE(s_getset);

  /* payload */
  m.msg.getSetStruct.hCard=card->pcScHandle;
  m.msg.getSetStruct.dwAttrId=dwAttrId;
  if (pcbAttrLen && pbAttr) {
    if (*pcbAttrLen>sizeof(m.msg.getSetStruct.pbAttr))
      m.msg.getSetStruct.cbAttrLen=sizeof(m.msg.getSetStruct.pbAttr);
    else
      m.msg.getSetStruct.cbAttrLen=*pcbAttrLen;
  }

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  if (pcbAttrLen)
    *pcbAttrLen=m.msg.getSetStruct.cbAttrLen;

  if (pbAttr && pcbAttrLen) {
    uint32_t l;

    l=m.msg.getSetStruct.cbAttrLen;
    if (l>*pcbAttrLen)
      l=*pcbAttrLen;
    memmove(pbAttr, m.msg.getSetStruct.pbAttr, l);
  }

  return (LONG) m.msg.getSetStruct.rv;
}



PP_EXPORT LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
			      LPCBYTE pbAttr, DWORD cbAttrLen) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  PP_CARD_CONTEXT *card;

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  card=pp_findCard(hCard);
  if (card==NULL) {
    DEBUGPE("ERROR: Card %08x not found\n", (unsigned int) hCard);
    return SCARD_E_INVALID_VALUE;
  }

  if (cbAttrLen>sizeof(m.msg.getSetStruct.pbAttr)) {
    DEBUGPE("ERROR: Attribute too long\n");
    return SCARD_E_INVALID_VALUE;
  }

  /* header */
  m.msg.header.type=SCARD_SET_ATTRIB;
  m.msg.header.len=PP_MSG_SIZE(s_getset);

  /* payload */
  m.msg.getSetStruct.hCard=card->pcScHandle;
  m.msg.getSetStruct.dwAttrId=dwAttrId;
  if (cbAttrLen && pbAttr) {
    m.msg.getSetStruct.cbAttrLen=cbAttrLen;
    memmove(m.msg.getSetStruct.pbAttr,
	    pbAttr,
	    cbAttrLen);
  }

  /* exchange */
  rv=pp_exchangeMsg(card->clientContext, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  return (LONG) m.msg.getSetStruct.rv;
}



PP_EXPORT LONG SCardListReaders(SCARDCONTEXT hContext,
				LPCSTR mszGroups,
				LPSTR mszReaders,
				LPDWORD pcchReaders) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  int maxDataLength;
  int dataLength;
  PP_CLIENT_CONTEXT *ctx;

  maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_listreaders)-1);

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  if (pcchReaders==NULL) {
    DEBUGPE("ERROR: Nullpointer given for pcchReaders\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (*pcchReaders==SCARD_AUTOALLOCATE) {
    DEBUGPE("ERROR: SCARD_AUTOALLOCATE not yet supported\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  dataLength=*pcchReaders;
  if (dataLength>maxDataLength)
    dataLength=maxDataLength;

  /* header */
  m.msg.header.type=SCARD_LIST_READERS;
  m.msg.header.len=PP_MSG_SIZE(s_listreaders)-1;

  /* payload */
  m.msg.listReadersStruct.hContext=ctx->pcScContext;
  m.msg.listReadersStruct.cchReaderLen=dataLength;

  if (mszReaders)
    m.msg.listReadersStruct.isSizeQueryOnly=0;
  else
    m.msg.listReadersStruct.isSizeQueryOnly=1;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  if (mszReaders) {
    if (m.msg.listReadersStruct.cchReaderLen>*pcchReaders) {
      DEBUGPE("ERROR: Server returned a too long response\n");
      return SCARD_F_INTERNAL_ERROR;
    }
#ifdef ENABLE_DEBUG
    {
      const char *p;

      p=m.msg.listReadersStruct.szReader;
      while(*p) {
	DEBUGPI("INFO: Found reader [%s]\n", p);
	while(*p)
	  p++;
	p++;
      }
    }
#endif
    memmove(mszReaders,
	    m.msg.listReadersStruct.szReader,
	    m.msg.listReadersStruct.cchReaderLen);
  }
  else {
    DEBUGPI("INFO: SCardListReaders needs %d bytes\n",
           m.msg.listReadersStruct.cchReaderLen);
    *pcchReaders=m.msg.listReadersStruct.cchReaderLen;
  }

  return (LONG) m.msg.listReadersStruct.rv;
}



PP_EXPORT LONG SCardListReaderGroups(SCARDCONTEXT hContext,
				     LPSTR mszGroups,
				     LPDWORD pcchGroups) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;
  int maxDataLength;
  int dataLength;
  PP_CLIENT_CONTEXT *ctx;

  maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_listgroups)-1);

  DEBUGPI("INFO: Entering here\n");

  memset(m.buffer, 0, PP_MAX_MESSAGE_LEN);

  ctx=pp_findContext(hContext);
  if (ctx==NULL) {
    DEBUGPE("ERROR: Context %08x not found\n", (unsigned int) hContext);
    return SCARD_F_INTERNAL_ERROR;
  }

  if (pcchGroups==NULL) {
    DEBUGPE("ERROR: Nullpointer given for pcchGroups\n");
    return SCARD_E_INVALID_VALUE;
  }

  if (*pcchGroups==SCARD_AUTOALLOCATE) {
    DEBUGPE("ERROR: SCARD_AUTOALLOCATE not yet supported\n");
    return SCARD_F_INTERNAL_ERROR;
  }

  dataLength=*pcchGroups;
  if (dataLength>maxDataLength)
    dataLength=maxDataLength;

  /* header */
  m.msg.header.type=SCARD_LIST_GROUPS;
  m.msg.header.len=PP_MSG_SIZE(s_listgroups)-1;

  /* payload */
  m.msg.listGroupsStruct.hContext=ctx->pcScContext;
  m.msg.listGroupsStruct.cchGroupLen=dataLength;

  if (mszGroups)
    m.msg.listGroupsStruct.isSizeQueryOnly=0;
  else
    m.msg.listGroupsStruct.isSizeQueryOnly=1;

  /* exchange */
  rv=pp_exchangeMsg(ctx, &(m.msg));
  if (rv<0) {
    return SCARD_F_INTERNAL_ERROR;
  }

  if (mszGroups) {
    if (m.msg.listGroupsStruct.cchGroupLen>*pcchGroups) {
      DEBUGPE("ERROR: Server returned a too long response\n");
      return SCARD_F_INTERNAL_ERROR;
    }
#ifdef ENABLE_DEBUG
    {
      const char *p;

      p=m.msg.listGroupsStruct.szGroup;
      while(*p) {
	DEBUGPI("INFO: Found reader [%s]\n", p);
	while(*p)
	  p++;
	p++;
      }
    }
#endif
    memmove(mszGroups,
	    m.msg.listGroupsStruct.szGroup,
	    m.msg.listGroupsStruct.cchGroupLen);
  }
  else {
    DEBUGPI("INFO: SCardListGroups needs %d bytes\n",
           m.msg.listGroupsStruct.cchGroupLen);
    *pcchGroups=m.msg.listGroupsStruct.cchGroupLen;
  }

  return (LONG) m.msg.listGroupsStruct.rv;
}



PP_EXPORT LONG SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem) {
  free((void *)pvMem);
  return 0;
}



/* copied from error.c of pcsc-lite-1.5.5 */
PP_EXPORT char *pcsc_stringify_error(const long pcscError) {
  static char strError[75];

  switch (pcscError) {
  case SCARD_S_SUCCESS:
    (void)strncpy(strError, "Command successful.", sizeof(strError));
    break;
  case SCARD_E_CANCELLED:
    (void)strncpy(strError, "Command cancelled.", sizeof(strError));
    break;
  case SCARD_E_CANT_DISPOSE:
    (void)strncpy(strError, "Cannot dispose handle.", sizeof(strError));
    break;
  case SCARD_E_INSUFFICIENT_BUFFER:
    (void)strncpy(strError, "Insufficient buffer.", sizeof(strError));
    break;
  case SCARD_E_INVALID_ATR:
    (void)strncpy(strError, "Invalid ATR.", sizeof(strError));
    break;
  case SCARD_E_INVALID_HANDLE:
    (void)strncpy(strError, "Invalid handle.", sizeof(strError));
    break;
  case SCARD_E_INVALID_PARAMETER:
    (void)strncpy(strError, "Invalid parameter given.", sizeof(strError));
    break;
  case SCARD_E_INVALID_TARGET:
    (void)strncpy(strError, "Invalid target given.", sizeof(strError));
    break;
  case SCARD_E_INVALID_VALUE:
    (void)strncpy(strError, "Invalid value given.", sizeof(strError));
    break;
  case SCARD_E_NO_MEMORY:
    (void)strncpy(strError, "Not enough memory.", sizeof(strError));
    break;
  case SCARD_F_COMM_ERROR:
    (void)strncpy(strError, "RPC transport error.", sizeof(strError));
    break;
  case SCARD_F_INTERNAL_ERROR:
    (void)strncpy(strError, "Internal error.", sizeof(strError));
    break;
  case SCARD_F_UNKNOWN_ERROR:
    (void)strncpy(strError, "Unknown error.", sizeof(strError));
    break;
  case SCARD_F_WAITED_TOO_LONG:
    (void)strncpy(strError, "Waited too long.", sizeof(strError));
    break;
  case SCARD_E_UNKNOWN_READER:
    (void)strncpy(strError, "Unknown reader specified.", sizeof(strError));
    break;
  case SCARD_E_TIMEOUT:
    (void)strncpy(strError, "Command timeout.", sizeof(strError));
    break;
  case SCARD_E_SHARING_VIOLATION:
    (void)strncpy(strError, "Sharing violation.", sizeof(strError));
    break;
  case SCARD_E_NO_SMARTCARD:
    (void)strncpy(strError, "No smart card inserted.", sizeof(strError));
    break;
  case SCARD_E_UNKNOWN_CARD:
    (void)strncpy(strError, "Unknown card.", sizeof(strError));
    break;
  case SCARD_E_PROTO_MISMATCH:
    (void)strncpy(strError, "Card protocol mismatch.", sizeof(strError));
    break;
  case SCARD_E_NOT_READY:
    (void)strncpy(strError, "Subsystem not ready.", sizeof(strError));
    break;
  case SCARD_E_SYSTEM_CANCELLED:
    (void)strncpy(strError, "System cancelled.", sizeof(strError));
    break;
  case SCARD_E_NOT_TRANSACTED:
    (void)strncpy(strError, "Transaction failed.", sizeof(strError));
    break;
  case SCARD_E_READER_UNAVAILABLE:
    (void)strncpy(strError, "Reader is unavailable.", sizeof(strError));
    break;
  case SCARD_W_UNSUPPORTED_CARD:
    (void)strncpy(strError, "Card is not supported.", sizeof(strError));
    break;
  case SCARD_W_UNRESPONSIVE_CARD:
    (void)strncpy(strError, "Card is unresponsive.", sizeof(strError));
    break;
  case SCARD_W_UNPOWERED_CARD:
    (void)strncpy(strError, "Card is unpowered.", sizeof(strError));
    break;
  case SCARD_W_RESET_CARD:
    (void)strncpy(strError, "Card was reset.", sizeof(strError));
    break;
  case SCARD_W_REMOVED_CARD:
    (void)strncpy(strError, "Card was removed.", sizeof(strError));
    break;
  case SCARD_STATE_PRESENT:
    (void)strncpy(strError, "Card was inserted.", sizeof(strError));
    break;
  case SCARD_E_UNSUPPORTED_FEATURE:
    (void)strncpy(strError, "Feature not supported.", sizeof(strError));
    break;
  case SCARD_E_PCI_TOO_SMALL:
    (void)strncpy(strError, "PCI struct too small.", sizeof(strError));
    break;
  case SCARD_E_READER_UNSUPPORTED:
    (void)strncpy(strError, "Reader is unsupported.", sizeof(strError));
    break;
  case SCARD_E_DUPLICATE_READER:
    (void)strncpy(strError, "Reader already exists.", sizeof(strError));
    break;
  case SCARD_E_CARD_UNSUPPORTED:
    (void)strncpy(strError, "Card is unsupported.", sizeof(strError));
    break;
  case SCARD_E_NO_SERVICE:
    (void)strncpy(strError, "Service not available.", sizeof(strError));
    break;
  case SCARD_E_SERVICE_STOPPED:
    (void)strncpy(strError, "Service was stopped.", sizeof(strError));
    break;
  case SCARD_E_NO_READERS_AVAILABLE:
    (void)strncpy(strError, "Cannot find a smart card reader.", sizeof(strError));
    break;
  default:
    (void)snprintf(strError, sizeof(strError)-1, "Unkown error: 0x%08lX",
		   pcscError);
  };

  /* add a null byte */
  strError[sizeof(strError)-1] = '\0';

  return strError;
}








