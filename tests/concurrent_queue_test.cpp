#include "concurrent_queue.hpp"

#include <chrono>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

namespace
{

using std::chrono::milliseconds;

TEST(ConcurrentQueue, PopsInFifoOrder)
{
	ConcurrentQueue<int> queue;
	queue.push(1);
	queue.push(2);

	EXPECT_EQ(queue.try_pop(), 1);
	EXPECT_EQ(queue.wait_and_pop(), 2);
	EXPECT_EQ(queue.try_pop(), std::nullopt);
	EXPECT_TRUE(queue.empty());
}

TEST(ConcurrentQueue, WaitBlocksUntilPush)
{
	ConcurrentQueue<int> queue;
	std::thread          producer([&queue] {
		std::this_thread::sleep_for(milliseconds(50));
		queue.push(42);
	});

	EXPECT_EQ(queue.wait_and_pop(), 42);
	producer.join();
}

TEST(ConcurrentQueue, CloseReleasesWaitingConsumer)
{
	ConcurrentQueue<int> queue;
	std::optional<int>   result = 0;
	std::thread          consumer([&] { result = queue.wait_and_pop(); });

	std::this_thread::sleep_for(milliseconds(50));
	queue.close();
	consumer.join();

	EXPECT_EQ(result, std::nullopt);
}

TEST(ConcurrentQueue, ClosedQueueDrainsRemainingItemsAndRejectsNewOnes)
{
	ConcurrentQueue<int> queue;
	queue.push(1);
	queue.push(2);
	queue.close();
	queue.push(3);

	EXPECT_EQ(queue.wait_and_pop(), 1);
	EXPECT_EQ(queue.wait_and_pop(), 2);
	EXPECT_EQ(queue.wait_and_pop(), std::nullopt);
}

} // namespace
