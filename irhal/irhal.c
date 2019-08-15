#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "irhal.h"

#define LOCAL_TAG "IRDA HAL"

int irhal_init(struct irhal* hal, struct irhal_hal_ops* hal_ops, uint64_t max_time_val, uint64_t timescale) {
  int err;
  memset(hal, 0, sizeof(*hal));
  hal->max_time_val = max_time_val;
  hal->timescale = timescale;
  hal->hal_ops = *hal_ops;
  hal->timers = calloc(IRHAL_NUM_TIMER_DEFAULT, sizeof(struct irhal_timer));
  if(!hal->timers) {
    err = -ENOMEM;
    goto fail;
  }
  hal->fire_list = calloc(IRHAL_NUM_TIMER_DEFAULT, sizeof(struct irhal_timer_fire));
  if(!hal->fire_list) {
    err = -ENOMEM;
    goto fail_timers;
  }
  hal->num_timers = IRHAL_NUM_TIMER_DEFAULT;
  err = irhal_lock_alloc(hal, &hal->timer_lock);
  if(err) {
    goto fail_fire_list;
  }
  return 0;

fail_fire_list:
  free(hal->fire_list);
fail_timers:
  free(hal->timers);
fail:
  return err;
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
  int fire_index = 0;
  time_ns_t earliest_deadline = TIME_NS_MAX;
  time_ns_t now;
  uint64_t delta_ns;
  IRHAL_LOGV(hal, "Recalculating timeouts");
  if(assumed_now) {
    now = *assumed_now;
  } else {
    irhal_now(hal, &now);
  }
  IRHAL_LOGV(hal, "Now is sec = %lu, nsec = %lu", now.sec, now.nsec);
  if(fire_cbs) {
    memset(hal->fire_list, 0, hal->num_timers * sizeof(*hal->fire_list));
  }
  for(i = 0; i < hal->num_timers; i++) {
    struct irhal_timer* timer = &hal->timers[i];
    // Ignore timers that are not running
    if(!timer->enabled) {
      continue;
    }
    IRHAL_LOGV(hal, "Timer %d is enabled, deadline: sec = %lu, nsec = %lu", i, timer->deadline.sec, timer->deadline.nsec);
    if(fire_cbs) {
      if(TIME_NS_LE(timer->deadline, now)) {
        IRHAL_LOGV(hal, "  Adding timer %d to fire list", i);
        hal->fire_list[fire_index].cb = timer->cb;
        hal->fire_list[fire_index].priv = timer->priv;
        fire_index++;
//        timer->cb(timer->priv);
        timer->enabled = false;
        continue;
      }      
    }
    // Ignore timers whose timeout has not fired yet
    // It might be a good idea to terminate here, the
    // incoming timer event will cause recalculation
    // anyways
    if(TIME_NS_LT(timer->deadline, now)) {
      IRHAL_LOGV(hal, "  Timer %d is due", i);
      continue;
    }
    if(TIME_NS_LT(timer->deadline, earliest_deadline)) {
      earliest_deadline = timer->deadline;
    }
  }
  // Check if timeout is still TIME_NS_MAX, then there is nothing to do
  if(time_is_max(earliest_deadline)) {
    IRHAL_LOGV(hal, "Recalculation finished, no timer required");
    // No timer required
    return 0;
  }

  // Check if timer is already set for calculated time
  if(TIME_NS_EQ(earliest_deadline, hal->current_timer_deadline)) {
    IRHAL_LOGV(hal, "Recalculation finished, no new timer required");
    // Timer correctly set, nothing to do
    return 0;
  }

  // New timeout does not match old timeout
  // We need to calculate the delta t to the earliest deadline

  time_sub(&earliest_deadline, &now);
  IRHAL_LOGV(hal, "Delta to deadline is sec = %lu, nsec = %lu", earliest_deadline.sec, earliest_deadline.nsec);
  delta_ns = time_to_ns(&earliest_deadline);
  IRHAL_LOGV(hal, "Recalculation finished, next timer fires in %llu ns", delta_ns);

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

  hal->hal_ops.clear_alarm(hal->priv);
  return hal->hal_ops.set_alarm(hal, irhal_alarm_callback, delta_ns, hal->priv);
}

static void irhal_alarm_callback(struct irhal* hal) {
  int i;
  time_ns_t now;
  irhal_now(hal, &now);
  IRHAL_LOGV(hal, "Got alarm callback");
  irhal_lock_take(hal, hal->timer_lock);
  irhal_recalculate_timeout(hal, true, &now);
  irhal_lock_put(hal, hal->timer_lock);
  for(i = 0; i < hal->num_timers; i++) {
    struct irhal_timer_fire* fire_entry = &hal->fire_list[i];
    if(fire_entry->cb) {
      fire_entry->cb(fire_entry->priv);
    }
  }
}

static int irhal_set_timer_(struct irhal* hal, struct irhal_timer* timer, const time_ns_t* timeout, irhal_timer_cb cb, void* priv) {
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
  struct irhal_timer_fire* new_fire_list;
  struct irhal_timer* new_timers = realloc(hal->timers, sizeof(struct irhal_timer) * new_num_timers);
  if(!new_timers) {
    return -ENOMEM;
  }
  // Initialize new timers
  memset(&new_timers[hal->num_timers], 0, (new_num_timers - hal->num_timers) * sizeof(struct irhal_timer));
  hal->timers = new_timers;

  new_fire_list = realloc(hal->fire_list, sizeof(struct irhal_timer_fire) * new_num_timers);
  if(!new_fire_list) {
    return -ENOMEM;
  }
  hal->fire_list = new_fire_list;
  hal->num_timers = new_num_timers;
  return 0;
}

int irhal_set_timer(struct irhal* hal, time_ns_t* timeout, irhal_timer_cb cb, void* priv) {
  int i;
  int err;
  IRHAL_LOGV(hal, "Setting up timer for sec = %lu sec, nsec = %lu", timeout->sec, timeout->nsec);
  irhal_lock_take(hal, hal->timer_lock);
  for(i = 0; i < hal->num_timers; i++) {
    struct irhal_timer* timer = &hal->timers[i];
    if(!timer->enabled) {
      err = irhal_set_timer_(hal, timer, timeout, cb, priv);
      if(err) {
        goto out;
      }
      err = i;
      goto out;
    }
  }
  err = irhal_request_timers_(hal);
  if(err) {
    goto out;
  }
  err = irhal_set_timer(hal, timeout, cb, priv);
out:
  irhal_lock_put(hal, hal->timer_lock);
  return err;
}

int irhal_clear_timer(struct irhal* hal, int timerid) {
  int err;
  struct irhal_timer* timer;
  if(timerid < 0 || timerid >= hal->num_timers) {
    return -EINVAL;
  }
  irhal_lock_take(hal, hal->timer_lock);
  timer = &hal->timers[timerid];
  if(!timer->enabled) {
    err = -EINVAL;
    goto out;
  }
  timer->enabled = false;
  err = irhal_recalculate_timeout(hal, false, NULL);
out:
  irhal_lock_put(hal, hal->timer_lock);
  return err;
}

int irhal_random_bytes(struct irhal* hal, uint8_t* data, size_t len) {
  return hal->hal_ops.get_random_bytes(data, len, hal->priv);
}
