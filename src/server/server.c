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


#include "message.h"
#include "network.h"
#include "tls.h"

#include <string.h>
#include <errno.h>

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <winscard.h>



static int pp_daemon_abort=0;



/* Signal handler */

struct sigaction saINT,saTERM, saINFO, saHUP, saCHLD;

void signalHandler(int s) {
  switch(s) {
  case SIGINT:
  case SIGTERM:
#ifdef SIGHUP
  case SIGHUP:
#endif
    pp_daemon_abort=1;
    DEBUGPI("INFO: Terminating daemon.\n");
    break;

#ifdef SIGCHLD
  case SIGCHLD:
    for (;;) {
      pid_t pid;
      int stat_loc;

      pid=waitpid((pid_t)-1, &stat_loc, WNOHANG);
      if (pid==-1 || pid==0)
	break;
      else {
	DEBUGPI("INFO: Service %d finished.\n", (int)pid);
      }
    }
    break;
#endif

  default:
    DEBUGPI("INFO: Unhandled signal %d\n", s);
    break;
  } /* switch */
}



int setSingleSignalHandler(struct sigaction *sa, int sig) {
  sa->sa_handler=signalHandler;
  sigemptyset(&sa->sa_mask);
  sa->sa_flags=0;
  if (sigaction(sig, sa,0)) {
    DEBUGPE("ERROR: sigaction(%d): %d=%s",
	   sig, errno, strerror(errno));
    return -1;
  }
  return 0;
}



int setSignalHandler() {
  int rv;

  rv=setSingleSignalHandler(&saINT, SIGINT);
  if (rv)
    return rv;
#ifdef SIGCHLD
  rv=setSingleSignalHandler(&saCHLD, SIGCHLD);
  if (rv)
    return rv;
#endif
  rv=setSingleSignalHandler(&saTERM, SIGTERM);
  if (rv)
    return rv;
#ifdef SIGHUP
  rv=setSingleSignalHandler(&saHUP, SIGHUP);
  if (rv)
    return rv;
#endif

  return 0;
}






static int handleEstablishContext(s_message *msg) {
  LONG res;
  SCARDCONTEXT hContext;

  DEBUGPI("INFO: EstablishContext\n");
  res=SCardEstablishContext(msg->establishStruct.dwScope,
			    NULL,
			    NULL,
                            &hContext);
  msg->establishStruct.hContext=hContext;
  msg->establishStruct.rv=res;
  return 0;
}



static int handleReleaseContext(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: ReleaseContext\n");
  res=SCardReleaseContext(msg->releaseStruct.hContext);
  msg->establishStruct.rv=res;
  return 0;
}



static int handleIsValidContext(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: IsValidContext\n");
  res=SCardIsValidContext(msg->isValidStruct.hContext);
  msg->isValidStruct.rv=res;
  return 0;
}



static int handleSetTimeout(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: SetTimeout\n");
  res=SCardSetTimeout(msg->setTimeoutStruct.hContext,
		      msg->setTimeoutStruct.dwTimeout);
  msg->setTimeoutStruct.rv=res;
  return 0;
}



static int handleConnect(s_message *msg) {
  LONG res;
  SCARDHANDLE hCard;
  DWORD dwActiveProtocol;

  DEBUGPI("INFO: Connect\n");
  res=SCardConnect(msg->connectStruct.hContext,
		   msg->connectStruct.szReader,
		   msg->connectStruct.dwShareMode,
		   msg->connectStruct.dwPreferredProtocols,
		   &hCard,
                   &dwActiveProtocol);

  msg->connectStruct.hCard=hCard;
  msg->connectStruct.dwActiveProtocol=dwActiveProtocol;

  msg->connectStruct.rv=res;
  return 0;
}



static int handleReconnect(s_message *msg) {
  LONG res;
  DWORD dwActiveProtocol;

  DEBUGPI("INFO: Reconnect\n");
  res=SCardReconnect(msg->reconnectStruct.hCard,
		     msg->reconnectStruct.dwShareMode,
		     msg->reconnectStruct.dwPreferredProtocols,
                     msg->reconnectStruct.dwInitialization,
		     &dwActiveProtocol);

  msg->reconnectStruct.dwActiveProtocol=dwActiveProtocol;

  msg->reconnectStruct.rv=res;
  return 0;
}



