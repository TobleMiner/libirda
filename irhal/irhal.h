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

#define IHAL_LOG_LEVEL IRHAL_LOG_LEVEL_DEBUG

struct irhal;
struct irhal_timer;
struct irhal_timer_fire;

typedef void (*irhal_alarm_cb)(struct irhal* hal);

typedef uint64_t (*irhal_get_time_f)(void* arg);
typedef int (*irhal_set_alarm_f)(struct irhal* hal, irhal_alarm_cb cb, uint64_t timeout, void* arg);
typedef int (*irhal_clear_alarm_f)(void* arg);
typedef int (*irhal_get_random_bytes_f)(uint8_t* data, size_t len, void* arg);
typedef int (*irhal_lock_alloc_f)(void** lock, void* priv);
typedef void (*irhal_lock_free_f)(void* lock, void* priv);
typedef void (*irhal_lock_take_f)(void* lock, void* priv);
typedef void (*irhal_lock_put_f)(void* lock, void* priv);

#define IRHAL_LOG(hal, level, fmt, ...) if(level <= IHAL_LOG_LEVEL) { if((hal)->hal_ops.log) { (hal)->hal_ops.log(hal->priv, level, LOCAL_TAG, fmt, ##__VA_ARGS__); } }
#define IRHAL_LOGV(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_VERBOSE, fmt, ##__VA_ARGS__)
#define IRHAL_LOGD(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define IRHAL_LOGI(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define IRHAL_LOGW(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define IRHAL_LOGE(hal, fmt, ...) IRHAL_LOG(hal, IRHAL_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

typedef void (*irhal_log_f)(void* priv, int level, const char* tag, const char* fmt, ...);

struct irhal_hal_ops {
  irhal_get_time_f         get_time;
  irhal_set_alarm_f        set_alarm;
  irhal_clear_alarm_f      clear_alarm;
  irhal_log_f              log;
  irhal_get_random_bytes_f get_random_bytes;
  irhal_lock_alloc_f       lock_alloc;
  irhal_lock_free_f        lock_free;
  irhal_lock_take_f        lock_take;
  irhal_lock_put_f         lock_put;
};

struct irhal {
  uint64_t max_time_val;
  uint64_t timescale;
  struct irhal_timer* timers;
  struct irhal_timer_fire* fire_list;
  struct irhal_hal_ops hal_ops;
  size_t num_timers;
  uint64_t last_timestamp;
  time_ns_t last_time;
  time_ns_t current_timer_deadline;
  void* timer_lock;
  void* priv;
};

typedef void (*irhal_timer_cb)(void* priv);

struct irhal_timer_fire {
  irhal_timer_cb cb;
  void* priv;
};

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
int irhal_random_bytes(struct irhal* hal, uint8_t* data, size_t len);

static inline int irhal_lock_alloc(struct irhal* hal, void** lock) {
  return hal->hal_ops.lock_alloc(lock, hal->priv);
}

static inline void irhal_lock_free(struct irhal* hal, void* lock) {
  return hal->hal_ops.lock_free(lock, hal->priv);
}

static inline void irhal_lock_take(struct irhal* hal, void* lock) {
  return hal->hal_ops.lock_take(lock, hal->priv);
}

static inline void irhal_lock_put(struct irhal* hal, void* lock) {
  return hal->hal_ops.lock_put(lock, hal->priv);
}
