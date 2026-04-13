#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <iostream>
#include "StringList.hpp"

// Forward declaration
class IndexStore;

class KillSwitch {
    std::promise<void> prom;
    std::shared_future<void> fut;
    std::mutex mtx;
    bool signaled = false;

public:
    KillSwitch() : fut(prom.get_future().share()) {}

    bool KillEvent() const {
        return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    void SignalKill() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!signaled) {
            prom.set_value();
            signaled = true;
        }
    }
};

class WorkQueue {
    StringList items;
    std::mutex mtx;
    std::condition_variable cv;
    bool doneFeeding = false;

public:
    void push(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        items.add(path);
        cv.notify_one();
    }

    void setDone() {
        std::lock_guard<std::mutex> lock(mtx);
        doneFeeding = true;
        cv.notify_all();
    }
    
    void notifyAll() {
        std::lock_guard<std::mutex> lock(mtx);
        cv.notify_all();
    }

    bool pop_wait(std::string& out, const KillSwitch& kill) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]{ 
            return kill.KillEvent() || items.size() > 0 || doneFeeding; 
        });

        if (kill.KillEvent()) return false;

        if (items.size() == 0 && doneFeeding) return false;

        if (items.size() > 0) {
            out = items.pop_back();
            return true;
        }
        
        return false;
    }
};

typedef void (*ProcessFileFunc)(const std::string&, const std::string&, IndexStore*, long&);

class Reader {
    std::thread th;
    WorkQueue& queue;
    IndexStore* store;
    const KillSwitch& kill;
    std::string rootPath;
    ProcessFileFunc processFunc;
    
public:
    long bytesReadLocal = 0;

    Reader(WorkQueue& q, IndexStore* s, const KillSwitch& k, const std::string& root, ProcessFileFunc func) 
        : queue(q), store(s), kill(k), rootPath(root), processFunc(func) {
        std::cout << "Reader thread created" << std::endl;
        th = std::thread(&Reader::run, this);
    }

    ~Reader() {
        if (th.joinable()) th.join();
        std::cout << "Reader thread destroyed" << std::endl;
    }

    void join() {
        if (th.joinable()) th.join();
    }

    void run() {
        std::string path;
        while (!kill.KillEvent()) {
            if (!queue.pop_wait(path, kill)) {
                break;
            }
            processFunc(path, rootPath, store, bytesReadLocal);
        }
    }
};

#endif
