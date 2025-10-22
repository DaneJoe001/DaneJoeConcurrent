#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cassert>
#include <algorithm>
#include <numeric>

using namespace std::literals::chrono_literals;

#include "danejoe/concurrent/blocking/mpmc_bounded_queue.hpp"
#include "demo_concurrent.hpp"

using DaneJoe::Concurrent::Blocking::MpmcBoundedQueue;

static void test_push_try_pop_single_thread()
{
    MpmcBoundedQueue<int> q;
    assert(q.empty());
    assert(q.size() == 0);

    for (int i = 0; i < 5; ++i)
    {
        bool ok = q.push(i);
        assert(ok);
    }
    assert(!q.empty());
    assert(q.size() == 5);

    for (int i = 0; i < 5; ++i)
    {
        auto v = q.try_pop();
        assert(v.has_value());
        assert(*v == i);
    }
    assert(q.empty());
}

static void test_blocking_pop_wakes_on_push()
{
    MpmcBoundedQueue<int> q;
    std::atomic<bool> started{ false };
    std::atomic<bool> got{ false };

    std::thread t([&]() {
        started.store(true);
        auto v = q.pop();
        if (v.has_value() && *v == 42)
        {
            got.store(true);
        }
        });

    while (!started.load())
    {
        std::this_thread::sleep_for(1ms);
    }

    std::this_thread::sleep_for(10ms);
    bool ok = q.push(42);
    assert(ok);

    t.join();
    assert(got.load());
}

static void test_pop_for_timeout()
{
    MpmcBoundedQueue<int> q;
    auto v = q.pop_for<std::chrono::milliseconds>(20ms);
    assert(!v.has_value());
}

static void test_close_wakes_and_stops_push()
{
    MpmcBoundedQueue<int> q;
    std::atomic<bool> finished{ false };

    std::thread t([&]() {
        auto v = q.pop();
        assert(!v.has_value());
        finished.store(true);
        });

    std::this_thread::sleep_for(10ms);
    q.close();

    t.join();
    assert(finished.load());

    // After close, push should fail and queue stays closed
    bool ok = q.push(1);
    assert(!ok);
    assert(!q.is_running());
}

static void test_front_waits_and_does_not_pop()
{
    MpmcBoundedQueue<int> q;

    std::thread producer([&]() {
        std::this_thread::sleep_for(10ms);
        bool ok = q.push(7);
        assert(ok);
        });

    auto f = q.front();
    assert(f.has_value());
    assert(*f == 7);
    assert(q.size() == 1);

    auto v = q.try_pop();
    assert(v.has_value() && *v == 7);

    producer.join();
}

static void test_move_semantics()
{
    MpmcBoundedQueue<int> q1;
    for (int i = 0; i < 3; ++i) { assert(q1.push(i)); }

    MpmcBoundedQueue<int> q2(std::move(q1));
    // q1 is moved-from and stopped
    assert(!q1.is_running());

    for (int i = 0; i < 3; ++i)
    {
        auto v = q2.try_pop();
        assert(v.has_value());
        assert(*v == i);
    }

    MpmcBoundedQueue<int> q3;
    assert(q3.push(10));
    q3 = std::move(q2);
    // q2 is moved-from and stopped; q3 now empty (q2 was emptied above)
    assert(!q2.is_running());
    assert(q3.empty());
}

static void test_multi_producer_consumer()
{
    MpmcBoundedQueue<int> q;
    constexpr int producer_count = 3;
    constexpr int items_per_producer = 100;
    constexpr int total_items = producer_count * items_per_producer;

    std::atomic<int> produced{ 0 };
    std::atomic<int> consumed{ 0 };

    std::vector<std::thread> producers;
    producers.reserve(producer_count);
    for (int p = 0; p < producer_count; ++p)
    {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i)
            {
                bool ok = q.push(p * items_per_producer + i);
                if (ok) produced.fetch_add(1);
            }
            });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(2);
    for (int c = 0; c < 2; ++c)
    {
        consumers.emplace_back([&]() {
            while (true)
            {
                auto v = q.pop_for<std::chrono::milliseconds>(5ms);
                if (!v.has_value())
                {
                    if (!q.is_running()) break;
                    // If still running and timeout, keep trying
                    continue;
                }
                consumed.fetch_add(1);
                if (consumed.load() >= total_items)
                {
                    // Once we've got all items, close to stop remaining waits
                    q.close();
                }
            }
            });
    }

    for (auto& t : producers) t.join();

    // Wait for consumers to drain
    for (auto& t : consumers) t.join();

    assert(produced.load() == total_items);
    assert(consumed.load() == total_items);
}

