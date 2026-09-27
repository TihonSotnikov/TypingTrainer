#include "smart_text_generator.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

std::vector<std::u32string> split_words(const std::u32string& text)
{
	std::vector<std::u32string> words;
	std::u32string              current;
	for (char32_t const ch : text)
		if (ch == U' ')
		{
			words.push_back(current);
			current.clear();
		}
		else
		{
			current.push_back(ch);
		}
	if (!current.empty()) words.push_back(current);
	return words;
}

std::vector<NgramScore> weak(std::u32string gram, double weight = 1.0)
{ return {NgramScore{.gram = std::move(gram), .weight = weight}}; }

const std::vector<std::u32string>& test_dictionary()
{
	static const std::vector<std::u32string> dictionary
	    = {U"apple", U"pizza", U"jazz", U"cat", U"dog", U"house", U"tree"};
	return dictionary;
}

TEST(SmartTextGenerator, EmptyDictionaryGivesEmptyText)
{
	SmartTextGenerator generator(1);
	EXPECT_TRUE(generator.generate({}, {}, 0.3, 100).empty());
}

TEST(SmartTextGenerator, ReachesTargetLengthWithDictionaryWords)
{
	SmartTextGenerator generator(2);
	auto const         text = generator.generate({}, test_dictionary(), 0.3, 200);

	EXPECT_GE(text.size(), 200U);
	for (auto const& word : split_words(text))
		EXPECT_NE(std::ranges::find(test_dictionary(), word), test_dictionary().end());
}

TEST(SmartTextGenerator, WithoutFillersEveryWordContainsWeakNgram)
{
	SmartTextGenerator generator(3);
	auto const         text = generator.generate(weak(U"zz"), test_dictionary(), 0.0, 150);

	for (auto const& word : split_words(text))
		EXPECT_NE(word.find(U"zz"), std::u32string::npos) << "слово без проблемной граммы";
}

TEST(SmartTextGenerator, WeakNgramsAreMatchedCaseInsensitively)
{
	SmartTextGenerator generator(4);
	auto const         text = generator.generate(weak(U"ZZ"), test_dictionary(), 0.0, 100);

	for (auto const& word : split_words(text))
		EXPECT_NE(word.find(U"zz"), std::u32string::npos);
}

TEST(SmartTextGenerator, NgramsOutsideDictionaryAlphabetAreSkipped)
{
	// Граммы другого языка и переходы через пробел не должны «съедать» места в топе.
	std::vector<NgramScore> ngrams;
	ngrams.reserve(42);
	for (int i = 0; i < 40; ++i)
		ngrams.push_back(
		    NgramScore{.gram = U"щ" + std::u32string(1, U'а' + (i % 20)), .weight = 9.0});
	ngrams.push_back(NgramScore{.gram = U"e ", .weight = 8.0});
	ngrams.push_back(NgramScore{.gram = U"zz", .weight = 1.0});

	SmartTextGenerator generator(5);
	auto const         text = generator.generate(ngrams, test_dictionary(), 0.0, 120);

	for (auto const& word : split_words(text))
		EXPECT_NE(word.find(U"zz"), std::u32string::npos);
}

TEST(SmartTextGenerator, SameWordIsNotRepeatedBackToBack)
{
	SmartTextGenerator generator(6);
	auto const         words = split_words(generator.generate({}, {U"one", U"two"}, 0.3, 400));

	for (std::size_t i = 1; i < words.size(); ++i)
		EXPECT_NE(words[i], words[i - 1]);
}

TEST(SmartTextGenerator, SameSeedGivesSameText)
{
	SmartTextGenerator first(42);
	SmartTextGenerator second(42);
	EXPECT_EQ(first.generate(weak(U"a"), test_dictionary(), 0.3, 100),
	          second.generate(weak(U"a"), test_dictionary(), 0.3, 100));
}

} // namespace
} // namespace typing_trainer
