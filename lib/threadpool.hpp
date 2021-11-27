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
            m_enabled(true),
            m_pool(nthreads) {
            run();
        }

        ~ThreadPool() {
            stop();
        }

        ThreadPool(ThreadPool const&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        template<class TaskT>
        auto enqueue(TaskT task) -> std::future<decltype(task())>
        {
            using ReturnT = decltype(task());
            auto promise = std::make_shared<std::promise<ReturnT>>();
            auto result = promise->get_future();

            auto t = [p = std::move(promise), t = std::move(task)] () mutable { execute(*p, t); };

            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_tasks.push(std::move(t));
            }

            m_cv.notify_one();

            return result;
        }

    private:

        std::mutex m_mu;
        std::condition_variable m_cv;

        bool m_enabled;
        std::vector<std::thread> m_pool;
        std::queue<std::function<void()>> m_tasks;

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
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_enabled = false;
            }

            m_cv.notify_all();

            for (auto& t : m_pool)
                t.join();
        }

        void run() {
            auto f = [this] () {
                while (true) {
                    std::unique_lock<std::mutex> lock{ m_mu };
                    m_cv.wait(lock, [&] () { return !m_enabled || !m_tasks.empty(); });

                    if (!m_enabled)
                        break;

                    assert(!m_tasks.empty());

                    auto task = std::move(m_tasks.front());
                    m_tasks.pop();

                    lock.unlock();
                    task();
                }
            };

            for (auto& t : m_pool)
                t = std::thread(f);
        }
};