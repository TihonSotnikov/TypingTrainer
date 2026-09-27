#include "json_storage.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace typing_trainer
{
namespace
{

namespace fs = std::filesystem;

TEST(JsonStorage, WriteThenReadRoundTrip)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "data.json";

	nlohmann::json const document = {{"version", 1}, {"items", {1, 2, 3}}};
	ASSERT_TRUE(storage::write_json(path, document));

	EXPECT_EQ(storage::read_json(path), document);
}

TEST(JsonStorage, MissingFileReadsAsNothing)
{
	test::TempDir const dir;
	EXPECT_FALSE(storage::read_json(dir.path() / "absent.json").has_value());
}

TEST(JsonStorage, WriteCreatesMissingDirectoriesAndLeavesNoTempFile)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "nested" / "deeper" / "data.json";

	ASSERT_TRUE(storage::write_json(path, nlohmann::json{{"a", 1}}));
	EXPECT_TRUE(fs::exists(path));
	EXPECT_FALSE(fs::exists(fs::path(path) += ".tmp"));
}

TEST(JsonStorage, WriteReplacesExistingFile)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "data.json";

	ASSERT_TRUE(storage::write_json(path, nlohmann::json{{"a", 1}}));
	ASSERT_TRUE(storage::write_json(path, nlohmann::json{{"a", 2}}));

	EXPECT_EQ(storage::read_json(path), (nlohmann::json{{"a", 2}}));
}

TEST(JsonStorage, CorruptedFileIsMovedAsideAsBackup)
{
	test::TempDir const dir;
	auto const          path = dir.path() / "data.json";
	{
		std::ofstream file(path);
		file << "{ not a json";
	}

	EXPECT_FALSE(storage::read_json(path).has_value());
	EXPECT_FALSE(fs::exists(path));
	EXPECT_TRUE(fs::exists(fs::path(path) += ".bak"));
}

} // namespace
} // namespace typing_trainer
