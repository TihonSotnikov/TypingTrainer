#include "test_utils.hpp"
#include "typing_trainer_core.hpp"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

using test::Clock;
using test::key;
using test::milliseconds;
using test::start_free;

/// \brief Собирает события ядра, приходящие из рабочего потока.
class EventCollector
{
public:
	explicit EventCollector(ITypingTrainerCore& core) : core_(core)
	{
		core_.set_output_ready_callback([this] {
			std::scoped_lock const lock(mutex_);
			++notifications_;
			cv_.notify_all();
		});
	}

	~EventCollector() { core_.set_output_ready_callback(nullptr); }

	EventCollector(const EventCollector&)            = delete;
	EventCollector(EventCollector&&)                 = delete;
	EventCollector& operator=(const EventCollector&) = delete;
	EventCollector& operator=(EventCollector&&)      = delete;

	/// \brief Дождаться события, удовлетворяющего условию (или таймаута).
	bool wait_for(const std::function<bool(const BackendEvent&)>& predicate)
	{
		auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (std::chrono::steady_clock::now() < deadline)
		{
			while (auto event = core_.poll_output())
			{
				received_.push_back(*event);
				if (predicate(*event)) return true;
			}
			std::unique_lock lock(mutex_);
			cv_.wait_for(lock, milliseconds(20));
		}
		return false;
	}

private:
	ITypingTrainerCore&       core_;
	std::mutex                mutex_;
	std::condition_variable   cv_;
	int                       notifications_ = 0;
	std::vector<BackendEvent> received_;
};

bool is_completed_update(const BackendEvent& event)
{
	auto const* update = std::get_if<StateUpdate>(&event);
	return update != nullptr && update->is_completed;
}

TEST(TypingTrainerCore, ProcessesEventsInBackgroundAndNotifiesUi)
{
	TypingTrainerCore core({});
	EventCollector    collector(core);

	auto now = Clock::now();
	core.push_input(start_free(U"go"));
	core.push_input(key(U'g', now += milliseconds(150)));
	core.push_input(key(U'o', now += milliseconds(150)));

	EXPECT_TRUE(collector.wait_for(is_completed_update));
}

TEST(TypingTrainerCore, SavesStatisticsWhenDestroyedMidSession)
{
	test::TempDir const dir;
	{
		TypingTrainerCore core(dir.path());
		EventCollector    collector(core);

		auto now = Clock::now();
		core.push_input(start_free(U"long text"));
		core.push_input(key(U'l', now += milliseconds(150)));
		ASSERT_TRUE(collector.wait_for(
		    [](const BackendEvent& event) { return std::holds_alternative<StateUpdate>(event); }));
	} // сессия не завершена, ядро уничтожается

	EXPECT_TRUE(std::filesystem::exists(dir.path() / "ngram_stats.json"));
}

} // namespace
} // namespace typing_trainer