static int handleDisconnect(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: Disconnect\n");
  res=SCardDisconnect(msg->disconnectStruct.hCard,
		      msg->disconnectStruct.dwDisposition);

  msg->disconnectStruct.rv=res;
  return 0;
}



static int handleBeginTransaction(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: BeginTransaction\n");
  res=SCardBeginTransaction(msg->beginStruct.hCard);
  msg->beginStruct.rv=res;
  return 0;
}



static int handleEndTransaction(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: EndTransaction\n");
  res=SCardEndTransaction(msg->endStruct.hCard,
			  msg->endStruct.dwDisposition);
  msg->endStruct.rv=res;
  return 0;
}



static int handleCancelTransaction(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: CancelTransaction\n");
  res=SCardCancelTransaction(msg->cancelStruct.hCard);
  msg->cancelStruct.rv=res;
  return 0;
}



static int handleStatus(s_message *msg) {
  LONG res;
  DWORD cchReaderLen;
  DWORD dwState;
  DWORD dwProtocol;
  DWORD cbAtrLen;

  DEBUGPI("INFO: Status\n");
  cchReaderLen=msg->statusStruct.pcchReaderLen;
  if (cchReaderLen>sizeof(msg->statusStruct.mszReaderName))
    cchReaderLen=sizeof(msg->statusStruct.mszReaderName);
  cbAtrLen=msg->statusStruct.pcbAtrLen;
  if (cbAtrLen>sizeof(msg->statusStruct.pbAtr))
    cbAtrLen=sizeof(msg->statusStruct.pbAtr);

  res=SCardStatus(msg->statusStruct.hCard,
		  msg->statusStruct.mszReaderName,
		  &cchReaderLen,
		  &dwState,
		  &dwProtocol,
		  msg->statusStruct.pbAtr,
		  &cbAtrLen);
  /* results */
  msg->statusStruct.pcchReaderLen=cchReaderLen;
  msg->statusStruct.dwState=dwState;
  msg->statusStruct.dwProtocol=dwProtocol;
  msg->statusStruct.pcbAtrLen=cbAtrLen;

  msg->statusStruct.rv=res;
  return 0;
}



