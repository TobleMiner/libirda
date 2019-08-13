#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "../util/time.h"
#include "../irhal/irhal.h"

typedef void (*irphy_cd_cb)(bool carrier_detected, void* priv);

struct irphy;

typedef void (*irphy_rx_cb)(struct irphy* phy, size_t len);
typedef void (*irphy_carrier_cb)(struct irphy* phy);

typedef int (*irphy_hal_set_baudrate)(uint32_t rate, void* priv);
typedef int (*irphy_hal_tx_enable)(void* priv);
typedef ssize_t (*irphy_hal_tx)(const void* data, size_t len, void* priv);
typedef int (*irphy_hal_tx_wait)(void* priv);
typedef int (*irphy_hal_tx_disable)(void* priv);
typedef int (*irphy_hal_rx_enable)(const struct irphy* phy, irphy_rx_cb cb, void* priv);
typedef ssize_t (*irphy_hal_rx)(void* data, size_t len, void* priv);
typedef int (*irphy_hal_rx_disable)(void* priv);
typedef int (*irphy_hal_cd_enable)(const struct irphy* phy, irphy_carrier_cb cb, void* priv);
typedef int (*irphy_hal_cd_disable)(void* priv);

struct irphy_hal_ops {
  irphy_hal_set_baudrate          set_baudrate;
  irphy_hal_tx_enable             tx_enable;
  irphy_hal_tx                    tx;
  irphy_hal_tx_wait               tx_wait;
  irphy_hal_tx_disable            tx_disable;
  irphy_hal_rx_enable             rx_enable;
  irphy_hal_rx                    rx;
  irphy_hal_rx_disable            rx_disable;
  irphy_hal_cd_enable             cd_enable;
  irphy_hal_cd_disable            cd_disable;
};

struct irphy {
  struct irhal* hal;
  struct irphy_hal_ops hal_ops;
  void* hal_priv;
  struct {
    void* priv;
    irphy_cd_cb cb;
    int timer;
  } cd;
};

int irphy_init(struct irphy* phy, struct irhal* hal, const struct irphy_hal_ops* hal_ops);
int irphy_run_cd(struct irphy* phy, const time_ns_t* duration, const irphy_cd_cb cb, void* priv);
