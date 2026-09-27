#include "ngram_statistics.hpp"

#include "json_storage.hpp"
#include "text_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace typing_trainer
{

namespace
{

constexpr int K_SCHEMA_VERSION        = 3; // v3: затухающие веса вместо счётчиков
constexpr int K_LEGACY_SCHEMA_VERSION = 2; // v2: occurrences / total_time / errors / attempts

/// \brief Равновесная сумма весов при затухании K_DECAY (≈ «окно» наблюдений).
constexpr double K_WEIGHT_CAP = 1.0 / (1.0 - NgramStatistics::K_DECAY);

/// \brief Перевести запись формата v2 (простые счётчики) в формат v3.
/// \note Старые счётчики ограничиваются «окном» затухания: средние сохраняются,
///       но новые наблюдения сразу получают свой обычный вес.
NgramStat from_legacy(const nlohmann::json& j)
{
	auto const occurrences = j.at("occurrences").get<std::uint64_t>();
	auto const total_time  = j.at("total_time").get<double>();
	auto const errors      = j.at("errors").get<std::uint64_t>();
	auto const attempts    = j.at("attempts").get<std::uint64_t>();

	NgramStat stat;
	stat.attempts       = attempts;
	stat.attempt_weight = std::min(static_cast<double>(attempts), K_WEIGHT_CAP);
	stat.error_weight   = (attempts > 0) ? stat.attempt_weight * static_cast<double>(errors)
	                                           / static_cast<double>(attempts)
	                                     : 0.0;
	stat.time_weight    = std::min(static_cast<double>(occurrences), K_WEIGHT_CAP);
	stat.avg_time       = (occurrences > 0) ? total_time / static_cast<double>(occurrences) : 0.0;
	return stat;
}

/// \brief Порядок «самые проблемные первыми»; при равенстве - по грамме, для детерминизма.
bool by_weight_desc(const NgramScore& a, const NgramScore& b)
{
	if (a.weight != b.weight) return a.weight > b.weight;
	return a.gram < b.gram;
}

} // namespace

void to_json(nlohmann::json& j, const NgramStat& stat)
{
	j = nlohmann::json{{"attempts", stat.attempts},
	                   {"attempt_weight", stat.attempt_weight},
	                   {"error_weight", stat.error_weight},
	                   {"time_weight", stat.time_weight},
	                   {"avg_time", stat.avg_time}};
}

void from_json(const nlohmann::json& j, NgramStat& stat)
{
	j.at("attempts").get_to(stat.attempts);
	j.at("attempt_weight").get_to(stat.attempt_weight);
	j.at("error_weight").get_to(stat.error_weight);
	j.at("time_weight").get_to(stat.time_weight);
	j.at("avg_time").get_to(stat.avg_time);
}

void NgramStatistics::feed(char32_t expected, std::chrono::steady_clock::time_point timestamp,
                           bool correct)
{
	std::optional<double> flight;
	if (last_ts_)
	{
		double const sec = std::chrono::duration<double>(timestamp - *last_ts_).count();
		// Пауза «подумать» даёт огромный интервал и портит T_avg - по времени такие
		// наблюдения отбрасываем. Точку отсчёта ниже всё равно сдвигаем: интервал
		// до следующего символа - реальное время и от текущего отброса не зависит.
		if (sec >= 0.0 && sec <= K_MAX_FLIGHT_SECONDS) flight = sec;
	}

	accumulate(expected, flight, correct);

	context_.push_back(expected);
	if (context_.size() > K_MAX_CONTEXT) context_.erase(context_.begin());
	last_ts_ = timestamp;
}

void NgramStatistics::accumulate(char32_t expected, std::optional<double> flight, bool correct)
{
	// Граммы, оканчивающиеся на expected, с наращиванием контекста слева:
	//   [expected] -> [c-1, expected] -> [c-2, c-1, expected].
	// Время идёт в среднее только для корректных нажатий с валидным flight;
	// ошибки учитываются всегда.
	std::u32string    gram(1, expected);
	std::size_t const ctx = context_.size();

	for (std::size_t k = 0; k <= ctx; ++k)
	{
		if (k > 0) gram.insert(gram.begin(), context_.at(ctx - k));

		NgramStat& stat = stats_[gram];
		++stat.attempts;
		stat.attempt_weight = (stat.attempt_weight * K_DECAY) + 1.0;
		stat.error_weight   = (stat.error_weight * K_DECAY) + (correct ? 0.0 : 1.0);

		if (correct && flight)
		{
			stat.time_weight = (stat.time_weight * K_DECAY) + 1.0;
			stat.avg_time += (*flight - stat.avg_time) / stat.time_weight;
		}
	}
}

void NgramStatistics::reset_context()
{
	context_.clear();
	last_ts_.reset();
}

void NgramStatistics::clear()
{
	stats_.clear();
	reset_context();
}

bool NgramStatistics::save(const std::filesystem::path& path) const
{
	nlohmann::json ngrams = nlohmann::json::array();
	for (auto const& [gram, stat] : stats_)
	{
		nlohmann::json entry = stat;
		entry.emplace("gram", std::vector<std::uint32_t>(gram.begin(), gram.end()));
		ngrams.push_back(std::move(entry));
	}

	nlohmann::json root;
	root.emplace("version", K_SCHEMA_VERSION);
	root.emplace("ngrams", std::move(ngrams));

	return storage::write_json(path, root);
}

void NgramStatistics::load(const std::filesystem::path& path)
{
	clear();

	auto const root = storage::read_json(path);
	if (!root) return;

	try
	{
		int const version = root->value("version", 0);
		if (version != K_SCHEMA_VERSION && version != K_LEGACY_SCHEMA_VERSION)
		{
			storage::backup_file(path);
			return;
		}

		for (auto const& entry : root->at("ngrams"))
		{
			auto const           code_points = entry.at("gram").get<std::vector<std::uint32_t>>();
			std::u32string const gram(code_points.begin(), code_points.end());
			stats_[gram]
			    = (version == K_SCHEMA_VERSION) ? entry.get<NgramStat>() : from_legacy(entry);
		}
	}
	catch (std::exception const&)
	{
		stats_.clear();
		storage::backup_file(path);
	}
}

std::vector<NgramScore> NgramStatistics::ranked(std::uint64_t min_attempts) const
{
	// Априорные значения - средние по всем одиночным символам.
	double time_sum = 0.0;
	double time_den = 0.0;
	double err_sum  = 0.0;
	double err_den  = 0.0;
	for (auto const& [gram, stat] : stats_)
	{
		if (gram.size() != 1) continue;
		time_sum += stat.avg_time * stat.time_weight;
		time_den += stat.time_weight;
		err_sum += stat.error_weight;
		err_den += stat.attempt_weight;
	}
	double const prior_time  = (time_den > 0.0) ? time_sum / time_den : 0.0;
	double const prior_error = (err_den > 0.0) ? err_sum / err_den : 0.0;

	std::vector<NgramScore> result;
	result.reserve(stats_.size());

	for (auto const& [gram, stat] : stats_)
	{
		if (stat.attempts < min_attempts) continue;

		// Сглаживание: K_PRIOR_STRENGTH «воображаемых» наблюдений со средними значениями.
		double const smoothed_time
		    = ((stat.avg_time * stat.time_weight) + (prior_time * K_PRIOR_STRENGTH))
		      / (stat.time_weight + K_PRIOR_STRENGTH);
		double const smoothed_error = (stat.error_weight + (prior_error * K_PRIOR_STRENGTH))
		                              / (stat.attempt_weight + K_PRIOR_STRENGTH);

		result.push_back(NgramScore{
		    .gram     = gram,
		    .avg_time = stat.avg_time,
		    .error_rate
		    = (stat.attempt_weight > 0.0) ? stat.error_weight / stat.attempt_weight : 0.0,
		    .attempts = stat.attempts,
		    .weight   = smoothed_time + (K_ERROR_WEIGHT * smoothed_error),
		});
	}

	std::ranges::sort(result, by_weight_desc);
	return result;
}

std::vector<NgramScore> letter_ngrams(const std::vector<NgramScore>& ranked)
{
	struct Accumulator
	{
		double        time_sum   = 0.0;
		double        error_sum  = 0.0;
		double        weight_sum = 0.0;
		std::uint64_t attempts   = 0;
	};

	std::unordered_map<std::u32string, Accumulator> merged;
	for (auto const& score : ranked)
	{
		if (!std::ranges::all_of(score.gram, is_letter)) continue;

		auto const   weight = static_cast<double>(score.attempts);
		Accumulator& acc    = merged[to_lower(score.gram)];
		acc.time_sum += score.avg_time * weight;
		acc.error_sum += score.error_rate * weight;
		acc.weight_sum += score.weight * weight;
		acc.attempts += score.attempts;
	}

	std::vector<NgramScore> result;
	result.reserve(merged.size());
	for (auto& [gram, acc] : merged)
	{
		if (acc.attempts == 0) continue;
		auto const n = static_cast<double>(acc.attempts);
		result.push_back(NgramScore{.gram       = gram,
		                            .avg_time   = acc.time_sum / n,
		                            .error_rate = acc.error_sum / n,
		                            .attempts   = acc.attempts,
		                            .weight     = acc.weight_sum / n});
	}

	std::ranges::sort(result, by_weight_desc);
	return result;
}

} // namespace typing_trainer
