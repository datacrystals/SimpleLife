#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) 
        : stop(false), activeTasks(0) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        // Safely wait for a task to be pushed into the queue
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        
                        if (this->stop && this->tasks.empty()) return;
                        
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    
                    // Execute the task outside the lock
                    task();
                    
                    // Notify waitAll() if this was the last active task
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        --activeTasks;
                        if (activeTasks == 0 && tasks.empty()) {
                            waitCondition.notify_all();
                        }
                    }
                }
            });
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
            activeTasks++;
        }
        condition.notify_one();
    }

    // Blocks the calling thread until the queue is empty and all workers are idle
    void waitAll() {
        std::unique_lock<std::mutex> lock(queueMutex);
        waitCondition.wait(lock, [this]{ return tasks.empty() && activeTasks == 0; });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable waitCondition;
    bool stop;
    int activeTasks;
};