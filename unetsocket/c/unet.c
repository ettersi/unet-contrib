#define _BSD_SOURCE
#include <stdlib.h>
#include <errno.h>
#include "pthreadwindows.h"
#include "fjage.h"
#include "unet.h"
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

typedef struct {
  fjage_gw_t gw;
  pthread_t tid;
  pthread_mutex_t rxlock, txlock;
  int local_protocol;
  int remote_address;
  int remote_protocol;
  long timeout;
  fjage_aid_t provider;
  bool quit;
  fjage_msg_t ntf;
} _unetsocket_t;

static long long _time_in_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

static void *monitor(void *p) {
  _unetsocket_t *usock = p;
  long deadline = -1;
  int rv;
  if (usock->timeout == 0) deadline = 0;
  if (usock->timeout > 0) deadline = _time_in_ms() + usock->timeout;
  const char *list[] = {"org.arl.unet.DatagramNtf", "org.arl.unet.phy.RxFrameNtf"};
  while (!usock->quit) {
    long time_remaining = -1;
    if (usock->timeout == 0) time_remaining = 0;
  	if (usock->timeout > 0) {
  	  time_remaining = deadline - _time_in_ms();
  	  if (time_remaining <= 0) return NULL;
  	}
    pthread_mutex_lock(&usock->rxlock);
  	fjage_msg_t msg = fjage_receive_any(usock->gw, list, 2, usock->timeout<0?15*TIMEOUT:time_remaining);
  	pthread_mutex_unlock(&usock->rxlock);
  	if (msg == NULL) return NULL;
  	if (msg != NULL) {
  	  rv = fjage_msg_get_int(msg, "protocol", 0);
  	  if (rv == DATA || rv >= USER) {
    		if (usock->local_protocol < 0) {
    		  usock->ntf = msg;
    		  pthread_exit(NULL);
    		}
    		if (usock->local_protocol == rv) {
    		  usock->ntf = msg;
    		  pthread_exit(NULL);
    		}
  	  }
  	  fjage_msg_destroy(msg);
  	}
  }
  return NULL;
}

static fjage_msg_t receive(_unetsocket_t *usock, const char *clazz, const char *id, long timeout) {
  pthread_mutex_lock(&usock->txlock);
  fjage_interrupt(usock->gw);
  int rv = pthread_mutex_trylock(&usock->rxlock);
  while (rv == EBUSY)
  {
    Sleep(100);
    fjage_interrupt(usock->gw);
    rv = pthread_mutex_trylock(&usock->rxlock);
  }
  fjage_msg_t msg = fjage_receive(usock->gw, clazz, id, timeout);
  pthread_mutex_unlock(&usock->rxlock);
  pthread_mutex_unlock(&usock->txlock);
  return msg;
}

static fjage_msg_t request(_unetsocket_t *usock, const fjage_msg_t request, long timeout) {
  pthread_mutex_lock(&usock->txlock);
  fjage_interrupt(usock->gw);
  int rv = pthread_mutex_trylock(&usock->rxlock);
  while (rv == EBUSY)
  {
    Sleep(100);
    fjage_interrupt(usock->gw);
    rv = pthread_mutex_trylock(&usock->rxlock);
  }
  fjage_msg_t msg = fjage_request(usock->gw, request, timeout);
  pthread_mutex_unlock(&usock->rxlock);
  pthread_mutex_unlock(&usock->txlock);
  return msg;
}

static fjage_aid_t agent_for_service(_unetsocket_t *usock, const char *service) {
  pthread_mutex_lock(&usock->txlock);
  fjage_interrupt(usock->gw);
  int rv = pthread_mutex_trylock(&usock->rxlock);
  while (rv == EBUSY)
  {
    Sleep(100);
    fjage_interrupt(usock->gw);
    rv = pthread_mutex_trylock(&usock->rxlock);
  }
  fjage_aid_t aid = fjage_agent_for_service(usock->gw, service);
  pthread_mutex_unlock(&usock->rxlock);
  pthread_mutex_unlock(&usock->txlock);
  return aid;
}

