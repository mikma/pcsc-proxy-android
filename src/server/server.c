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
#include "unix.h"
#ifdef USE_BLUETOOTH
# include "bluetooth.h"
#endif
#include "tls.h"
#include "tlsopts.h"

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


#define HAVE_IPV6

static int pp_daemon_abort=0;
static int pp_wake_pipe[2] = {-1, -1};


int pp_stop() {
  char buf = 0;

  pp_daemon_abort=1;
  DEBUGPI("INFO: Terminating daemon.\n");

  if (pp_wake_pipe[1] < 0) {
    return -1;
  }

  write(pp_wake_pipe[1], &buf, 1);
  close(pp_wake_pipe[1]);
  pp_wake_pipe[1] = -1;
  return 0;
}


/* Signal handler */

struct sigaction saINT,saTERM, saINFO, saHUP, saCHLD;

static void signalHandler(int s) {
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


/*
static int handleSetTimeout(s_message *msg) {
  LONG res;

  DEBUGPI("INFO: SetTimeout\n");
  res=SCardSetTimeout(msg->setTimeoutStruct.hContext,
		      msg->setTimeoutStruct.dwTimeout);
  msg->setTimeoutStruct.rv=res;
  return 0;
}
*/



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
  res=SCardCancel(msg->cancelStruct.hCard);
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
  LPSCARD_READERSTATE rgReaderStates=NULL;

  DEBUGPI("INFO: GetStatusChange (sizeof SCARD_READERSTATE: %d)\n",
	 (int)(sizeof(SCARD_READERSTATE)));
  if (msg->getStatusChangeStruct.cReaders) {
    uint32_t i;
    LPSCARD_READERSTATE pDst;
    s_readerstate *pSrc;

    rgReaderStates=(LPSCARD_READERSTATE) malloc(sizeof(SCARD_READERSTATE)*
						  msg->getStatusChangeStruct.cReaders);
    memset(rgReaderStates, 0, sizeof(SCARD_READERSTATE)*msg->getStatusChangeStruct.cReaders);

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
    LPSCARD_READERSTATE pSrc;

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








static int pp_nextMsg(const tlsopts_t *tlsopts,PP_TLS_SESSION *session) {
  union {
    char buffer[PP_MAX_MESSAGE_LEN];
    s_message msg;
  } m;
  int rv;

  /* recv message from client */
  rv=tlsopts->recv(session, &m.msg);
  if (rv==0) {
    return 1;
  }
  else if (rv<0) {
    DEBUGPI("INFO: Could not receive message\n");
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
    //rv=handleSetTimeout(&m.msg);
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

  rv=tlsopts->send(session, &m.msg);
  if (rv<0) {
    DEBUGPE("ERROR: Could not send message\n");
    return rv;
  }

  return 0;
}



static int pp_handleConnection(const tlsopts_t *tlsopts,
                               PP_TLS_SESSION *session) {
  int rv=0;

  for (;;) {
    rv=pp_nextMsg(tlsopts, session);
    if (rv)
      break;
  }

  return rv;
}

static int write_pid_file(const char *pid_file) {
  FILE *file = fopen(pid_file, "w");

  if (file == NULL)
    return -1;
  fprintf(file, "%d", getpid());
  fclose(file);
  return 0;
}


int main(int argc, char **argv) {
  int sk;
  int rv;
  const char *addr=NULL;
  PP_TLS_SERVER_CONTEXT *ctx = NULL;
  netopts_t *opts=NULL;
  int port=-1;
  int opt;
  int family = AF_INET;
  int unenc = 0;
  tlsopts_t *tlsopts = NULL;
  int i;
  const char *pid_file=NULL;

  pp_daemon_abort=0;

  if (pipe(pp_wake_pipe)) {
    DEBUGPE("ERROR: Could not setup exit pipe");
    return 2;
  }

  rv=setSignalHandler();
  if (rv) {
    DEBUGPE("ERROR: Could not setup signal handler\n");
    return 2;
  }

  DEBUGPD("DEBUG argc:%d", argc);
  for (i=0; i<argc; i++) {
    DEBUGPD("DEBUG argv[%d]:%s", i, argv[i]);
  }

  while ((opt = getopt(argc, argv, "b:f:p:ui:")) != -1) {
    DEBUGPD("DEBUG getopt %c %p %c", opt, optarg, optarg?optarg[0]:'X');

    switch (opt) {
    case 'b':
      addr=optarg;
      break;
    case 'f':
      switch (optarg[0]) {
#ifdef USE_BLUETOOTH
      case 'b':
        family = AF_BLUETOOTH;
        break;
#endif
      case '4':
        family = AF_INET;
        break;
#ifdef HAVE_IPV6
      case '6':
        family = AF_INET6;
        break;
#endif
      case 'u':
        family = AF_UNIX;
        break;
      default:
        DEBUGPE("Usage: %s [-b addr]\n", argv[0]);
        exit(-1);
      }
      break;
    case 'p':
      port=atoi(optarg);
      break;
#ifdef ENABLE_NULLENC
    case 'u':
      unenc=1;
      break;
#endif
    case 'i':
      pid_file=optarg;
      DEBUGPI("INFO: Pid file: %s", pid_file);
      break;
    default:
      DEBUGPE("Usage: %s [-b addr]\n", argv[0]);
      exit(-1);
    }
  }

  if (pid_file)
    unlink(pid_file);

  if (pp_tlsopts_init_any(unenc, &tlsopts) < 0) {
    DEBUGPE("ERROR: Failed to initialize tls methods\n");
    exit(-1);
  }

  if (addr == NULL) {
    if (family == AF_INET)
      addr="0.0.0.0";
#ifdef HAVE_IPV6
    else if (family == AF_INET6)
      addr="::";
#endif
#ifdef USE_BLUETOOTH
    else if (family == AF_BLUETOOTH)
      addr="00:00:00:00:00:00";
#endif

    if (addr == NULL) {
      DEBUGPE("ERROR: Address/path not set and no default available\n");
      exit(-1);
    }
  }

  if (port == -1) {
    if (family == AF_INET)
      port=PP_TCP_PORT;
#ifdef USE_BLUETOOTH
    else if (family == AF_BLUETOOTH)
      /* Default port */
      port=0;
#endif
  }

  DEBUGPD("DEBUG -f %d -b %s", family, addr);

  if (opts == NULL) {
#ifdef HAVE_IPV6
    if (family == AF_INET || family == AF_INET6)
#else
    if (family == AF_INET)
#endif
      pp_network_init(&opts);
    else if (family == AF_UNIX)
      pp_unix_init(&opts);
#ifdef USE_BLUETOOTH
    else if (family == AF_BLUETOOTH)
      pp_bluetooth_init(&opts);
#endif
    else {
      DEBUGPE("ERROR: Unsupported device type: %d\n", family);
      exit(-1);
    }
  }

  rv=tlsopts->init_server(&ctx);
  if (rv<0) {
    DEBUGPE("ERROR: Unable to init TLS context\n");
    return 2;
  }

  sk=opts->listen(family, addr, port);
  if (sk<0) {
    DEBUGPE("ERROR: Could not start listening.\n");
    return 2;
  }

  if (opts->getport)
    port = opts->getport(sk);

  DEBUGPI("INFO: Listening on [%s]:%d\n", addr, port);

  if (pid_file)
    write_pid_file(pid_file);

  while(!pp_daemon_abort) {
    PP_TLS_SESSION *session = NULL;
    int newS;
    int maxFd;
    fd_set readfds;
    int res;

    FD_ZERO(&readfds);
    FD_SET(sk, &readfds);
    maxFd = sk;

    FD_SET(pp_wake_pipe[0], &readfds);
    if (maxFd < pp_wake_pipe[0])
      maxFd = pp_wake_pipe[0];

    res = select(maxFd + 1, &readfds, NULL, NULL, NULL);
    if (res < 0) {
      DEBUGPE("ERROR: Select failed");
      break;
    }

    if (FD_ISSET(pp_wake_pipe[0], &readfds)) {
      DEBUGPI("INFO: Awaken");
      continue;
    } else if (!FD_ISSET(sk, &readfds)) {
      continue;
    }

    newS=opts->accept(sk);
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

	rv=tlsopts->init_server_session(ctx, &session, newS);
	if (rv<0) {
	  DEBUGPE("ERROR: Could not setup server session.\n");
	  exit(2);
	}

	rv=pp_handleConnection(tlsopts, session);
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

  close(sk);

  if (pp_wake_pipe[0] >= 0) {
    close(pp_wake_pipe[0]);
    pp_wake_pipe[0] = -1;
  }

  switch (family) {
  case AF_INET:
  case AF_INET6:
    /* pp_network_fini(); */
    break;
#ifdef USE_BLUETOOTH
  case AF_BLUETOOTH:
    pp_bluetooth_fini();
    break;
#endif
  }

  return 0;
}



