#include "metrics.hpp"

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

TEST(RunningStats, MatchesDirectComputation)
{
	RunningStats stats;
	for (double const value : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0})
		stats.add(value);

	EXPECT_EQ(stats.count(), 8U);
	EXPECT_DOUBLE_EQ(stats.mean(), 5.0);
	EXPECT_DOUBLE_EQ(stats.stddev(), 2.0);
	EXPECT_DOUBLE_EQ(stats.coefficient_of_variation(), 0.4);
}

TEST(RunningStats, EmptyIsZero)
{
	RunningStats const stats;
	EXPECT_EQ(stats.count(), 0U);
	EXPECT_DOUBLE_EQ(stats.stddev(), 0.0);
	EXPECT_DOUBLE_EQ(stats.coefficient_of_variation(), 0.0);
}

TEST(Consistency, PerfectRhythmIsHundredPercent)
{ EXPECT_DOUBLE_EQ(consistency_percent(0.0), 100.0); }

TEST(Consistency, DecreasesWithSpreadAndStaysNonNegative)
{
	double previous = consistency_percent(0.0);
	for (int step = 1; step <= 30; ++step)
	{
		double const cv      = step / 10.0;
		double const current = consistency_percent(cv);
		// При большом разбросе tanh насыщается, и значение упирается в ноль.
		if (cv <= 1.5) EXPECT_LT(current, previous);
		else EXPECT_LE(current, previous);
		EXPECT_GE(current, 0.0);
		previous = current;
	}
}

} // namespace
} // namespace typing_trainer
