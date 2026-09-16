#include <cwm/concurrency/bounded_queue.h>

#include <chrono>
#include <stop_token>
#include <thread>

#include <gtest/gtest.h>

namespace cwm::concurrency
{

TEST(BoundedQueueTest, RejectsZeroCapacity)
{
    EXPECT_THROW(
        BoundedQueue<int> queue(0),
        std::invalid_argument);
}

TEST(BoundedQueueTest, ReportsCapacity)
{
    BoundedQueue<int> queue(3);

    EXPECT_EQ(queue.capacity(), 3u);
}

TEST(BoundedQueueTest, PushAndPop)
{
    BoundedQueue<int> queue(2);

    int first = 10;
    int second = 20;

    EXPECT_EQ(
        queue.try_push(first),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(second),
        QueuePushResult::Accepted);

    EXPECT_EQ(queue.size(), 2u);

    auto value = queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 10);

    EXPECT_EQ(queue.size(), 1u);
}

TEST(BoundedQueueTest, PreservesFifoOrder)
{
    BoundedQueue<int> queue(3);

    int first = 1;
    int second = 2;
    int third = 3;

    EXPECT_EQ(
        queue.try_push(first),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(second),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(third),
        QueuePushResult::Accepted);

    auto value1 = queue.wait_pop(std::stop_token{});
    auto value2 = queue.wait_pop(std::stop_token{});
    auto value3 = queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    ASSERT_TRUE(value3.has_value());

    EXPECT_EQ(*value1, 1);
    EXPECT_EQ(*value2, 2);
    EXPECT_EQ(*value3, 3);
}

TEST(BoundedQueueTest, ReportsFullWhenCapacityIsReached)
{
    BoundedQueue<int> queue(2);

    int first = 1;
    int second = 2;
    int third = 3;

    EXPECT_EQ(
        queue.try_push(first),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(second),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(third),
        QueuePushResult::Full);

    EXPECT_EQ(queue.size(), 2u);
}

TEST(BoundedQueueTest, FullQueueDoesNotConsumeValue)
{
    BoundedQueue<int> queue(1);

    int first = 1;
    int second = 2;

    EXPECT_EQ(
        queue.try_push(first),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(second),
        QueuePushResult::Full);

    // The caller must retain ownership of the value when the queue is full.
    EXPECT_EQ(second, 2);
}

TEST(BoundedQueueTest, ClosePreventsFurtherPushes)
{
    BoundedQueue<int> queue(2);

    queue.close();

    int value = 42;

    EXPECT_EQ(
        queue.try_push(value),
        QueuePushResult::Closed);

    EXPECT_EQ(queue.size(), 0u);
}

TEST(BoundedQueueTest, ClosedQueueDoesNotConsumeValue)
{
    BoundedQueue<int> queue(2);

    queue.close();

    int value = 42;

    EXPECT_EQ(
        queue.try_push(value),
        QueuePushResult::Closed);

    EXPECT_EQ(value, 42);
}

TEST(BoundedQueueTest, HighWaterMarkTracksMaximumSize)
{
    BoundedQueue<int> queue(3);

    EXPECT_EQ(queue.high_water_mark(), 0u);

    int first = 1;
    int second = 2;

    EXPECT_EQ(
        queue.try_push(first),
        QueuePushResult::Accepted);

    EXPECT_EQ(queue.high_water_mark(), 1u);

    EXPECT_EQ(
        queue.try_push(second),
        QueuePushResult::Accepted);

    EXPECT_EQ(queue.high_water_mark(), 2u);

    auto value = queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(queue.size(), 1u);

    // High-water mark is historical and must not decrease.
    EXPECT_EQ(queue.high_water_mark(), 2u);
}

TEST(BoundedQueueTest, WaitPopReturnsEmptyWhenQueueIsClosed)
{
    BoundedQueue<int> queue(1);

    queue.close();

    auto value = queue.wait_pop(std::stop_token{});

    EXPECT_FALSE(value.has_value());
}

TEST(BoundedQueueTest, WaitPopUnblocksWhenItemIsPushed)
{
    BoundedQueue<int> queue(1);

    std::jthread producer(
        [&queue]
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(50));

            int value = 123;

            EXPECT_EQ(
                queue.try_push(value),
                QueuePushResult::Accepted);
        });

    auto value = queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 123);
}

TEST(BoundedQueueTest, WaitPopCanBeCancelled)
{
    BoundedQueue<int> queue(1);

    std::jthread consumer(
        [&queue](std::stop_token stop_token)
        {
            auto value = queue.wait_pop(stop_token);

            EXPECT_FALSE(value.has_value());
        });

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));

    consumer.request_stop();
}

TEST(BoundedQueueTest, CloseWakesWaitingConsumer)
{
    BoundedQueue<int> queue(1);

    std::atomic<bool> consumer_finished{false};

    std::jthread consumer(
        [&queue, &consumer_finished]
        {
            auto value = queue.wait_pop(std::stop_token{});

            EXPECT_FALSE(value.has_value());

            consumer_finished.store(
                true,
                std::memory_order_release);
        });

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));

    queue.close();

    for (int i = 0;
         i < 100 && !consumer_finished.load(
                         std::memory_order_acquire);
         ++i)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(
        consumer_finished.load(
            std::memory_order_acquire));
}

} // namespace cwm::concurrency