static int agents_for_service(_unetsocket_t *usock, const char *service, fjage_aid_t* agents, int max) {
  pthread_mutex_lock(&usock->txlock);
  fjage_interrupt(usock->gw);
  int rv = pthread_mutex_trylock(&usock->rxlock);
  while (rv == EBUSY)
  {
    Sleep(100);
    fjage_interrupt(usock->gw);
    rv = pthread_mutex_trylock(&usock->rxlock);
  }
  int as = fjage_agents_for_service(usock->gw, service, agents, max);
  pthread_mutex_unlock(&usock->rxlock);
  pthread_mutex_unlock(&usock->txlock);
  return as;
}

unetsocket_t unetsocket_open(const char* hostname, int port) {
  _unetsocket_t *usock = malloc(sizeof(_unetsocket_t));
  if (usock == NULL) return NULL;
  usock->gw = fjage_tcp_open(hostname, port);
  if (usock->gw == NULL) {
    free(usock);
    return NULL;
  }
  pthread_mutex_init(&usock->rxlock, NULL);
  pthread_mutex_init(&usock->txlock, NULL);
  int nagents = agents_for_service(usock, "org.arl.unet.Services.DATAGRAM", NULL, 0);
  // fjage_aid_t agents[nagents];
  fjage_aid_t* agents = malloc((unsigned long)nagents*sizeof(fjage_aid_t));
  if (agents_for_service(usock, "org.arl.unet.Services.DATAGRAM", agents, nagents) < 0) {
    free(usock);
    free(agents);
    return NULL;
  }
  for(int i = 0; i < nagents; i++) {
    fjage_subscribe_agent(usock->gw, agents[i]);
  }
  free(agents);
  return usock;
}

#ifndef _WIN32
unetsocket_t unetsocket_rs232_open(const char* devname, int baud, const char* settings) {
  _unetsocket_t *usock = malloc(sizeof(_unetsocket_t));
  if (usock == NULL) return NULL;
  usock->gw = fjage_rs232_open(devname, baud, settings);
  if (usock->gw == NULL) {
    free(usock);
    return NULL;
  }
  pthread_mutex_init(&usock->rxlock, NULL);
  pthread_mutex_init(&usock->txlock, NULL);
  int nagents = agents_for_service(usock, "org.arl.unet.Services.DATAGRAM", NULL, 0);
  fjage_aid_t agents[nagents];
  if (agents_for_service(usock, "org.arl.unet.Services.DATAGRAM", agents, nagents) < 0) {
    free(usock);
    return NULL;
  }
  for(int i = 0; i < nagents; i++) {
    fjage_subscribe_agent(usock->gw, agents[i]);
  }
  return usock;
}
#endif

int unetsocket_close(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  usock->quit = true;
  fjage_interrupt(usock->gw);
  fjage_close(usock->gw);
  free(usock);
  return 0;
}

int unetsocket_is_closed(unetsocket_t sock) {
  if (sock == NULL) return -1;
  return 0;
}

int unetsocket_bind(unetsocket_t sock, int protocol) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  if (protocol == DATA || (protocol >= USER && protocol <= MAX)) {
    usock->local_protocol = protocol;
    return 0;
  }
  return -1;
}

void unetsocket_unbind(unetsocket_t sock) {
  _unetsocket_t *usock = sock;
  usock->local_protocol = -1;
}

int unetsocket_is_bound(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  if (usock->local_protocol >= 0) return 0;
  return -1;
}

int unetsocket_connect(unetsocket_t sock, int to, int protocol) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  if ((to >= 0) && (protocol == DATA || (protocol >= USER && protocol <= MAX))) {
    usock->remote_address = to;
  	usock->remote_protocol = protocol;
  	return 0;
  }
  return -1;
}

void unetsocket_disconnect(unetsocket_t sock) {
  _unetsocket_t *usock = sock;
  usock->remote_address = -1;
  usock->remote_protocol = 0;
}

int unetsocket_is_connected(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  if (usock->remote_address >= 0) return 0;
  return -1;
}

