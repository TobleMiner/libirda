#include "time.h"

void time_zero(time_ns_t* t) {
  t->sec = 0;
  t->nsec = 0;
}

void time_normalize(time_ns_t* t) {
  while(t->nsec >= TIME_NSEC_PER_SEC) {
    t->nsec -= TIME_NSEC_PER_SEC;
    t->sec++;
  }
}

void time_add(time_ns_t* a, const time_ns_t* b) {
  a->sec += b->sec;
  a->nsec += b->nsec;
  time_normalize(a);
}

// a MUST be greter or equal d, else undefined behaviour
void time_sub(time_ns_t* a, const time_ns_t* b) {
  a->sec -= b->sec;
  if(b->nsec > a->nsec) {
    a->sec--;
    a->nsec += TIME_NSEC_PER_SEC;
  }
  a->nsec -= b->nsec;
}

void time_add_ns(time_ns_t* t, uint64_t nsec) {
  while(nsec >= (uint64_t)TIME_NSEC_PER_SEC) {
    nsec -= (uint64_t)TIME_NSEC_PER_SEC;
    t->sec++;
  }
  t->nsec += (uint32_t)nsec;
  time_normalize(t);
}

uint64_t time_to_ns(const time_ns_t* t) {
  return (uint64_t)t->sec * (uint64_t)TIME_NSEC_PER_SEC + (uint64_t)t->nsec;
}
