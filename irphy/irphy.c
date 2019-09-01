#include <string.h>

#include "irphy.h"

#define LOCAL_TAG "IRDA PHY"

#define IRPHY_LOGV(phy, fmt, ...) IRHAL_LOGV(phy->hal, fmt, #__VA_ARGS__)

int irphy_init(struct irphy* phy, struct irhal* hal, const struct irphy_hal_ops* hal_ops, irphy_capability_baudrate_t supported_baudrates, uint32_t rx_turn_around_latency_us) {
  memset(phy, 0, sizeof(*phy));
  phy->hal = hal;
  phy->hal_ops = *hal_ops;
  phy->supported_baudrates = supported_baudrates;
  phy->rx_turn_around_latency_us = rx_turn_around_latency_us;
  return 0;
}