int unetsocket_get_local_address(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  fjage_aid_t node;
  int rv;
  node = agent_for_service(usock, "org.arl.unet.Services.NODE_INFO");
  if (node == NULL) return -1;
  msg = fjage_msg_create("org.arl.unet.ParameterReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, node);
  fjage_msg_add_int(msg, "index", -1);
  fjage_msg_add_string(msg, "param", "address");
  msg = request(usock, msg, 5*TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_INFORM) {
  	rv = fjage_msg_get_int(msg, "value", 0);
  	fjage_msg_destroy(msg);
  	free(node);
  	return rv;
  }
  fjage_msg_destroy(msg);
  fjage_aid_destroy(node);
  return -1;
}

int unetsocket_get_local_protocol(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  return usock->local_protocol;
}

int unetsocket_get_remote_address(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  return usock->remote_address;
}

int unetsocket_get_remote_protocol(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  return usock->remote_protocol;
}

void unetsocket_set_timeout(unetsocket_t sock, long ms) {
  _unetsocket_t *usock = sock;
  if (ms < 0) {
  	ms = -1;
  }
  usock->timeout = ms;
}

long unetsocket_get_timeout(unetsocket_t sock) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  return usock->timeout;
}

// NOTE: changed const uint8_t* to uint8_t*
int unetsocket_send(unetsocket_t sock, uint8_t* data, int len, int to, int protocol) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  int rv;
  msg = fjage_msg_create("org.arl.unet.DatagramReq", FJAGE_REQUEST);
  if (data != NULL) fjage_msg_add_byte_array(msg, "data", data, len);
  fjage_msg_add_int(msg, "to", to);
  fjage_msg_add_int(msg, "protocol", protocol);
  rv = unetsocket_send_request(usock, msg);
  return rv;
}

int unetsocket_send_request(unetsocket_t sock, fjage_msg_t req) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  int protocol;
  const char* recipient;
  protocol = fjage_msg_get_int(req, "protocol", 0);
  if (protocol != DATA && (protocol < USER || protocol > MAX)) return -1;
  recipient = fjage_msg_get_string(req, "recipient");
  if (recipient == NULL) {
  	if (usock->provider == NULL) usock->provider = agent_for_service(usock, "org.arl.unet.Services.TRANSPORT");
  	if (usock->provider == NULL) usock->provider = agent_for_service(usock, "org.arl.unet.Services.ROUTING");
  	if (usock->provider == NULL) usock->provider = agent_for_service(usock, "org.arl.unet.Services.LINK");
  	if (usock->provider == NULL) usock->provider = agent_for_service(usock, "org.arl.unet.Services.PHYSICAL");
  	if (usock->provider == NULL) usock->provider = agent_for_service(usock, "org.arl.unet.Services.DATAGRAM");
  	if (usock->provider == NULL) return -1;
  	fjage_msg_set_recipient(req, usock->provider);
  }
  req = request(usock, req, TIMEOUT);
  if (req != NULL && fjage_msg_get_performative(req) == FJAGE_AGREE) {
  	fjage_msg_destroy(req);
  	return 0;
  }
  fjage_msg_destroy(req);
     return -1;
}

fjage_msg_t unetsocket_receive(unetsocket_t sock) {
  if (sock == NULL) return NULL;
  _unetsocket_t *usock = sock;
  if (pthread_create(&usock->tid, NULL, monitor, usock) < 0) {
  	pthread_mutex_destroy(&usock->rxlock);
  	pthread_mutex_destroy(&usock->txlock);
  	fjage_close(usock->gw);
    free(usock);
    return NULL;
  }
  usock->quit = false;
  pthread_join(usock->tid, NULL);
  return usock->ntf;
}

int unetsocket_get_range(unetsocket_t sock, int to, float* range) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_aid_t ranging;
  fjage_msg_t msg;
  ranging = fjage_agent_for_service(usock->gw, "org.arl.unet.Services.RANGING");
  fjage_subscribe_agent(usock->gw, ranging);
  msg = fjage_msg_create("org.arl.unet.localization.RangeReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, ranging);
  fjage_msg_add_int(msg, "to", to);
  msg = request(usock, msg, TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_AGREE) {
    fjage_msg_destroy(msg);
    msg = receive(usock, "org.arl.unet.localization.RangeNtf", NULL, 10000);
    if (msg != NULL) {
      *range = fjage_msg_get_float(msg, "range", 0);
      fjage_msg_destroy(msg);
      return 0;
    }
  }
  fjage_msg_destroy(msg);
  return -1;
}

int unetsocket_set_powerlevel(unetsocket_t sock, int index, float value) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  fjage_aid_t phy;
  phy = fjage_agent_for_service(usock->gw, "org.arl.unet.Services.PHYSICAL");
  msg = fjage_msg_create("org.arl.unet.ParameterReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, phy);
  if (index == 0) index = -1;
  fjage_msg_add_int(msg, "index", index);
  fjage_msg_add_string(msg, "param", "powerLevel");
  fjage_msg_add_float(msg, "value", value);
  msg = request(usock, msg, TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_INFORM) {
    fjage_msg_destroy(msg);
    fjage_aid_destroy(phy);
    return 0;
  }
  fjage_msg_destroy(msg);
  fjage_aid_destroy(phy);
  return -1;
}