static int handleGetStatusChange(s_message *msg) {
  LONG res;
  LPSCARD_READERSTATE_A rgReaderStates=NULL;

  DEBUGPI("INFO: GetStatusChange (sizeof SCARD_READERSTATE_A: %d)\n",
	 (int)(sizeof(SCARD_READERSTATE_A)));
  if (msg->getStatusChangeStruct.cReaders) {
    uint32_t i;
    LPSCARD_READERSTATE_A pDst;
    s_readerstate *pSrc;

    rgReaderStates=(LPSCARD_READERSTATE_A) malloc(sizeof(SCARD_READERSTATE_A)*
						  msg->getStatusChangeStruct.cReaders);
    memset(rgReaderStates, 0, sizeof(SCARD_READERSTATE_A)*msg->getStatusChangeStruct.cReaders);

    pDst=rgReaderStates;
    pSrc=msg->getStatusChangeStruct.readerStates;
    for (i=0; i<msg->getStatusChangeStruct.cReaders; i++) {
      if (pSrc->mszReaderName[0])
	pDst->szReader=pSrc->mszReaderName;
      else
        pDst->szReader=NULL;

      /* copy states */
      pDst->dwCurrentState=pSrc->dwCurrentState;
      pDst->dwEventState=pSrc->dwEventState;

      /* copy ATR */
      pDst->cbAtr=pSrc->cbAtrLen;
      if (pDst->cbAtr>sizeof(pDst->rgbAtr))
	pDst->cbAtr=sizeof(pDst->rgbAtr);
      memmove(pDst->rgbAtr, pSrc->pbAtr, pDst->cbAtr);

      DEBUGPI("INFO: Status vars of reader [%s]: %08lx [%08lx] (%s%s%s%s%s%s%s%s%s%s%s)\n",
	     pDst->szReader,
	     pDst->dwCurrentState,
	     pDst->dwEventState,
	     (pDst->dwCurrentState & SCARD_STATE_IGNORE)?" ignore":"",
	     (pDst->dwCurrentState & SCARD_STATE_CHANGED)?" changed":"",
	     (pDst->dwCurrentState & SCARD_STATE_UNKNOWN)?" unknown":"",
	     (pDst->dwCurrentState & SCARD_STATE_UNAVAILABLE)?" unavailable":"",
	     (pDst->dwCurrentState & SCARD_STATE_EMPTY)?" empty":"",
	     (pDst->dwCurrentState & SCARD_STATE_PRESENT)?" present":"",
	     (pDst->dwCurrentState & SCARD_STATE_ATRMATCH)?" atrmatch":"",
	     (pDst->dwCurrentState & SCARD_STATE_EXCLUSIVE)?" exclusive":"",
	     (pDst->dwCurrentState & SCARD_STATE_INUSE)?" inuse":"",
	     (pDst->dwCurrentState & SCARD_STATE_MUTE)?" mute":"",
	     (pDst->dwCurrentState & SCARD_STATE_UNPOWERED)?" unpowered":"");

      /* next */
      pSrc++;
      pDst++;
    }
  }

  res=SCardGetStatusChange(msg->getStatusChangeStruct.hContext,
			   msg->getStatusChangeStruct.dwTimeout,
			   (msg->getStatusChangeStruct.cReaders)?rgReaderStates:NULL,
			   msg->getStatusChangeStruct.cReaders);

  if (msg->getStatusChangeStruct.cReaders) {
    uint32_t i;
    s_readerstate *pDst;
    LPSCARD_READERSTATE_A pSrc;

    pSrc=rgReaderStates;
    pDst=msg->getStatusChangeStruct.readerStates;
    for (i=0; i<msg->getStatusChangeStruct.cReaders; i++) {
      DEBUGPI("INFO: Status of reader after GetStatusChange [%s]: %08lx [%08lx] (%s%s%s%s%s%s%s%s%s%s%s)\n",
	     pSrc->szReader,
	     pSrc->dwCurrentState,
	     pSrc->dwEventState,
	     (pSrc->dwCurrentState & SCARD_STATE_IGNORE)?" ignore":"",
	     (pSrc->dwCurrentState & SCARD_STATE_CHANGED)?" changed":"",
	     (pSrc->dwCurrentState & SCARD_STATE_UNKNOWN)?" unknown":"",
	     (pSrc->dwCurrentState & SCARD_STATE_UNAVAILABLE)?" unavailable":"",
	     (pSrc->dwCurrentState & SCARD_STATE_EMPTY)?" empty":"",
	     (pSrc->dwCurrentState & SCARD_STATE_PRESENT)?" present":"",
	     (pSrc->dwCurrentState & SCARD_STATE_ATRMATCH)?" atrmatch":"",
	     (pSrc->dwCurrentState & SCARD_STATE_EXCLUSIVE)?" exclusive":"",
	     (pSrc->dwCurrentState & SCARD_STATE_INUSE)?" inuse":"",
	     (pSrc->dwCurrentState & SCARD_STATE_MUTE)?" mute":"",
	     (pSrc->dwCurrentState & SCARD_STATE_UNPOWERED)?" unpowered":"");

      /* copy states */
      pDst->dwCurrentState=pSrc->dwCurrentState;
      pDst->dwEventState=pSrc->dwEventState;

      /* copy ATR */
      pDst->cbAtrLen=pSrc->cbAtr;
      if (pDst->cbAtrLen>sizeof(pDst->pbAtr))
	pDst->cbAtrLen=sizeof(pDst->pbAtr);
      memmove(pDst->pbAtr, pSrc->rgbAtr, pDst->cbAtrLen);

      /* next */
      pSrc++;
      pDst++;
    }
  }

  msg->getStatusChangeStruct.rv=res;

  return 0;
}



