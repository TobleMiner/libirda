#include <stdlib.h>
#include <string.h>

#include "irlap_connect.h"
#include "irlap.h"
#include "irlap_connection.h"

#define IRLAP_CONNECT_TO_IRLAP(conn) (container_of((conn), struct irlap, connect))

#define LOCAL_TAG "IRDA LAP CONNECT"

#define IRLAP_CONN_LOGV(conn, fmt, ...) IRHAL_LOGV(IRLAP_CONNECT_TO_IRLAP(conn)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONN_LOGD(conn, fmt, ...) IRHAL_LOGD(IRLAP_CONNECT_TO_IRLAP(conn)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONN_LOGI(conn, fmt, ...) IRHAL_LOGI(IRLAP_CONNECT_TO_IRLAP(conn)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONN_LOGW(conn, fmt, ...) IRHAL_LOGW(IRLAP_CONNECT_TO_IRLAP(conn)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONN_LOGE(conn, fmt, ...) IRHAL_LOGE(IRLAP_CONNECT_TO_IRLAP(conn)->phy->hal, fmt, ##__VA_ARGS__)

int irlap_connect_init(struct irlap_connect* conn) {
  struct irlap* lap = IRLAP_CONNECT_TO_IRLAP(conn);
  int err;

  memset(conn, 0, sizeof(*conn));
  err = irlap_lock_alloc(lap, &conn->connect_lock);
  if(err) {
    IRLAP_CONN_LOGE(conn, "Failed to allocate connection lock");
    goto fail;
  }

  return 0;

fail:
  return err;
}

void irlap_connect_free(struct irlap_connect* conn) {
  struct irlap* lap = IRLAP_CONNECT_TO_IRLAP(conn);
  irlap_lock_free(lap, &conn->connect_lock);  
}

static int irlap_connect_request_sniff(struct irlap_connect* conn, irlap_addr_t target_addr, struct irlap_connect_req_qos* qos) {
  struct irlap* lap = IRLAP_CONNECT_TO_IRLAP(conn);
  int err = 0;

  irlap_lock_take_reentrant(lap, lap->state_lock);
  if(lap->state != IRLAP_STATION_MODE_NDM) {
    IRLAP_CONN_LOGW(conn, "Station is not in NDM state, can't connect to sniffer");
    err = -IRLAP_ERR_STATION_STATE;
    goto fail_state_locked;
  }

  lap->state = IRLAP_STATION_MODE_SCONN;
  conn->current_req_qos = *qos;
  conn->current_target_addr = target_addr;

fail_state_locked:
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

int irlap_connect_request(struct irlap_connect* conn, irlap_addr_t target_addr, struct irlap_connect_req_qos* qos, bool sniff) {
  struct irlap* lap = IRLAP_CONNECT_TO_IRLAP(conn);

  return 0;
}

int irlap_connect_handle_sniff_xid_req_sconn(struct irlap* lap, irlap_addr_t addr) {
  struct irlap_connect* conn = &lap->connect;
  struct irlap_connection* connection;
  int err = IRLAP_FRAME_HANDLED;
  
  irlap_lock_take_reentrant(lap, &conn->connect_lock);
  if(addr != conn->current_target_addr) {
    IRLAP_CONN_LOGD(conn, "Ignoring sniff xid response from unexpected address %08x", addr);
    err = IRLAP_FRAME_NOT_HANDLED;
    goto fail_connect_locked;
  }

  irlap_lock_take_reentrant(lap, &lap->connection_lock);
  err = irlap_connection_alloc(lap, &connection);
  if(err) {
    IRLAP_CONN_LOGE(conn, "Failed to allocate connection");
    goto fail_connect_locked;    
  }

fail_connection_alloc:
  irlap_connection_free(lap, connection);
fail_connect_locked:
  irlap_lock_take_reentrant(lap, &conn->connect_lock);
  return IRLAP_FRAME_HANDLED;
}
