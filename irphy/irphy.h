#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "../util/time.h"
#include "../irhal/irhal.h"

typedef void (*irphy_cd_cb)(bool carrier_detected, void* priv);

typedef enum {
  IRPHY_EVENT_DATA_RX,
  IRPHY_EVENT_FRAMING_ERROR,
  IRPHY_EVENT_RX_OVERFLOW,
} irphy_event_t;

struct irphy;

typedef void (*irphy_rx_cb)(struct irphy* phy, irphy_event_t event, void* priv);

typedef int (*irphy_hal_set_baudrate)(uint32_t rate, void* priv);
typedef int (*irphy_hal_tx_enable)(void* priv);
typedef ssize_t (*irphy_hal_tx)(const void* data, size_t len, void* priv);
typedef int (*irphy_hal_tx_wait)(void* priv);
typedef int (*irphy_hal_tx_disable)(void* priv);
typedef int (*irphy_hal_rx_enable)(const struct irphy* phy, void* priv, irphy_rx_cb cb, void* cb_priv);
typedef ssize_t (*irphy_hal_rx)(void* data, size_t len, void* priv);
typedef int (*irphy_hal_rx_disable)(void* priv);

struct irphy_hal_ops {
  irphy_hal_set_baudrate          set_baudrate;
  irphy_hal_tx_enable             tx_enable;
  irphy_hal_tx                    tx;
  irphy_hal_tx_wait               tx_wait;
  irphy_hal_tx_disable            tx_disable;
  irphy_hal_rx_enable             rx_enable;
  irphy_hal_rx                    rx;
  irphy_hal_rx_disable            rx_disable;
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

static inline int irphy_set_baudrate(struct irphy* phy, uint32_t rate) {
  return phy->hal_ops.set_baudrate(rate, phy->hal_priv);
}

static inline int irphy_tx_enable(struct irphy* phy) {
  return phy->hal_ops.tx_enable(phy->hal_priv);
}

static inline ssize_t irphy_tx(struct irphy* phy, const void* data, size_t len) {
  return phy->hal_ops.tx(data, len, phy->hal_priv);
}

static inline int irphy_tx_wait(struct irphy* phy) {
  return phy->hal_ops.tx_wait(phy->hal_priv);
}

static inline int irphy_tx_disable(struct irphy* phy) {
  return phy->hal_ops.tx_disable(phy->hal_priv);
}

static inline int irphy_rx_enable(const struct irphy* phy, irphy_rx_cb cb, void* cb_priv) {
  return phy->hal_ops.rx_enable(phy, phy->hal_priv, cb, cb_priv);
}

static inline ssize_t irphy_rx(struct irphy* phy, void* data, size_t len) {
  return phy->hal_ops.rx(data, len, phy->hal_priv);
}

static inline int irphy_rx_disable(struct irphy* phy) {
  return phy->hal_ops.rx_disable(phy->hal_priv);
}
