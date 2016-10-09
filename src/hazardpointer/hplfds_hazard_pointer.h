#ifndef HPLFDS_HAZARD_POINTER
#define HPLFDS_HAZARD_POINTER
#include "../common/hplfds_define.h"
namespace hplfds_sync
{
  template<typename MemoryAllocator>
  class HplfdsHazardPointer
  {
  private:
    struct HazardPointerNode
    {
      void *p;
      HazardPointerNode *next;
    };
    struct HazardPointerList
    {
      int16_t len;
      int16_t num;
      HazardPointerNode list;
    }CACHE_ALIGNED;
  public:
    HplfdsHazardPointer();
    ~HplfdsHazardPointer();
    int acquire(void *p, int thread_id);
    int retire(void *p, int thread_id);
    int release(void *p, int thread_id);
    int reclaim(int thread_id);
  private:
    int help(HazardPointerList *list, void *p, int thread_id);
  private:
    static const int16_t NUM_TO_RECLAIM_P = 100;
    static const int16_t LEN_TO_RECLAIM_HP = 100;
  private:
    HazardPointerList hp_list_[MAX_THREAD_NUM];
    HazardPointerList retire_list_[MAX_THREAD_NUM];
  };
  template<typename MemoryAllocator>
  HplfdsHazardPointer<MemoryAllocator>::HplfdsHazardPointer()
  {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
      hp_list_[i].len = 0;
      hp_list_[i].num = 0;
      hp_list_[i].list.next = NULL;
      hp_list_[i].list.p = NULL;
      retire_list_[i].len = 0;
      retire_list_[i].num = 0;
      retire_list_[i].list.next = NULL;
      retire_list_[i].list.p = NULL;
    }
  }
  template<typename MemoryAllocator>
  HplfdsHazardPointer<MemoryAllocator>::~HplfdsHazardPointer()
  {
    //todo
  }
  template<typename MemoryAllocator>
  int HplfdsHazardPointer<MemoryAllocator>::acquire(void *p, int thread_id)
  {
    return help(hp_list_, p, thread_id);
  }
  template<typename MemoryAllocator>
  int HplfdsHazardPointer<MemoryAllocator>::release(void *p, int thread_id)
  {
    HazardPointerNode *tmp = NULL;
    for (tmp = &hp_list_[thread_id].list;tmp != NULL; tmp = tmp->next) {
      if (tmp->p == p) {
        tmp->p = 0;
        hp_list_[thread_id].num--;
        return 0;
      }
    }
    return -1;
  }
  template<typename MemoryAllocator>
  int HplfdsHazardPointer<MemoryAllocator>::reclaim(int thread_id)
  {
    MY_ASSERT(retire_list_[thread_id].num >= 0);
    MY_ASSERT(retire_list_[thread_id].len >= 0);
    if (retire_list_[thread_id].num < NUM_TO_RECLAIM_P) {
      return 0;
    } else {
      bool can_be_freed = true;
      HazardPointerNode *my = NULL;
      HazardPointerNode *other = NULL;
      for (my = &retire_list_[thread_id].list;my != NULL; my = my->next) {
        void *candidate = my->p;
        if (candidate == NULL) {
          continue;
        }
        can_be_freed = true;
        for (int i = 0; i < MAX_THREAD_NUM && can_be_freed;i++) {
          if (i != thread_id) {
            for (other = &hp_list_[i].list;other != NULL; other = other->next) {
              if (other->p == candidate) {
                can_be_freed = false;
                break;
              }
            }
          }
        }
        if (can_be_freed) {
          MemoryAllocator::free(candidate);
          my->p = NULL;
          retire_list_[thread_id].num--;
        }
      }
    }
    return 0;
  }
  template<typename MemoryAllocator>
  int HplfdsHazardPointer<MemoryAllocator>::retire(void *p, int thread_id)
  {
    return help(retire_list_, p, thread_id);
  }
  template<typename MemoryAllocator>
  int HplfdsHazardPointer<MemoryAllocator>::help(HazardPointerList *list, void *p, int thread_id)
  {
    if (p == NULL) {
      return 0;
    }
    bool found = false;
    HazardPointerNode *tmp = NULL;
    for (tmp = &list[thread_id].list;tmp != NULL; tmp = tmp->next) {
      if (tmp->p == 0) {
        found = true;
        break;
      }
    }
    if (found) {
      tmp->p = p;
      list[thread_id].num++;
    } else {
      HazardPointerNode *new_hp = (HazardPointerNode*)MemoryAllocator::allocate(sizeof(HazardPointerNode));
      if (new_hp == NULL) {
        return -1;
      }
      new_hp->next = list[thread_id].list.next;
      list[thread_id].list.next = new_hp;
      new_hp->p = p;
      list[thread_id].len++;
      list[thread_id].num++;
    }
    return 0;
  }
}
#endif //HPLFDS_HAZARD_POINTER
