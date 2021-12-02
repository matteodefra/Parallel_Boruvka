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

        explicit ThreadPool(std::size_t nthreads) :
            nthreads(nthreads),
            m_mu(nthreads),
            m_cv(nthreads),
            m_enabled(nthreads),
            m_tasks(nthreads),
            m_pool(nthreads) {
                for (uint i = 0; i < nthreads; i++) 
                    m_enabled[i] = true;
                for (uint i = 0; i < nthreads; i++) 
                    run(i);
        }

        ~ThreadPool() {
            stop();
        }

        ThreadPool(ThreadPool const&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        template<class TaskT>
        auto enqueue(TaskT task, int index) -> std::future<decltype(task())>
        {
            using ReturnT = decltype(task());
            auto promise = std::make_shared<std::promise<ReturnT>>();
            auto result = promise->get_future();

            auto t = [p = std::move(promise), t = std::move(task)] () mutable { execute(*p, t); };

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

        template<class ResultT, class TaskT>
        static void execute(std::promise<ResultT>& p, TaskT& task) {
            p.set_value(task()); // syntax doesn't work with void ResultT :(
        }

        template<class TaskT>
        static void execute(std::promise<void>& p, TaskT& task) {
            task();
            p.set_value();
        }

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