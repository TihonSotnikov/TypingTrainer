#include "json_storage.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <system_error>

#include <nlohmann/json.hpp>

namespace typing_trainer::storage
{

namespace fs = std::filesystem;

std::optional<nlohmann::json> read_json(const fs::path& path)
{
	std::error_code ec;
	if (!fs::exists(path, ec)) return std::nullopt;

	nlohmann::json document;
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) return std::nullopt;
		document = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
	}

	if (document.is_discarded())
	{
		backup_file(path);
		return std::nullopt;
	}
	return document;
}

bool write_json(const fs::path& path, const nlohmann::json& document)
{
	std::error_code ec;
	if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);

	fs::path temp_path = path;
	temp_path += ".tmp";

	{
		std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
		if (!file) return false;
		file << document.dump(2);
		file.flush();
		if (!file)
		{
			file.close();
			fs::remove(temp_path, ec);
			return false;
		}
	}

	fs::rename(temp_path, path, ec);
	if (ec)
	{
		fs::remove(temp_path, ec);
		return false;
	}
	return true;
}

void backup_file(const fs::path& path)
{
	fs::path backup_path = path;
	backup_path += ".bak";

	std::error_code ec;
	fs::remove(backup_path, ec);
	fs::rename(path, backup_path, ec);
}

} // namespace typing_trainer::storage
