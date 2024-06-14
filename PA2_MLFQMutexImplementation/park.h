#ifndef GARAGE_H
#define GARAGE_H

#include <iostream>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <thread>

using namespace std;

class Garage {
private:
    unordered_map<pthread_t, atomic<bool>> flag_map;

public:
    Garage() = default;
    ~Garage() = default;

    void setPark() {
        pthread_t current_id = pthread_self();
        atomic<bool>& flag = flag_map[current_id];
        flag = false;
    }

    void park() {
        pthread_t current_id = pthread_self();
        atomic<bool>& flag = flag_map[current_id];

        flag.wait(false);        
    }

    void unpark(pthread_t id) {
        auto it = flag_map.find(id);
        if (it != flag_map.end()) {
            it->second.store(true);
            it->second.notify_one();
            
        }
    }
};

#endif
