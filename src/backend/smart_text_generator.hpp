#pragma once

#include "ngram_statistics.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace typing_trainer
{

/// \brief Генератор практического текста для Smart-режима.
///
/// Из проблемных n-грамм и словаря собирает строку, насыщенную
/// проблемными граммами, но с долей обычных слов для читаемости.
class SmartTextGenerator
{
public:
	/// \param seed Зерно генератора случайных чисел (фиксированное - для тестов).
	explicit SmartTextGenerator(std::uint32_t seed = std::random_device{}());
	~SmartTextGenerator() = default;

	SmartTextGenerator(const SmartTextGenerator&)            = delete;
	SmartTextGenerator(SmartTextGenerator&&)                 = delete;
	SmartTextGenerator& operator=(const SmartTextGenerator&) = delete;
	SmartTextGenerator& operator=(SmartTextGenerator&&)      = delete;

	/// \brief Сгенерировать батч практики.
	/// \param weak_ngrams   Проблемные граммы по убыванию веса. Граммы с символами, которых
	///                      нет в словаре (другой язык, пробелы, знаки), не учитываются.
	/// \param dictionary    Частотный словарь языка (слова в нижнем регистре).
	/// \param filler_ratio  Доля обычных слов (0..1): чем больше, тем легче текст.
	/// \param target_length Минимальная длина текста в символах.
	/// \return Текст практики; пустая строка, если словарь пуст.
	[[nodiscard]] std::u32string generate(const std::vector<NgramScore>&     weak_ngrams,
	                                      const std::vector<std::u32string>& dictionary,
	                                      double filler_ratio, std::size_t target_length);

	/// \brief Сколько худших грамм участвуют в генерации.
	static constexpr std::size_t K_TARGET_NGRAMS = 30;

private:
	std::mt19937 rng_;
};

} // namespace typing_trainer
