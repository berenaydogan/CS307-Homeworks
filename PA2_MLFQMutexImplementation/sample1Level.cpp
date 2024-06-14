#include <iostream>
#include <string>
#include <pthread.h>
#include <chrono>
#include "MLFQmutex.h"
#include <vector>
#include <random>
#include <stdio.h>
#include <unistd.h>
using namespace std;


MLFQMutex _lock(1, 0.01); // number of levels in MLFQ and time span for each level

void* worker(void* args) { 
    long id = (long) args;
    //cout << "Thread with program ID: "<<id<<" and thread ID: "<< pthread_self()<<" started."<<endl;
    double sleep_time = (id+1)* 0.02;
    for (int i = 0; i < 3; i++) {
        _lock.lock();
        printf("Thread with program ID %ld and thread ID %ld acquired lock\n",id,pthread_self());
        
        sleep(sleep_time);
        printf("Thread with program ID %ld and thread ID %ld releasing lock\n",id,pthread_self());
        _lock.unlock();
    }
    return NULL;
}


int main() {

    // replace thread with pthread
    vector<pthread_t> threads;
    chrono::high_resolution_clock::time_point begin = chrono::high_resolution_clock::now();

    for (long i = 0; i < 4; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, worker, (void*)i);
        threads.push_back(thread);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
    double duration = chrono::duration_cast<chrono::duration<double>>(end - begin).count();
    cout<<"Threads terminated. Total duration is: "<< duration<<" seconds."<<endl;
    return 0;
}
