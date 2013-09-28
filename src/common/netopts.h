/***************************************************************************
    begin       : Sat Sep 28 2013
    copyright   : (C) 2013 by Mikael Magnusson

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


#ifndef PP_NETOPTS_H
#define PP_NETOPTS_H


#include "message.h"


struct netopts {
  int (*listen)(int family, const char *ip, int port);
  int (*accept)(int sk);

  int (*connect)(const char *ip, int port);

  int (*recv)(int sk, s_message *msg);
  int (*send)(int sk, const s_message *msg);
  int (*getport)(int sk);
};

typedef struct netopts netopts_t;

int pp_netopts_init(int family, netopts_t **opts);

#endif
