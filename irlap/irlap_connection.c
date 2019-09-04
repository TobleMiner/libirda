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

void irlap_connection_set_default_negotiation_params(struct irlap* lap, irlap_negotiation_params_t* params) {
  irlap_negotiation_values_t values =  {
    .max_turn_around_time_ms = 500,
    .data_size = 2048,
    .window_size = 7,
    .additional_bofs = 0,
    .min_turn_around_time_us = 0,
    .disconnect_threshold_time_s = 30
  };
	irlap_negotiation_translate_values_to_params(params, &values, irlap_get_supported_baudrates(lap));
}

struct irlap_connection* irlap_connection_get(struct irlap* lap, irlap_connection_addr_t connection_addr) {
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
  conn->remote_address = remote_addr;
  err = irlap_lock_alloc_reentrant(lap, &conn->state_lock);
  if(err) {
    goto fail_connection_alloc;
  }
  irlap_lock_take_reentrant(lap, lap->connection_lock);

  // Calculate number of free addresses
  for(i = IRLAP_CONNECTION_ADDRESS_MIN; i < IRLAP_CONNECTION_ADDRESS_MAX; i++) {
    if(!irlap_connection_get(lap, i)) {
      num_free_addrs++;
    }
  }

  IRLAP_CONNECTION_LOGD(conn, "Have %u free connection numbers", num_free_addrs);
  if(num_free_addrs == 0) {
    IRLAP_CONNECTION_LOGW(conn, "Failed to set up lap connection, no free connection addresses available");
    err = -IRLAP_ERR_NO_CONNECTION_ADDRESS_AVAILABLE;
    goto fail_connections_locked;
  }

  err = irlap_random_u8(lap, &connection_addr_idx, 0, num_free_addrs);
  if(err) {
    IRLAP_CONNECTION_LOGE(conn, "Failed to get random connection number index");
    goto fail_connections_locked;
  }

  // Get n-th free address
  for(i = IRLAP_CONNECTION_ADDRESS_MIN; i < IRLAP_CONNECTION_ADDRESS_MAX; i++) {
    if(!irlap_connection_get(lap, i)) {
      if(connection_addr_idx-- == 0) {
        connection_addr = i;
      }
    }
  }

  IRLAP_CONNECTION_LOGD(conn, "Got connection address %u", connection_addr);

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
    timeout = conn->local_negotiation_values.max_turn_around_time_ms;
  }
  err = irlap_set_timer(conn->lap, timeout, cb, conn);
	if(err < 0) {
		goto fail_locked;
	}
	if(conn->p_timer >= 0) {
		irlap_clear_timer(conn->lap, conn->p_timer);
	}
	conn->p_timer = err;
	err = 0;

fail_locked:
  irlap_lock_put_reentrant(conn->lap, conn->state_lock);
  return err;
}

void irlap_connection_stop_p_timer(struct irlap_connection* conn) {
  irlap_lock_take_reentrant(conn->lap, conn->state_lock);
	if(conn->p_timer >= 0) {
		irlap_clear_timer(conn->lap, conn->p_timer);
    conn->p_timer = -1;
	}
  irlap_lock_put_reentrant(conn->lap, conn->state_lock);
}

int irlap_connection_send_snrm_cmd(struct irlap_connection* conn) {
  struct irlap* lap = conn->lap;
	ssize_t data_len;
	irlap_negotiation_params_t negotiation_params;
  union irlap_snrm_frame frame;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_SNRM | IRLAP_CMD_POLL,
  };

  frame.src_address = irlap_get_address(lap);
  frame.dst_address = conn->remote_address;
  frame.connection_addr = conn->connection_addr;

  irlap_connection_set_default_negotiation_params(lap, &negotiation_params);
	data_len = irlap_negotiation_populate_params(frame.negotiation_params, IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN, &negotiation_params);
	if(data_len < 0) {
		IRLAP_CONNECTION_LOGE(conn, "Failed to write connection parameters to snrm cmd frame");
		return (int)data_len;
	}
	return irlap_send_frame(lap, &hdr, frame.data_params, sizeof(frame.data) + data_len);
}

#define SUPERVISORY_NR(nr) (((nr) << 5) & IRLAP_SUPERVISORY_NR_MASK)

int irlap_connection_send_rr_cmd(struct irlap_connection* conn, uint8_t nr) {
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(conn->connection_addr),
    .control = IRLAP_FRAME_FORMAT_SUPERVISORY | IRLAP_CMD_RR | IRLAP_CMD_POLL | SUPERVISORY_NR(nr),
  };
	return irlap_send_frame(conn->lap, &hdr, NULL, 0);
}

uint32_t irlap_connection_get_baudrate(struct irlap_connection* conn) {
  if(IRLAP_CONNECTION_IS_NEGOTIATED(conn)) {
    return conn->local_negotiation_values.baudrate;
  }
  return IRLAP_BAUDRATE_CONTENTION;
}

uint8_t irlap_connection_get_num_additional_bofs(struct irlap_connection* conn) {
  if(IRLAP_CONNECTION_IS_NEGOTIATED(conn)) {
    return conn->remote_negotiation_values.additional_bofs;
  }
  return IRLAP_FRAME_ADDITIONAL_BOF_CONTENTION;
}

void irlap_connection_p_timeout(void* priv) {

}
