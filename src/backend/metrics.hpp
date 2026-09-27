#pragma once

#include <cmath>
#include <cstddef>

namespace typing_trainer
{

/// \brief Онлайн-среднее и дисперсия (алгоритм Уэлфорда) без хранения выборки.
class RunningStats
{
public:
	void add(double value)
	{
		++count_;
		double const delta = value - mean_;
		mean_ += delta / static_cast<double>(count_);
		m2_ += delta * (value - mean_);
	}

	void reset() { *this = RunningStats{}; }

	[[nodiscard]] std::size_t count() const { return count_; }
	[[nodiscard]] double      mean() const { return mean_; }

	/// \brief Стандартное отклонение по генеральной совокупности.
	[[nodiscard]] double stddev() const
	{ return count_ > 0 ? std::sqrt(m2_ / static_cast<double>(count_)) : 0.0; }

	/// \brief Коэффициент вариации (stddev / mean); 0 для пустой выборки.
	[[nodiscard]] double coefficient_of_variation() const
	{ return mean_ > 0.0 ? stddev() / mean_ : 0.0; }

private:
	std::size_t count_ = 0;
	double      mean_  = 0.0;
	double      m2_    = 0.0;
};

/// \brief Ритмичность набора в процентах по коэффициенту вариации интервалов между нажатиями.
///
/// Идеально ровный темп (cv = 0) даёт 100%. Кривая 1 - tanh(cv + cv³/3 + cv⁵/5) плавно
/// уходит к нулю и не становится отрицательной при большом разбросе.
[[nodiscard]] inline double consistency_percent(double coefficient_of_variation)
{
	double const cv = coefficient_of_variation;
	double const x  = cv + (std::pow(cv, 3) / 3.0) + (std::pow(cv, 5) / 5.0);
	return 100.0 * (1.0 - std::tanh(x));
}

} // namespace typing_trainer
