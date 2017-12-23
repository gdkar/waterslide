#ifdef __cplusplus
#include <atomic>
using std::atomic;
using std::atomic_compare_exchange_strong;
using std::atomic_compare_exchange_weak;
using std::atomic_fetch_add;
using std::atomic_fetch_sub;
using std::atomic_fetch_and;
using std::atomic_fetch_or;
using std::atomic_exchange;
using std::atomic_load;
using std::atomic_store;
#ifndef _Atomic
#define _Atomic(x) atomic<x>
#endif
#else
#include <stdatomic.h>
#endif
