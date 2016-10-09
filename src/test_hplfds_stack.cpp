#include <iostream>
#include "common/hplfds_memory.h"
#include "stack/hplfds_stack.h"
#include <pthread.h>
#include <time.h>
using namespace std;
using namespace hplfds_sync;
HplfdsStack<int, HplfdsMemoryAllocator> stack;
#define NUM 10000000
void* f(void *arg)
{
  int id = *((int*)(arg));
  int *q = NULL;
  int ret = 0;
  int j = NUM * id;
  for (int i = 0; i < NUM; i++) {
    int *p = new int(j++);
    while(stack.push(p, id) != 0) {}
    while(1) {
      ret = stack.pop(q, id);
      if (ret == 0 || ret == ERROR_EMPTY) {
        break;
      }
    }
    //delete p;
  }
  return NULL;
}
void test()
{
  pthread_t t1;
  pthread_t t2;
  pthread_t t3;
  pthread_t t4;
  int id1 = 0;
  int id2 = 1;
  int id3 = 2;
  int id4 = 3;
  timespec ts1, ts2;
  clock_gettime(CLOCK_MONOTONIC, &ts1);
  pthread_create(&t1, NULL, f, &id1);
  pthread_create(&t2, NULL, f, &id2);
  pthread_create(&t3, NULL, f, &id3);
  pthread_create(&t4, NULL, f, &id4);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);
  pthread_join(t4, NULL);
  clock_gettime(CLOCK_MONOTONIC, &ts2);
  int64_t takes = (ts2.tv_sec - ts1.tv_sec) *1000000000LL + (ts2.tv_nsec - ts1.tv_nsec);
  cout<<"=====>"<<takes<<endl;
}
int main()
{
  srand(time(NULL));
  test();
  return 0;
}
