#pragma once

#include "../concurrent_queue.hpp"
#include "../contracts.hpp"
#include "session_engine.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace typing_trainer
{

/// \brief Реализация ядра тренажера слепой печати.
/// Принимает события ввода, обрабатывает их в фоновом потоке через SessionEngine
/// и складывает результаты в очередь для интерфейса.
class TypingTrainerCore final : public ITypingTrainerCore
{
public:
	/// \param data_dir Каталог пользовательских данных (статистика n-грамм).
	///                 Пустой путь - работа без сохранения на диск.
	explicit TypingTrainerCore(const std::filesystem::path& data_dir);
	~TypingTrainerCore() override;

	TypingTrainerCore(const TypingTrainerCore&)            = delete;
	TypingTrainerCore(TypingTrainerCore&&)                 = delete;
	TypingTrainerCore& operator=(const TypingTrainerCore&) = delete;
	TypingTrainerCore& operator=(TypingTrainerCore&&)      = delete;

	/// \brief Отправка события ввода во внутреннюю очередь бэкенда.
	void push_input(InputEvent event) override;

	/// \brief Неблокирующее извлечение события для графического интерфейса.
	std::optional<BackendEvent> poll_output() override;

	/// \brief Установка функции обратного вызова для уведомления UI о наличии данных.
	void set_output_ready_callback(std::function<void()> callback) override;

private:
	/// \brief Основной рабочий цикл фонового потока: работает, пока очередь не закрыта.
	void process_loop();

	/// \brief Потокобезопасный вызов колбэка интерфейса.
	void notify_ui();

	// Очереди асинхронного обмена сообщениями
	ConcurrentQueue<InputEvent>   input_queue_;
	ConcurrentQueue<BackendEvent> output_queue_;

	// Синхронизация доступа к callback-функции
	std::mutex            callback_mutex_;
	std::function<void()> output_ready_callback_;

	// Логика тренировки. Доступ только из рабочего потока.
	SessionEngine engine_;

	// Фоновый поток обработки ввода. Объявлен последним: останавливается раньше,
	// чем разрушаются данные, с которыми он работает.
	std::thread worker_thread_;
};

} // namespace typing_trainer
