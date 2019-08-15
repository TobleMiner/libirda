#include <string.h>

#include "irphy.h"

#define LOCAL_TAG "IRDA PHY"

#define IRPHY_LOGV(phy, fmt, ...) IRHAL_LOGV(phy->hal, fmt, #__VA_ARGS__)

int irphy_init(struct irphy* phy, struct irhal* hal, const struct irphy_hal_ops* hal_ops) {
  memset(phy, 0, sizeof(*phy));
  phy->hal = hal;
  phy->hal_ops = *hal_ops;
  return 0;
}
