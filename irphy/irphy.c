#include <string.h>

#include "irphy.h"

int irphy_init(struct irhal* hal, const struct irphy_hal_ops* hal_ops) {
  memset(hal, 0, sizeof(*hal));
  hal->hal_ops = *hal_ops;
  return 0;
}

static void irphy_cd_detected(void* priv) {
  struct irphy* phy = priv;
  irhal_clear_timer(phy->hal, phy->cb.timer);
  phy->cd.timer = IRHAL_TIMER_INVALID;
  phy->hal_ops.cd_disable(phy->cd.priv);
  phy->cd.cb(true, phy->cd.priv);
}

static void irphy_cd_timeout(void* priv) {
  struct irphy* phy = priv;
  phy->cd.timer = IRHAL_TIMER_INVALID;
  phy->hal_ops.cd_disable(phy->cd.priv);
  phy->cd.cb(false, phy->cd.priv);
}

int irphy_run_cd(struct irphy* phy, time_ns_t duration, const irphy_cd_cb cb, void* priv) {
  int err;
  phy->cd.cb = cb;
  phy->cd.priv = priv;
  err = irhal_start_timer(phy->hal, duration, irphy_cd_timeout, phy);
  if(err < 0) {
    goto fail;
  }
  phy->cd.timer = err;

  err = phy->hal_ops.cd_enable(phy, irphy_cd_detected, phy);
  if(err) {
    goto fail_timer;
  }

  return 0;

fail_timer:
  irhal_clear_timer(phy->hal, phy->cd.timer);
fail:
  return err;
}
