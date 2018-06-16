#include <iostream>
#include <vector>
#include <chrono>
#include "include/thread_pool.h"

int calc(int n)
{
    std::cout<< "--- thread id: " << std::this_thread::get_id() << " param: " << n << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(n));
    std::cout<< "### thread id: " << std::this_thread::get_id() << " param: " << n << std::endl;
    return n;
}

int main()
{
    ThreadPool pool(2);

    std::vector<std::future<int>> v;
    for (int i = 0; i < 6; i++) {
        auto r = pool.addTask(std::bind(&calc, i));
        v.push_back(std::move(r));
    }
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    while(!v.empty()) {
        for (auto iter = v.begin(); iter != v.end();) {
            if (iter->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                int res = iter->get();
                iter = v.erase(iter);
                std::cout<< "res --> " << res << std::endl;
            } else {
                iter++;
            }
        }
    }

    return 0;
}
