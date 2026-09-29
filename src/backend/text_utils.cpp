#include "text_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace typing_trainer
{

namespace
{

/// \brief Ширина табуляции в отступе строки, пробелов.
constexpr std::size_t K_TAB_WIDTH = 4;

/// \brief Что делать с символом при нормализации.
enum class CharClass : std::uint8_t
{
	Keep,
	Space,
	LineBreak,
	Drop,
	DoubleQuote,
	SingleQuote,
	Dash,
	Ellipsis
};

/// \brief Комбинируемый знак: ударение, диерезис и т.п. поверх предыдущей буквы.
bool is_combining_mark(char32_t ch)
{
	return (ch >= 0x0300 && ch <= 0x036F)     // основные
	       || (ch >= 0x1AB0 && ch <= 0x1AFF)  // расширенные
	       || (ch >= 0x1DC0 && ch <= 0x1DFF)  // дополнительные
	       || (ch >= 0x20D0 && ch <= 0x20FF)  // для символов
	       || (ch >= 0xFE20 && ch <= 0xFE2F); // половинные
}

CharClass classify(char32_t ch)
{
	switch (ch)
	{
	case U'\n':
	case U'\v':
	case U'\f':
	case U'\r':
	case 0x0085: // NEXT LINE
	case 0x2028: // разделитель строк
	case 0x2029: // разделитель абзацев
		return CharClass::LineBreak;

	case U' ':
	case U'\t':
	case 0x00A0: // неразрывный пробел
	case 0x1680: // пробел огамического письма
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
	if (is_combining_mark(ch)) return CharClass::Drop;
	return CharClass::Keep;
}

/// \brief Строка текста: отступ в пробелах и содержимое без него.
struct Line
{
	std::size_t    indent = 0;
	std::u32string body;
};

/// \brief Отступ после ещё одного пробельного символа в начале строки.
std::size_t widen_indent(std::size_t indent, char32_t space)
{ return (space == U'\t') ? ((indent / K_TAB_WIDTH) + 1) * K_TAB_WIDTH : indent + 1; }

/// \brief Собрать строки в текст: общий отступ убирается, пустые строки по краям
///        отбрасываются, несколько пустых подряд сводятся к одной.
/// \note Без общего отступа фрагмент кода из середины файла начинается с края,
///       но сохраняет вложенность.
std::u32string join_lines(const std::vector<Line>& lines)
{
	std::size_t common_indent = std::numeric_limits<std::size_t>::max();
	for (auto const& line : lines)
		if (!line.body.empty()) common_indent = std::min(common_indent, line.indent);

	std::u32string result;
	bool           blank_before = false;
	for (auto const& line : lines)
	{
		if (line.body.empty())
		{
			blank_before = !result.empty();
			continue;
		}
		if (!result.empty()) result += blank_before ? U"\n\n" : U"\n";
		result.append(line.indent - common_indent, U' ');
		result += line.body;
		blank_before = false;
	}
	return result;
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
	std::vector<Line> lines(1);
	bool              pending_space = false; // пробел внутри строки, ещё не записанный
	auto const        append        = [&](std::u32string_view piece) {
		Line& line = lines.back();
		if (pending_space && !line.body.empty()) line.body.push_back(U' ');
		pending_space = false;
		line.body.append(piece);
	};

	for (std::size_t i = 0; i < text.size(); ++i)
	{
		char32_t const ch = text.at(i);
		switch (classify(ch))
		{
		case CharClass::Keep:
			append(std::u32string_view(&ch, 1));
			break;
		case CharClass::Space:
			if (lines.back().body.empty())
				lines.back().indent = widen_indent(lines.back().indent, ch);
			else pending_space = true;
			break;
		case CharClass::LineBreak:
			if (ch == U'\r' && i + 1 < text.size() && text.at(i + 1) == U'\n') ++i; // CRLF
			lines.emplace_back();
			pending_space = false;
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

	return join_lines(lines);
}

} // namespace typing_trainer
