#include "dictionaries.hpp"

#include "../contracts.hpp"

#include <string>
#include <vector>

namespace typing_trainer
{

const std::vector<std::u32string>& dictionary(Language language)
{
	// Слова берутся из dictionaries/*.txt, см. tt_embed_word_list в CMakeLists.txt.
	static const std::vector<std::u32string> dict_en = {
#include "dictionary_en.inc"
	};

	static const std::vector<std::u32string> dict_ru = {
#include "dictionary_ru.inc"
	};

	switch (language)
	{
	case Language::Russian:
		return dict_ru;
	case Language::English:
		break;
	}
	return dict_en;
}

} // namespace typing_trainer
