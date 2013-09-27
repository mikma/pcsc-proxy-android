/***************************************************************************
    begin       : Fri Sep 27 2013
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "bluetooth.h"
#include "message.h"
#include "network.h"

#include <errno.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>


#define BT_PCSC_NAME "PC/SC Proxy"

struct svc_info {
  uint8_t channel;
  uint32_t handle;
};

typedef struct svc_info svc_info_t;

/* FIXME change */
#define PCSC_UUID {0x42, 0x21, 0x9a, 0xbb, 0x16, 0x15, 0x44, 0x86, 0xbd, 0x50, 0x49, 0x6b, 0xd5, 0x04, 0x96, 0xd8}
#define CHAT_INSECURE {0x8c, 0xe2, 0x55, 0xc0, 0x20, 0x0a, 0x11, 0xe0, 0xac, 0x64, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } // BluetoothChatInsecure

#define BT_PCSC_UUID CHAT_INSECURE


sdp_session_t *g_sess;
bdaddr_t g_iface;


static int pp_btgetport(int s);


static void set_public_browse_group(sdp_record_t *record) {
  uuid_t root_uuid;
  sdp_list_t *root = NULL;

  sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
  root = sdp_list_append(0, &root_uuid);
  sdp_set_browse_groups(record, root);
  sdp_list_free(root, 0);
}

static void set_service_class(sdp_record_t *record, uint8_t uuid[16]) {
  uuid_t sp_uuid;
  sdp_list_t *svclass_id = NULL;

  sdp_uuid128_create(&sp_uuid, uuid);
  svclass_id = sdp_list_append(0, &sp_uuid);
  sdp_set_service_classes(record, svclass_id);
  sdp_list_free(svclass_id, 0);
}


static void set_rfcomm_protos(sdp_record_t *record, uint8_t channel) {
  uuid_t l2cap;
  uuid_t rfcomm;
  sdp_list_t *proto_l2cap = NULL;
  sdp_list_t *proto_rfcomm = NULL;
  sdp_list_t *ap_seq = NULL;
  sdp_list_t *access_proto = NULL;
  sdp_data_t *l2cap_chan = NULL;

  sdp_uuid16_create(&l2cap, L2CAP_UUID);
  proto_l2cap = sdp_list_append(0, &l2cap);
  ap_seq = sdp_list_append(0, proto_l2cap);

  sdp_uuid16_create(&rfcomm, RFCOMM_UUID);
  proto_rfcomm = sdp_list_append(0, &rfcomm);
  l2cap_chan = sdp_data_alloc(SDP_UINT8, &channel);
  proto_rfcomm = sdp_list_append(proto_rfcomm, l2cap_chan);
  ap_seq = sdp_list_append(ap_seq, proto_rfcomm);

  access_proto = sdp_list_append(0, ap_seq);
  sdp_set_access_protos(record, access_proto);

  sdp_list_free(access_proto, 0); access_proto = NULL;
  sdp_list_free(ap_seq, 0); ap_seq = NULL;
  sdp_data_free(l2cap_chan); l2cap_chan = NULL;
  sdp_list_free(proto_rfcomm, 0); proto_rfcomm = NULL;
  sdp_list_free(proto_l2cap, 0); proto_l2cap = NULL;
}


static int add_sp(sdp_session_t *session, svc_info_t *si)
{
  sdp_record_t record;
  uint8_t chan = si->channel;
  uint8_t pcsc_uuid[] = BT_PCSC_UUID;
  const char *name = BT_PCSC_NAME;
  const char *prov = NULL;
  const char *desc = NULL;
  int ret = 0;

  memset(&record, 0, sizeof(sdp_record_t));
  record.handle = si->handle;

  set_public_browse_group(&record);
  set_service_class(&record, pcsc_uuid);
  set_rfcomm_protos(&record, chan);
  sdp_set_info_attr(&record, name, prov, desc);

  if (sdp_device_record_register(session, &g_iface, &record, 0) < 0) {
    printf("Service Record registration failed\n");
    ret = -1;
    goto end;
  }

  printf("Serial Port service registered\n");

 end:
  return ret;
}

