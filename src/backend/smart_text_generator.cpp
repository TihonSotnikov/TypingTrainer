#include "smart_text_generator.hpp"

#include "ngram_statistics.hpp"
#include "text_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace typing_trainer
{

namespace
{

/// \brief Предохранитель перевыбора слова (при двух словах в пуле шанс исчерпать его ~2^-64).
constexpr int K_MAX_PICK_ATTEMPTS = 64;

/// \brief Отобрать до K_TARGET_NGRAMS худших грамм, которые встречаются в словах словаря.
/// \note Так отсекаются переходы через пробел, знаки препинания и граммы другого языка.
std::vector<NgramScore> select_targets(const std::vector<NgramScore>&     weak_ngrams,
                                       const std::vector<std::u32string>& dictionary)
{
	std::unordered_set<char32_t> alphabet;
	for (auto const& word : dictionary)
		alphabet.insert(word.begin(), word.end());
	auto const in_alphabet = [&alphabet](char32_t ch) { return alphabet.contains(ch); };

	std::vector<NgramScore> targets;
	targets.reserve(SmartTextGenerator::K_TARGET_NGRAMS);
	for (auto const& ngram : weak_ngrams)
	{
		if (targets.size() >= SmartTextGenerator::K_TARGET_NGRAMS) break;

		std::u32string gram = to_lower(ngram.gram);
		if (gram.empty() || !std::ranges::all_of(gram, in_alphabet)) continue;
		if (std::ranges::find(targets, gram, &NgramScore::gram) != targets.end()) continue;

		targets.push_back(NgramScore{.gram = std::move(gram), .weight = ngram.weight});
	}
	return targets;
}

/// \brief Слова, содержащие целевые граммы, и их вес (сумма весов найденных грамм).
struct ScoredWords
{
	std::vector<const std::u32string*> words;
	std::vector<double>                weights;
};

ScoredWords score_words(const std::vector<std::u32string>& dictionary,
                        const std::vector<NgramScore>&     targets)
{
	ScoredWords scored;
	for (auto const& word : dictionary)
	{
		double score = 0.0;
		for (auto const& target : targets)
			if (word.find(target.gram) != std::u32string::npos) score += target.weight;

		if (score > 0.0)
		{
			scored.words.push_back(&word);
			scored.weights.push_back(score);
		}
	}
	return scored;
}

} // namespace

SmartTextGenerator::SmartTextGenerator(std::uint32_t seed) : rng_(seed) {}

std::u32string SmartTextGenerator::generate(const std::vector<NgramScore>&     weak_ngrams,
                                            const std::vector<std::u32string>& dictionary,
                                            double filler_ratio, std::size_t target_length)
{
	if (dictionary.empty()) return {};

	ScoredWords const scored     = score_words(dictionary, select_targets(weak_ngrams, dictionary));
	bool const        has_scored = !scored.words.empty();

	std::discrete_distribution<std::size_t>    weighted_pick(scored.weights.begin(),
	                                                         scored.weights.end());
	std::uniform_int_distribution<std::size_t> filler_pick(0, dictionary.size() - 1);
	std::uniform_real_distribution<double>     coin(0.0, 1.0);

	auto const pick_word = [&](bool filler) -> const std::u32string& {
		return filler ? dictionary.at(filler_pick(rng_)) : *scored.words.at(weighted_pick(rng_));
	};

	std::u32string        result;
	const std::u32string* previous = nullptr;
	while (result.size() < target_length)
	{
		bool const        take_filler = !has_scored || (coin(rng_) < filler_ratio);
		std::size_t const pool_size   = take_filler ? dictionary.size() : scored.words.size();

		// Одно и то же слово дважды подряд читается как ошибка генератора.
		const std::u32string* word = &pick_word(take_filler);
		for (int attempt = 1; attempt < K_MAX_PICK_ATTEMPTS && pool_size > 1 && previous != nullptr
		                      && *word == *previous;
		     ++attempt)
			word = &pick_word(take_filler);

		if (!result.empty()) result.push_back(U' ');
		result += *word;
		previous = word;
	}

	return result;
}

} // namespace typing_trainer
