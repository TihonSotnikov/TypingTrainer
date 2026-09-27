#include "text_utils.hpp"

#include <initializer_list>
#include <string>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

/// \brief Строка из кодов символов: типографские и невидимые символы нагляднее задавать числами.
std::u32string chars(std::initializer_list<char32_t> codes) { return {codes}; }

TEST(TextUtils, ScriptOfLetters)
{
	EXPECT_EQ(script_of(U'a'), Script::Latin);
	EXPECT_EQ(script_of(U'Z'), Script::Latin);
	EXPECT_EQ(script_of(0x00E9), Script::Latin); // é
	EXPECT_EQ(script_of(U'ж'), Script::Cyrillic);
	EXPECT_EQ(script_of(U'Ё'), Script::Cyrillic);
	EXPECT_EQ(script_of(U'1'), Script::Other);
	EXPECT_EQ(script_of(U' '), Script::Other);
	EXPECT_EQ(script_of(U','), Script::Other);
	EXPECT_EQ(script_of(0x00D7), Script::Other); // знак умножения
}

TEST(TextUtils, ToLowerHandlesLatinAndCyrillic)
{
	EXPECT_EQ(to_lower(U'Q'), U'q');
	EXPECT_EQ(to_lower(U'Ж'), U'ж');
	EXPECT_EQ(to_lower(U'Ё'), U'ё');
	EXPECT_EQ(to_lower(U'я'), U'я');
	EXPECT_EQ(to_lower(U'7'), U'7');
	EXPECT_EQ(to_lower(U"Привет, World"), U"привет, world");
}

TEST(TextUtils, LayoutMismatchOnlyBetweenAlphabets)
{
	EXPECT_TRUE(is_layout_mismatch(U'р', U'h')); // русская «р» на месте английской «h»
	EXPECT_TRUE(is_layout_mismatch(U'h', U'р'));
	EXPECT_FALSE(is_layout_mismatch(U'h', U'j')); // обычная опечатка
	EXPECT_FALSE(is_layout_mismatch(U'.', U'ю')); // не буква - не судим
	EXPECT_FALSE(is_layout_mismatch(U'ж', U';'));
}

TEST(TextUtils, NormalizeCollapsesWhitespace)
{
	EXPECT_EQ(normalize_text(U"  first line\n\nsecond\tline  "), U"first line second line");
	EXPECT_EQ(normalize_text(chars({U'a', 0x00A0, U'b', 0x2009, U'c', 0x2028, U'd'})), U"a b c d");
}

TEST(TextUtils, NormalizeReplacesTypography)
{
	EXPECT_EQ(normalize_text(chars({0x00AB, U'x', 0x00BB})), U"\"x\"");
	EXPECT_EQ(normalize_text(chars({0x201C, U'y', 0x201D})), U"\"y\"");
	EXPECT_EQ(normalize_text(chars({U'I', 0x2019, U'm'})), U"I'm");
	EXPECT_EQ(normalize_text(chars({U'a', U' ', 0x2014, U' ', U'b', U' ', 0x2013, U' ', U'c'})),
	          U"a - b - c");
	EXPECT_EQ(normalize_text(chars({U'x', 0x2026})), U"x...");
}

TEST(TextUtils, NormalizeDropsInvisibleCharacters)
{ EXPECT_EQ(normalize_text(chars({0xFEFF, U'a', 0x00AD, U'b', 0x200B, U'c', 0x0007})), U"abc"); }

TEST(TextUtils, NormalizeKeepsRegularText)
{
	std::u32string const text
	    = U"Съешь же ещё этих мягких французских булок, да выпей чаю! №5 (ok)";
	EXPECT_EQ(normalize_text(text), text);
	EXPECT_TRUE(normalize_text(U" \n\t ").empty());
}

} // namespace
} // namespace typing_trainer
