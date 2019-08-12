#pragma once

#define TIME_NSEC_PER_SEC 1000000000UL

#define TIME_NS_MAX ({ 0xFFFFFFFF, TIME_NSEC_PER_SEC })

struct time_ns {
  int32_t sec;
  int32_t nsec;
};

typedef struct time_ns time_ns_t;

#define TIME_NS_LT(a, b) \
  ((a).sec < (b).sec || \
  ((a).sec == (b).sec && (a).nsec < (b).nsec))

#define TIME_NS_GT(a, b) TIME_NS_LT((b), (a))

#define TIME_NS_EQ(a, b) \
  ((a).sec == (b).sec && (a).nsec == (b).nsec)

#define TIME_NS_LE(a, b) (TIME_NS_LT((a), (b)) || TIME_NS_EQ((a), (b))

#define TIME_NS_GE(a, b) (TIME_NS_GT((a), (b)) || TIME_NS_EQ((a), (b))

void time_zero(time_ns_t* t);
void time_normalize(time_ns_t* t);
void time_add(time_ns_t* a, const time_ns_t* b);
void time_sub(time_ns_t* a, const time_ns_t* b);
void time_add_ns(time_ns_t* t, uint64_t nsec);
uint64_t time_to_ns(const time_ns_t* t);
