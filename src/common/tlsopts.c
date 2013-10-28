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


#include "tlsopts.h"
#include "unix.h"
#include <sys/types.h>
#include <sys/socket.h>

extern int pp_nullenc_init(tlsopts_t **opts);
extern int pp_tlsopts_init(tlsopts_t **opts);

int pp_tlsopts_init_any(int nullenc, tlsopts_t **opts) {
  if (nullenc) {
#ifdef ENABLE_NULLENC
    return pp_nullenc_init(opts);
#else
    return -1;
#endif
  } else {
#ifdef USE_OPENSSL
    return pp_tlsopts_init(opts);
#else
    return -1;
#endif
  }
}
