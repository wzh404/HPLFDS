#ifndef HPLFDS_STACK
#define HPLFDS_STACK
#include "../hazardpointer/hplfds_hazard_pointer.h"
namespace hplfds_sync
{
  template<class T, class MemoryAllocator>
  class HplfdsStack
  {
  private:
    struct StackCell
    {
      T *p;
      StackCell *next;
    };
    enum StackOp
    {
      PUSH = 0,
      POP = 1
    };
    struct ThreadInfo
    {
      int thread_id;
      StackOp op;
      StackCell *cell;
      int64_t spin;
      static const int64_t MAX_LOOP = 102400000000LL;
      static const int64_t INIT_LOOP = 100000LL;
    }CACHE_ALIGNED;
    struct CollisionInfo
    {
      volatile int thread_id;
    }CACHE_ALIGNED;
  public:
    HplfdsStack();
    int push(T *p, int thread_id);
    int pop(T *&p, int thread_id);
    bool empty(int thread_id);
  private:
    INLINE int try_stack_push(StackCell *cell, int thread_id);
    INLINE int try_stack_pop(T *&p, int thread_id);
    INLINE int try_les_op(ThreadInfo *thread_info);
    INLINE int try_collision(ThreadInfo *p, ThreadInfo *q, int me, int him);
    INLINE int finish_collision(ThreadInfo *p, int me);
    void spin(int64_t &delay);
  private:
    StackCell *volatile top_;
    StackCell *dummy_node_;
    HplfdsHazardPointer<MemoryAllocator> memory_manager_;
    CollisionInfo collision_[MAX_THREAD_NUM];
    ThreadInfo *volatile location_[MAX_THREAD_NUM];
  };
  template<class T, class MemoryAllocator>
  HplfdsStack<T, MemoryAllocator>::HplfdsStack()
  {
    dummy_node_ = (StackCell*)(MemoryAllocator::allocate(sizeof(StackCell)));
    MY_ASSERT(dummy_node_ != NULL);
    dummy_node_->next = NULL;
    dummy_node_->p = NULL;
    top_ = dummy_node_;
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
      collision_[i].thread_id = -1;
      location_[i] = NULL;
    }
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::push(T *p, int thread_id)
  {
    if (UNLIKELY(thread_id < 0 || thread_id >= MAX_THREAD_NUM)) {
      return ERROR_INVALID_ARGUMENT;
    }
    StackCell *cell = (StackCell*)(MemoryAllocator::allocate(sizeof(StackCell)));
    if (UNLIKELY(cell == NULL)) {
      return ERROR_NO_MEMORY;
    }
    cell->p = p;
    int ret = 0;
    ThreadInfo *thread_info = NULL;
    while (true) {
      ret = try_stack_push(cell, thread_id);
      if (ret == SUCCESS) {
        if (thread_info != NULL) {
          memory_manager_.retire(thread_info, thread_id);
          //can NOT && should NOT retire cell here
          //since it has been pushed to the stack!!!
        }
        return ret;
      } else if (ret == ERROR_AGAIN) {
        if (UNLIKELY(thread_info == NULL)) {
          thread_info = (ThreadInfo*)(MemoryAllocator::allocate(sizeof(ThreadInfo)));
          if (UNLIKELY(thread_info == NULL)) {
            return ERROR_NO_MEMORY;
          }
          thread_info->thread_id = thread_id;
          thread_info->op = PUSH;
          thread_info->cell = cell;
          thread_info->spin = ThreadInfo::INIT_LOOP;
        }
        ret = try_les_op(thread_info);
        if (ret == SUCCESS) {
          return ret;
          //do not worry. thread_info and cell will be reclaimed in another thread
          //which performed a push op with me
        }
      }
    }
    return ERROR_UNEXPECTED;
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::pop(T *&p, int thread_id)
  {
    int ret = ERROR_AGAIN;
    p = NULL;
    if (UNLIKELY(thread_id < 0 || thread_id >= MAX_THREAD_NUM)) {
      return ERROR_INVALID_ARGUMENT;
    }
    ThreadInfo *thread_info = NULL;
    while(true) {
      ret = try_stack_pop(p, thread_id);
      if (ret == SUCCESS || ret == ERROR_EMPTY) {
        if (thread_info != NULL) {
          memory_manager_.retire(thread_info, thread_id);
          //no need to retire thread_info->cell here
          //since it is NULL.
        }
        return ret;
      } else if (ret == ERROR_AGAIN) {
        if (UNLIKELY(thread_info == NULL)) {
          thread_info = (ThreadInfo*)(MemoryAllocator::allocate(sizeof(ThreadInfo)));
          if (UNLIKELY(thread_info == NULL)) {
            return ERROR_NO_MEMORY;
          }
          thread_info->thread_id = thread_id;
          thread_info->op = POP;
          thread_info->cell = NULL;
          thread_info->spin = ThreadInfo::INIT_LOOP;
        }
        ret = try_les_op(thread_info);
        if (ret == SUCCESS) {
          MY_ASSERT(thread_info->cell != NULL);
          p = thread_info->cell->p;
          MY_ASSERT(p != NULL);
          memory_manager_.retire(thread_info, thread_id);
          memory_manager_.retire(thread_info->cell, thread_id);
          return ret;
        }
      }
    }
    return ret;
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::try_stack_push(StackCell *cell, int thread_id)
  {
    int ret = ERROR_AGAIN;
    StackCell *tmp = top_;
    memory_manager_.acquire(tmp, thread_id);
    if (tmp == top_) {
      cell->next = tmp;
      if (CAS(&top_, tmp, cell)) {
        ret = SUCCESS;
      }
    }
    memory_manager_.release(tmp, thread_id);
    return ret;
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::try_stack_pop(T *&p, int thread_id)
  {
    StackCell *tmp = top_;
    p = NULL;
    memory_manager_.acquire(tmp, thread_id);
    if (tmp == top_) {
      if (tmp != dummy_node_) {
        StackCell *q = tmp->next;
        if (CAS(&top_, tmp, q)) { //pop successfully
          p = tmp->p;
          memory_manager_.release(tmp, thread_id);
          memory_manager_.retire(tmp, thread_id);
          memory_manager_.reclaim(thread_id);
          return SUCCESS;
        } else { //pop failed. someone had changed the top_
          memory_manager_.release(tmp, thread_id);
          return ERROR_AGAIN;
        }
      } else { //empty stack
        memory_manager_.release(tmp, thread_id);
        return ERROR_EMPTY;
      }
    } else {//top_ node had been reclaimed
      memory_manager_.release(tmp, thread_id);
      return ERROR_AGAIN;
    }
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::try_les_op(ThreadInfo *p)
  {
    int me = p->thread_id;
    MY_ASSERT(me >= 0 && me < MAX_THREAD_NUM);
    location_[me] = p;
    uint64_t pos = ((uint64_t)rand()) % MAX_THREAD_NUM;
    //uint64_t pos = ((uint64_t)p >>3) % MAX_THREAD_NUM;
    MY_ASSERT(pos >=0 && pos < MAX_THREAD_NUM);
    int him = collision_[pos].thread_id;
    while (!CAS(&collision_[pos].thread_id, him, me)) {
      him = collision_[pos].thread_id;
    }
    if (him != -1) {
      int ret = SUCCESS;
      ThreadInfo *q = location_[him];
      memory_manager_.acquire(q, me);
      if (q != location_[him]) {
        //do nothing
      } else if (q != NULL && q->thread_id == him && q->op != p->op) {
        if (CAS(&location_[me], p, NULL)) {
          ret = try_collision(p, q, me, him);
        } else {
          ret = finish_collision(p, me);
          memory_manager_.release(q, me);
        }
        return ret;
      }
      memory_manager_.release(q, me);
    }//end if
    spin(p->spin);
    if (!CAS(&location_[me], p, NULL)) {
      finish_collision(p, me);
      return SUCCESS;
    }
    return ERROR_AGAIN;
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::try_collision(ThreadInfo *p, ThreadInfo *q, int me, int him)
  {
    int ret = SUCCESS;
    if (p->op == PUSH) {
      if (!CAS(&location_[him], q, p)) {
        ret  = ERROR_AGAIN;
      }
      memory_manager_.release(q, me);
    } else if (p->op == POP) {
      if (CAS(&location_[him], q, NULL)) {
        p->cell = q->cell;//safe !
        location_[me] = NULL;
        memory_manager_.release(q, me);
        memory_manager_.retire(q, me);
      } else {
        ret = ERROR_AGAIN;
        memory_manager_.release(q, me);
      }
    }
    return ret;
  }
  template<class T, class MemoryAllocator>
  int HplfdsStack<T, MemoryAllocator>::finish_collision(ThreadInfo *p, int me)
  {
    if (p->op == POP) {
      ThreadInfo *tmp = location_[me];
      p->cell = location_[me]->cell;
      location_[me] = NULL;
      memory_manager_.retire(tmp, me);
    }
    return SUCCESS;
  }
  template<class T, class MemoryAllocator>
  bool HplfdsStack<T, MemoryAllocator>::empty(int thread_id)
  {
    while(true) {
      StackCell *tmp = top_;
      memory_manager_.acquire(tmp, thread_id);
      if (tmp == top_) {
        bool ret = (tmp == dummy_node_);
        memory_manager_.release(tmp, thread_id);
        return ret;
      }
      memory_manager_.release(tmp, thread_id);
    }
  }
  template<class T, class MemoryAllocator>
  void HplfdsStack<T, MemoryAllocator>::spin(int64_t &delay)
  {
    if (delay <= 0) {
      delay = ThreadInfo::INIT_LOOP;
    }
    for (int64_t i = 0; i < delay; i++) {
      CPU_RELAX();
    }
    int64_t new_delay = delay << 1LL;
    if (new_delay <= 0 || new_delay >= ThreadInfo::MAX_LOOP) {
      new_delay = ThreadInfo::INIT_LOOP;
    }
    delay = new_delay;
  }
}
#endif //HPLFDS_STACK