int unetsocket_tx_signal(unetsocket_t sock, float *signal, int nsamples, int rate, float fc, char *id) {
  if (sock == NULL) return -1;
  if (nsamples < 0) return -1;
  if (rate != TXSAMPLINGFREQ) return -1;
  if (nsamples > 0 && signal == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  fjage_aid_t bb;
  bb = fjage_agent_for_service(usock->gw, "org.arl.unet.Services.BASEBAND");
  msg = fjage_msg_create("org.arl.unet.bb.TxBasebandSignalReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, bb);
  if (fc == 0.0) fjage_msg_add_float(msg, "fc", fc);
  if (signal != NULL) fjage_msg_add_float_array(msg, "signal", signal, ((int)fc ? 2 : 1)*nsamples);
  if (id != NULL) strcpy(id, fjage_msg_get_id(msg));
  msg = request(usock, msg, 5 * TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_AGREE)
  {
    fjage_msg_destroy(msg);
    return 0;
  }
  fjage_msg_destroy(msg);
  return -1;
}

int unetsocket_record(unetsocket_t sock, float *buf, int nsamples) {
  if (sock == NULL) return -1;
  if (nsamples < 0) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  fjage_aid_t bb;
  bb = fjage_agent_for_service(usock->gw, "org.arl.unet.Services.BASEBAND");
  msg = fjage_msg_create("org.arl.unet.bb.RecordBasebandSignalReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, bb);
  fjage_msg_add_int(msg, "recLength", nsamples);
  msg = request(usock, msg, 5 * TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_AGREE)
  {
    fjage_msg_destroy(msg);
    msg = receive(usock, "org.arl.unet.bb.RxBasebandSignalNtf", NULL, 5 * TIMEOUT);
    if (msg != NULL)
    {
      fjage_msg_get_float_array(msg, "signal", buf, 2 * nsamples);
      fjage_msg_destroy(msg);
      return 0;
    }
  }
  fjage_msg_destroy(msg);
  return -1;
}

void unetsocket_cancel(unetsocket_t sock) {
  _unetsocket_t *usock = sock;
  usock->quit = true;
}

fjage_gw_t unetsocket_get_gateway(unetsocket_t sock) {
  if (sock == NULL) return NULL;
  _unetsocket_t *usock = sock;
  return usock->gw;
}

fjage_aid_t unetsocket_agent_for_service(unetsocket_t sock, const char* svc) {
  if (sock == NULL) return NULL;
  _unetsocket_t *usock = sock;
  return agent_for_service(usock, svc);
}

int unetsocket_agents_for_service(unetsocket_t sock, const char* svc, fjage_aid_t* agents, int max) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  return agents_for_service(usock, svc, agents, max);
}

// NOTE: removed the first argument
fjage_aid_t unetsocket_agent(const char* name) {
  return fjage_aid_create(name);
}

int unetsocket_host(unetsocket_t sock, const char* node_name) {
  if (sock == NULL) return -1;
  _unetsocket_t *usock = sock;
  fjage_msg_t msg;
  fjage_aid_t arp;
  int rv;
  arp = agent_for_service(usock, "org.arl.unet.Services.ADDRESS_RESOLUTION");
  if (arp == NULL) return -1;
  msg = fjage_msg_create("org.arl.unet.addr.AddressResolutionReq", FJAGE_REQUEST);
  fjage_msg_set_recipient(msg, arp);
  fjage_msg_add_int(msg, "index", -1);
  fjage_msg_add_string(msg, "param", "name");
  fjage_msg_add_string(msg, "value", node_name);
  msg = request(usock, msg, 5*TIMEOUT);
  if (msg != NULL && fjage_msg_get_performative(msg) == FJAGE_INFORM) {
  	rv = fjage_msg_get_int(msg, "address", 0);
  	fjage_msg_destroy(msg);
  	fjage_aid_destroy(arp);
  	return rv;
  }
  fjage_msg_destroy(msg);
  return -1;
}
