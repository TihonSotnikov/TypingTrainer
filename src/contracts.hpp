#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace typing_trainer
{

// ----- БАЗОВЫЕ КОНТРАКТЫ -----

enum class TrainingMode : std::uint8_t
{
	Free, ///< Тренировка на загруженном пользователем тексте.
	Smart ///< Умная генерация текста на основе ошибок и других метрик.
};

/// \brief Язык для Smart-генерации.
enum class Language : std::uint8_t
{
	English,
	Russian
};

enum class CharStatus : std::uint8_t
{
	Pending,
	Correct,
	Wrong
};

/// \brief Статус жизненного цикла сессии.
enum class SessionStatus : std::uint8_t
{
	Inactive, ///< Не запущена.
	Active,   ///< Запущена и идет набор текста.
	Paused,   ///< Приостановлена. Ввод заблокирован.
	Completed ///< Успешно завершена.
};

struct CharState
{
	char32_t   character;
	CharStatus status;
};

struct SessionConfig
{
	TrainingMode   mode = TrainingMode::Free;
	std::u32string custom_text;
	bool           ignore_case   = false;
	Language       language      = Language::English; ///< Словарь для Smart-режима.
	double         filler_ratio  = 0.3; ///< Доля обычных слов в Smart-режиме («сложность»).
	std::size_t    target_length = 250; ///< Размер блока Smart-режима в символах.
};


// ----- FRONTEND -> BACKEND (ВВОД) -----

enum class ControlKey : std::uint8_t
{
	Backspace,
	Escape,
	Enter ///< Перевод строки, если текст ждёт его; в середине строки игнорируется.
};

struct KeyPressData
{
	std::variant<char32_t, ControlKey>    key;
	std::chrono::steady_clock::time_point timestamp; ///< Время нажатия для расчета скорости.
};

struct StartSessionCommand
{ SessionConfig config; };

/// \brief Команда начать заново на том же тексте с теми же настройками.
struct RestartSessionCommand
{};

struct StopSessionCommand
{};

/// \brief Команда приостановки сессии (от фронтенд-таймера или кнопки UI).
struct PauseSessionCommand
{};

/// \brief Команда возобновления сессии.
struct ResumeSessionCommand
{};

/// \brief Запрос накопленной статистики (ответ - StatisticsSnapshot).
struct RequestStatisticsCommand
{};

/// \brief Команда стереть статистику n-грамм и историю сессий (ответ - StatisticsSnapshot).
struct ResetStatisticsCommand
{};

using InputEvent = std::variant<KeyPressData, StartSessionCommand, RestartSessionCommand,
                                StopSessionCommand, PauseSessionCommand, ResumeSessionCommand,
                                RequestStatisticsCommand, ResetStatisticsCommand>;


// ----- BACKEND -> FRONTEND (ОБРАБОТКА И ВЫВОД) -----

struct SessionMetrics
{
	double      wpm             = 0.0;   ///< Слов в минуту (слово = 5 знаков).
	double      cpm             = 0.0;   ///< Верно набранных знаков в минуту.
	double      accuracy        = 100.0; ///< Точность в % (0..100).
	double      consistency     = 0.0;   ///< Ритмичность темпа в % (0..100); 0 - мало данных.
	double      elapsed_seconds = 0.0;   ///< Чистое время набора, с (без пауз и простоя).
	std::size_t keystrokes      = 0;     ///< Нажатий символьных клавиш за сессию.
};

/// \brief Полный снимок состояния для (ре)инициализации UI.
struct SessionState
{
	std::vector<CharState> chars;
	size_t                 cursor_position = 0;
	SessionMetrics         metrics;
	SessionStatus          status = SessionStatus::Inactive; ///< Текущий статус сессии.
};

/// \brief Дельта-обновление для быстрой отрисовки в процессе печати.
struct StateUpdate
{
	size_t         changed_index{}; ///< Позиция символа, изменившего статус.
	CharState      changed_char{};
	size_t         cursor_position{};
	SessionMetrics metrics;
	bool           is_completed = false;                 ///< Флаг успешного набора всего текста.
	SessionStatus  status       = SessionStatus::Active; ///< Текущий статус сессии.
};

/// \brief Нажатие отброшено: похоже на набор в раскладке другого алфавита.
/// \note Такое нажатие не считается ни ошибкой, ни попыткой - UI стоит подсказать
///       пользователю переключить раскладку.
struct LayoutMismatch
{
	char32_t expected{}; ///< Ожидаемый символ.
	char32_t pressed{};  ///< Фактически нажатый символ.
};

/// \brief Проблемность буквенного сочетания для отчётов.
struct NgramReport
{
	std::u32string gram;
	double         avg_time   = 0.0; ///< Средний интервал до символа, с.
	double         error_rate = 0.0; ///< Доля ошибок, 0..1.
	std::uint64_t  attempts   = 0;   ///< Сколько раз встречалось.
};

/// \brief Итог завершённой сессии - запись истории.
struct SessionRecord
{
	std::int64_t   finished_at = 0; ///< Unix-время завершения, с.
	TrainingMode   mode        = TrainingMode::Free;
	Language       language    = Language::English; ///< Язык текста.
	std::size_t    length      = 0;                 ///< Длина текста, символов.
	std::size_t    errors      = 0;                 ///< Ошибочных нажатий.
	SessionMetrics metrics;
};

/// \brief Результат только что завершённой сессии.
struct SessionResult
{
	SessionRecord            record;
	std::vector<NgramReport> weakest; ///< Самые проблемные сочетания этой сессии.
	bool is_personal_best = false; ///< Лучшая скорость среди прошлых сессий того же режима и языка.
};

/// \brief Накопленная статистика для экрана статистики.
struct StatisticsSnapshot
{
	std::vector<NgramReport>   weakest; ///< Самые проблемные сочетания за всё время.
	std::vector<SessionRecord> history; ///< Завершённые сессии в хронологическом порядке.
};

using BackendEvent
    = std::variant<SessionState, StateUpdate, SessionResult, StatisticsSnapshot, LayoutMismatch>;


// ----- ИНТЕРФЕЙС ВЗАИМОДЕЙСТВИЯ -----

/// Основной интерфейс взаимодействия frontend/backend, реализация в backend.
class ITypingTrainerCore
{
public:
	ITypingTrainerCore()          = default;
	virtual ~ITypingTrainerCore() = default;

	ITypingTrainerCore(const ITypingTrainerCore&) = delete;
	ITypingTrainerCore(ITypingTrainerCore&&)      = delete;

	ITypingTrainerCore& operator=(const ITypingTrainerCore&) = delete;
	ITypingTrainerCore& operator=(ITypingTrainerCore&&)      = delete;

	/// \brief Отправка события во внутреннюю очередь Backend.
	virtual void push_input(InputEvent event) = 0;

	/// \brief Неблокирующее извлечение события для Frontend.
	/// \return Событие BackendEvent или std::nullopt, если новых событий нет.
	virtual std::optional<BackendEvent> poll_output() = 0;

	/// \brief Установка callback'а для уведомления UI-потока о новых данных.
	/// \param callback Обратный вызов для уведомления UI.
	/// \note Для неблокирующего ожидания для непрерывной работы UI.
	virtual void set_output_ready_callback(std::function<void()> callback) = 0;
};

} // namespace typing_trainer
