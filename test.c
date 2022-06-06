#include <stdio.h>
#include <pthread.h>
#include "green.h"

int flag = 0;
green_cond_t cond;
green_mutex_t mutex;
int temperar = 0;
pthread_cond_t pcond;
pthread_mutex_t pmutex;

void *test1(void *arg) {
  int i = *(int*)arg;
  int loop = 4;
  while(loop > 0) {
    printf("thread %d: %d\n", i, loop);
    loop--;
    green_yield();
  }
}

void *test2(void *arg) {
  int id = *(int*)arg;
  int loop = 4;
  while(loop > 0) {
    if(flag == id) {
      printf("thread %d: %d\n", id, loop);
      loop--;
      flag = (id + 1) % 2;
      green_cond_signal(&cond);
    } else {
      green_cond_wait(&cond, &mutex);
    }
  }
}

/* Test 3, tries to test section 4 in task description. */
void *test3(void *arg) {

}

/* Test 4, tries to test section 5 in task description. */
void *test4(void *arg) {
  int i = *(int*)arg;
  int loop = 4;
  while(loop > 0) {
    printf("thread %d: %d, %d\n", i, loop, temperar);
    loop--;
    for(int i = 0; i < 1000000; i++) {
      int s = green_mutex_lock(&mutex);
      int temp = temperar;

      int temp2 = 0;
      if(temp > 1337) temp2++;

      temp++;
      temperar = temp;
      s = green_mutex_unlock(&mutex);

    }
  }
}

/* Test 5, tries to test section 6 part 1 in task description */
void *test5(void *arg) {
  int loop = 4;
  int id = *(int*)arg;
  while(loop > 0) {
    printf("thread %d: %d\n", id, loop);
    green_mutex_lock(&mutex);
    while(flag != id){
      green_mutex_unlock(&mutex);
      green_cond_wait(&cond, &mutex);
    }
    flag = (id + 1) % 2;
    green_cond_signal(&cond); //ger deadlock.
    green_mutex_unlock(&mutex);
    loop--;
  }
}

void* testFunc(void* arg) {
  int i = *(int*)arg;
  if(i % 2 == 0) {
    for()
  } else {

  }
}

void greenTest(int numberOfThreads, int* args) {
  green_t threadsArray[numberOfThreads];

  for(int i = 0; i < numberOfThreads; i++) {
    green_create(&threads[i], testFunc, &args[i]);
  }

  for(int i = 0; i < numberOfThreads; i++) {
    green_join(&threads[i]);
  }
}

void pthreadTest(int numberOfThreads, int* args) {
  pthread_t threadsArray[numberOfThreads];
  //mera
}

int main() {
  clock_t c_start, c_stop;
  green_cond_init(&cond);
  green_mutex_init(&mutex);

  pthread_cond_init(&pcond, NULL);
  pthread_mutex_init(&pmutex, NULL);

  double timeGreen = 0, timePthread = 0;
  int numberOfThreads = 2;

  int args[numberOfThreads];
  for(int i = 0; i < numberOfThreads; i++) {
    args[i] = i;
  }


  c_start = clock();
  greenTest(numberOfThreads, args);
  c_stop = clock();

  timeGreen = ((double)(c_stop - c_start)) / ((double)CLOCKS_PER_SEC/1000);

  c_start = clock();
  pthreadTest(numberOfThreads, args);
  c_stop = clock();

  timePthread = ((double)(c_stop - c_start)) / ((double)CLOCKS_PER_SEC/1000);

  printf("%f\t%f \n", timeGreen, timePthread);

/*  green_t g0, g1;
  int a0 = 0;
  int a1 = 1;

  green_create(&g0, test5, &a0);
  green_create(&g1, test5, &a1);

  green_join(&g0, NULL);
  green_join(&g1, NULL);
  printf("done\n"); */
  return 0;
}