static int handleCancel(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: Cancel\n");
  res=SCardCancel(msg->cancelStruct.hCard);
  msg->cancelStruct.rv=res;
  return 0;
}



static int handleControl(s_message *msg) {
  LONG res;
  DWORD bytesReturned=0;

  DEBUGPI("INFO: Control\n");
  res=SCardControl(msg->controlStruct.hCard,
		   msg->controlStruct.dwControlCode,
		   msg->controlStruct.pbSendBuffer,
		   msg->controlStruct.cbSendLength,
		   msg->controlStruct.pbRecvBuffer,
		   msg->controlStruct.cbRecvLength,
                   &bytesReturned);

  msg->controlStruct.dwBytesReturned=bytesReturned;

  msg->controlStruct.rv=res;
  return 0;
}



static int handleTransmit(s_message *msg) {
  LONG res;
  DWORD cbRecvLength;
  SCARD_IO_REQUEST pioSendPci;
  SCARD_IO_REQUEST pioRecvPci;
  uint8_t sendBuffer[PP_MAX_MESSAGE_LEN];
  int maxDataLength;

  DEBUGPI("INFO: Transmit\n");

  maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_transmit)-1);

  if (msg->transmitStruct.cbBufferLength>maxDataLength) {
    DEBUGPE("ERROR: SendMessage too long (%d>%d)\n",
	   msg->transmitStruct.cbBufferLength, maxDataLength);
    msg->header.len=PP_MSG_SIZE(s_transmit)-1;
    msg->transmitStruct.rv=SCARD_F_INTERNAL_ERROR;
    return 0;
  }

  /* prepare */
  cbRecvLength=msg->transmitStruct.pcbRecvLength;
  if (cbRecvLength>maxDataLength) {
    DEBUGPI("INFO: Cutting cbRecvLength to maxDataLength (%d>%d)\n",
	   (int) cbRecvLength, (int) maxDataLength);
    cbRecvLength=maxDataLength;
  }

  pioRecvPci.dwProtocol=msg->transmitStruct.ioRecvPciProtocol;
  pioRecvPci.cbPciLength=msg->transmitStruct.ioRecvPciLength;

  pioSendPci.dwProtocol=msg->transmitStruct.ioSendPciProtocol;
  pioSendPci.cbPciLength=msg->transmitStruct.ioSendPciLength;

  if (msg->transmitStruct.cbBufferLength)
    /* copy send buffer into local buffer, so that we can
     * use the message buffer for receiption */
    memmove(sendBuffer,
	    msg->transmitStruct.pcbBuffer,
	    msg->transmitStruct.cbBufferLength);

  /* exec */
  res=SCardTransmit(msg->transmitStruct.hCard,
		    &pioSendPci,
                    sendBuffer,
		    msg->transmitStruct.cbBufferLength,
		    &pioRecvPci,
		    msg->transmitStruct.pcbBuffer,
		    &cbRecvLength);

  /* post process */
  msg->transmitStruct.cbBufferLength=cbRecvLength;
  msg->transmitStruct.ioRecvPciProtocol=pioRecvPci.dwProtocol;
  msg->transmitStruct.ioRecvPciLength=pioRecvPci.cbPciLength;

  /* adapt message size */
  msg->header.len=PP_MSG_SIZE(s_transmit)+msg->transmitStruct.cbBufferLength-1;

  msg->transmitStruct.rv=res;
  return 0;
}



static int handleGetAttrib(s_message *msg) {
  LONG res;
  DWORD cbAttrLen;

  DEBUGPI("INFO: GetAttrib\n");

  /* prepare */
  cbAttrLen=msg->getSetStruct.cbAttrLen;

  /* exec */
  res=SCardGetAttrib(msg->getSetStruct.hCard,
		     msg->getSetStruct.dwAttrId,
		     msg->getSetStruct.pbAttr,
		     &cbAttrLen);

  /* post process */
  msg->getSetStruct.cbAttrLen=cbAttrLen;
  msg->getSetStruct.rv=res;
  return 0;
}



