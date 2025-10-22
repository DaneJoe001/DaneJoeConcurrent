#include <iostream>
#include <optional>
#include <string>
#include "danejoe/concurrent/blocking/mpmc_bounded_queue.hpp"
#include "demo_concurrent.hpp"

using DaneJoe::Concurrent::Blocking::MpmcBoundedQueue;

namespace demo {

void run_concurrent_demo()
{
    std::cout << "Concurrent demo:\n";

    MpmcBoundedQueue<std::string> q(4);
    q.push(std::string{"hello"});
    q.push(std::string{"world"});

    auto a = q.try_pop();
    auto b = q.try_pop();

    std::cout << "  popped: "
              << (a.has_value() ? *a : std::string{"<none>"})
              << ", "
              << (b.has_value() ? *b : std::string{"<none>"})
              << "\n";
}

} // namespace demo
