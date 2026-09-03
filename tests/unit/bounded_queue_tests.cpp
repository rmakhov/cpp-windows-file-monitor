#include <cwm/concurrency/bounded_queue.h>

#include <gtest/gtest.h>

#include <chrono>
#include <stop_token>
#include <thread>

namespace
{

using cwm::concurrency::BoundedQueue;
using cwm::concurrency::QueuePushResult;

TEST(BoundedQueueTest, RejectsZeroCapacity)
{
    EXPECT_THROW(
        BoundedQueue<int> queue(0),
        std::invalid_argument);
}

TEST(BoundedQueueTest, PushAndPop)
{
    BoundedQueue<int> queue(10);

    EXPECT_EQ(
        queue.try_push(42),
        QueuePushResult::Accepted);

    std::stop_source stop_source;

    const auto value =
        queue.wait_pop(stop_source.get_token());

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

TEST(BoundedQueueTest, PreservesFifoOrder)
{
    BoundedQueue<int> queue(10);

    EXPECT_EQ(
        queue.try_push(1),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(2),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(3),
        QueuePushResult::Accepted);

    std::stop_source stop_source;

    const auto first = queue.wait_pop(stop_source.get_token());
    const auto second = queue.wait_pop(stop_source.get_token());
    const auto third = queue.wait_pop(stop_source.get_token());

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());

    EXPECT_EQ(*first, 1);
    EXPECT_EQ(*second, 2);
    EXPECT_EQ(*third, 3);
}

TEST(BoundedQueueTest, ReportsFull)
{
    BoundedQueue<int> queue(2);

    EXPECT_EQ(
        queue.try_push(1),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(2),
        QueuePushResult::Accepted);

    EXPECT_EQ(
        queue.try_push(3),
        QueuePushResult::Full);

    EXPECT_EQ(queue.size(), 2u);
}

TEST(BoundedQueueTest, ReportsClosed)
{
    BoundedQueue<int> queue(10);

    queue.close();

    EXPECT_TRUE(queue.is_closed());

    EXPECT_EQ(
        queue.try_push(42),
        QueuePushResult::Closed);
}

TEST(BoundedQueueTest, CloseDoesNotDiscardQueuedItems)
{
    BoundedQueue<int> queue(10);

    EXPECT_EQ(
        queue.try_push(42),
        QueuePushResult::Accepted);

    queue.close();

    std::stop_source stop_source;

    const auto value =
        queue.wait_pop(stop_source.get_token());

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);

    const auto empty =
        queue.wait_pop(stop_source.get_token());

    EXPECT_FALSE(empty.has_value());
}

TEST(BoundedQueueTest, HighWaterMarkIsTracked)
{
    BoundedQueue<int> queue(10);

    EXPECT_EQ(queue.high_water_mark(), 0u);

    queue.try_push(1);
    queue.try_push(2);
    queue.try_push(3);

    EXPECT_EQ(queue.high_water_mark(), 3u);

    std::stop_source stop_source;

    queue.wait_pop(stop_source.get_token());

    EXPECT_EQ(queue.high_water_mark(), 3u);
}

TEST(BoundedQueueTest, StopTokenCancelsWait)
{
    BoundedQueue<int> queue(10);

    std::stop_source stop_source;

    std::optional<int> result;

    std::jthread consumer(
        [&]
        {
            result =
                queue.wait_pop(stop_source.get_token());
        });

    std::this_thread::sleep_for(
        std::chrono::milliseconds(20));

    stop_source.request_stop();

    consumer.join();

    EXPECT_FALSE(result.has_value());
}

TEST(BoundedQueueTest, ConsumerWakesWhenItemArrives)
{
    BoundedQueue<int> queue(10);

    std::stop_source stop_source;

    std::optional<int> result;

    std::jthread consumer(
        [&]
        {
            result =
                queue.wait_pop(stop_source.get_token());
        });

    std::this_thread::sleep_for(
        std::chrono::milliseconds(20));

    EXPECT_EQ(
        queue.try_push(123),
        QueuePushResult::Accepted);

    consumer.join();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

} // namespace
