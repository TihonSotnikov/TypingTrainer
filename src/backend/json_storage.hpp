#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp> // NOLINT(misc-include-cleaner)
#include <optional>

/// \brief Надёжное хранение JSON-файлов пользовательских данных.
namespace typing_trainer::storage
{

/// \brief Прочитать JSON-документ из файла.
/// \return Документ; std::nullopt, если файла нет или он повреждён.
/// \note Повреждённый файл откладывается рядом как <имя>.bak, чтобы следующая
///       запись не уничтожила данные пользователя безвозвратно.
[[nodiscard]] std::optional<nlohmann::json> read_json(const std::filesystem::path& path);

/// \brief Атомарно записать JSON: сначала во временный файл, затем переименование.
/// \note Падение посреди записи оставляет прежний файл целым. Недостающие каталоги создаются.
/// \return true при успешной записи.
[[nodiscard]] bool write_json(const std::filesystem::path& path, const nlohmann::json& document);

/// \brief Отложить файл как <имя>.bak (старая копия заменяется).
void backup_file(const std::filesystem::path& path);

} // namespace typing_trainer::storage
