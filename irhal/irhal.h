#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../util/time.h"

#define IRHAL_NUM_TIMER_DEFAULT 8
#define IRHAL_TIMER_INVALID -1

#define IRHAL_LOG_LEVEL_NONE    0
#define IRHAL_LOG_LEVEL_ERROR   1
#define IRHAL_LOG_LEVEL_WARNING 2
#define IRHAL_LOG_LEVEL_INFO    3
#define IRHAL_LOG_LEVEL_DEBUG   4
#define IRHAL_LOG_LEVEL_VERBOSE 5

struct irhal;
struct irhal_timer;

typedef void (*irhal_alarm_cb)(struct irhal* hal);

typedef uint64_t (*irhal_get_time)(void* arg);
typedef int (*irhal_set_alarm)(struct irhal* hal, irhal_alarm_cb cb, uint64_t timeout, void* arg);
typedef int (*irhal_clear_alarm)(void* arg);


#define IRHAL_LOG(hal, level, fmt, ...) if((hal)->hal_ops.log) { (hal)->hal_ops.log(hal->priv, level, LOCAL_TAG, fmt, ##__VA_ARGS__); }
#define IRHAL_LOGV(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_VERBOSE, fmt, ##__VA_ARGS__)
#define IRHAL_LOGD(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define IRHAL_LOGI(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define IRHAL_LOGW(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define IRHAL_LOGE(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

typedef void (*irhal_log)(void* priv, int level, const char* tag, const char* fmt, ...);

struct irhal_hal_ops {
  irhal_get_time         get_time;
  irhal_set_alarm        set_alarm;
  irhal_clear_alarm      clear_alarm;
  irhal_log              log;
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

