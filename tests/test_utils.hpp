#pragma once

#include "contracts.hpp"

#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace typing_trainer::test
{

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

/// \brief Уникальный временный каталог, удаляется в деструкторе.
class TempDir
{
public:
	TempDir()
	{
		std::random_device rd;
		path_ = std::filesystem::temp_directory_path()
		        / ("typing_trainer_test_" + std::to_string(rd()) + std::to_string(rd()));
		std::filesystem::create_directories(path_);
	}

	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(path_, ec);
	}

	TempDir(const TempDir&)            = delete;
	TempDir(TempDir&&)                 = delete;
	TempDir& operator=(const TempDir&) = delete;
	TempDir& operator=(TempDir&&)      = delete;

	[[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};

/// \brief Нажатие символа в заданный момент.
inline InputEvent key(char32_t ch, Clock::time_point at)
{ return KeyPressData{.key = ch, .timestamp = at}; }

/// \brief Нажатие управляющей клавиши в заданный момент.
inline InputEvent control(ControlKey ctrl, Clock::time_point at = Clock::now())
{ return KeyPressData{.key = ctrl, .timestamp = at}; }

/// \brief Команда старта свободной тренировки на тексте.
inline InputEvent start_free(std::u32string text, bool ignore_case = false)
{
	SessionConfig config;
	config.mode        = TrainingMode::Free;
	config.custom_text = std::move(text);
	config.ignore_case = ignore_case;
	return StartSessionCommand{.config = config};
}

/// \brief Все события заданного типа из пачки.
template<typename T>
std::vector<T> events_of(const std::vector<BackendEvent>& events)
{
	std::vector<T> result;
	for (auto const& event : events)
		if (auto const* value = std::get_if<T>(&event)) result.push_back(*value);
	return result;
}

} // namespace typing_trainer::test
