#include "text_utils.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace typing_trainer
{

namespace
{

/// \brief Что делать с символом при нормализации.
enum class CharClass : std::uint8_t
{
	Keep,
	Space,
	Drop,
	DoubleQuote,
	SingleQuote,
	Dash,
	Ellipsis
};

CharClass classify(char32_t ch)
{
	switch (ch)
	{
	case U' ':
	case U'\t':
	case U'\n':
	case U'\v':
	case U'\f':
	case U'\r':
	case 0x0085: // NEXT LINE
	case 0x00A0: // неразрывный пробел
	case 0x1680: // пробел огамического письма
	case 0x2028: // разделитель строк
	case 0x2029: // разделитель абзацев
	case 0x202F: // узкий неразрывный пробел
	case 0x205F: // средний математический пробел
	case 0x3000: // идеографический пробел
		return CharClass::Space;

	case 0x00AD: // мягкий перенос
	case 0x200B: // пробел нулевой ширины
	case 0x200C: // разъединитель нулевой ширины
	case 0x200D: // соединитель нулевой ширины
	case 0x2060: // word joiner
	case 0xFEFF: // BOM
		return CharClass::Drop;

	case 0x00AB: // левая «ёлочка»
	case 0x00BB: // правая «ёлочка»
	case 0x201C: // левая английская двойная
	case 0x201D: // правая английская двойная
	case 0x201E: // нижняя двойная («лапка»)
	case 0x201F: // перевёрнутая двойная
	case 0x2033: // двойной штрих
		return CharClass::DoubleQuote;

	case 0x2018: // левая одинарная
	case 0x2019: // правая одинарная (апостроф)
	case 0x201A: // нижняя одинарная
	case 0x201B: // перевёрнутая одинарная
	case 0x2032: // штрих
		return CharClass::SingleQuote;

	case 0x2010: // дефис
	case 0x2011: // неразрывный дефис
	case 0x2012: // цифровое тире
	case 0x2013: // короткое тире
	case 0x2014: // длинное тире
	case 0x2015: // горизонтальная черта
	case 0x2212: // знак минуса
		return CharClass::Dash;

	case 0x2026: // многоточие
		return CharClass::Ellipsis;

	default:
		break;
	}

	if (ch >= 0x2000 && ch <= 0x200A) return CharClass::Space;           // типографские пробелы
	if (ch < 0x20 || (ch >= 0x7F && ch <= 0x9F)) return CharClass::Drop; // управляющие
	return CharClass::Keep;
}

} // namespace

Script script_of(char32_t ch)
{
	if ((ch >= U'A' && ch <= U'Z') || (ch >= U'a' && ch <= U'z')) return Script::Latin;
	// Latin-1 Supplement и Latin Extended-A/B, кроме знаков умножения и деления.
	if (ch >= 0x00C0 && ch <= 0x024F && ch != 0x00D7 && ch != 0x00F7) return Script::Latin;
	if (ch >= 0x0400 && ch <= 0x04FF) return Script::Cyrillic;
	return Script::Other;
}

char32_t to_lower(char32_t ch)
{
	if (ch >= U'A' && ch <= U'Z') return ch + 0x20;
	if (ch >= 0x00C0 && ch <= 0x00DE && ch != 0x00D7) return ch + 0x20; // заглавные Latin-1
	if (ch >= 0x0410 && ch <= 0x042F) return ch + 0x20;                 // А..Я -> а..я
	if (ch >= 0x0400 && ch <= 0x040F) return ch + 0x50;                 // Ѐ..Џ (в т.ч. Ё -> ё)
	return ch;
}

std::u32string to_lower(std::u32string_view text)
{
	std::u32string result(text);
	for (char32_t& ch : result)
		ch = to_lower(ch);
	return result;
}

bool is_layout_mismatch(char32_t expected, char32_t pressed)
{
	Script const expected_script = script_of(expected);
	Script const pressed_script  = script_of(pressed);
	return expected_script != Script::Other && pressed_script != Script::Other
	       && expected_script != pressed_script;
}

std::u32string normalize_text(std::u32string_view text)
{
	std::u32string result;
	result.reserve(text.size());

	bool       pending_space = false;
	auto const append        = [&](std::u32string_view piece) {
		if (pending_space && !result.empty()) result.push_back(U' ');
		pending_space = false;
		result.append(piece);
	};

	for (char32_t const ch : text)
	{
		switch (classify(ch))
		{
		case CharClass::Keep:
			append(std::u32string_view(&ch, 1));
			break;
		case CharClass::Space:
			pending_space = true;
			break;
		case CharClass::Drop:
			break;
		case CharClass::DoubleQuote:
			append(U"\"");
			break;
		case CharClass::SingleQuote:
			append(U"'");
			break;
		case CharClass::Dash:
			append(U"-");
			break;
		case CharClass::Ellipsis:
			append(U"...");
			break;
		}
	}

	return result;
}

} // namespace typing_trainer
