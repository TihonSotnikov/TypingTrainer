#include "session_engine.hpp"

#include "../contracts.hpp"
#include "dictionaries.hpp"
#include "text_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace typing_trainer
{

namespace
{

/// \brief Язык текста по преобладающему алфавиту букв.
Language detect_language(const std::u32string& text)
{
	std::size_t cyrillic = 0;
	std::size_t latin    = 0;
	for (char32_t const ch : text)
	{
		Script const script = script_of(ch);
		if (script == Script::Cyrillic) ++cyrillic;
		else if (script == Script::Latin) ++latin;
	}
	return (cyrillic > latin) ? Language::Russian : Language::English;
}

/// \brief Первые limit буквенных сочетаний в виде отчёта для UI.
std::vector<NgramReport> to_reports(const std::vector<NgramScore>& ranked, std::size_t limit)
{
	std::vector<NgramReport> reports;
	reports.reserve(std::min(limit, ranked.size()));
	for (auto const& score : ranked)
	{
		if (reports.size() >= limit) break;
		reports.push_back(NgramReport{.gram       = score.gram,
		                              .avg_time   = score.avg_time,
		                              .error_rate = score.error_rate,
		                              .attempts   = score.attempts});
	}
	return reports;
}

std::int64_t unix_now()
{
	auto const since_epoch = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
}

} // namespace

SessionEngine::SessionEngine(const std::filesystem::path& data_dir)
{
	if (data_dir.empty()) return;
	stats_path_   = data_dir / "ngram_stats.json";
	history_path_ = data_dir / "history.json";
}

void SessionEngine::load()
{
	if (!stats_path_.empty()) ngram_stats_.load(stats_path_);
	if (!history_path_.empty()) history_.load(history_path_);
	stats_dirty_   = false;
	history_dirty_ = false;
}

void SessionEngine::persist()
{
	// Не удалось записать (диск, права) - данные остаются «грязными», повторим позже.
	if (stats_dirty_ && !stats_path_.empty() && ngram_stats_.save(stats_path_))
		stats_dirty_ = false;
	if (history_dirty_ && !history_path_.empty() && history_.save(history_path_))
		history_dirty_ = false;
}

std::vector<BackendEvent> SessionEngine::handle(const InputEvent& event)
{
	std::vector<BackendEvent> out;
	std::visit(
	    [this, &out](auto&& arg) {
		    using T = std::decay_t<decltype(arg)>;
		    if constexpr (std::is_same_v<T, StartSessionCommand>) start_session(arg.config, out);
		    else if constexpr (std::is_same_v<T, RestartSessionCommand>) restart_session(out);
		    else if constexpr (std::is_same_v<T, StopSessionCommand>) stop_session(out);
		    else if constexpr (std::is_same_v<T, PauseSessionCommand>) pause_session(out);
		    else if constexpr (std::is_same_v<T, ResumeSessionCommand>) resume_session(out);
		    else if constexpr (std::is_same_v<T, KeyPressData>) process_key_press(arg, out);
		    else if constexpr (std::is_same_v<T, RequestStatisticsCommand>)
			    out.emplace_back(statistics_snapshot());
		    else if constexpr (std::is_same_v<T, ResetStatisticsCommand>)
		    {
			    reset_statistics();
			    out.emplace_back(statistics_snapshot());
		    }
	    },
	    event);
	return out;
}

void SessionEngine::start_session(const SessionConfig& config, std::vector<BackendEvent>& out)
{
	std::u32string text;
	if (config.mode == TrainingMode::Free) { text = normalize_text(config.custom_text); }
	else
	{
		auto const weak = letter_ngrams(ngram_stats_.ranked());
		text = smart_generator_.generate(weak, dictionary(config.language), config.filler_ratio,
		                                 config.target_length);
	}

	begin_session(std::move(text), config, out);
}

void SessionEngine::restart_session(std::vector<BackendEvent>& out)
{
	if (last_text_.empty()) return;
	begin_session(last_text_, config_, out);
}

void SessionEngine::begin_session(std::u32string text, const SessionConfig& config,
                                  std::vector<BackendEvent>& out)
{
	if (text.empty()) return;

	config_       = config;
	last_text_    = text;
	text_to_type_ = std::move(text);

	chars_.clear();
	chars_.reserve(text_to_type_.size());
	for (char32_t const ch : text_to_type_)
		chars_.push_back(CharState{.character = ch, .status = CharStatus::Pending});

	cursor_        = 0;
	status_        = SessionStatus::Active;
	accumulated_   = Clock::duration::zero();
	total_presses_ = 0;
	errors_count_  = 0;
	correct_count_ = 0;
	metrics_       = SessionMetrics{};
	segment_start_.reset();
	last_key_time_.reset();
	rhythm_.reset();
	session_ngrams_.clear();
	break_typing_chain();

	out.emplace_back(snapshot());
}

void SessionEngine::stop_session(std::vector<BackendEvent>& out)
{
	status_ = SessionStatus::Inactive;
	chars_.clear();
	text_to_type_.clear();
	cursor_      = 0;
	accumulated_ = Clock::duration::zero();
	metrics_     = SessionMetrics{};
	segment_start_.reset();
	last_key_time_.reset();

	persist();

	out.emplace_back(snapshot());
}

void SessionEngine::pause_session(std::vector<BackendEvent>& out)
{
	if (status_ != SessionStatus::Active) return;

	close_segment();
	break_typing_chain();
	status_ = SessionStatus::Paused;

	out.emplace_back(snapshot());
}

void SessionEngine::resume_session(std::vector<BackendEvent>& out)
{
	if (status_ != SessionStatus::Paused) return;

	// Новый отрезок начнётся с первого нажатия после паузы.
	status_ = SessionStatus::Active;

	out.emplace_back(snapshot());
}

void SessionEngine::process_key_press(const KeyPressData& key_data, std::vector<BackendEvent>& out)
{
	if (status_ != SessionStatus::Active) return;

	if (auto const* ctrl = std::get_if<ControlKey>(&key_data.key))
	{
		if (*ctrl == ControlKey::Escape) pause_session(out);
		else if (*ctrl == ControlKey::Backspace) process_backspace(out);
		return;
	}

	process_char(std::get<char32_t>(key_data.key), key_data.timestamp, out);
}

void SessionEngine::process_backspace(std::vector<BackendEvent>& out)
{
	break_typing_chain(); // перепечатывание даст мусорный контекст и интервалы

	if (cursor_ == 0) return;

	--cursor_;
	if (chars_.at(cursor_).status == CharStatus::Correct) --correct_count_;
	chars_.at(cursor_).status = CharStatus::Pending;

	out.emplace_back(StateUpdate{.changed_index   = cursor_,
	                             .changed_char    = chars_.at(cursor_),
	                             .cursor_position = cursor_,
	                             .metrics         = metrics_,
	                             .is_completed    = false,
	                             .status          = status_});
}

void SessionEngine::process_char(char32_t pressed, Clock::time_point timestamp,
                                 std::vector<BackendEvent>& out)
{
	if (cursor_ >= text_to_type_.size()) return;

	char32_t const expected = text_to_type_.at(cursor_);

	// Буква чужого алфавита - почти наверняка не та раскладка, а не опечатка.
	// Не засчитываем такое нажатие, чтобы не портить точность и статистику n-грамм.
	if (is_layout_mismatch(expected, pressed))
	{
		out.emplace_back(LayoutMismatch{.expected = expected, .pressed = pressed});
		return;
	}

	bool const is_correct
	    = config_.ignore_case ? (to_lower(pressed) == to_lower(expected)) : (pressed == expected);

	chars_.at(cursor_).status = is_correct ? CharStatus::Correct : CharStatus::Wrong;

	++total_presses_;
	if (is_correct) ++correct_count_;
	else ++errors_count_;

	// Время: отрезок начинается с первого нажатия, а не со старта или возобновления.
	if (!segment_start_) segment_start_ = timestamp;
	last_key_time_ = timestamp;

	// Ритм: интервалы между соседними нажатиями без раздумий.
	if (rhythm_anchor_)
	{
		double const interval = std::chrono::duration<double>(timestamp - *rhythm_anchor_).count();
		if (interval > 0.0 && interval <= K_MAX_RHYTHM_INTERVAL) rhythm_.add(interval);
	}
	rhythm_anchor_ = timestamp;

	ngram_stats_.feed(expected, timestamp, is_correct);
	session_ngrams_.feed(expected, timestamp, is_correct);
	stats_dirty_ = true;

	bool const is_completed = (cursor_ == text_to_type_.size() - 1);
	recalculate_metrics(timestamp, is_completed);

	if (is_completed)
	{
		status_ = SessionStatus::Completed;
		close_segment();
	}

	out.emplace_back(StateUpdate{.changed_index   = cursor_,
	                             .changed_char    = chars_.at(cursor_),
	                             .cursor_position = is_completed ? cursor_ : cursor_ + 1,
	                             .metrics         = metrics_,
	                             .is_completed    = is_completed,
	                             .status          = status_});

	if (is_completed)
	{
		out.emplace_back(finish_session());
		persist();
	}
	else
	{
		cursor_++;
	}
}

SessionResult SessionEngine::finish_session()
{
	SessionRecord const record{
	    .finished_at = unix_now(),
	    .mode        = config_.mode,
	    .language
	    = (config_.mode == TrainingMode::Smart) ? config_.language : detect_language(text_to_type_),
	    .length  = text_to_type_.size(),
	    .errors  = errors_count_,
	    .metrics = metrics_,
	};

	auto const previous_best = history_.best_wpm(record.mode, record.language);

	SessionResult result{
	    .record  = record,
	    .weakest = to_reports(letter_ngrams(session_ngrams_.ranked(K_SESSION_MIN_ATTEMPTS)),
		                      K_SESSION_WEAKEST),
	    .is_personal_best = previous_best.has_value() && record.metrics.wpm > *previous_best,
	};

	history_.add(record);
	history_dirty_ = true;

	return result;
}

StatisticsSnapshot SessionEngine::statistics_snapshot() const
{
	return StatisticsSnapshot{
	    .weakest = to_reports(letter_ngrams(ngram_stats_.ranked()), K_REPORT_WEAKEST),
	    .history = history_.records(),
	};
}

void SessionEngine::reset_statistics()
{
	ngram_stats_.clear();
	history_.clear();
	stats_dirty_   = true;
	history_dirty_ = true;
	persist();
}

void SessionEngine::close_segment()
{
	if (segment_start_ && last_key_time_) accumulated_ += *last_key_time_ - *segment_start_;
	segment_start_.reset();
	last_key_time_.reset();
}

void SessionEngine::break_typing_chain()
{
	rhythm_anchor_.reset();
	ngram_stats_.reset_context();
	session_ngrams_.reset_context();
}

double SessionEngine::elapsed_seconds(Clock::time_point now) const
{
	auto total = accumulated_;
	if (segment_start_) total += now - *segment_start_;
	return std::chrono::duration<double>(total).count();
}

void SessionEngine::recalculate_metrics(Clock::time_point now, bool is_final)
{
	double const seconds = elapsed_seconds(now);

	metrics_.elapsed_seconds = seconds;
	metrics_.keystrokes      = total_presses_;

	bool const enough_time = seconds >= K_MIN_SECONDS_FOR_SPEED || (is_final && seconds > 0.0);
	metrics_.cpm = enough_time ? static_cast<double>(correct_count_) / seconds * 60.0 : 0.0;
	metrics_.wpm = metrics_.cpm / 5.0;

	metrics_.accuracy = (total_presses_ > 0) ? (static_cast<double>(total_presses_ - errors_count_)
	                                            / static_cast<double>(total_presses_))
	                                               * 100.0
	                                         : 100.0;

	metrics_.consistency = (rhythm_.count() >= K_MIN_RHYTHM_SAMPLES)
	                           ? consistency_percent(rhythm_.coefficient_of_variation())
	                           : 0.0;
}

SessionState SessionEngine::snapshot() const
{
	return SessionState{
	    .chars = chars_, .cursor_position = cursor_, .metrics = metrics_, .status = status_};
}

} // namespace typing_trainer
