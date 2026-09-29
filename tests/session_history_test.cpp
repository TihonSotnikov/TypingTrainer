#include "session_history.hpp"
#include "test_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

SessionRecord record(TrainingMode mode, Language language, double wpm, std::int64_t at = 0)
{
	SessionRecord result;
	result.finished_at = at;
	result.mode        = mode;
	result.language    = language;
	result.length      = 250;
	result.errors      = 3;
	result.metrics.wpm = wpm;
	result.metrics.cpm = wpm * 5.0;
	return result;
}

TEST(SessionHistory, KeepsRecordsInOrderAndDropsOldest)
{
	SessionHistory history;
	for (std::size_t i = 0; i < SessionHistory::K_MAX_RECORDS + 10; ++i)
		history.add(
		    record(TrainingMode::Free, Language::English, 40.0, static_cast<std::int64_t>(i)));

	ASSERT_EQ(history.records().size(), SessionHistory::K_MAX_RECORDS);
	EXPECT_EQ(history.records().front().finished_at, 10);
	EXPECT_EQ(history.records().back().finished_at,
	          static_cast<std::int64_t>(SessionHistory::K_MAX_RECORDS + 9));
}

TEST(SessionHistory, BestWpmIsPerModeAndLanguage)
{
	SessionHistory history;
	EXPECT_FALSE(history.best_wpm(TrainingMode::Smart, Language::Russian).has_value());

	history.add(record(TrainingMode::Smart, Language::Russian, 45.0));
	history.add(record(TrainingMode::Smart, Language::Russian, 52.0));
	history.add(record(TrainingMode::Smart, Language::English, 70.0));
	history.add(record(TrainingMode::Free, Language::Russian, 90.0));

	EXPECT_EQ(history.best_wpm(TrainingMode::Smart, Language::Russian), 52.0);
	EXPECT_EQ(history.best_wpm(TrainingMode::Smart, Language::English), 70.0);
	EXPECT_EQ(history.best_wpm(TrainingMode::Free, Language::Russian), 90.0);
	EXPECT_FALSE(history.best_wpm(TrainingMode::Free, Language::English).has_value());
}

TEST(SessionHistory, SaveAndLoadRoundTrip)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "history.json";

	SessionHistory original;
	auto           smart      = record(TrainingMode::Smart, Language::Russian, 61.5, 1'700'000'000);
	smart.metrics.accuracy    = 97.5;
	smart.metrics.consistency = 71.25;
	smart.metrics.elapsed_seconds = 48.0;
	smart.metrics.keystrokes      = 260;
	original.add(smart);
	original.add(record(TrainingMode::Free, Language::English, 38.0, 1'700'000'100));
	ASSERT_TRUE(original.save(path));

	SessionHistory loaded;
	loaded.load(path);

	ASSERT_EQ(loaded.records().size(), 2U);
	auto const& first = loaded.records().front();
	EXPECT_EQ(first.finished_at, 1'700'000'000);
	EXPECT_EQ(first.mode, TrainingMode::Smart);
	EXPECT_EQ(first.language, Language::Russian);
	EXPECT_EQ(first.length, 250U);
	EXPECT_EQ(first.errors, 3U);
	EXPECT_DOUBLE_EQ(first.metrics.wpm, 61.5);
	EXPECT_DOUBLE_EQ(first.metrics.accuracy, 97.5);
	EXPECT_DOUBLE_EQ(first.metrics.consistency, 71.25);
	EXPECT_DOUBLE_EQ(first.metrics.elapsed_seconds, 48.0);
	EXPECT_EQ(first.metrics.keystrokes, 260U);
	EXPECT_EQ(loaded.records().back().mode, TrainingMode::Free);
}

TEST(SessionHistory, CorruptedFileStartsEmptyAndIsKept)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "history.json";
	{
		std::ofstream file(path);
		file << R"({"version": 1, "sessions": [{"wpm": "oops"}]})";
	}

	SessionHistory history;
	history.load(path);

	EXPECT_TRUE(history.records().empty());
	EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(path) += ".bak"));
}

} // namespace
} // namespace typing_trainer
