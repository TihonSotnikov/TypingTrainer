#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json_fwd.hpp> // NOLINT(misc-include-cleaner)
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace typing_trainer
{

/// \brief Накопленная статистика по одной n-грамме.
///
/// Время и ошибки считаются с затуханием: новое наблюдение весит 1, а все прежние
/// умножаются на NgramStatistics::K_DECAY. Поэтому статистика отражает примерно
/// последние 1 / (1 - K_DECAY) нажатий и успевает за прогрессом пользователя.
struct NgramStat
{
	std::uint64_t attempts       = 0;   ///< Все нажатия за всё время, без затухания.
	double        attempt_weight = 0.0; ///< Затухающая сумма попыток.
	double        error_weight   = 0.0; ///< Затухающая сумма ошибок.
	double        time_weight    = 0.0; ///< Затухающая сумма замеров времени.
	double        avg_time       = 0.0; ///< Затухающее среднее flight-time, с.
};

/// \brief n-грамма с оценкой проблемности.
struct NgramScore
{
	std::u32string gram;
	double         avg_time   = 0.0; ///< Средний flight-time по свежим замерам, с.
	double         error_rate = 0.0; ///< Доля ошибок по свежим попыткам, 0..1.
	std::uint64_t  attempts   = 0;   ///< Все нажатия за всё время.
	double         weight     = 0.0; ///< Проблемность W = T + λ·E по сглаженным T и E.
};

/// \brief Сбор статистики проблемных n-грамм (1..3 символа) по потоку нажатий.
///
/// Ключ карты - целевая (ожидаемая) подстрока, поэтому контекст всегда чистый,
/// без опечаток пользователя.
///
/// \note НЕ потокобезопасен: владелец - worker-поток ядра, доступ только из него.
class NgramStatistics
{
public:
	NgramStatistics()  = default;
	~NgramStatistics() = default;

	NgramStatistics(const NgramStatistics&)            = delete;
	NgramStatistics(NgramStatistics&&)                 = delete;
	NgramStatistics& operator=(const NgramStatistics&) = delete;
	NgramStatistics& operator=(NgramStatistics&&)      = delete;

	/// \brief Учесть одно нажатие на текущей позиции целевого текста.
	/// \param expected  Ожидаемый символ позиции (из него строится контекст n-грамм).
	/// \param timestamp Время нажатия; flight-time от прошлой позиции считается внутри.
	/// \param correct   true, если введён ожидаемый символ.
	void feed(char32_t expected, std::chrono::steady_clock::time_point timestamp, bool correct);

	/// \brief Сбросить контекст набора (окно символов + точку отсчёта времени),
	///        не затрагивая накопленную статистику.
	/// \note Вызывать там, где разрыв до следующего символа невалиден как flight-time:
	///       старт сессии, backspace, пауза.
	void reset_context();

	/// \brief Забыть всю накопленную статистику.
	void clear();

	/// \brief Нет ни одного наблюдения.
	[[nodiscard]] bool empty() const { return stats_.empty(); }

	/// \brief Атомарно сохранить накопленную статистику в JSON-файл.
	/// \return true при успешной записи.
	[[nodiscard]] bool save(const std::filesystem::path& path) const;

	/// \brief Загрузить статистику из JSON-файла (поддерживается и формат v2).
	/// \note Отсутствие, повреждение или несовместимость по версии не считаются ошибкой:
	///       статистика просто остаётся пустой, а непрочитанный файл откладывается
	///       как .bak, чтобы не затереть его при следующем сохранении.
	void load(const std::filesystem::path& path);

	/// \brief Все n-граммы с оценкой проблемности, по убыванию веса.
	/// \param min_attempts Граммы с меньшим числом попыток отбрасываются как шум.
	/// \note Время и доля ошибок редких грамм сглаживаются к средним по всем буквам,
	///       чтобы пара случайных промахов не выводила грамму в лидеры.
	[[nodiscard]] std::vector<NgramScore> ranked(std::uint64_t min_attempts = K_MIN_ATTEMPTS) const;

	/// \brief Множитель затухания старых наблюдений на каждое новое.
	static constexpr double K_DECAY = 0.98;
	/// \brief Порог попыток по умолчанию: ниже - шум.
	static constexpr std::uint64_t K_MIN_ATTEMPTS = 5;

private:
	/// \brief Записать наблюдение во все n-граммы (1..3), оканчивающиеся на expected.
	void accumulate(char32_t expected, std::optional<double> flight_sec, bool correct);

	static constexpr double      K_MAX_FLIGHT_SECONDS = 1.5; ///< Отсечение пауз/выбросов.
	static constexpr std::size_t K_MAX_CONTEXT        = 2;   ///< До 2 символов слева -> n до 3.
	static constexpr double      K_ERROR_WEIGHT       = 1.0; ///< λ: вес доли ошибок в W.
	static constexpr double      K_PRIOR_STRENGTH     = 5.0; ///< Сила сглаживания, в наблюдениях.

	std::unordered_map<std::u32string, NgramStat> stats_;
	std::u32string context_; ///< До K_MAX_CONTEXT последних expected.
	std::optional<std::chrono::steady_clock::time_point> last_ts_;
};

/// \brief Только буквенные n-граммы в нижнем регистре, по убыванию веса.
/// \note Граммы с пробелами, цифрами и знаками отбрасываются (в словарных словах их нет),
///       варианты одной граммы в разном регистре объединяются.
[[nodiscard]] std::vector<NgramScore> letter_ngrams(const std::vector<NgramScore>& ranked);

/// \note Для nlohmann.
void to_json(nlohmann::json& j, const NgramStat& stat);
/// \note Для nlohmann.
void from_json(const nlohmann::json& j, NgramStat& stat);

} // namespace typing_trainer
