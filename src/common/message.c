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


#define ENABLE_DEBUG

#include "message.h"



const char *pp_getMsgTypeText(int t) {
  switch(t) {
  case SCARD_ESTABLISH_CONTEXT:   return "establishContext";
  case SCARD_RELEASE_CONTEXT:     return "releaseContext";
  case SCARD_ISVALIDCONTEXT:      return "isValidContext";
  case SCARD_SETTIMEOUT:          return "setTimeout";
  case SCARD_CONNECT:             return "connect";
  case SCARD_RECONNECT:           return "reconnect";
  case SCARD_DISCONNECT:          return "disconnect";
  case SCARD_BEGIN_TRANSACTION:   return "beginTransaction";
  case SCARD_END_TRANSACTION:     return "endTransaction";
  case SCARD_CANCEL_TRANSACTION:  return "cancelTransaction";
  case SCARD_STATUS:              return "status";
  case SCARD_GET_STATUS_CHANGE:   return "getStatusChange";
  case SCARD_TRANSMIT:            return "transmit";
  case SCARD_CONTROL:             return "control";
  case SCARD_CANCEL:              return "cancel";
  case SCARD_GET_ATTRIB:          return "getAttrib";
  case SCARD_SET_ATTRIB:          return "setAttrib";
  case SCARD_LIST_READERS:        return "listReaders";
  case SCARD_LIST_GROUPS:         return "listGroups";
  }

  return "(unknown)";
}


void pp_dumpString(const char *s, unsigned int l,
		   FILE *f, unsigned int insert) {
  unsigned int i;
  unsigned int j;
  unsigned int pos;
  unsigned k;

  pos=0;
  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f,"String size is %d:\n",l);
  while(pos<l) {
    for (k=0; k<insert; k++)
      fprintf(f, " ");
    fprintf(f,"%04x: ",pos);
    j=pos+16;
    if (j>=l)
      j=l;

    /* show hex dump */
    for (i=pos; i<j; i++) {
      fprintf(f,"%02x ",(unsigned char)s[i]);
    }
    if (j-pos<16)
      for (i=0; i<16-(j-pos); i++)
	fprintf(f,"   ");
    /* show text */
    for (i=pos; i<j; i++) {
      if (s[i]<32)
	fprintf(f,".");
      else
	fprintf(f,"%c",s[i]);
    }
    fprintf(f,"\n");
    pos+=16;
  }
}