static int handleSetAttrib(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: SetAttrib\n");

  /* exec */
  res=SCardSetAttrib(msg->getSetStruct.hCard,
		     msg->getSetStruct.dwAttrId,
		     msg->getSetStruct.pbAttr,
		     msg->getSetStruct.cbAttrLen);

  /* post process */
  msg->getSetStruct.rv=res;
  return 0;
}



static int handleListReaders(s_message *msg) {
  LONG res;
  DWORD cchReaderLen;

  DEBUGPI("INFO: ListReaders\n");

  /* prepare */
  cchReaderLen=msg->listReadersStruct.cchReaderLen;
  if (msg->listReadersStruct.isSizeQueryOnly) {
    res=SCardListReaders(msg->listReadersStruct.hContext,
			 NULL,
			 NULL,
			 &cchReaderLen);
    if (res!=SCARD_S_SUCCESS) {
      DEBUGPE("ERROR: SCardListReaders (%08x: %s)\n",
	     (unsigned int) res,
	     pcsc_stringify_error(res));
    }
    /* post process */
    msg->listReadersStruct.cchReaderLen=cchReaderLen;
  }
  else {
    int maxDataLength;

    maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_listreaders)-1);

    if (cchReaderLen>maxDataLength)
      cchReaderLen=maxDataLength;

    /* exec */
    res=SCardListReaders(msg->listReadersStruct.hContext,
			 NULL,
			 msg->listReadersStruct.szReader,
			 &cchReaderLen);
    if (res!=SCARD_S_SUCCESS) {
      DEBUGPE("ERROR: SCardListReaders (%08x: %s)\n",
	     (unsigned int) res,
	     pcsc_stringify_error(res));
    }

    /* post process */
    msg->listReadersStruct.cchReaderLen=cchReaderLen;
    msg->header.len=(PP_MSG_SIZE(s_listreaders)+cchReaderLen)-1;
  }

  msg->listReadersStruct.rv=res;
  return 0;
}



static int handleListGroups(s_message *msg) {
  LONG res;
  DWORD cchGroupLen;

  DEBUGPI("INFO: ListGroups\n");

  /* prepare */
  cchGroupLen=msg->listGroupsStruct.cchGroupLen;
  if (msg->listGroupsStruct.isSizeQueryOnly) {
    res=SCardListReaderGroups(msg->listGroupsStruct.hContext,
			      NULL,
			      &cchGroupLen);
    if (res!=SCARD_S_SUCCESS) {
      DEBUGPE("ERROR: SCardListGroups (%08x: %s)\n",
	     (unsigned int) res,
	     pcsc_stringify_error(res));
    }
    /* post process */
    msg->listGroupsStruct.cchGroupLen=cchGroupLen;
  }
  else {
    int maxDataLength;

    maxDataLength=PP_MAX_MESSAGE_LEN-(PP_MSG_SIZE(s_listgroups)-1);

    if (cchGroupLen>maxDataLength)
      cchGroupLen=maxDataLength;

    /* exec */
    res=SCardListReaderGroups(msg->listGroupsStruct.hContext,
			      msg->listGroupsStruct.szGroup,
			      &cchGroupLen);
    if (res!=SCARD_S_SUCCESS) {
      DEBUGPE("ERROR: SCardListGroups (%08x: %s)\n",
	     (unsigned int) res,
	     pcsc_stringify_error(res));
    }

    /* post process */
    msg->listGroupsStruct.cchGroupLen=cchGroupLen;
    msg->header.len=(PP_MSG_SIZE(s_listgroups)+cchGroupLen)-1;
  }

  msg->listGroupsStruct.rv=res;
  return 0;
}








