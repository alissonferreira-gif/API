#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <future>
#include <stdexcept>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t n_threads = 0) {
        if (n_threads == 0)
            n_threads = std::max(2u, std::thread::hardware_concurrency());

        workers_.reserve(n_threads);
        for (size_t i = 0; i < n_threads; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using RetType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<RetType> fut = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool parado");
            tasks_.emplace([task]{ (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    [[nodiscard]] size_t thread_count()    const noexcept { return workers_.size(); }
    [[nodiscard]] size_t pending_tasks()   const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]{ return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex                mutex_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};
