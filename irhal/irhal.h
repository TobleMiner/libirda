#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../util/time.h"

#define IRHAL_NUM_TIMER_DEFAULT 8
#define IRHAL_TIMER_INVALID -1

struct irhal;
struct irhal_timer;

typedef void (*irhal_alarm_cb)(struct irhal* hal);

typedef uint64_t (*irhal_get_time)(void* arg);
typedef int (*irhal_set_alarm)(struct irhal* hal, irhal_alarm_cb cb, uint64_t timeout, void* arg);
typedef int (*irhal_clear_alarm)(void* arg);

struct irhal_hal_ops {
  irhal_get_time         get_time;
  irhal_set_alarm        set_alarm;
  irhal_clear_alarm      clear_alarm;
};

struct irhal {
  uint64_t max_time_val;
  uint64_t timescale;
  struct irhal_timer* timers;
  struct irhal_hal_ops hal_ops;
  size_t num_timers;
  uint64_t last_timestamp;
  time_ns_t last_time;
  time_ns_t current_timer_deadline;
  void* priv;
};

typedef void (*irhal_timer_cb)(void* priv);

struct irhal_timer {
  bool enabled;
  irhal_timer_cb cb;
  void* priv; 
  time_ns_t deadline;
};

int irhal_init(struct irhal* hal, struct irhal_hal_ops* hal_ops, uint64_t max_time_val, uint64_t timescale);
void irhal_now(struct irhal* hal, time_ns_t* t);
int irhal_set_timer(struct irhal* hal, time_ns_t* timeout, irhal_timer_cb cb, void* priv);
int irhal_clear_timer(struct irhal* hal, int timer);
