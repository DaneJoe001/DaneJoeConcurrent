#pragma once

/**
 * @file spsc_ring_queue.hpp
 * @author DaneJoe (danejoe001.github)
 * @brief 线程安全的单生产者单消费者循环队列
 * @version 0.1.1
 * @date 2025-10-24
 */

#include <atomic>
#include <vector>
#include <cstdint>
#include <optional>

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
         * @namespace LockFree
         */
        namespace LockFree
        {
            /**
             * @brief 线程安全的单生产者单消费者循环队列
             * @tparam T 元素类型
             */
            template <typename T>
            class SpscRingQueue
            {
            public:
                /**
                 * @brief 构造函数
                 * @param capacity 队列容量
                 */
                SpscRingQueue(std::size_t capacity)
                {
                    m_capacity.store(capacity);
                    m_data = std::vector<T>(capacity);
                    m_head_index.store(0);
                    m_tail_index.store(0);
                }
                /**
                 * @brief 弹出队首元素
                 * @return std::optional<T> 弹出的元素，若队列为空则返回std::nullopt
                 */
                std::optional<T> pop()
                {
                    if (is_empty())
                    {
                        return std::nullopt;
                    }
                    uint32_t current = m_tail_index.load();
                    uint32_t next_index = current + 1;
                    next_index=next_index==m_capacity.load()?0:next_index;
                    m_tail_index.store(next_index);
                    --m_current_size;
                    return std::move(m_data[current]);
                }
                /**
                 * @brief 批量弹出队首元素
                 * @param nums 要弹出的元素数量
                 * @return std::optional<std::vector<T>> 批量弹出的元素，若队列为空则返回std::nullopt
                 */
                std::optional<std::vector<T>> pop(std::size_t nums)
                {
                    std::vector<T> result;
                    result.reserve(nums);
                    for (std::size_t i = 0;i < nums;i++)
                    {
                        auto data = pop();
                        if(data==std::nullopt)
                        {
                            return std::nullopt;
                        }
                        result.emplace_back(std::move(data.value()));
                    }
                    return result;
                }
                /**
                 * @brief 向队尾添加元素
                 * @param data 要添加的元素
                 */
                void push(const T& data)
                {

                    if (is_full())
                    {
                        return;
                    }
                    uint32_t current = m_head_index.load();
                    m_data[current] = std::move(data);
                    uint32_t next_index = current + 1;
                    next_index=next_index==m_capacity.load()?0:next_index;
                    m_head_index.store(next_index);
                    ++m_current_size;
                }
                /**
                 * @brief 向队尾添加元素
                 * @param data 要添加的元素
                 */
                void push(T&& data)
                {
                    if(is_full())
                    {
                        return;
                    }
                    uint32_t current = m_head_index.load();
                    m_data[current] = std::move(data);
                    uint32_t next_index = current + 1;
                    next_index=next_index==m_capacity.load()?0:next_index;
                    m_head_index.store(next_index);
                    ++m_current_size;
                }
                /**
                 * @brief 向队尾添加元素
                 * @param datas 要添加的元素
                 */
                void push(const std::vector<T>& datas)
                {
                    for(auto&& data:datas)
                    {
                        push(std::move(data));
                    }
                }
                /**
                 * @brief 判断队列是否为空
                 * @return true 队列为空
                 * @return false 队列不为空
                 */
                bool is_empty()const
                {
                    return m_current_size.load() == 0;
                }
                /**
                 * @brief 判断队列是否已满
                 * @return true 队列已满
                 * @return false 队列未满
                 */
                bool is_full()const
                {
                    return m_current_size.load()==m_capacity.load();
                }
                /**
                 * @brief 获取队列大小
                 * @return std::size_t 队列大小
                 */
                std::size_t size()const
                {
                    return m_current_size.load();
                }
            private:
                /**
                 * @brief 删除拷贝构造函数
                 */
                SpscRingQueue(const SpscRingQueue&) = delete;
                /**
                 * @brief 删除拷贝赋值运算符
                 */
                SpscRingQueue& operator=(const SpscRingQueue&) = delete;
                /**
                 * @brief 删除移动构造函数
                 */
                SpscRingQueue(SpscRingQueue&&) = delete;
                /**
                 * @brief 删除移动赋值运算符
                 */
                SpscRingQueue& operator=(SpscRingQueue&&) = delete;
            private:
                /// @brief 头索引
                std::atomic<uint32_t> m_head_index=0;
                /// @brief 尾索引
                std::atomic<uint32_t> m_tail_index=0;
                /// @brief 数据
                std::vector<T> m_data;
                /// @brief 当前大小
                std::atomic<std::size_t> m_current_size = 0;
                /// @brief 容量
                std::atomic<std::size_t> m_capacity = 0;
            };
        }
    }
}