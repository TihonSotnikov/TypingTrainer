#pragma once

#include "../contracts.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace typing_trainer
{

/// \brief История завершённых сессий с сохранением в JSON.
/// \note НЕ потокобезопасна: владелец - worker-поток ядра.
class SessionHistory
{
public:
	/// \brief Добавить запись; самые старые записи сверх лимита отбрасываются.
	void add(const SessionRecord& record);

	/// \brief Забыть всю историю.
	void clear() { records_.clear(); }

	/// \brief Записи в хронологическом порядке.
	[[nodiscard]] const std::vector<SessionRecord>& records() const { return records_; }

	/// \brief Лучшая скорость среди сессий того же режима и языка.
	/// \note Скорость на разных языках несравнима: другие буквы, другая привычка к раскладке.
	/// \return std::nullopt, если подходящих сессий ещё не было.
	[[nodiscard]] std::optional<double> best_wpm(TrainingMode mode, Language language) const;

	/// \brief Атомарно сохранить историю в JSON-файл.
	[[nodiscard]] bool save(const std::filesystem::path& path) const;

	/// \brief Загрузить историю; повреждённый или несовместимый файл откладывается как .bak.
	void load(const std::filesystem::path& path);

	/// \brief Сколько последних сессий хранить.
	static constexpr std::size_t K_MAX_RECORDS = 1000;

private:
	std::vector<SessionRecord> records_;
};

} // namespace typing_trainer
