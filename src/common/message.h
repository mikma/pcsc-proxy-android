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

#ifndef PP_MESSAGE_H
#define PP_MESSAGE_H




/* enable or disable error messages */
#define ENABLE_DEBUGPE

/* enable or disable info messages */
/*#define ENABLE_DEBUGPI*/

/* enable or disable debug messages */
/*#define ENABLE_DEBUGPD*/


#include <stdint.h>
#include <stdio.h>

#include <pcsclite.h>


#ifdef ENABLE_DEBUGPE
# define DEBUGPE(format, args...) \
    fprintf(stderr, __FILE__":%5d:" format, __LINE__, ## args);
#else
# define DEBUGPE(format, args...)
#endif


#ifdef ENABLE_DEBUGPI
# define DEBUGPI(format, args...) \
    fprintf(stderr, __FILE__":%5d:" format, __LINE__, ## args);
#else
# define DEBUGPI(format, args...)
#endif


#ifdef ENABLE_DEBUGPD
# define DEBUGPD(format, args...) \
    fprintf(stderr, __FILE__":%5d:" format, __LINE__, ## args);
#else
# define DEBUGPD(format, args...)
#endif



#pragma pack(push, 1)


#define PP_MAX_MESSAGE_LEN (65*1024)
#define PP_MAX_BUFFER_LEN  4096


enum message_commands {
  SCARD_ESTABLISH_CONTEXT,
  SCARD_RELEASE_CONTEXT,
  SCARD_ISVALIDCONTEXT,
  SCARD_SETTIMEOUT,
  SCARD_CONNECT,
  SCARD_RECONNECT,
  SCARD_DISCONNECT,
  SCARD_BEGIN_TRANSACTION,
  SCARD_END_TRANSACTION,
  SCARD_CANCEL_TRANSACTION,
  SCARD_STATUS,
  SCARD_GET_STATUS_CHANGE,
  SCARD_TRANSMIT,
  SCARD_CONTROL,
  SCARD_CANCEL,
  SCARD_GET_ATTRIB,
  SCARD_SET_ATTRIB,
  SCARD_LIST_READERS,
  SCARD_LIST_GROUPS
};




/**
 * @brief Information contained in \ref SCARD_ESTABLISH_CONTEXT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_establish
{
  uint32_t dwScope;
  uint32_t hContext;
  uint32_t rv;
};
typedef struct s_establish s_establish;

/**
 * @brief Information contained in \ref SCARD_RELEASE_CONTEXT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_release
{
  uint32_t hContext;
  uint32_t rv;
};
typedef struct s_release s_release;


struct s_isvalid
{
  uint32_t hContext;
  uint32_t rv;
};
typedef struct s_isvalid s_isvalid;


struct s_settimeout
{
  uint32_t hContext;
  uint32_t dwTimeout;
  uint32_t rv;
};
typedef struct s_settimeout s_settimeout;

/**
 * @brief contained in \ref SCARD_CONNECT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_connect
{
  uint32_t hContext;
  char szReader[MAX_READERNAME];
  uint32_t dwShareMode;
  uint32_t dwPreferredProtocols;
  int32_t hCard;
  uint32_t dwActiveProtocol;
  uint32_t rv;
};
typedef struct s_connect s_connect;

/**
 * @brief contained in \ref SCARD_RECONNECT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_reconnect
{
  int32_t hCard;
  uint32_t dwShareMode;
  uint32_t dwPreferredProtocols;
  uint32_t dwInitialization;
  uint32_t dwActiveProtocol;
  uint32_t rv;
};
typedef struct s_reconnect s_reconnect;

/**
 * @brief contained in \ref SCARD_DISCONNECT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_disconnect
{
  int32_t hCard;
  uint32_t dwDisposition;
  uint32_t rv;
};
typedef struct s_disconnect s_disconnect;

/**
 * @brief contained in \ref SCARD_BEGIN_TRANSACTION Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_begin
{
  int32_t hCard;
  uint32_t rv;
};
typedef struct s_begin s_begin;

/**
 * @brief contained in \ref SCARD_END_TRANSACTION Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_end
{
  int32_t hCard;
  uint32_t dwDisposition;
  uint32_t rv;
};
typedef struct s_end s_end;

/**
 * @brief contained in \ref SCARD_CANCEL Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_cancel
{
  int32_t hCard;
  uint32_t rv;
};
typedef struct s_cancel s_cancel;

/**
 * @brief contained in \ref SCARD_STATUS Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_status
{
  int32_t hCard;
  char mszReaderName[MAX_READERNAME];
  uint32_t pcchReaderLen;
  uint32_t dwState;
  uint32_t dwProtocol;
  uint8_t pbAtr[MAX_ATR_SIZE];
  uint32_t pcbAtrLen;
  uint32_t rv;
};
typedef struct s_status s_status;



struct s_readerstate
{
  char mszReaderName[MAX_READERNAME];
  uint32_t dwCurrentState;
  uint32_t dwEventState;
  uint8_t pbAtr[MAX_ATR_SIZE];
  uint32_t cbAtrLen;
};
typedef struct s_readerstate s_readerstate;


struct s_getstatuschange
{
  uint32_t hContext;
  uint32_t dwTimeout;
  uint32_t rv;
  uint32_t cReaders;
  s_readerstate readerStates[1];

};
typedef struct s_getstatuschange s_getstatuschange;


/**
 * @brief contained in \ref SCARD_TRANSMIT Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_transmit
{
  int32_t hCard;
  uint32_t ioSendPciProtocol;
  uint32_t ioSendPciLength;
  uint32_t ioRecvPciProtocol;
  uint32_t ioRecvPciLength;
  uint32_t pcbRecvLength;
  uint32_t rv;

  uint32_t cbBufferLength;
  uint8_t pcbBuffer[1];

};
typedef struct s_transmit s_transmit;


/**
 * @brief contained in \ref SCARD_CONTROL Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_control
{
  int32_t hCard;
  uint32_t dwControlCode;
  uint8_t pbSendBuffer[PP_MAX_BUFFER_LEN];
  uint32_t cbSendLength;
  uint8_t pbRecvBuffer[PP_MAX_BUFFER_LEN];
  uint32_t cbRecvLength;
  uint32_t dwBytesReturned;
  uint32_t rv;
};
typedef struct s_control s_control;

/**
 * @brief contained in \ref SCARD_GET_ATTRIB and \c  Messages.
 *
 * These data are passed throw the field \c sharedSegmentMsg.data.
 */
struct s_getset
{
  int32_t hCard;
  uint32_t dwAttrId;
  uint8_t pbAttr[PP_MAX_BUFFER_LEN];
  uint32_t cbAttrLen;
  uint32_t rv;
};
typedef struct s_getset s_getset;



struct s_listreaders
{
  int32_t hContext;
  uint32_t cchReaderLen;
  uint32_t isSizeQueryOnly;
  uint32_t rv;
  char szReader[1];
};
typedef struct s_listreaders s_listreaders;



struct s_listgroups
{
  int32_t hContext;
  uint32_t cchGroupLen;
  uint32_t isSizeQueryOnly;
  uint32_t rv;
  char szGroup[1];
};
typedef struct s_listgroups s_listgroups;





struct s_msg_header {
  uint8_t type;
  uint32_t len;
};
typedef struct s_msg_header s_msg_header;


struct s_message {
  s_msg_header header;
  union {
    s_establish establishStruct;
    s_release releaseStruct;
    s_isvalid isValidStruct;
    s_settimeout setTimeoutStruct;
    s_connect connectStruct;
    s_reconnect reconnectStruct;
    s_disconnect disconnectStruct;
    s_begin beginStruct;
    s_end endStruct;
    s_cancel cancelStruct;
    s_status statusStruct;
    s_getstatuschange getStatusChangeStruct;
    s_transmit transmitStruct;
    s_control controlStruct;
    s_getset getSetStruct;
    s_listreaders listReadersStruct;
    s_listgroups listGroupsStruct;
  };
};
typedef struct s_message s_message;


#pragma pack(pop)


#define PP_MSG_SIZE(tp) (\
  sizeof(struct s_msg_header)+\
  sizeof(tp))


#ifndef SCARD_F_INTERNAL_ERROR
# define SCARD_F_INTERNAL_ERROR  0x80100001
#endif


const char *pp_getMsgTypeText(int t);
void pp_dumpString(const char *s, unsigned int l,
		   FILE *f, unsigned int insert);


#endif


