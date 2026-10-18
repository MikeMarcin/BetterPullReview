#include "bpr/adapters/sqlite/insights_storage.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::uint64_t mib = 1024U * 1024U;
		constexpr std::array    sidecars{ std::string_view{ "" }, std::string_view{ "-wal" }, std::string_view{ "-shm" }, std::string_view{ "-journal" }, std::string_view{ "-super-journal" } };

		auto add_bytes(std::uint64_t& aTotal, const std::uint64_t aValue) -> bool
		{
			if (aValue > std::numeric_limits<std::uint64_t>::max() - aTotal) {
				return false;
			}
			aTotal += aValue;
			return true;
		}

		auto directory_bytes(const std::filesystem::path& aPath) -> std::optional<std::uint64_t>
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (status.type() == std::filesystem::file_type::not_found) {
				return 0;
			}
			if (error || !std::filesystem::is_directory(status)) {
				return std::nullopt;
			}
			std::uint64_t total = 0;
			std::size_t   count = 0;
			for (auto it = std::filesystem::recursive_directory_iterator{ aPath, std::filesystem::directory_options::none, error };
				 !error && it != std::filesystem::recursive_directory_iterator{ }; it.increment(error)) {
				if (++count > 65536 || it.depth() > 16) {
					return std::nullopt;
				}
				const auto entry = it->symlink_status(error);
				if (error || std::filesystem::is_symlink(entry)) {
					return std::nullopt;
				}
				if (std::filesystem::is_directory(entry)) {
					continue;
				}
				if (!std::filesystem::is_regular_file(entry)) {
					return std::nullopt;
				}
				const auto length = it->file_size(error);
				if (error || !add_bytes(total, length)) {
					return std::nullopt;
				}
			}
			return error ? std::nullopt : std::optional{ total };
		}

		auto state_rows(const std::filesystem::path& aRoot) -> std::vector<application::insights_storage_row>
		{
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				throw std::runtime_error{ "Insights state unavailable" };
			}
			std::map<std::string, std::uint64_t> sizes;
			std::error_code                      error;
			std::size_t                          count = 0;
			for (auto it = std::filesystem::directory_iterator{ directory->path(), error }; !error && it != std::filesystem::directory_iterator{ }; it.increment(error)) {
				if (++count > 4096) {
					throw std::runtime_error{ "Insights store limit" };
				}
				const auto name      = it->path().filename().string();
				const auto extension = name.find(".sqlite3");
				if (extension == std::string::npos || std::ranges::find(sidecars, std::string_view{ name }.substr(extension + 8)) == sidecars.end()) {
					continue;
				}
				if (!std::filesystem::is_regular_file(it->symlink_status(error)) || error) {
					throw std::runtime_error{ "Insights store refused" };
				}
				const auto length = it->file_size(error);
				if (error || !add_bytes(sizes[name.substr(0, extension)], length)) {
					throw std::runtime_error{ "Insights size unavailable" };
				}
			}
			if (error) {
				throw std::runtime_error{ "Insights inventory unavailable" };
			}
			std::vector<application::insights_storage_row> result;
			for (const auto& [name, length] : sizes) {
				std::optional<std::uint64_t> budget;
				if (name == "markdown-images") {
					budget = 128U * mib;
				}
				if (name == "review-cycles") {
					budget = 768U * mib;
				}
				result.push_back({ name + ".sqlite3", length, budget, false });
			}
			result.push_back({ "Diagnostic logs", directory_bytes(aRoot / "logs"), 50U * mib, false });
			result.push_back({ "Review Cycle publication objects", directory_bytes(aRoot / "publication-git-v1"), std::nullopt, false });
			return result;
		}

		struct connection
		{
			detail::state_directory                               Directory;
			std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> Database{ nullptr, sqlite3_close_v2 };
		};
		auto open_database(const std::filesystem::path& aRoot) -> std::unique_ptr<connection>
		{
			if (!detail::sqlite_runtime_ready()) {
				return { };
			}
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				return { };
			}
			auto handle = detail::open_private_store_database(*directory, "insights.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_NOFOLLOW);
			if (!handle) {
				return { };
			}
			auto       opened   = std::make_unique<connection>(connection{ std::move(*directory), { *handle, sqlite3_close_v2 } });
			auto*      db       = opened->Database.get();
			const auto identity = detail::scalar_integer(db, "PRAGMA application_id");
			const auto version  = detail::scalar_integer(db, "PRAGMA user_version");
			if (!identity || (*identity != 0 && *identity != 1112557902) || !version || *version > 1) {
				return { };
			}
			if (!detail::configure_connection(db)) {
				return { };
			}
			if (*version == 0) {
				constexpr std::array schema{
					"BEGIN IMMEDIATE", "PRAGMA application_id=1112557902",
					"CREATE TABLE insights_metadata(singleton INTEGER PRIMARY KEY CHECK(singleton=1), started_unix_seconds INTEGER NOT NULL) STRICT",
					"INSERT INTO insights_metadata VALUES(1,unixepoch())",
					"CREATE TABLE insights_storage_summary(singleton INTEGER PRIMARY KEY CHECK(singleton=1),bytes INTEGER,git_bytes INTEGER) STRICT",
					"INSERT INTO insights_storage_summary VALUES(1,NULL,NULL)",
					"CREATE TABLE insights_repository_labels(cache TEXT PRIMARY KEY CHECK(length(cache)=64),name TEXT NOT NULL CHECK(length(name)<=1024)) STRICT",
					"PRAGMA user_version=1", "COMMIT"
				};
				for (const auto sql : schema) {
					if (!detail::execute(db, sql)) {
						return { };
					}
				}
			}
			return opened;
		}

		class storage final : public application::insights_storage_port
		{
		public:
			storage(std::filesystem::path aRoot, std::function<std::vector<application::insights_storage_row>()> aGit)
				: Root{ std::move(aRoot) }
				, Git{ std::move(aGit) }
				, Available{ static_cast<bool>(open_database(Root)) }
			{ }
			auto measure() -> application::insights_snapshot override
			{
				auto result = application::insights_snapshot{ };
				if (!Available) {
					return result;
				}
				result.Storage = state_rows(Root);
				auto git       = Git();
				if (auto connection = open_database(Root)) {
					for (auto& row : git) {
						auto query = detail::prepare(connection->Database.get(), "SELECT name FROM insights_repository_labels WHERE cache=?1");
						if (query && sqlite3_bind_text(query->Value, 1, row.Name.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query->Value) == SQLITE_ROW) {
							const auto* name   = sqlite3_column_text(query->Value, 0);
							const auto  length = sqlite3_column_bytes(query->Value, 0);
							if (name && length > 0 && length <= 1024) {
								row.Name.assign(reinterpret_cast<const char*>(name), static_cast<std::size_t>(length));
							}
						} else if (row.Name.size() == 64) {
							row.Name = "Repository cache " + row.Name.substr(0, 12);
						}
					}
				}
				std::uint64_t total    = 0;
				std::uint64_t gitTotal = 0;
				bool          complete = true;
				for (const auto& row : result.Storage) {
					complete = row.Bytes && add_bytes(total, *row.Bytes) && complete;
				}
				for (const auto& row : git) {
					complete = row.Bytes && add_bytes(gitTotal, *row.Bytes) && complete;
				}
				result.Storage.push_back({ "Git cache", complete ? std::optional{ gitTotal } : std::nullopt, std::nullopt, false });
				result.Storage.insert(result.Storage.end(), git.begin(), git.end());
				if (complete && add_bytes(total, gitTotal)) {
					result.TotalBytes = total;
					result.GitBytes   = gitTotal;
					if (auto connection = open_database(Root); connection && total <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
						auto query = detail::prepare(connection->Database.get(), "UPDATE insights_storage_summary SET bytes=?1,git_bytes=?2 WHERE singleton=1");
						if (query) {
							sqlite3_bind_int64(query->Value, 1, static_cast<std::int64_t>(total));
							sqlite3_bind_int64(query->Value, 2, static_cast<std::int64_t>(gitTotal));
							static_cast<void>(sqlite3_step(query->Value));
						}
					}
				}
				return result;
			}

		private:
			std::filesystem::path                                           Root;
			std::function<std::vector<application::insights_storage_row>()> Git;
			bool                                                            Available = false;
		};
	}

	auto make_insights_storage(const std::filesystem::path& aRoot, std::function<std::vector<application::insights_storage_row>()> aGitInventory) -> std::unique_ptr<application::insights_storage_port>
	{
		return std::make_unique<storage>(aRoot, std::move(aGitInventory));
	}

	auto store_insights_repository_label(const std::filesystem::path& aRoot, const std::string& aCache, const std::string& aName) -> bool
	{
		if (aCache.size() != 64 || aName.empty() || aName.size() > 1024) {
			return false;
		}
		auto connection = open_database(aRoot);
		if (!connection) {
			return false;
		}
		auto query = detail::prepare(connection->Database.get(), "INSERT INTO insights_repository_labels(cache,name) VALUES(?1,?2) ON CONFLICT(cache) DO UPDATE SET name=excluded.name");
		return query && sqlite3_bind_text(query->Value, 1, aCache.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK
			&& sqlite3_bind_text(query->Value, 2, aName.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query->Value) == SQLITE_DONE;
	}

	auto insights_storage_export(const std::filesystem::path& aRoot) -> std::string
	{
		try {
			auto connection = open_database(aRoot);
			if (!connection) {
				return "{\"section\":\"insights\",\"storage_available\":false}\n";
			}
			auto query = detail::prepare(connection->Database.get(), "SELECT bytes,git_bytes FROM insights_storage_summary WHERE singleton=1");
			if (!query || sqlite3_step(query->Value) != SQLITE_ROW || sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER || sqlite3_column_type(query->Value, 1) != SQLITE_INTEGER) {
				return "{\"section\":\"insights\",\"storage_available\":false}\n";
			}
			return "{\"section\":\"insights\",\"local_store_bytes\":" + std::to_string(sqlite3_column_int64(query->Value, 0)) + ",\"git_cache_bytes\":" + std::to_string(sqlite3_column_int64(query->Value, 1)) + "}\n";
		} catch (...) {
			return "{\"section\":\"insights\",\"storage_available\":false}\n";
		}
	}
}