static int register_sdp(int s) {
  svc_info_t si;
  uint8_t channel;

  channel = pp_btgetport(s);
  if (channel < 0)
    return -1;

  printf("Channel %d\n", channel);

  si.handle = 0xffffffff;
  si.channel = channel;
  
  if (g_sess == NULL) {
    bacpy(&g_iface, BDADDR_ANY);
    g_sess = sdp_connect(&g_iface, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
  }

  add_sp(g_sess, &si);
  return channel;
}


static int pp_btlisten(int family, const char *btaddr, int channel) {
  union {
    struct sockaddr raw;
    struct sockaddr_rc rc;
  } addr;
  int s;
  int fl;

  memset(&addr, 0, sizeof(addr));

  if (str2ba(btaddr, &addr.rc.rc_bdaddr)) {
    DEBUGPE("ERROR: str2ba(): %d=%s\n", errno, strerror(errno));
    return -1;
  }
  addr.rc.rc_channel=channel;

  s=socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  fl=RFCOMM_LM_ENCRYPT;
  if (setsockopt(s,
		 SOL_RFCOMM,
		 RFCOMM_LM,
		 &fl,
		 sizeof(fl))) {
    DEBUGPE("ERROR: setsockopt(): %s", strerror(errno));
    return -1;
  }

  addr.rc.rc_family = AF_BLUETOOTH;
  addr.rc.rc_channel = channel;

  if (bind(s, &addr.raw, sizeof(addr))) {
    DEBUGPE("ERROR: bind(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  if (listen(s, 10)) {
    DEBUGPE("ERROR: listen(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  if (register_sdp(s) < 0) {
    DEBUGPE("ERROR: Failed to register SDP\n");
    close(s);
    return -1;
  }

  return s;
}



static int pp_btaccept(int sk) {
  socklen_t addrLen;
  int newS;
  struct sockaddr peerAddr;

  addrLen=sizeof(peerAddr);
  newS=accept(sk, &peerAddr, &addrLen);
  if (newS!=-1)
    return newS;
  else {
    if (errno!=EINTR) {
      DEBUGPE("ERROR: accept(): %d=%s\n", errno, strerror(errno));
    }
    return -1;
  }
}



static int pp_btconnect(const char *btaddr, int channel) {
  union {
    struct sockaddr raw;
    struct sockaddr_rc rc;
  } addr;
  int s;

  memset(&addr, 0, sizeof(addr));
  addr.raw.sa_family=AF_BLUETOOTH;

  if (!str2ba(btaddr, &addr.rc.rc_bdaddr)) {
    DEBUGPE("ERROR: str2ba(): %d=%s\n", errno, strerror(errno));
    return -1;
  }
  addr.rc.rc_channel=channel;

  s=socket(AF_BLUETOOTH, SOCK_STREAM,0);

  if (s==-1) {
    DEBUGPE("ERROR: socket(): %d=%s\n", errno, strerror(errno));
    return -1;
  }

  if (connect(s, &addr.raw, sizeof(addr))) {
    DEBUGPE("ERROR: connect(): %d=%s\n", errno, strerror(errno));
    close(s);
    return -1;
  }

  return s;
}


static int pp_btgetport(int s) {
  struct sockaddr_rc rc;
  socklen_t len = sizeof(rc);

  memset(&rc, 0, sizeof(rc));

  if (getsockname(s, (struct sockaddr*)&rc, &len) == -1) {
    DEBUGPE("ERROR: getsockname %d\n", errno);
    return -1;
  }

  return rc.rc_channel;
}


static netopts_t pp_bluetooth_opts = {
  .listen = pp_btlisten,
  .accept = pp_btaccept,
  .connect = pp_btconnect,
  .recv = pp_recv,
  .send = pp_send,
  .getport = pp_btgetport,
};


void pp_bluetooth_init(netopts_t **opts) {
  *opts = &pp_bluetooth_opts;
}

void pp_bluetooth_fini() {
  if (g_sess != NULL) {
    sdp_close(g_sess);
    g_sess = NULL;
  }
}
