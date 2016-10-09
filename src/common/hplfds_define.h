#ifndef HPLFDS_DEFINE
#define HPLFDS_DEFINE
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define MAX_THREAD_NUM 4
#define MY_ASSERT(condition)\
  if (!(condition)) {\
    abort();\
  }
#define LIKELY(x) __builtin_expect(!!(x),1)
#define UNLIKELY(x) __builtin_expect(!!(x),0)
#define INLINE inline __attribute__((always_inline))
#define CAS(address,oldValue,newValue) __sync_bool_compare_and_swap(address,oldValue,newValue)
#define CACHELINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHELINE_SIZE)))
#define CPU_RELAX() __asm__ __volatile__("pause\n": : :"memory")
//error
#define SUCCESS 0
#define ERROR_ENTRY_ALREADY_EXISTS  -1
#define ERROR_NOT_INITED  -2
#define ERROR_NO_MEMORY -3
#define ERROR_EMPTY -4
#define ERROR_INDEX_OUT_OF_RANGE -5
#define ERROR_INVALID_ARGUMENT -6
#define ERROR_ENTRY_NOT_EXIST -7
#define ERROR_TABLE_FULL -8
#define ERROR_TABLE_NEED_RESIZE -9
#define ERROR_UNEXPECTED -10
#define ERROR_NOT_SUPPORT -11
#define ERROR_MEMORY_LIMITED -12
#define ERROR_AGAIN -13
#define LOG(m, p, tid, other_id, x) hplfds_sync::logger.log(m, p, tid, other_id, x)
//#define LOG(m, p, tid, other_id)
#define SHOW() hplfds_sync::logger.show()
#endif //HPLFDS_DEFINE
