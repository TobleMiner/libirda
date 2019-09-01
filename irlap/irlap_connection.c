#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "irlap_connection.h"
#include "irlap.h"

#define LOCAL_TAG "IRDA LAP CONNECTION"

#define IRLAP_CONNECTION_LOGV(conn, fmt, ...) IRHAL_LOGV(conn->lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONNECTION_LOGD(conn, fmt, ...) IRHAL_LOGD(conn->lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONNECTION_LOGI(conn, fmt, ...) IRHAL_LOGI(conn->lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONNECTION_LOGW(conn, fmt, ...) IRHAL_LOGW(conn->lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_CONNECTION_LOGE(conn, fmt, ...) IRHAL_LOGE(conn->lap->phy->hal, fmt, ##__VA_ARGS__)

static void set_default_negotiation_params(struct irlap* lap, irlap_negotiation_params_t* params) {
  values->
}

struct irlap_connection* irlap_connection_get_connection(struct irlap* lap, irlap_connection_addr_t connection_addr) {
  struct irlap_connection* conn = NULL;
  irlap_discovery_log_list_t* cursor;

  irlap_lock_take_reentrant(lap, lap->connection_lock);
  LIST_FOR_EACH(cursor, &lap->connections) {
    struct irlap_connection* entry = LIST_GET_ENTRY(cursor, struct irlap_connection, list);
    if(IRLAP_CONNECTION_ADDRESS_MASK_CMD_BIT(entry->connection_addr) == IRLAP_CONNECTION_ADDRESS_MASK_CMD_BIT(connection_addr)) {
      conn = entry;
      break;
    }
  }

  irlap_lock_put_reentrant(lap, lap->connection_lock);
  return conn;
}

int irlap_connection_alloc(struct irlap* lap, irlap_addr_t remote_addr, struct irlap_connection** retval) {
  int err;
  irlap_connection_addr_t i;
  irlap_connection_addr_t connection_addr = 0;
  uint8_t num_free_addrs = 0;
  uint8_t connection_addr_idx;
  struct irlap_connection* conn = calloc(1, sizeof(struct irlap_connection));
  if(!conn) {
    err = -ENOMEM;
    goto fail;
  }
  conn->lap = lap;
  conn->remote_addr = remote_addr;
  err = irlap_lock_alloc_reentrant(lap, &conn->state_lock);
  if(err) {
    goto fail_connection_alloc;
  }
  irlap_lock_take_reentrant(lap, lap->connection_lock);

  // Calculate number of free addresses
  for(i = IRLAP_CONNECTION_ADDRESS_MIN; i < IRLAP_CONNECTION_ADDRESS_MAX; i++) {
    if(!irlap_connection_get_connection(lap, i)) {
      num_free_addrs++;
    }
  }

  IRLAP_LOGD(lap, "Have %u free connection numbers", num_free_addrs);
  if(num_free_addrs == 0) {
    IRLAP_LOGW(lap, "Failed to set up lap connection, no free connection addresses available");
    err = -IRLAP_ERR_NO_CONNECTION_ADDRESS_AVAILABLE;
    goto fail_connections_locked;
  }

  err = irlap_random_u8(lap, &connection_addr_idx, 0, num_free_addrs);
  if(err) {
    IRLAP_LOGE(lap, "Failed to get random connection number index");
    goto fail_connections_locked;
  }

  // Get n-th free address
  for(i = IRLAP_CONNECTION_ADDRESS_MIN; i < IRLAP_CONNECTION_ADDRESS_MAX; i++) {
    if(!irlap_connection_get_connection(lap, i)) {
      if(connection_addr_idx-- == 0) {
        connection_addr = i;
      }
    }
  }

  IRLAP_LOGD(lap, "Got connection address %u", connection_addr);

  conn->connection_addr = connection_addr;
  LIST_APPEND(&conn->list, &lap->connections);

  irlap_lock_put_reentrant(lap, lap->connection_lock);
  *retval = conn;
  return 0;

fail_connections_locked:
  irlap_lock_put_reentrant(lap, lap->connection_lock);
fail_connection_alloc:
  free(conn);
fail:
  return err;
}

void irlap_connection_free(struct irlap_connection* conn) {
  struct irlap* lap = conn->lap;
  irlap_lock_take_reentrant(lap, lap->connection_lock);
  LIST_DELETE(&conn->list);
  irlap_lock_free_reentrant(lap, conn->state_lock);
  free(conn);
  irlap_lock_put_reentrant(lap, lap->connection_lock);
}

int irlap_connection_start_p_timer(struct irlap_connection* conn, irhal_timer_cb cb) {
  unsigned int timeout = IRLAP_P_TIMEOUT_MAX;
  int err;
  irlap_lock_take_reentrant(conn->lap, conn->state_lock);
  if(IRLAP_CONNECTION_IS_NEGOTIATED(conn)) {
    timeout = conn->local_negotiation_values->max_turn_around_time_ms;
  }
  err = irlap_set_timer(conn->lap, timeout, cb);
  irlap_lock_put_reentrant(conn->lap, conn->state_lock);
  return err;
}

int irlap_connection_send_snrm_cmd(struct irlap_connection* conn) {
  union irlap_snrm_frame frame;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_SNRM | IRLAP_CMD_POLL,
  };
  frame.src_addr = irlap_get_address(conn->lap);
  frame.dst_addr = conn->remote_address;
  frame.connection_addr = conn->connection_addr;
  
}
