#include "dictionaries.hpp"
#include "text_utils.hpp"

#include <algorithm>
#include <set>
#include <string>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

struct DictionaryCase
{
	Language language;
	Script   script;
};

class DictionaryTest : public ::testing::TestWithParam<DictionaryCase>
{};

TEST_P(DictionaryTest, IsLargeEnoughForVariedTexts)
{ EXPECT_GE(dictionary(GetParam().language).size(), 1000U); }

TEST_P(DictionaryTest, HasNoDuplicates)
{
	auto const&                    words = dictionary(GetParam().language);
	std::set<std::u32string> const unique(words.begin(), words.end());
	EXPECT_EQ(unique.size(), words.size());
}

TEST_P(DictionaryTest, ContainsOnlyLowercaseLettersOfItsAlphabet)
{
	for (auto const& word : dictionary(GetParam().language))
	{
		ASSERT_FALSE(word.empty());
		EXPECT_TRUE(std::ranges::all_of(word, [](char32_t ch) {
			return GetParam().script == script_of(ch);
		})) << "символ чужого алфавита";
		EXPECT_EQ(to_lower(word), word) << "заглавная буква в словаре";
	}
}

INSTANTIATE_TEST_SUITE_P(Languages, DictionaryTest,
                         ::testing::Values(DictionaryCase{Language::English, Script::Latin},
                                           DictionaryCase{Language::Russian, Script::Cyrillic}));

} // namespace
} // namespace typing_trainer
