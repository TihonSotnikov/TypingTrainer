#include "json_storage.hpp"
#include "ngram_statistics.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace typing_trainer
{
namespace
{

using test::Clock;
using test::milliseconds;

std::optional<NgramScore> find(const std::vector<NgramScore>& ranked, const std::u32string& gram)
{
	auto const it = std::ranges::find(ranked, gram, &NgramScore::gram);
	if (it == ranked.end()) return std::nullopt;
	return *it;
}

/// \brief Оценка граммы из рейтинга; отсутствие граммы - провал теста.
NgramScore score_of(const std::vector<NgramScore>& ranked, const std::u32string& gram)
{
	auto const score = find(ranked, gram);
	if (!score)
	{
		ADD_FAILURE() << "нет граммы в рейтинге";
		return {};
	}
	return *score;
}

double weight_of(const std::vector<NgramScore>& ranked, const std::u32string& gram)
{ return score_of(ranked, gram).weight; }

/// \brief Напечатать пары символов: first через first_ms, second через second_ms.
Clock::time_point type_pairs(NgramStatistics& stats, char32_t first, char32_t second, int first_ms,
                             int second_ms, int repeats, bool second_correct = true,
                             Clock::time_point from = Clock::time_point{})
{
	auto time = from;
	for (int i = 0; i < repeats; ++i)
	{
		time += milliseconds(first_ms);
		stats.feed(first, time, true);
		time += milliseconds(second_ms);
		stats.feed(second, time, second_correct);
	}
	return time;
}

TEST(NgramStatistics, SlowTransitionRanksHigher)
{
	NgramStatistics stats;
	type_pairs(stats, U'a', U'b', 100, 400, 10); // b после a даётся тяжело

	auto const ranked = stats.ranked();
	EXPECT_GT(weight_of(ranked, U"b"), weight_of(ranked, U"a"));
	EXPECT_GT(weight_of(ranked, U"ab"), weight_of(ranked, U"ba"));
	EXPECT_TRUE(std::ranges::is_sorted(ranked, std::ranges::greater{}, &NgramScore::weight));
}

TEST(NgramStatistics, ErrorsIncreaseWeight)
{
	NgramStatistics stats;
	auto            time = Clock::time_point{};
	for (int i = 0; i < 10; ++i)
	{
		time += milliseconds(200);
		stats.feed(U'x', time, true);
		time += milliseconds(200);
		stats.feed(U'y', time, i % 2 == 0); // половина нажатий y - ошибки
	}

	auto const ranked = stats.ranked();
	EXPECT_GT(weight_of(ranked, U"y"), weight_of(ranked, U"x"));
	EXPECT_NEAR(score_of(ranked, U"y").error_rate, 0.5, 0.05);
	EXPECT_EQ(score_of(ranked, U"y").attempts, 10U);
}

TEST(NgramStatistics, RareNgramsAreIgnored)
{
	NgramStatistics stats;
	type_pairs(stats, U'a', U'b', 150, 150, 2);

	EXPECT_TRUE(stats.ranked().empty());
	EXPECT_EQ(stats.ranked(/*min_attempts=*/1).size(), 6U); // a, b, ab, ba, aba, bab
}

TEST(NgramStatistics, ResetContextBreaksNgrams)
{
	NgramStatistics stats;
	auto            time = Clock::time_point{};
	for (int i = 0; i < 10; ++i)
	{
		stats.feed(U'a', time += milliseconds(150), true);
		stats.reset_context();
		stats.feed(U'b', time += milliseconds(150), true);
		stats.reset_context();
	}

	auto const ranked = stats.ranked();
	EXPECT_TRUE(find(ranked, U"a").has_value());
	EXPECT_FALSE(find(ranked, U"ab").has_value());
	EXPECT_FALSE(find(ranked, U"ba").has_value());
}

TEST(NgramStatistics, LongPausesDoNotAffectTiming)
{
	NgramStatistics stats;
	auto            time = Clock::time_point{};
	for (int i = 0; i < 10; ++i)
		stats.feed(U'a', time += milliseconds(200), true);
	stats.feed(U'a', time += milliseconds(10'000), true); // «задумался»

	EXPECT_NEAR(score_of(stats.ranked(), U"a").avg_time, 0.2, 1e-9);
}

TEST(NgramStatistics, RecentObservationsOutweighOldOnes)
{
	NgramStatistics stats;
	// Долго печатал «a» медленно, потом натренировался.
	auto const time = type_pairs(stats, U'a', U'b', 500, 150, 100);
	type_pairs(stats, U'a', U'b', 120, 150, 200, true, time);

	// Старые 0.5 с почти забыты: среднее близко к свежим 0.12 с.
	EXPECT_NEAR(score_of(stats.ranked(), U"a").avg_time, 0.12, 0.01);
}

TEST(NgramStatistics, RecoveredErrorsFadeAway)
{
	NgramStatistics stats;
	auto const      time = type_pairs(stats, U'a', U'b', 150, 150, 50, /*second_correct=*/false);
	type_pairs(stats, U'a', U'b', 150, 150, 300, true, time);

	EXPECT_LT(score_of(stats.ranked(), U"b").error_rate, 0.01);
}

TEST(NgramStatistics, EvidenceOutweighsNoise)
{
	NgramStatistics stats;
	auto            time = Clock::time_point{};
	// Фон: много верных нажатий разных букв задаёт общую низкую долю ошибок.
	for (int i = 0; i < 300; ++i)
		stats.feed(U"cdefgh"[i % 6], time += milliseconds(200), true);
	stats.reset_context();

	// q: 1 ошибка из 5 (20%) - может быть случайностью.
	for (int i = 0; i < 5; ++i)
	{
		stats.feed(U'q', time += milliseconds(200), i != 0);
		stats.reset_context();
	}
	// z: 15% ошибок на 200 попытках - устойчивая проблема.
	for (int i = 0; i < 200; ++i)
	{
		stats.feed(U'z', time += milliseconds(200), i % 20 >= 3);
		stats.reset_context();
	}

	auto const ranked = stats.ranked();
	EXPECT_GT(score_of(ranked, U"q").error_rate, score_of(ranked, U"z").error_rate);
	EXPECT_GT(weight_of(ranked, U"z"), weight_of(ranked, U"q"));
}

TEST(NgramStatistics, ClearForgetsEverything)
{
	NgramStatistics stats;
	type_pairs(stats, U'a', U'b', 150, 150, 10);
	ASSERT_FALSE(stats.empty());

	stats.clear();
	EXPECT_TRUE(stats.empty());
	EXPECT_TRUE(stats.ranked(1).empty());
}

TEST(NgramStatistics, LetterNgramsDropNonLettersAndMergeCase)
{
	std::vector<NgramScore> const ranked = {
	    {.gram = U"e ", .avg_time = 0.5, .error_rate = 0.0, .attempts = 10, .weight = 0.9},
	    {.gram = U"Th", .avg_time = 0.4, .error_rate = 0.0, .attempts = 10, .weight = 0.6},
	    {.gram = U"th", .avg_time = 0.2, .error_rate = 0.1, .attempts = 30, .weight = 0.3},
	    {.gram = U"a,", .avg_time = 0.5, .error_rate = 0.0, .attempts = 10, .weight = 0.8},
	    {.gram = U"щ", .avg_time = 0.3, .error_rate = 0.2, .attempts = 10, .weight = 0.5},
	};

	auto const letters = letter_ngrams(ranked);
	ASSERT_EQ(letters.size(), 2U);
	EXPECT_EQ(letters[0].gram, U"щ");
	EXPECT_EQ(letters[1].gram, U"th");
	EXPECT_EQ(letters[1].attempts, 40U);
	EXPECT_NEAR(letters[1].avg_time, ((0.4 * 10) + (0.2 * 30)) / 40, 1e-12);
	EXPECT_NEAR(letters[1].weight, ((0.6 * 10) + (0.3 * 30)) / 40, 1e-12);
}

TEST(NgramStatistics, SaveAndLoadRoundTrip)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "ngram_stats.json";

	NgramStatistics original;
	type_pairs(original, U'п', U'р', 180, 260, 20);
	ASSERT_TRUE(original.save(path));

	NgramStatistics loaded;
	loaded.load(path);

	auto const expected = original.ranked();
	auto const actual   = loaded.ranked();
	ASSERT_EQ(actual.size(), expected.size());
	for (std::size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(actual[i].gram, expected[i].gram);
		EXPECT_DOUBLE_EQ(actual[i].weight, expected[i].weight);
		EXPECT_EQ(actual[i].attempts, expected[i].attempts);
	}
}

TEST(NgramStatistics, LoadsLegacyVersion2Files)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "ngram_stats.json";

	nlohmann::json const legacy = {{"version", 2},
	                               {"ngrams",
	                                {{{"gram", {104}},
	                                  {"occurrences", 8},
	                                  {"total_time", 2.4},
	                                  {"errors", 2},
	                                  {"attempts", 10}},
	                                 {{"gram", {116, 104}},
	                                  {"occurrences", 400},
	                                  {"total_time", 80.0},
	                                  {"errors", 100},
	                                  {"attempts", 500}}}}};
	ASSERT_TRUE(storage::write_json(path, legacy));

	NgramStatistics stats;
	stats.load(path);

	auto const ranked = stats.ranked();
	auto const h      = score_of(ranked, U"h");
	auto const th     = score_of(ranked, U"th");
	EXPECT_NEAR(h.avg_time, 0.3, 1e-12);
	EXPECT_NEAR(h.error_rate, 0.2, 1e-12);
	EXPECT_EQ(h.attempts, 10U);
	EXPECT_NEAR(th.avg_time, 0.2, 1e-12);
	EXPECT_NEAR(th.error_rate, 0.2, 1e-12);
	EXPECT_EQ(th.attempts, 500U);
}

TEST(NgramStatistics, IncompatibleFileIsBackedUpInsteadOfOverwritten)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "ngram_stats.json";
	ASSERT_TRUE(storage::write_json(
	    path, nlohmann::json{{"version", 999}, {"ngrams", nlohmann::json::array()}}));

	NgramStatistics stats;
	stats.load(path);

	EXPECT_TRUE(stats.empty());
	EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(path) += ".bak"));
}

} // namespace
} // namespace typing_trainer