// New tests for bounded queue semantics
static void test_block_on_full_then_unblock_after_pop()
{
    MpmcBoundedQueue<int> q(2); // capacity 2
    assert(q.push(1));
    assert(q.push(2));

    std::atomic<bool> pushing{ false };
    std::atomic<bool> done{ false };
    std::atomic<bool> push_result{ false };

    std::thread producer([&]() {
        pushing.store(true);
        bool ok = q.push(3); // should block until a pop occurs
        push_result.store(ok);
        done.store(true);
        });

    // Ensure producer started and is likely blocked
    while (!pushing.load()) std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(5ms);
    assert(!done.load());

    // Pop one to make space
    auto v = q.pop_for<std::chrono::milliseconds>(10ms);
    assert(v.has_value());

    producer.join();
    assert(done.load());
    assert(push_result.load());
    assert(q.size() == 2);
}

static void test_close_unblocks_full_waiting_producer_and_returns_false()
{
    MpmcBoundedQueue<int> q(1);
    assert(q.push(1)); // now full

    std::atomic<bool> started{ false };
    std::atomic<bool> finished{ false };
    std::atomic<bool> push_ok{ true };

    std::thread producer([&]() {
        started.store(true);
        bool ok = q.push(2); // should be waiting on full
        push_ok.store(ok);
        finished.store(true);
        });

    while (!started.load()) std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(5ms);
    assert(!finished.load());

    q.close();

    producer.join();
    assert(finished.load());
    // After close, push must fail and must not enqueue
    assert(!push_ok.load());
    // Queue should still contain the original element (not the closed push)
    auto v = q.try_pop();
    assert(v.has_value() && *v == 1);
    assert(q.try_pop() == std::nullopt);
}

static void test_set_max_size_wakes_waiting_producer()
{
    MpmcBoundedQueue<int> q(1);
    assert(q.push(10)); // now full

    std::atomic<bool> started{ false };
    std::atomic<bool> finished{ false };
    std::atomic<bool> push_ok{ false };

    std::thread producer([&]() {
        started.store(true);
        bool ok = q.push(20); // wait on full
        push_ok.store(ok);
        finished.store(true);
        });

    while (!started.load()) std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(5ms);
    assert(!finished.load());

    q.set_max_size(2); // expand capacity should wake producer via cv_not_full

    producer.join();
    assert(finished.load());
    assert(push_ok.load());

    // Now size should be 2, and we can pop both in order
    auto a = q.try_pop();
    auto b = q.try_pop();
    assert(a.has_value());
    assert(b.has_value());
    std::vector<int> got{ *a, *b };
    std::sort(got.begin(), got.end());
    assert(got[0] == 10 && got[1] == 20);
}

// 批量入队测试函数
static void test_batch_push_empty_vector()
{
    MpmcBoundedQueue<int> q;
    std::vector<int> empty_vec;

    bool result = q.push(empty_vec.begin(), empty_vec.end());
    assert(!result);  // 空范围应该返回false
    assert(q.empty());
    assert(q.size() == 0);
}

static void test_batch_push_single_element()
{
    MpmcBoundedQueue<int> q;
    std::vector<int> vec = { 42 };

    bool result = q.push(vec.begin(), vec.end());
    assert(result);
    assert(!q.empty());
    assert(q.size() == 1);

    auto v = q.try_pop();
    assert(v.has_value());
    assert(*v == 42);
}

