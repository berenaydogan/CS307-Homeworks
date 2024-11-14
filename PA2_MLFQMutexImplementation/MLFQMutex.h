#include <iostream>
#include <random>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "park.h"
#include <sched.h>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <cmath> 
#include <fstream>     
#include <map>            
#include <list>             
#include <ctime>  

using namespace std;

class MLFQMutex {

    private:
        unordered_map<pthread_t, int> threads; // Maps threads to their current priority levels

        atomic_flag flag; // Atomic lock flag to control mutex access
        atomic_flag guard; // Atomic guard flag to protect lock() and unlock() bodies
        vector<Queue<pthread_t>*> levels; // Vector of queues, each queue represents a priority level

        int noOfPriorityLevels; // Total number of priority levels
        double Qval; // Quantum value used in priority calculation

        chrono::time_point<std::chrono::high_resolution_clock> start; // Holds the time when the mutex is locked
        chrono::time_point<std::chrono::high_resolution_clock> stop; // Holds the time when the mutex is unlocked

        Garage garObj; // Object for managing thread parking 

        // Update priority based on mutex hold time
        void updatePriorityLevel(pthread_t tid, chrono::seconds duration) {
            double execTime = duration.count();  // Get execution time in seconds
            int currentLevel = threads[tid];  // Get current priority level of the thread
            int newPriorityLevel = currentLevel + static_cast<int>(floor(execTime / Qval));  // Calculate new priority level based on execution time

            // Ensure the new priority level does not exceed the maximum available level
            if (newPriorityLevel >= noOfPriorityLevels) {
                newPriorityLevel = noOfPriorityLevels - 1;
            }

            threads[tid] = newPriorityLevel; 
        }

        // Enqueue thread to its priority level
        void enqueueThread() {
            pthread_t currentThread = pthread_self();

            // Enqueue the thread ID provided by 'tid' to the queue corresponding to its level
            levels[threads[currentThread]]->enqueue(currentThread);
        }

        // Return the thread ID of the highest priority thread that can be run next
        pthread_t highestPriorityThread() {
            pthread_t next_thread = -1; // Initialize to -1, indicating no thread is available

            // Loop through priority levels from highest to lowest to find a runnable thread
            for(int i = 0; i < noOfPriorityLevels; i++) {
                if(!levels[i]->isEmpty()) {
                    next_thread = levels[i]->dequeue();
                    break;
                }
            }

            return next_thread;
        }

        
    public:
        MLFQMutex(int givenNoOfPriorityLevels, double givenQval)
            : noOfPriorityLevels(givenNoOfPriorityLevels), Qval(givenQval) {
            for (int i = 0; i < givenNoOfPriorityLevels; i++) {
                levels.push_back(new Queue<pthread_t>()); // Initialize queues for each priority level
            }

            // Initialize flags
            flag.clear(std::memory_order_release);
            guard.clear(std::memory_order_release);
        }

        ~MLFQMutex() {
            for (Queue<pthread_t>* level : levels) {
                delete level;  // Clean up memory by deleting each queue
        }
}


        
       void lock() {
            // Spin until the guard is successfully acquired
            while (guard.test_and_set(std::memory_order_acquire));

            if (!flag.test_and_set(std::memory_order_acquire)) {
                // If mutex is free, acquire it and start timing
                guard.clear(std::memory_order_release);
            }
             
            else {

                garObj.setPark(); // Prepare the thread to park
                
                printf("Adding thread with ID: %lu to level %d\n", (unsigned long)pthread_self(), threads[pthread_self()]);
                fflush(stdout);
                
                // Release guard and park the thread
                guard.clear(std::memory_order_release);
                garObj.park();
            }

            start = std::chrono::high_resolution_clock::now(); // Record the time when the mutex is acquired
           
        }

        void unlock() {

            while (guard.test_and_set(memory_order_acquire));

            stop = chrono::high_resolution_clock::now(); // Record the time when the mutex is released

            if (threads.find(pthread_self()) != threads.end()) {
                auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
                updatePriorityLevel(pthread_self(), duration); // Adjust priority based on the hold time
            }

            pthread_t highestPriorityThreadId = highestPriorityThread(); // Get the highest priority thread ready to run
            
            guard.clear(memory_order_release); // Release the guard
        }

        // Print the queue of each level
        void print() {
            printf("Waiting threads:");
            for(int i = 0; i < noOfPriorityLevels + 1; i++) {
                printf("\nLevel %d:",i);
                fflush(stdout);
                levels[i]->print();
            }
            cout << endl;
        }
};
