#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <utility>

namespace cwm::concurrency
{

enum class QueuePushResult
{
    Accepted,
    Full,
    Closed
};

template <typename T>
class BoundedQueue
{
public:
    explicit BoundedQueue(std::size_t capacity)
        : capacity_(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument(
                "BoundedQueue capacity must be greater than zero");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    ~BoundedQueue() = default;

    QueuePushResult try_push(T value)
    {
        {
            std::lock_guard lock(mutex_);

            if (closed_)
            {
                return QueuePushResult::Closed;
            }

            if (queue_.size() >= capacity_)
            {
                return QueuePushResult::Full;
            }

            queue_.push_back(std::move(value));

            if (queue_.size() > high_water_mark_)
            {
                high_water_mark_ = queue_.size();
            }
        }

        condition_.notify_one();

        return QueuePushResult::Accepted;
    }

    std::optional<T> wait_pop(std::stop_token stop_token)
    {
        std::unique_lock lock(mutex_);

        const bool ready = condition_.wait(
            lock,
            stop_token,
            [this]
            {
                return closed_ || !queue_.empty();
            });

        if (!ready)
        {
            return std::nullopt;
        }

        if (queue_.empty())
        {
            // Queue is closed and all queued items have been consumed.
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();

        return value;
    }

    void close()
    {
        {
            std::lock_guard lock(mutex_);

            if (closed_)
            {
                return;
            }

            closed_ = true;
        }

        condition_.notify_all();
    }

    [[nodiscard]]
    bool is_closed() const
    {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]]
    std::size_t size() const
    {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]]
    std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    [[nodiscard]]
    std::size_t high_water_mark() const
    {
        std::lock_guard lock(mutex_);
        return high_water_mark_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable_any condition_;

    std::deque<T> queue_;

    const std::size_t capacity_;

    bool closed_{false};

    std::size_t high_water_mark_{0};
};

} // namespace cwm::concurrency
