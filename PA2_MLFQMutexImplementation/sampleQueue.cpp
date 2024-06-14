#include <iostream>
#include <random>
#include <pthread.h>
#include <unistd.h>
#include <queue.h>
#include <sched.h>
using namespace std;

Queue<pthread_t> q;

void* enq(void* arg) { 
    pthread_t base = (pthread_t) arg;
    printf("Thread with base: %ld started.\n",base);
    for (int i = base; i < base+100; i++)
        q.enqueue(i);
    return NULL;

}


int main() {
    printf("Hello, from main.\n");
    pthread_t e1, e2;
    pthread_create(&e1, NULL, enq, (void*)0);
    pthread_create(&e2, NULL, enq, (void*)100);
    pthread_join(e1, NULL);
    pthread_join(e2, NULL);
    for (int i=0; i < 100; i++)
        int x = q.dequeue();
    printf("Threads terminated. Resulting queue state:\n");
    q.print();
    return 0;
}
