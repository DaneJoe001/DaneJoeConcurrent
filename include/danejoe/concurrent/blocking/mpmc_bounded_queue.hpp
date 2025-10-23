#pragma once

/**
 * @file mpmc_bounded_queue.hpp
 * @author DaneJoe (danejoe001.github)
 * @brief 线程安全的队列
 * @version 0.1.1
 * @date 2025-09-04
 * @copyright Copyright (c) 2025
 */

#include <mutex>
#include <queue>
#include <chrono>
#include <vector>
#include <thread>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <condition_variable>

/**
 * @namespace DaneJoe
 */
namespace DaneJoe
{
    /**
     * @namespace Concurrent
     */
    namespace Concurrent
    {
        /**
         * @namespace Blocking
         */
        namespace Blocking
        {
            /**
             * @brief 线程安全的队列
             * @tparam T 队列元素类型
             */
            template<class T>
            class MpmcBoundedQueue
            {
            public:
                /**
                 * @brief 构造函数
                 * @param max_size 队列最大容量
                 */
                MpmcBoundedQueue(int max_size = 50) : m_max_size(max_size) {}
                /**
                 * @brief 弹出队首元素
                 * @return std::optional<T> 弹出的元素，若队列为空则返回std::nullopt
                 */
                std::optional<T> pop()
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_empty_cv.wait(lock, [this]()
                        {
                            return !m_queue.empty() || !m_is_running;
                        });
                    if (m_queue.empty())
                    {
                        return std::nullopt;
                    }
                    T item = std::move(m_queue.front());
                    m_queue.pop();
                    lock.unlock();
                    m_full_cv.notify_one();
                    return item;
                }
                /**
                 * @brief 批量弹出队首元素
                 * @param nums 要弹出的元素数量
                 * @return std::optional<std::vector<T>> 批量弹出的元素，若队列为空则返回std::nullopt
                 */
                std::optional<std::vector<T>> pop(int nums)
                {
                
                    int has_popped = 0;
                    std::vector<T> result;
                    std::unique_lock<std::mutex> lock(m_mutex);
                    while (has_popped < nums)
                    {
                        m_empty_cv.wait(lock, [this]()
                            {
                                return !m_queue.empty() || !m_is_running;
                            });
                        if (m_queue.empty() && !m_is_running)
                        {
                            return std::nullopt;
                        }
                        result.emplace_back(std::move(m_queue.front()));
                        has_popped++;
                        m_queue.pop();
                    }
                    lock.unlock();
                    m_full_cv.notify_one();
                    return result;
                }
                /**
                 * @brief 尝试弹出队首元素
                 * @return std::optional<T> 弹出的元素，若队列为空则返回std::nullopt
                 */
                std::optional<T> try_pop()
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    if (m_queue.empty())
                    {
                        return std::nullopt;
                    }
                    T item = std::move(m_queue.front());
                    m_queue.pop();
                    lock.unlock();
                    m_full_cv.notify_one();
                    return item;
                }
                /**
                 * @brief 尝试打包弹出元素
                 * @param nums 要弹出的元素数量
                 * @return std::vector<T> 批量弹出的元素
                 */
                std::vector<T> try_pop(std::size_t nums)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    int has_popped = 0;
                    std::vector<T> result;
                    while (has_popped < nums)
                    {
                        if (m_queue.empty())
                        {
                            return result;
                        }
                        result.push_back(std::move(m_queue.front()));
                        m_queue.pop();
                        has_popped++;
                    }
                    lock.unlock();
                    m_full_cv.notify_all();
                    return result;
                }
                /**
                 * @brief 等待弹出队首元素
                 * @tparam Period 等待时间类型
                 * @param timeout 等待时间
                 * @return std::optional<T> 弹出的元素，若队列为空则返回std::nullopt
                 */
                template<class Period>
                std::optional<T> pop_until(Period timeout)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_empty_cv.wait_until(lock, timeout, [this]()
                        {
                            return !m_queue.empty() || !m_is_running;
                        });
                    if (m_queue.empty())
                    {
                        return std::nullopt;
                    }
                    T item = std::move(m_queue.front());
                    m_queue.pop();
                    lock.unlock();
                    m_full_cv.notify_one();
                    return item;
                }
                /**
                 * @brief 等待弹出队首元素
                 * @tparam Period 等待时间类型
                 * @param timeout 等待时间
                 * @return std::optional<T> 弹出的元素，若队列为空则返回std::nullopt
                 */
                template<class Period>
                std::optional<T> pop_for(Period timeout)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_empty_cv.wait_for(lock, timeout, [this]()
                        {
                            return !m_queue.empty() || !m_is_running;
                        });
                    if (m_queue.empty())
                    {
                        return std::nullopt;
                    }
                    T item = std::move(m_queue.front());
                    m_queue.pop();
                    lock.unlock();
                    m_full_cv.notify_one();
                    return item;
                }
                /**
                 * @brief 判断队列是否为空
                 * @return bool 队列是否为空
                 */
                bool empty()const
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    return m_queue.empty();
                }
                /**
                 * @brief 判断队列是否已满
                 * @return bool 队列是否已满
                 */
                bool full()const
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    return m_queue.size() >= m_max_size;
                }
                /**
                 * @brief 获取队列大小
                 * @return std::size_t 队列大小
                 */
                std::size_t size()const
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    return m_queue.size();
                }
                /**
                 * @brief 判断队列是否正在运行
                 * @return bool 队列是否正在运行
                 */
                bool is_running()const
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    return m_is_running;
                }
                /**
                 * @brief 获取队首元素
                 * @note 模板元素需要可拷贝
                 * @return std::optional<T> 队首元素，若队列为空则返回std::nullopt
                 */
                std::optional<T> front()const
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_empty_cv.wait(lock, [this]()
                        {
                            return !m_queue.empty() || !m_is_running;
                        });
                    if (!m_queue.empty())
                    {
                        T item = m_queue.front();
                        return item;
                    }
                    return std::nullopt;
                }
                /**
                 * @brief 添加元素到队列
                 * @param item 元素
                 * @return bool 是否成功添加
                 */
                bool push(T item)
                {
                    bool is_pushed = false;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        if (m_is_running)
                        {
                            m_full_cv.wait(lock, [this]()
                                {
                                    return !m_is_running || (m_queue.size() < m_max_size);
                                });
                            if (!m_is_running)
                            {
                                return false;
                            }
                            m_queue.push(std::move(item));
                            is_pushed = true;
                        }
                    }
                    if (is_pushed)
                    {
                        m_empty_cv.notify_one();
                    }
                    return is_pushed;
                }
                /**
                 * @brief 添加元素到队列
                 * @tparam U 元素类型
                 * @param begin 元素起始迭代器
                 * @param end 元素结束迭代器
                 * @return bool 是否成功添加
                 */
                template<typename U>
                bool push(U begin, U end)
                {
                    std::size_t nums = static_cast<int>(std::distance(begin, end));
                    bool is_pushed = false;
                
                    while (nums > 0)
                    {
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            if (!m_is_running)
                            {
                                return false;
                            }
                            m_full_cv.wait(lock, [this]()
                                {
                                    return (m_queue.size() < m_max_size) || !m_is_running;
                                });
                            if (!m_is_running)
                            {
                                return false;
                            }
                            std::size_t to_insert = std::min(nums, m_max_size - m_queue.size());
                            for (int i = 0; i < to_insert; ++i)
                            {
                                m_queue.push(*begin);
                                ++begin;
                            }
                            nums -= to_insert;
                            is_pushed = true;
                        }
                        if (is_pushed)
                        {
                            m_empty_cv.notify_all();
                        }
                    }
                    return is_pushed;
                }
                /**
                 * @brief 析构函数
                 */
                ~MpmcBoundedQueue()noexcept
                {
                    close();
                }
                /**
                 * @brief 移动构造函数
                 * @param other 其他队列
                 */
                MpmcBoundedQueue(MpmcBoundedQueue&& other) noexcept
                {
                    std::scoped_lock<std::mutex, std::mutex> lock(m_mutex, other.m_mutex);
                    m_queue = std::move(other.m_queue);
                    m_is_running = other.m_is_running;
                    other.m_is_running = false;
                }
                /**
                 * @brief 移动赋值运算符
                 * @param other 其他队列
                 * @return MpmcBoundedQueue&
                 */
                MpmcBoundedQueue& operator=(MpmcBoundedQueue&& other)noexcept
                {
                    if (this == &other)
                    {
                        return *this;
                    }
                    std::scoped_lock<std::mutex, std::mutex> lock(m_mutex, other.m_mutex);
                    m_queue = std::move(other.m_queue);
                    m_is_running = other.m_is_running;
                    other.m_is_running = false;
                    return *this;
                }
                /**
                 * @brief 关闭队列
                 * @note 关闭后，队列不再接受新元素，但已有的元素仍然可以被弹出
                 */
                void close()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_is_running = false;
                    }
                    m_empty_cv.notify_all();
                    m_full_cv.notify_all();
                }
                /**
                 * @brief 设置队列最大长度
                 * @param max_size 最大长度
                 */
                void set_max_size(std::size_t max_size)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    std::size_t temp_raw = m_max_size;
                    m_max_size = max_size;
                    if (temp_raw < max_size)
                    {
                        m_full_cv.notify_all();
                    }
                }
                /**
                 * @brief 获取队列最大长度
                 * @return std::size_t
                 */
                std::size_t get_max_size() const
                {
                    return m_max_size;
                }
            private:
                /**
                 * @brief 拷贝构造函数
                 * @note 禁止拷贝构造
                 */
                MpmcBoundedQueue(const MpmcBoundedQueue&) = delete;
                /**
                 * @brief 拷贝赋值运算符
                 * @note 禁止拷贝赋值
                 */
                MpmcBoundedQueue& operator=(const MpmcBoundedQueue&) = delete;
            private:
                /// @brief 队列最大容量
                std::size_t m_max_size = 0;
                /// @brief 互斥锁
                mutable std::mutex m_mutex;
                /// @brief 空条件变量
                mutable std::condition_variable m_empty_cv;
                /// @brief 满条件变量
                mutable std::condition_variable m_full_cv;
                /// @brief 队列
                std::queue<T> m_queue;
                /// @brief 是否正在运行
                bool m_is_running = true;
            };
        }
    }
}