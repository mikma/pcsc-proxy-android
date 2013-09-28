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


#include "netopts.h"
#include "network.h"
#include "unix.h"
#ifdef USE_BLUETOOTH
#include "bluetooth.h"
#endif
#include <sys/types.h>
#include <sys/socket.h>

int pp_netopts_init(int family, netopts_t **opts) {
  switch(family) {
  case AF_INET:
  case AF_INET6:
    return pp_network_init(opts);
#ifdef USE_BLUETOOTH
  case AF_BLUETOOTH:
    return pp_bluetooth_init(opts);
#endif
  case AF_UNIX:
    return pp_unix_init(opts);
  default:
    return -1;
  }
}
