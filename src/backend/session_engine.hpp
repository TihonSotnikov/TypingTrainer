#pragma once

#include "../contracts.hpp"
#include "metrics.hpp"
#include "ngram_statistics.hpp"
#include "session_history.hpp"
#include "smart_text_generator.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace typing_trainer
{

/// \brief Машина состояний тренировки: принимает события ввода и возвращает события для UI.
///
/// Класс полностью синхронный и ничего не знает о потоках - это делает его
/// детерминированным и удобным для тестов. Многопоточность обеспечивает TypingTrainerCore.
class SessionEngine
{
public:
	/// \param data_dir Каталог пользовательских данных. Пустой путь - работа без диска.
	explicit SessionEngine(const std::filesystem::path& data_dir = {});

	/// \brief Загрузить сохранённые статистику и историю из каталога данных.
	void load();

	/// \brief Сохранить статистику и историю, если они изменились с последнего сохранения.
	void persist();

	/// \brief Обработать одно событие ввода.
	/// \return События для интерфейса в порядке отправки (может быть пусто).
	[[nodiscard]] std::vector<BackendEvent> handle(const InputEvent& event);

private:
	using Clock = std::chrono::steady_clock;

	void start_session(const SessionConfig& config, std::vector<BackendEvent>& out);
	void restart_session(std::vector<BackendEvent>& out);
	void begin_session(std::u32string text, const SessionConfig& config,
	                   std::vector<BackendEvent>& out);
	void stop_session(std::vector<BackendEvent>& out);
	void pause_session(std::vector<BackendEvent>& out);
	void resume_session(std::vector<BackendEvent>& out);
	void process_key_press(const KeyPressData& key_data, std::vector<BackendEvent>& out);
	void process_backspace(std::vector<BackendEvent>& out);
	void process_char(char32_t pressed, Clock::time_point timestamp,
	                  std::vector<BackendEvent>& out);

	/// \brief Подвести итог завершённой сессии: запись в историю и отчёт для UI.
	[[nodiscard]] SessionResult finish_session();

	/// \brief Снимок накопленной статистики для UI.
	[[nodiscard]] StatisticsSnapshot statistics_snapshot() const;

	/// \brief Стереть статистику n-грамм и историю.
	void reset_statistics();

	/// \brief Закрыть текущий отрезок непрерывного набора.
	/// \note Время копится от первого до последнего нажатия отрезка, поэтому простой
	///       перед паузой (ручной или автоматической) в метрики не попадает.
	void close_segment();

	/// \brief Разорвать цепочку нажатий: интервал до следующего нажатия невалиден
	///        ни для ритма, ни для flight-time n-грамм.
	void break_typing_chain();

	/// \brief Чистое время набора на момент нажатия.
	[[nodiscard]] double elapsed_seconds(Clock::time_point now) const;

	/// \brief Перерасчёт метрик на момент нажатия.
	/// \param is_final Итоговый расчёт по завершении: скорость считается при любом времени.
	void recalculate_metrics(Clock::time_point now, bool is_final);

	/// \brief Полный снимок состояния для UI.
	[[nodiscard]] SessionState snapshot() const;

	/// \brief Скорость до этого времени набора не показываем: первые нажатия дают выбросы.
	static constexpr double K_MIN_SECONDS_FOR_SPEED = 1.0;
	/// \brief Интервалы длиннее - это раздумья, а не ритм.
	static constexpr double K_MAX_RHYTHM_INTERVAL = 2.0;
	/// \brief Минимум интервалов для осмысленной оценки ритма.
	static constexpr std::size_t K_MIN_RHYTHM_SAMPLES = 5;
	/// \brief Сколько слабых сочетаний показывать в итогах сессии.
	static constexpr std::size_t K_SESSION_WEAKEST = 5;
	/// \brief Порог попыток для сочетания в итогах одной сессии.
	static constexpr std::uint64_t K_SESSION_MIN_ATTEMPTS = 3;
	/// \brief Сколько слабых сочетаний отдавать на экран статистики.
	static constexpr std::size_t K_REPORT_WEAKEST = 60;

	// Хранилище
	std::filesystem::path stats_path_;   ///< Пустой - статистика не сохраняется.
	std::filesystem::path history_path_; ///< Пустой - история не сохраняется.
	bool                  stats_dirty_   = false;
	bool                  history_dirty_ = false;

	// Текущая сессия
	SessionStatus          status_ = SessionStatus::Inactive;
	SessionConfig          config_;    ///< Настройки последнего старта.
	std::u32string         last_text_; ///< Текст последнего старта (для «ещё раз»).
	std::u32string         text_to_type_;
	std::vector<CharState> chars_;
	std::size_t            cursor_ = 0;

	// Время: копится только внутри отрезков непрерывного набора
	Clock::duration                  accumulated_{0}; ///< Сумма закрытых отрезков.
	std::optional<Clock::time_point> segment_start_;  ///< Первое нажатие текущего отрезка.
	std::optional<Clock::time_point> last_key_time_;  ///< Последнее нажатие текущего отрезка.

	// Счётчики и метрики
	std::size_t                      total_presses_ = 0;
	std::size_t                      errors_count_  = 0;
	std::size_t                      correct_count_ = 0; ///< Символов в статусе Correct сейчас.
	RunningStats                     rhythm_;            ///< Интервалы между нажатиями, с.
	std::optional<Clock::time_point> rhythm_anchor_;     ///< Предыдущее нажатие цепочки.
	SessionMetrics                   metrics_;

	// Статистика и Smart-режим
	NgramStatistics    ngram_stats_;    ///< За всё время, сохраняется на диск.
	NgramStatistics    session_ngrams_; ///< Только текущая сессия - для её итогов.
	SessionHistory     history_;
	SmartTextGenerator smart_generator_;
};

} // namespace typing_trainer
