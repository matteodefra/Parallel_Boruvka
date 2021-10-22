#include <iostream>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <chrono>
#include <cstddef>
#include <math.h>
#include <string>



template <typename T>

class MyQueue {

    private:
    
        // Mutex for queue
        std::mutex              d_mutex;

        // Condition variable to update
        std::condition_variable d_condition;

        // Queue object: deque for low level container
        std::deque<T>           d_queue;
    
    public:

        MyQueue() {}
        
        ~MyQueue() {}

        /**
         * @brief Add a type T value into the queue
         * 
         * @param value of type T to push in the queue 
         */
        void push(T const& value) {

            {
                // Take the lock to add value
                std::unique_lock<std::mutex> lock(this->d_mutex);
                // Push the newly minimum edge found in the queue
                d_queue.push_front(value);
            }
            // Notify reduce workers
            this->d_condition.notify_one();
        }
        
        /**
         * @brief Remove a value from the queue
         * 
         * @return value of type T removed from the queue 
         */
        T pop() {

            // Ensure that lock is taken
            std::unique_lock<std::mutex> lock(this->d_mutex);
            // Wait until the queue is nonempty
            this->d_condition.wait(lock, [=]{ return !this->d_queue.empty(); });
            // Move in the queue on the last element
            T rc(std::move(this->d_queue.back()));
            // Pop the last element
            this->d_queue.pop_back();
            return rc;
        
        }

};

#define EOS NULL