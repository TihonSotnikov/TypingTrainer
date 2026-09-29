#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace typing_trainer
{

/// \brief Алфавит, к которому относится символ.
enum class Script : std::uint8_t
{
	Latin,
	Cyrillic,
	Other ///< Не буква: цифры, знаки препинания, пробелы и т.д.
};

/// \brief Алфавит буквы: латиница (включая Latin-1 и Latin Extended-A/B), кириллица или прочее.
[[nodiscard]] Script script_of(char32_t ch);

/// \brief Является ли символ буквой латиницы или кириллицы.
[[nodiscard]] inline bool is_letter(char32_t ch) { return script_of(ch) != Script::Other; }

/// \brief Нижний регистр для латиницы и кириллицы; прочие символы не меняются.
[[nodiscard]] char32_t to_lower(char32_t ch);

/// \brief Нижний регистр для строки.
[[nodiscard]] std::u32string to_lower(std::u32string_view text);

/// \brief Похоже ли нажатие на набор в раскладке другого алфавита.
/// \return true, если ожидалась буква одного алфавита, а нажата буква другого
///         (например, «р» вместо «h»).
[[nodiscard]] bool is_layout_mismatch(char32_t expected, char32_t pressed);

/// \brief Привести текст к виду, который можно набрать на обычной клавиатуре.
///
/// Любые пробельные символы (переводы строк, табуляции, неразрывные пробелы) сворачиваются
/// в один пробел, края обрезаются. Типографские кавычки заменяются на " и ', тире и минус -
/// на -, многоточие - на три точки. Невидимые и управляющие символы удаляются, как и
/// комбинируемые знаки (например, ударения): отдельно от буквы их не набрать.
/// \note Буквы вроде «й» и «ё» ожидаются в составной форме Unicode (NFC): в разложенной
///       они превратились бы в «и» и «е» без знака.
[[nodiscard]] std::u32string normalize_text(std::u32string_view text);

} // namespace typing_trainer