static int pp_nextMsg(PP_TLS_SERVER_CONTEXT *ctx) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;

  /* recv message from client */
  rv=pp_tls_recv(ctx->session, &m.msg);
  if (rv==0) {
    return 1;
  }
  else if (rv<0) {
    DEBUGPE("ERROR: Could not receive message\n");
    return rv;
  }

  switch(m.msg.header.type) {
  case SCARD_ESTABLISH_CONTEXT:
    rv=handleEstablishContext(&m.msg);
    break;
  case SCARD_RELEASE_CONTEXT:
    rv=handleReleaseContext(&m.msg);
    break;
  case SCARD_ISVALIDCONTEXT:
    rv=handleIsValidContext(&m.msg);
    break;
  case SCARD_SETTIMEOUT:
    rv=handleSetTimeout(&m.msg);
    break;
  case SCARD_CONNECT:
    rv=handleConnect(&m.msg);
    break;
  case SCARD_RECONNECT:
    rv=handleReconnect(&m.msg);
    break;
  case SCARD_DISCONNECT:
    rv=handleDisconnect(&m.msg);
    break;
  case SCARD_BEGIN_TRANSACTION:
    rv=handleBeginTransaction(&m.msg);
    break;
  case SCARD_END_TRANSACTION:
    rv=handleEndTransaction(&m.msg);
    break;
  case SCARD_CANCEL_TRANSACTION:
    rv=handleCancelTransaction(&m.msg);
    break;
  case SCARD_STATUS:
    rv=handleStatus(&m.msg);
    break;
  case SCARD_GET_STATUS_CHANGE:
    rv=handleGetStatusChange(&m.msg);
    break;
  case SCARD_CANCEL:
    rv=handleCancel(&m.msg);
    break;
  case SCARD_CONTROL:
    rv=handleControl(&m.msg);
    break;
  case SCARD_TRANSMIT:
    rv=handleTransmit(&m.msg);
    break;
  case SCARD_GET_ATTRIB:
    rv=handleGetAttrib(&m.msg);
    break;
  case SCARD_SET_ATTRIB:
    rv=handleSetAttrib(&m.msg);
    break;
  case SCARD_LIST_READERS:
    rv=handleListReaders(&m.msg);
    break;
  case SCARD_LIST_GROUPS:
    rv=handleListGroups(&m.msg);
    break;

  default:
    DEBUGPE("ERROR: Unknown message type %d\n", m.msg.header.type);
    exit(4);
  }

  rv=pp_tls_send(ctx->session, &m.msg);
  if (rv<0) {
    DEBUGPE("ERROR: Could not send message\n");
    return rv;
  }

  return 0;
}



static int pp_handleConnection(PP_TLS_SERVER_CONTEXT *ctx) {
  int rv=0;

  for (;;) {
    rv=pp_nextMsg(ctx);
    if (rv)
      break;
  }

  return rv;
}



int main(int argc, char **argv) {
  int sk;
  int rv;
  const char *addr="0.0.0.0";
  PP_TLS_SERVER_CONTEXT ctx;

  rv=setSignalHandler();
  if (rv) {
    DEBUGPE("ERROR: Could not setup signal handler\n");
    return 2;
  }

  if (argc>1)
    addr=argv[1];

  rv=pp_init_server(&ctx);
  if (rv<0) {
    DEBUGPE("ERROR: Unable to init TLS context\n");
    return 2;
  }

  DEBUGPI("INFO: Listening on [%s]:%d\n",
         addr, PP_TCP_PORT);

  sk=pp_listen(addr, PP_TCP_PORT);
  if (sk<0) {
    DEBUGPE("ERROR: Could not start listening.\n");
    return 2;
  }

  while(!pp_daemon_abort) {
    int newS;

    newS=pp_accept(sk);
    if (newS!=-1) {
      pid_t pid;

      pid=fork();
      if (pid<0) {
        /* error */
      }
      else if (pid==0) {
	int rv;

	/* child */
	close(sk);
	rv=setSignalHandler();
	if (rv) {
	  DEBUGPE("ERROR: Could not setup child's signal handler\n");
	  exit(2);
	}

        ctx.socket=newS;
	rv=pp_init_server_session(&ctx);
	if (rv<0) {
	  DEBUGPE("ERROR: Could not setup server session.\n");
	  exit(2);
	}

	rv=pp_handleConnection(&ctx);
	if (rv)
	  exit(3);
	exit(0);
      }
      else {
	/* parent */
	DEBUGPI("INFO: PCSC service spawned (%d)\n", (int)pid);
	close(newS);
      }
    }
  }

  return 0;
}



