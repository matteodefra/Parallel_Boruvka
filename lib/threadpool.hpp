#pragma once

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool final {

    public:

        /**
         * @brief Construct a new Thread Pool object
         * 
         * @param nthreads number of threads to start in the pool
         * 
         * It enables all the threads to execute, and then run them setting the CPU affinity
         */
        explicit ThreadPool(std::size_t nthreads) :
            nthreads(nthreads),
            m_mu(nthreads),
            m_cv(nthreads),
            m_enabled(nthreads),
            m_tasks(nthreads),
            m_pool(nthreads) {
                for (uint i = 0; i < nthreads; i++) 
                    m_enabled[i] = true;
                for (uint i = 0; i < nthreads; i++) {
                    run(i);
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    CPU_SET(i, &cpuset);
                    int rc = pthread_setaffinity_np(m_pool[i].native_handle(), sizeof(cpu_set_t), &cpuset);
                }
    
        }
        
        ~ThreadPool() {
            stop();
        }

        ThreadPool(ThreadPool const&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /**
         * @brief Enqueue a task into the specific thread queue
         * 
         * @tparam TaskT 
         * @param task the task to compute, a pointer to a function
         * @param index the index of the corresponding threads
         * @return std::future<decltype(task())> 
         * 
         * It enqueue a given task into the queue of the thread using a lock_guard, then notify the corresponding queue and 
         * return a generic pointer to the result function. It allows for arbitrary return types
         */
        template<class TaskT>
        auto enqueue(TaskT task, int index) -> std::future<decltype(task())>
        {
            /**
             * Infer the return type of the task function. Get a reference to the future value returned by the function
             * 
             */
            using ReturnT = decltype(task());
            auto promise = std::make_shared<std::promise<ReturnT>>();
            auto result = promise->get_future();

            // Create the task using the return type and the given function, then execute it
            auto t = [p = std::move(promise), t = std::move(task)] () mutable { execute(*p, t); };

            // Enqueue the given task into the corresponding thread queue and notify the thread
            {
                std::lock_guard<std::mutex> lock(m_mu[index]);
                m_tasks[index].push(std::move(t));
            }

            m_cv[index].notify_one();

            return result;
        }

    private:

        uint nthreads;
        std::vector<std::mutex> m_mu;
        std::vector<std::condition_variable> m_cv;

        std::vector<bool> m_enabled;
        std::vector<std::thread> m_pool;
        std::vector<std::queue<std::function<void()>>> m_tasks;

        /**
         * @brief Set the given task for the thread
         * 
         * @tparam ResultT return type of the task
         * @tparam TaskT function type to be executed
         * @param p the resulting object
         * @param task the function to execute
         */
        template<class ResultT, class TaskT>
        static void execute(std::promise<ResultT>& p, TaskT& task) {
            p.set_value(task()); 
        }

        /**
         * @brief Set the given task for the thread
         * 
         * @tparam TaskT 
         * @param p Return type of task (void)
         * @param task Function to be executed
         * 
         * Using ReturnT template does not work with void return type
         */
        template<class TaskT>
        static void execute(std::promise<void>& p, TaskT& task) {
            task();
            p.set_value();
        }

        /**
         * @brief Stop the threadpool joining all threads
         * 
         */
        void stop() {

            for (uint i=0; i<nthreads; i++) {
                {
                    std::lock_guard<std::mutex> lock(m_mu[i]);
                    m_enabled[i] = false;
                }

                m_cv[i].notify_all();

                m_pool[i].join();
            }
                
        }

        /**
         * @brief Execute the thread function
         * 
         * @param index the index of the corresponding thread
         * 
         * Continuous loop where thread awaits for task to be inserted into his queue. When a task is extracted, the 
         * correspoding function is executed
         */
        void run(int index) {
            auto f = [this] (int index) {
                while (true) {
                    std::unique_lock<std::mutex> lock{ m_mu[index] };
                    m_cv[index].wait(lock, [&] () { return !m_enabled[index] || !m_tasks[index].empty(); });

                    if (!m_enabled[index])
                        break;

                    assert(!m_tasks[index].empty());

                    auto task = std::move(m_tasks[index].front());
                    m_tasks[index].pop();

                    lock.unlock();
                    task();
                }
            };

            m_pool[index] = std::thread(f, index);

        }
};