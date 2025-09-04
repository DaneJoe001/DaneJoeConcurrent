#pragma once

#include <mutex>
#include <queue>
#include <optional>
#include <stdexcept>
#include <condition_variable>
#include <chrono>

namespace DaneJoe
{
    template<class T>
    class MTQueue
    {
    public:
        MTQueue() = default;
        std::optional<T> pop()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()
                {
                    return !m_queue.empty() || !m_is_running;
                });
            if (m_queue.empty())
            {
                return std::nullopt;
            }
            T item = std::move(m_queue.front());
            m_queue.pop();
            return item;
        }
        std::optional<T> try_pop()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                return std::nullopt;
            }
            T item = std::move(m_queue.front());
            m_queue.pop();
            return item;
        }
        template<class Period>
        std::optional<T> pop_until(Period timeout)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_until(lock, timeout, [this]()
                {
                    return !m_queue.empty() || !m_is_running;
                });
            if (m_queue.empty())
            {
                return std::nullopt;
            }
            T item = std::move(m_queue.front());
            m_queue.pop();
            return item;
        }
        template<class Period>
        std::optional<T> pop_for(Period timeout)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, timeout, [this]()
                {
                    return !m_queue.empty() || !m_is_running;
                });
            if (m_queue.empty())
            {
                return std::nullopt;
            }
            T item = std::move(m_queue.front());
            m_queue.pop();
            return item;
        }
        bool empty()const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }
        std::size_t size()const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }
        bool is_running()const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_is_running;
        }
        /**
         * @brief 获取队首元素
         * @note 模板元素需要可拷贝
         * @return std::optional<T>
         */
        std::optional<T> front()const
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()
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
        bool push(T item)
        {
            bool is_pushed = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_is_running)
                {
                    m_queue.push(std::move(item));
                    is_pushed = true;
                }
            }
            if (is_pushed)
            {
                m_cv.notify_one();
            }
            return is_pushed;
        }
        ~MTQueue()noexcept
        {
            close();
        }
        MTQueue(MTQueue&& other) noexcept
        {
            std::scoped_lock<std::mutex, std::mutex> lock(m_mutex, other.m_mutex);
            m_queue = std::move(other.m_queue);
            m_is_running = other.m_is_running;
            other.m_is_running = false;
        }
        MTQueue& operator=(MTQueue&& other)noexcept
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
        void close()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_is_running = false;
            }
            m_cv.notify_all();
        }
    private:
        MTQueue(const MTQueue&) = delete;
        MTQueue& operator=(const MTQueue&) = delete;
    private:
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cv;
        std::queue<T> m_queue;
        bool m_is_running = true;
    };
}