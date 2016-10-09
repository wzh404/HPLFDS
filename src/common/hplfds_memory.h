#ifndef HPLFDS_MEMORY
#define HPLFDS_MEMORY
#include "../common/hplfds_define.h"
namespace hplfds_sync
{
  class HplfdsMemoryAllocator
  {
  public:
    static void *allocate(int64_t size)
    {
      return malloc(size);
    }
    static void free(void *p)
    {
      return ::free(p);
    }
  };
}
#endif //HPLFDS_MEMORY
