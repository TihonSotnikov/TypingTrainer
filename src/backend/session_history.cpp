#include "session_history.hpp"

#include "../contracts.hpp"
#include "json_storage.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace typing_trainer
{

namespace
{

constexpr int K_SCHEMA_VERSION = 1;

std::string to_string(TrainingMode mode) { return mode == TrainingMode::Smart ? "smart" : "free"; }

std::string to_string(Language language) { return language == Language::Russian ? "ru" : "en"; }

nlohmann::json to_json(const SessionRecord& record)
{
	return nlohmann::json{
	    {"finished_at", record.finished_at},
	    {"mode", to_string(record.mode)},
	    {"language", to_string(record.language)},
	    {"length", record.length},
	    {"errors", record.errors},
	    {"wpm", record.metrics.wpm},
	    {"cpm", record.metrics.cpm},
	    {"accuracy", record.metrics.accuracy},
	    {"consistency", record.metrics.consistency},
	    {"duration", record.metrics.elapsed_seconds},
	    {"keystrokes", record.metrics.keystrokes},
	};
}

SessionRecord from_json(const nlohmann::json& j)
{
	SessionRecord record;
	record.finished_at = j.at("finished_at").get<std::int64_t>();
	record.mode
	    = (j.at("mode").get<std::string>() == "smart") ? TrainingMode::Smart : TrainingMode::Free;
	record.language
	    = (j.at("language").get<std::string>() == "ru") ? Language::Russian : Language::English;
	record.length                  = j.at("length").get<std::size_t>();
	record.errors                  = j.at("errors").get<std::size_t>();
	record.metrics.wpm             = j.at("wpm").get<double>();
	record.metrics.cpm             = j.at("cpm").get<double>();
	record.metrics.accuracy        = j.at("accuracy").get<double>();
	record.metrics.consistency     = j.at("consistency").get<double>();
	record.metrics.elapsed_seconds = j.at("duration").get<double>();
	record.metrics.keystrokes      = j.at("keystrokes").get<std::size_t>();
	return record;
}

} // namespace

void SessionHistory::add(const SessionRecord& record)
{
	records_.push_back(record);
	if (records_.size() > K_MAX_RECORDS)
	{
		auto const excess = static_cast<std::ptrdiff_t>(records_.size() - K_MAX_RECORDS);
		records_.erase(records_.begin(), std::next(records_.begin(), excess));
	}
}

std::optional<double> SessionHistory::best_wpm(TrainingMode mode, Language language) const
{
	std::optional<double> best;
	for (auto const& record : records_)
	{
		if (record.mode != mode) continue;
		if (mode == TrainingMode::Smart && record.language != language) continue;
		if (!best || record.metrics.wpm > *best) best = record.metrics.wpm;
	}
	return best;
}

bool SessionHistory::save(const std::filesystem::path& path) const
{
	nlohmann::json sessions = nlohmann::json::array();
	for (auto const& record : records_)
		sessions.push_back(to_json(record));

	return storage::write_json(
	    path, nlohmann::json{{"version", K_SCHEMA_VERSION}, {"sessions", std::move(sessions)}});
}

void SessionHistory::load(const std::filesystem::path& path)
{
	records_.clear();

	auto const root = storage::read_json(path);
	if (!root) return;

	try
	{
		if (root->value("version", 0) != K_SCHEMA_VERSION)
		{
			storage::backup_file(path);
			return;
		}
		for (auto const& entry : root->at("sessions"))
			add(from_json(entry));
	}
	catch (std::exception const&)
	{
		records_.clear();
		storage::backup_file(path);
	}
}

} // namespace typing_trainer
