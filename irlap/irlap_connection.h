#include <stdint.h>

#include "irlap_defs.h"
#include "irlap_negotiation.h"

struct irlap_connection {
  irlap_connection_list_t list;
  struct irlap* lap;
  irlap_connection_state_t connection_state;
  void* state_lock;
  irlap_connection_addr_t connection_addr;
  irlap_negotiation_params_t local_negotiation_params;
  irlap_negotiation_values_t local_negotiation_values;
  irlap_negotiation_values_t remote_negotiation_values;
  int p_timer;
  int f_timer;
  irlap_addr_t remote_address;
};

#define IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN 22

union irlap_snrm_frame {
  struct {
    irlap_addr_t            src_address;
    irlap_addr_t            dst_address;
    irlap_connection_addr_t connection_addr;
    uint8_t                 negotiation_params[IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
  } __attribute__((packed));
  uint8_t data[9];
  uint8_t data_params[9 + IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
};

struct irlap_connection* irlap_get_connection(struct irlap* lap, irlap_connection_addr_t connection_addr);
int irlap_connection_alloc(struct irlap* lap, struct irlap_connection** retval);
void irlap_connection_free(struct irlap* lap, struct irlap_connection* conn);
int irlap_connection_start_p_timer(struct irlap_connection* conn, irhal_timer_cb cb);