static void test_batch_push_multiple_elements()
{
    MpmcBoundedQueue<int> q(10); // 容量为10
    std::vector<int> vec = { 1, 2, 3, 4, 5 };

    bool result = q.push(vec.begin(), vec.end());
    assert(result);
    assert(q.size() == 5);

    for (int i = 1; i <= 5; ++i)
    {
        auto v = q.try_pop();
        assert(v.has_value());
        assert(*v == i);
    }
    assert(q.empty());
}

static void test_batch_push_exceeds_capacity()
{
    MpmcBoundedQueue<int> q(3); // 容量为3
    std::vector<int> vec = { 1, 2, 3, 4, 5 }; // 5个元素，超过容量
    
    std::atomic<bool> started{ false };
    std::atomic<bool> finished{ false };
    std::atomic<bool> push_result{ false };
    
    // 在另一个线程中执行批量插入，它会等待直到有足够空间
    std::thread producer([&]() {
        started.store(true);
        bool result = q.push(vec.begin(), vec.end());
        push_result.store(result);
        finished.store(true);
    });
    
    // 等待生产者开始
    while (!started.load()) std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(5ms);
    
    // 消费元素来释放空间，让批量插入能够完成
    std::thread consumer([&]() {
        for (int i = 0; i < 5; ++i) {
            auto v = q.pop_for<std::chrono::milliseconds>(100ms);
            if (v.has_value()) {
                // 元素被消费
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    assert(finished.load());
    assert(push_result.load());
    assert(q.empty()); // 所有元素都被消费了
}

static void test_batch_push_with_different_iterators()
{
    MpmcBoundedQueue<std::string> q(5);
    std::vector<std::string> vec = { "hello", "world", "test" };

    bool result = q.push(vec.begin(), vec.end());
    assert(result);
    assert(q.size() == 3);

    auto v1 = q.try_pop();
    auto v2 = q.try_pop();
    auto v3 = q.try_pop();

    assert(v1.has_value() && *v1 == "hello");
    assert(v2.has_value() && *v2 == "world");
    assert(v3.has_value() && *v3 == "test");
    assert(q.empty());
}

static void test_batch_push_on_closed_queue()
{
    MpmcBoundedQueue<int> q(5);
    q.close(); // 关闭队列

    std::vector<int> vec = { 1, 2, 3 };
    bool result = q.push(vec.begin(), vec.end());
    assert(!result); // 应该返回false
    assert(q.empty());
}

static void test_batch_push_concurrent()
{
    MpmcBoundedQueue<int> q(10);
    std::atomic<int> total_pushed{ 0 };
    std::atomic<int> total_popped{ 0 };

    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < 5; ++i)
        {
            std::vector<int> vec = { i * 3 + 1, i * 3 + 2, i * 3 + 3 };
            bool result = q.push(vec.begin(), vec.end());
            if (result)
            {
                total_pushed.fetch_add(3);
            }
            std::this_thread::sleep_for(1ms);
        }
        });

    // 消费者线程
    std::thread consumer([&]() {
        while (total_popped.load() < 15) // 总共15个元素
        {
            auto v = q.pop_for<std::chrono::milliseconds>(10ms);
            if (v.has_value())
            {
                total_popped.fetch_add(1);
            }
        }
        });

    producer.join();
    consumer.join();

    assert(total_pushed.load() == 15);
    assert(total_popped.load() == 15);
}

static void test_batch_push_large_vector()
{
    MpmcBoundedQueue<int> q(1000);
    std::vector<int> large_vec(500);
    std::iota(large_vec.begin(), large_vec.end(), 1); // 填充1到500

    bool result = q.push(large_vec.begin(), large_vec.end());
    assert(result);
    assert(q.size() == 500);

    // 验证所有元素都正确插入
    for (int i = 1; i <= 500; ++i)
    {
        auto v = q.try_pop();
        assert(v.has_value());
        assert(*v == i);
    }
}
int main()
{
    demo::run_concurrent_demo();
    return 0;
}