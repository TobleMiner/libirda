#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "irhal.h"

int irhal_init(struct irhal* hal, struct irhal_hal_ops* hal_ops, uint64_t max_time_val, uint64_t timescale) {
  memset(hal, 0, sizeof(*hal));
  hal->max_time_val = max_time_val;
  hal->timescale = timescale;
  hal->hal_ops = *hal_ops;
  hal->timers = calloc(IRHAL_NUM_TIMER_DEFAULT, sizeof(struct irhal_timer));
  if(!hal->timers) {
    return -ENOMEM;
  }
  hal->num_timers = IRHAL_NUM_TIMER_DEFAULT;
  return 0;
}

void irhal_now(struct irhal* hal, time_ns_t* t) {
  uint64_t delta_ns;
  uint64_t ts_now = hal->hal_ops.get_time(hal->priv);
  int64_t delta_t = ts_now - hal->last_timestamp;
  if(delta_t < 0) {
    // Messy, there was an overflow
    // Leftover from last iteration
    delta_t = hal->max_time_val - hal->last_timestamp;
    // Time on new iteartion
    delta_t += ts_now;
  }
  delta_ns = delta_t * hal->timescale;
  time_add_ns(&hal->last_time, delta_ns);
  hal->last_timestamp = ts_now;
  *t = hal->last_time;
}

static void irhal_alarm_callback(struct irhal* hal);

static int irhal_recalculate_timeout(struct irhal* hal, bool fire_cbs, time_ns_t* assumed_now) {
  int i;
  time_ns_t earliest_deadline = TIME_NS_MAX;
  time_ns_t now;
  uint64_t delta_ns;
  if(assumed_now) {
    now = *assumed_now;
  } else {
    irhal_now(hal, &now);
  }
  for(i = 0; i < hal->num_timers; i++) {
    struct irhal_timer* timer = &hal->timers[i];
    // Ignore timers that are not running
    if(!timer->enabled) {
      continue;
    }
    if(fire_cbs) {
      if(TIME_NS_LE(timer->deadline, now)) {
        timer->enabled = false;
        timer->cb(timer->priv);
        continue;
      }      
    }
    // Ignore timers whose timeout has not fired yet
    // It might be a good idea to terminate here, the
    // incoming timer event will cause recalculation
    // anyways
    if(TIME_NS_LT(timer->deadline, now)) {
      continue;
    }
    if(TIME_NS_LT(timer->deadline, earliest_deadline)) {
      earliest_deadline = timer->deadline;
    }
  }
  // Check if timeout is still TIME_NS_MAX, then there is nothing to do
  if(time_is_max(earliest_deadline)) {
    // No timer required
    return 0;
  }

  // Check if timer is already set for calculated time
  if(TIME_NS_EQ(earliest_deadline, hal->current_timer_deadline)) {
    // Timer correctly set, nothing to do
    return 0;
  }

  // New timeout does not match old timeout
  // We need to calculate the delta t to the earliest deadline

  time_sub(&now, &earliest_deadline);
  delta_ns = time_to_ns(&now);

  delta_ns += hal->timescale - 1ULL;
  // Convert into HAL API timescale
  delta_ns /= hal->timescale;

  // We can't use delta values higher than max_time_val
  // since we would be unable to recover the actual time
  // difference in irhal_now
  // Limit values to halfof that, just to be safe
  if(delta_ns > hal->max_time_val / 2ULL) {
    delta_ns = hal->max_time_val / 2ULL;
  }

  return hal->hal_ops.set_alarm(hal, irhal_alarm_callback, delta_ns, hal->priv);
}

static void irhal_alarm_callback(struct irhal* hal) {
  time_ns_t now;
  irhal_now(hal, &now);
  irhal_recalculate_timeout(hal, true, &now);
}

static int irhal_set_timer_(struct irhal* hal, struct irhal_timer* timer, time_ns_t* timeout, irhal_timer_cb cb, void* priv) {
  time_ns_t now;
  timer->enabled = true;
  timer->cb = cb;
  timer->priv = priv;
  irhal_now(hal, &now);
  timer->deadline = now;
  time_add(&timer->deadline, timeout);
  
  return irhal_recalculate_timeout(hal, false, &now);
}

static int irhal_request_timers_(struct irhal* hal) {
  size_t new_num_timers = hal->num_timers + IRHAL_NUM_TIMER_DEFAULT;
  struct irhal_timer* new_timers = realloc(hal->timers, sizeof(struct irhal_timer) * new_num_timers);
  if(!new_timers) {
    return -ENOMEM;
  }
  // Initialize new timers
  memset(&new_timers[hal->num_timers], 0, (new_num_timers - hal->num_timers) * sizeof(struct irhal_timer));
  hal->timers = new_timers;
  hal->num_timers = new_num_timers;
  return 0;
}

int irhal_set_timer(struct irhal* hal, time_ns_t* timeout, irhal_timer_cb cb, void* priv) {
  int i;
  int err;
  for(i = 0; i < hal->num_timers; i++) {
    struct irhal_timer* timer = &hal->timers[i];
    if(!timer->enabled) {
      err = irhal_set_timer_(hal, timer, timeout, cb, priv);
      if(err) {
        return err;
      }
      return i;
    }
  }
  err = irhal_request_timers_(hal);
  if(err) {
    return err;
  }
  return irhal_set_timer(hal, timeout, cb, priv);
}

int irhal_clear_timer(struct irhal* hal, int timerid) {
  struct irhal_timer* timer;
  if(timerid < 0 || timerid >= hal->num_timers) {
    return -EINVAL;
  }
  timer = &hal->timers[timerid];
  if(!timer->enabled) {
    return -EINVAL;
  }
  timer->enabled = false;
  return irhal_recalculate_timeout(hal, false, NULL);
}
