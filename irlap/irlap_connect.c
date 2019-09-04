#include <errno.h>
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

static void sniff_snrm_p_timeout(void* priv) {
  struct irlap_connection* conn = priv;
  struct irlap* lap = conn->lap;
  irlap_lock_take_reentrant(lap, lap->state_lock);
  lap->state = IRLAP_STATION_MODE_NDM;
  irlap_lock_take_reentrant(lap, &lap->connection_lock);
  if(lap->services.disconnect.indication) {
    lap->services.disconnect.indication(conn->connection_addr, NULL, lap->priv);
  }
  irlap_connection_free(conn);
  irlap_lock_put_reentrant(lap, &lap->connection_lock);
  irlap_lock_put_reentrant(lap, lap->state_lock);  
}

static int negotiate_params(struct irlap_connection* conn, irlap_negotiation_params_t* remote_params) {
  int err = irlap_negotiation_merge_params(&conn->local_negotiation_params, remote_params);
  if(err) {
    return err;
  }
  err = irlap_negotiation_translate_params_to_values(&conn->local_negotiation_values, &conn->local_negotiation_params);
  if(err) {
    return err;
  }
  return irlap_negotiation_translate_params_to_values(&conn->remote_negotiation_values, remote_params);
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
  err = irlap_connection_alloc(lap, addr, &connection);
  if(err) {
    IRLAP_CONN_LOGE(conn, "Failed to allocate connection");
    goto fail_connections_locked;    
  }

  err = irlap_connection_send_snrm_cmd(connection);
  if(err) {
    IRLAP_CONN_LOGE(conn, "Failed to send snrm connect cmd");
    goto fail_connection_alloc;
  }

  err = irlap_connection_start_p_timer(connection, sniff_snrm_p_timeout);

  lap->state = IRLAP_STATION_MODE_SSETUP;

  irlap_lock_put_reentrant(lap, &lap->connection_lock);
  irlap_lock_put_reentrant(lap, &conn->connect_lock);
  return IRLAP_FRAME_HANDLED;

fail_connection_alloc:
  irlap_connection_free(connection);
fail_connections_locked:
  irlap_lock_put_reentrant(lap, &lap->connection_lock);
fail_connect_locked:
  irlap_lock_put_reentrant(lap, &conn->connect_lock);
  return err;
}

static int irlap_connect_handle_ua_resp_ssetup(struct irlap_connection* conn, uint8_t* data, size_t len, bool pf) {
  int err;
  struct irlap* lap = conn->lap;
  union irlap_ua_frame frame;
  irlap_negotiation_params_t params;
  if(len < sizeof(frame.data)) {
    IRLAP_CONN_LOGW(&lap->connect, "Got invalid ua resp with len %zu < %zu", len, sizeof(frame.data));
    return -EINVAL;
  }

  memcpy(&frame.data, data, sizeof(frame.data));
  data += sizeof(frame.data);
  len -= sizeof(frame.data);

  irlap_connection_set_default_negotiation_params(lap, &params);
  err = irlap_negotiation_update_params(&params, data, len);
  if(err) {
    IRLAP_CONN_LOGW(&lap->connect, "Failed to decode ua resp negotiation params: %d", err);
    return err;
  }

  err = negotiate_params(conn, &params);
  if(err) {
    IRLAP_CONN_LOGW(&lap->connect, "Failed to negotiate connection parameters: %d", err);
    return err;
  }

  irlap_connection_stop_p_timer(conn);

  err = irlap_connection_send_rr_cmd(conn, 0);
  if(err) {
    IRLAP_CONN_LOGE(&lap->connect, "Failed to send initial RR frame: %d", err);
    return err;
  }

  lap->role = IRLAP_STATION_ROLE_PRIMARY;
  lap->state = IRLAP_STATION_MODE_NRM;
  conn->connection_state = IRLAP_CONNECTION_STATE_RECV;

  if(lap->services.connect.confirm) {
    struct irlap_connect_resp_qos resp_qos = {
      .baudrate = conn->remote_negotiation_values.baudrate,
      .data_size = conn->remote_negotiation_values.data_size,
      .disconnect_threshold = conn->remote_negotiation_values.disconnect_threshold_time_s,
    };
    lap->services.connect.confirm(conn->connection_addr, &resp_qos, lap->priv);
  }

  err = irlap_connection_start_p_timer(conn, irlap_connection_p_timeout);
  if(err) {
    IRLAP_CONN_LOGE(&lap->connect, "Failed to start p timer: %d", err);
    return err;
  }

  return IRLAP_FRAME_HANDLED;
}

int irlap_connect_handle_ua_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf) {
  int err;
  if(!conn) {
    IRLAP_CONN_LOGD(&lap->connect, "Ignoring ua resp outside connection");
    return -IRLAP_ERR_NO_CONNECTION;
  }
  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_SSETUP:
      err = irlap_connect_handle_ua_resp_ssetup(conn, data, len, pf);
      break;
    default:
      err = -IRLAP_ERR_STATION_STATE;
  }

  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

static int irlap_connect_handle_dm_resp_ssetup(struct irlap_connection* conn) {
  struct irlap* lap = conn->lap;
  irlap_connection_stop_p_timer(conn);
  lap->state = IRLAP_STATION_MODE_NDM;
  if(lap->services.disconnect.indication) {
    lap->services.disconnect.indication(conn->connection_addr, NULL, lap->priv);
  }
  irlap_connection_free(conn);
  return IRLAP_FRAME_HANDLED;
}

int irlap_connect_handle_dm_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf) {
  int err;
  if(!conn) {
    IRLAP_CONN_LOGD(&lap->connect, "Ignoring dm resp outside connection");
    return -IRLAP_ERR_NO_CONNECTION; 
  }
  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_SSETUP:
      err = irlap_connect_handle_dm_resp_ssetup(conn);
      break;
    default:
      err = -IRLAP_ERR_STATION_STATE;
  }

  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

/*
int irlap_service_disconnect_request(struct irlap* lap, irlap_connection_addr_t hndl) {

}
*/
