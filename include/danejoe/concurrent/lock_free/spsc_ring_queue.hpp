#pragma once

#include <atomic>
#include <vector>
#include <cstdint>
#include <optional>

namespace DaneJoe
{
    namespace Concurrent
    {
        namespace LockFree
        {
            template <typename T>
            class SpscRingQueue
            {
            public:
                SpscRingQueue(std::size_t capacity)
                {
                    m_capacity.store(capacity);
                    m_data = std::vector<T>(capacity);
                    m_head_index.store(0);
                    m_tail_index.store(0);
                }
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
                void push(const std::vector<T>& datas)
                {
                    for(auto&& data:datas)
                    {
                        push(std::move(data));
                    }
                }
                bool is_empty()const
                {
                    return m_current_size.load() == 0;
                }
                bool is_full()const
                {
                    return m_current_size.load()==m_capacity.load();
                }
                // void resize(std::size_t size);
                std::size_t size()const
                {
                    return m_current_size.load();
                }
            private:
                SpscRingQueue(const SpscRingQueue&) = delete;
                SpscRingQueue& operator=(const SpscRingQueue&) = delete;
                SpscRingQueue(SpscRingQueue&&) = delete;
                SpscRingQueue& operator=(SpscRingQueue&&) = delete;
            private:
                std::atomic<uint32_t> m_head_index=0;
                std::atomic<uint32_t> m_tail_index=0;
                std::vector<T> m_data;
                std::atomic<std::size_t> m_current_size = 0;
                std::atomic<std::size_t> m_capacity = 0;
            };
        }
    }
}