#include "typing_trainer_core.hpp"

#include "../contracts.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>

namespace typing_trainer
{

TypingTrainerCore::TypingTrainerCore(const std::filesystem::path& data_dir) :
    engine_(data_dir), worker_thread_([this] { process_loop(); })
{}

TypingTrainerCore::~TypingTrainerCore()
{
	// Закрытая очередь отдаёт оставшиеся события и отпускает поток. Дожидаемся его явно:
	// перед выходом он сохраняет статистику.
	input_queue_.close();
	if (worker_thread_.joinable()) worker_thread_.join();
}

void TypingTrainerCore::push_input(InputEvent event) { input_queue_.push(std::move(event)); }

std::optional<BackendEvent> TypingTrainerCore::poll_output() { return output_queue_.try_pop(); }

void TypingTrainerCore::set_output_ready_callback(std::function<void()> callback)
{
	std::scoped_lock const lock(callback_mutex_);
	output_ready_callback_ = std::move(callback);
}

void TypingTrainerCore::process_loop()
{
	engine_.load();

	while (auto event_opt = input_queue_.wait_and_pop())
	{
		auto events = engine_.handle(*event_opt);
		if (events.empty()) continue;

		for (auto& event : events)
			output_queue_.push(std::move(event));
		notify_ui();
	}

	// Приложение закрывают посреди сессии - накопленное за неё не должно пропасть.
	engine_.persist();
}

void TypingTrainerCore::notify_ui()
{
	std::scoped_lock const lock(callback_mutex_);
	if (output_ready_callback_) output_ready_callback_();
}

} // namespace typing_trainer
