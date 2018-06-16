#pragma once
#include <atomic>
#include <vector>
#include <mutex>
#include <deque>
#include <future>
#include <functional>
#include <thread>

class ThreadPool
{
private:
    class function : public std::function<void()>
    {
        template<typename F>
        class FakeCopyableWrapper
        {
        public:
            FakeCopyableWrapper(F&& f) : f_(std::move(f)) {}
            FakeCopyableWrapper(const FakeCopyableWrapper& o)
                : f_(nullptr)
            {
                throw -1;
            }
            FakeCopyableWrapper& operator=(const FakeCopyableWrapper& o)
            {
                throw -1;
                return *this;
            }
            FakeCopyableWrapper(FakeCopyableWrapper&& o) = default;
            FakeCopyableWrapper& operator=(FakeCopyableWrapper&& o) = default;
            ~FakeCopyableWrapper() = default;

            void operator() ()
            {
                f_();
            }

            // template<class... Args>
            // auto operator() (Args&&... args) -> decltype(f_(std::forward<Args>(args)...))
            // {
            //     return f_(std::forward<Args>(args)...);
            // }

        private:
            F f_;
        };
    public:
        template<typename F>
        function(F f) : std::function<void()>(FakeCopyableWrapper<F>(std::move(f))) {}
        using std::function<void()>::operator();
        using std::function<void()>::operator bool;
        // operator bool() const { return std::function<void()>::operator bool(); }

        function() : std::function<void()>() {}

        function(function&& o) = default;
        function& operator=(function&& o) = default;


        function(const function& o) = delete;
        function& operator=(const function& o) = delete;
    };
public:
    ThreadPool(int cnt)
    {
        for (int i = 0; i < cnt; i++) {
            threads_.push_back(std::move(std::thread(std::bind(&ThreadPool::work, this))));
        }
    }

    template<typename F>
    auto addTask(F&& f) -> std::future<decltype(f())>
    {
        std::packaged_task<decltype(f())(void)> p(std::forward<F>(f));
        auto future = p.get_future();

        auto runnable = std::bind(&ThreadPool::execRunnable<std::packaged_task<decltype(f())(void)>&>, std::move(p));
        function wrapper(std::move(runnable));

        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push_back(std::move(wrapper));
        }
        return future;
    }
    void stop()
    {
        stop_ = true;
    }

    ~ThreadPool()
    {
        stop();
        for(auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
private:
    template<typename T>
    static void execRunnable(T f)
    {
        f();
    }

    void work()
    {
        while(!stop_) {
            function func;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (!tasks_.empty()) {
                    func = std::move(tasks_.front());
                    tasks_.pop_front();
                }
            }
            if (func)
                func();
        }
    }
private:
    std::mutex mutex_;
    std::deque<function> tasks_;
    std::atomic<bool> stop_{false};
    std::vector<std::thread> threads_;
};
