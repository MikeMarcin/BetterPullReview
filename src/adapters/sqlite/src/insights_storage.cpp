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
			if (!identity || (*identity != 0 && *identity != 1112557902) || !version || *version > 4) {
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
			if (*version < 2) {
				constexpr std::array historySchema{
					"BEGIN IMMEDIATE",
					"CREATE TABLE insights_github_selection(singleton INTEGER PRIMARY KEY CHECK(singleton=1),host INTEGER NOT NULL,account INTEGER NOT NULL) STRICT",
					"CREATE TABLE insights_history(at INTEGER NOT NULL,host INTEGER NOT NULL,account INTEGER NOT NULL,kind INTEGER NOT NULL,resource INTEGER NOT NULL,category INTEGER NOT NULL,count INTEGER NOT NULL,lim INTEGER,remaining INTEGER,reset INTEGER NOT NULL,secondary INTEGER NOT NULL,retry INTEGER NOT NULL,failure INTEGER NOT NULL,duration INTEGER NOT NULL,launched INTEGER NOT NULL,quarantined INTEGER NOT NULL,abandoned INTEGER NOT NULL,PRIMARY KEY(at,host,account,kind,resource,category,reset,secondary,retry,failure,launched,quarantined,abandoned)) STRICT",
					"PRAGMA user_version=2", "COMMIT"
				};
				for (const auto sql : historySchema) {
					if (!detail::execute(db, sql)) {
						return { };
					}
				}
			}
			if (*version < 3) {
				constexpr std::array reviewSchema{
					"BEGIN IMMEDIATE",
					"CREATE TABLE insights_review_events(host TEXT NOT NULL,repository TEXT NOT NULL,pr INTEGER NOT NULL,kind INTEGER NOT NULL,identity TEXT NOT NULL,head TEXT NOT NULL,identity_day INTEGER NOT NULL,day INTEGER NOT NULL,at INTEGER NOT NULL,files INTEGER NOT NULL,c0 INTEGER NOT NULL CHECK(c0>=0),c1 INTEGER NOT NULL CHECK(c1>=0),c2 INTEGER NOT NULL CHECK(c2>=0),c3 INTEGER NOT NULL CHECK(c3>=0),c4 INTEGER NOT NULL CHECK(c4>=0),c5 INTEGER NOT NULL CHECK(c5>=0),c6 INTEGER NOT NULL CHECK(c6>=0),c7 INTEGER NOT NULL CHECK(c7>=0),c8 INTEGER NOT NULL CHECK(c8>=0),c9 INTEGER NOT NULL CHECK(c9>=0),c10 INTEGER NOT NULL CHECK(c10>=0),c11 INTEGER NOT NULL CHECK(c11>=0),c12 INTEGER NOT NULL CHECK(c12>=0),c13 INTEGER NOT NULL CHECK(c13>=0),c14 INTEGER NOT NULL CHECK(c14>=0),PRIMARY KEY(host,repository,pr,kind,identity,head,identity_day)) STRICT",
					"CREATE INDEX insights_review_days ON insights_review_events(day,repository)",
					"PRAGMA user_version=3", "COMMIT"
				};
				for (const auto sql : reviewSchema) {
					if (!detail::execute(db, sql)) {
						return { };
					}
				}
			}

			if (*version < 4) {
				constexpr std::array energySchema{
					"BEGIN IMMEDIATE",
					"CREATE TABLE insights_energy(at INTEGER NOT NULL,day INTEGER NOT NULL,width INTEGER NOT NULL,foreground INTEGER NOT NULL,samples INTEGER NOT NULL,wall INTEGER NOT NULL,cpu INTEGER NOT NULL,wakeups INTEGER,wake_wall INTEGER NOT NULL,gh_cpu INTEGER NOT NULL,git_cpu INTEGER NOT NULL,gh_count INTEGER NOT NULL,git_count INTEGER NOT NULL,gh_unknown INTEGER NOT NULL,git_unknown INTEGER NOT NULL,peak REAL NOT NULL,PRIMARY KEY(at,foreground,day)) STRICT",
					"PRAGMA user_version=4", "COMMIT"
				};
				for (const auto sql : energySchema) {
					if (!detail::execute(db, sql)) {
						return { };
					}
				}
			}

			return opened;
		}

		auto read_energy(sqlite3* aDatabase) -> std::vector<core::insights_energy_sample>
		{
			auto query = detail::prepare(aDatabase, "SELECT * FROM insights_energy ORDER BY at,foreground");
			if (!query) {
				throw std::runtime_error{ "Energy read unavailable" };
			}
			std::vector<core::insights_energy_sample> result;
			while (sqlite3_step(query->Value) == SQLITE_ROW) {
				const auto                   value = [&](int aColumn) { return sqlite3_column_int64(query->Value, aColumn); };
				core::insights_energy_sample sample;
				sample.AtMicroseconds   = value(0);
				sample.LocalDay         = value(1);
				sample.WidthSeconds     = value(2);
				sample.Foreground       = value(3) != 0;
				sample.Samples          = static_cast<std::uint64_t>(value(4));
				sample.WallMicroseconds = static_cast<std::uint64_t>(value(5));
				sample.CpuMicroseconds  = static_cast<std::uint64_t>(value(6));
				if (sqlite3_column_type(query->Value, 7) != SQLITE_NULL) {
					sample.Wakeups = static_cast<std::uint64_t>(value(7));
				}
				sample.WakeupWallMicroseconds = static_cast<std::uint64_t>(value(8));
				for (std::size_t index = 0; index < 2; ++index) {
					sample.ChildCpuMicroseconds[index] = static_cast<std::uint64_t>(value(static_cast<int>(9 + index)));
					sample.Children[index]             = static_cast<std::uint64_t>(value(static_cast<int>(11 + index)));
					sample.UnmeasuredChildren[index]   = static_cast<std::uint64_t>(value(static_cast<int>(13 + index)));
				}
				sample.PeakLoad = sqlite3_column_double(query->Value, 15);
				result.push_back(sample);
				if (result.size() > 50000) {
					throw std::runtime_error{ "Energy history limit" };
				}
			}
			return result;
		}
		void write_energy(sqlite3* aDatabase, const core::insights_energy_sample& aSample)
		{
			auto query = detail::prepare(aDatabase, "INSERT INTO insights_energy VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16) ON CONFLICT DO UPDATE SET samples=samples+excluded.samples,wall=wall+excluded.wall,cpu=cpu+excluded.cpu,wakeups=CASE WHEN excluded.wakeups IS NULL THEN wakeups ELSE COALESCE(wakeups,0)+excluded.wakeups END,wake_wall=wake_wall+excluded.wake_wall,gh_cpu=gh_cpu+excluded.gh_cpu,git_cpu=git_cpu+excluded.git_cpu,gh_count=gh_count+excluded.gh_count,git_count=git_count+excluded.git_count,gh_unknown=gh_unknown+excluded.gh_unknown,git_unknown=git_unknown+excluded.git_unknown,peak=MAX(peak,excluded.peak)");
			if (!query) {
				throw std::runtime_error{ "Energy write unavailable" };
			}
			const std::array<std::int64_t, 15> values{ aSample.AtMicroseconds, aSample.LocalDay, aSample.WidthSeconds, aSample.Foreground ? 1 : 0, static_cast<std::int64_t>(aSample.Samples), static_cast<std::int64_t>(aSample.WallMicroseconds), static_cast<std::int64_t>(aSample.CpuMicroseconds), static_cast<std::int64_t>(aSample.Wakeups.value_or(0)), static_cast<std::int64_t>(aSample.WakeupWallMicroseconds), static_cast<std::int64_t>(aSample.ChildCpuMicroseconds[0]), static_cast<std::int64_t>(aSample.ChildCpuMicroseconds[1]), static_cast<std::int64_t>(aSample.Children[0]), static_cast<std::int64_t>(aSample.Children[1]), static_cast<std::int64_t>(aSample.UnmeasuredChildren[0]), static_cast<std::int64_t>(aSample.UnmeasuredChildren[1]) };
			for (std::size_t index = 0; index < values.size(); ++index) {
				if (index == 7 && !aSample.Wakeups) {
					sqlite3_bind_null(query->Value, 8);
				} else {
					sqlite3_bind_int64(query->Value, static_cast<int>(index + 1), values[index]);
				}
			}
			sqlite3_bind_double(query->Value, 16, aSample.PeakLoad);
			if (sqlite3_step(query->Value) != SQLITE_DONE) {
				throw std::runtime_error{ "Energy write failed" };
			}
		}
		void compact_energy(sqlite3* aDatabase, const std::int64_t aNow)
		{
			const auto retained = core::rollup_insights_energy(read_energy(aDatabase), aNow);
			if (!detail::execute(aDatabase, "DELETE FROM insights_energy")) {
				throw std::runtime_error{ "Energy retention failed" };
			}
			for (const auto& sample : retained) {
				write_energy(aDatabase, sample);
			}
		}

		constexpr auto insert_history = "INSERT INTO insights_history VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17) ON CONFLICT DO UPDATE SET count=count+excluded.count,lim=COALESCE(excluded.lim,lim),remaining=CASE WHEN remaining IS NULL THEN excluded.remaining WHEN excluded.remaining IS NULL THEN remaining ELSE MIN(remaining,excluded.remaining) END,duration=MAX(duration,excluded.duration)";
		auto           history_event(sqlite3_stmt* aQuery) -> core::insights_event
		{
			core::insights_event event;
			const auto           value = [aQuery](int aColumn) { return sqlite3_column_int64(aQuery, aColumn); };
			event.UnixSeconds          = value(0);
			event.Scope                = { static_cast<std::uint64_t>(value(1)), static_cast<std::uint64_t>(value(2)) };
			event.Kind                 = static_cast<core::insights_event_kind>(value(3));
			event.Resource             = static_cast<core::insights_resource>(value(4));
			event.Category             = static_cast<core::insights_category>(value(5));
			event.Count                = static_cast<std::uint64_t>(value(6));
			if (sqlite3_column_type(aQuery, 7) == SQLITE_INTEGER) {
				event.Limit = static_cast<std::uint64_t>(value(7));
			}
			if (sqlite3_column_type(aQuery, 8) == SQLITE_INTEGER) {
				event.Remaining = static_cast<std::uint64_t>(value(8));
			}
			event.ResetUnixSeconds = value(9);
			event.SecondaryLimited = value(10) != 0;
			if (value(11) >= 0) {
				event.RetryAfterSeconds = static_cast<std::uint32_t>(value(11));
			}
			event.ProcessFailure       = static_cast<core::insights_process_failure>(value(12));
			event.DurationMicroseconds = static_cast<std::uint64_t>(value(13));
			event.Launched             = value(14) != 0;
			event.Quarantined          = value(15) != 0;
			event.Abandoned            = value(16) != 0;
			return event;
		}
		auto insert_event(sqlite3* aDatabase, const core::insights_event& aEvent, const std::int64_t aWidth) -> bool
		{
			auto query = detail::prepare(aDatabase, insert_history);
			if (!query || aWidth <= 0) {
				return false;
			}
			const std::array<std::int64_t, 17> values{
				aEvent.UnixSeconds / aWidth * aWidth, static_cast<std::int64_t>(aEvent.Scope.Host), static_cast<std::int64_t>(aEvent.Scope.Account),
				static_cast<std::int64_t>(aEvent.Kind), static_cast<std::int64_t>(aEvent.Resource), static_cast<std::int64_t>(aEvent.Category), static_cast<std::int64_t>(aEvent.Count),
				static_cast<std::int64_t>(aEvent.Limit.value_or(0)), static_cast<std::int64_t>(aEvent.Remaining.value_or(0)), aEvent.ResetUnixSeconds,
				aEvent.SecondaryLimited ? 1 : 0, aEvent.RetryAfterSeconds ? static_cast<std::int64_t>(*aEvent.RetryAfterSeconds) : -1,
				static_cast<std::int64_t>(aEvent.ProcessFailure), static_cast<std::int64_t>(aEvent.DurationMicroseconds), aEvent.Launched ? 1 : 0, aEvent.Quarantined ? 1 : 0, aEvent.Abandoned ? 1 : 0
			};
			for (std::size_t index = 0; index < values.size(); ++index) {
				const auto parameter = static_cast<int>(index + 1);
				if ((index == 7 && !aEvent.Limit) || (index == 8 && !aEvent.Remaining)) {
					sqlite3_bind_null(query->Value, parameter);
				} else {
					sqlite3_bind_int64(query->Value, parameter, values[index]);
				}
			}
			return sqlite3_step(query->Value) == SQLITE_DONE;
		}
		auto valid_history(const core::insights_event& aEvent) -> bool
		{
			return aEvent.UnixSeconds >= 0 && static_cast<unsigned>(aEvent.Kind) <= 2 && static_cast<unsigned>(aEvent.Resource) <= 2
				&& static_cast<unsigned>(aEvent.Category) <= 4 && static_cast<unsigned>(aEvent.ProcessFailure) <= 8
				&& aEvent.Count <= 1000000000 && aEvent.Scope.Host <= static_cast<std::uint64_t>(INT64_MAX) && aEvent.Scope.Account <= static_cast<std::uint64_t>(INT64_MAX)
				&& (!aEvent.Limit || *aEvent.Limit <= 1000000000) && (!aEvent.Remaining || (aEvent.Limit && *aEvent.Remaining <= *aEvent.Limit));
		}
		auto read_history(sqlite3* aDatabase) -> std::vector<core::insights_event>
		{
			std::vector<core::insights_event> result;
			auto                              query = detail::prepare(aDatabase, "SELECT * FROM insights_history ORDER BY at LIMIT 300000");
			if (!query) {
				return result;
			}
			while (sqlite3_step(query->Value) == SQLITE_ROW) {
				auto event = history_event(query->Value);
				if (valid_history(event)) {
					result.push_back(event);
				}
				if (result.size() >= 300000) {
					throw std::runtime_error{ "Insights history bound reached" };
				}
			}
			return result;
		}
		void compact_history(sqlite3* aDatabase, const std::int64_t aNow)
		{
			auto events = read_history(aDatabase);
			if (!detail::execute(aDatabase, "DELETE FROM insights_history")) {
				throw std::runtime_error{ "Insights retention failed" };
			}
			for (const auto& event : events) {
				const auto width = core::insights_history_width(event.UnixSeconds, aNow);
				if (width != 0 && !insert_event(aDatabase, event, width)) {
					throw std::runtime_error{ "Insights retention failed" };
				}
			}
		}

		auto insert_review(sqlite3* aDatabase, const core::insights_review_event& aEvent) -> bool
		{
			if (static_cast<unsigned>(aEvent.Kind) > 9 || aEvent.Host.size() > 255 || aEvent.Repository.empty() || aEvent.Repository.size() > 1024 || aEvent.Identity.empty() || aEvent.Identity.size() > 4096 || aEvent.Head.size() > 128 || aEvent.Disposition > 2 || aEvent.Lines > 1000000000 || aEvent.FilesChanged > 1000000 || aEvent.PullRequest > static_cast<std::uint64_t>(INT64_MAX)) {
				return false;
			}
			auto query = detail::prepare(aDatabase, "INSERT OR IGNORE INTO insights_review_events VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25)");
			if (!query) {
				return false;
			}
			const auto text = [&](int aIndex, const std::string& aValue) { return sqlite3_bind_text(query->Value, aIndex, aValue.c_str(), static_cast<int>(aValue.size()), SQLITE_TRANSIENT) == SQLITE_OK; };
			if (!text(1, aEvent.Host) || !text(2, aEvent.Repository) || !text(5, aEvent.Identity) || !text(6, aEvent.Head)) {
				return false;
			}
			sqlite3_bind_int64(query->Value, 3, static_cast<std::int64_t>(aEvent.PullRequest));
			sqlite3_bind_int(query->Value, 4, static_cast<int>(aEvent.Kind));
			sqlite3_bind_int64(query->Value, 7, core::insights_review_identity_day(aEvent));
			sqlite3_bind_int64(query->Value, 8, aEvent.LocalDay);
			sqlite3_bind_int64(query->Value, 9, aEvent.UnixSeconds);
			sqlite3_bind_int64(query->Value, 10, static_cast<std::int64_t>(aEvent.FilesChanged));
			const auto counts = core::insights_review_delta(aEvent);
			for (std::size_t index = 0; index < counts.size(); ++index) {
				sqlite3_bind_int64(query->Value, static_cast<int>(index + 11), static_cast<std::int64_t>(counts[index]));
			}
			return sqlite3_step(query->Value) == SQLITE_DONE;
		}
		auto review_text(sqlite3_stmt* aQuery, int aColumn) -> std::string
		{
			const auto* bytes = sqlite3_column_text(aQuery, aColumn);
			return bytes ? std::string{ reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(sqlite3_column_bytes(aQuery, aColumn)) } : std::string{ };
		}
		auto read_reviews(sqlite3* aDatabase, std::int64_t aNow) -> core::insights_reviews_snapshot
		{
			const auto started = detail::scalar_integer(aDatabase, "SELECT started_unix_seconds FROM insights_metadata WHERE singleton=1");
			if (!started) {
				return { };
			}
			auto query = detail::prepare(aDatabase, "SELECT day,repository,SUM(c0),SUM(c1),SUM(c2),SUM(c3),SUM(c4),SUM(c5),SUM(c6),SUM(c7),SUM(c8),SUM(c9),SUM(c10),SUM(c11),SUM(c12),SUM(c13),SUM(c14) FROM insights_review_events GROUP BY day,repository ORDER BY day,repository");
			if (!query) {
				return { };
			}
			std::vector<core::insights_review_bucket> buckets;
			int                                       status = SQLITE_ROW;
			while ((status = sqlite3_step(query->Value)) == SQLITE_ROW) {
				core::insights_review_bucket bucket;
				bucket.Day        = sqlite3_column_int64(query->Value, 0);
				bucket.Repository = review_text(query->Value, 1);
				for (std::size_t index = 0; index < bucket.Counts.size(); ++index) {
					bucket.Counts[index] = static_cast<std::uint64_t>(sqlite3_column_int64(query->Value, static_cast<int>(index + 2)));
				}
				buckets.push_back(std::move(bucket));
			}
			if (status != SQLITE_DONE) {
				throw std::runtime_error{ "Insights review buckets unavailable" };
			}
			auto records = detail::prepare(aDatabase, "SELECT host,repository,pr,SUM(c1),MAX(files) FROM insights_review_events WHERE kind=1 GROUP BY host,repository,pr ORDER BY host,repository,pr");
			if (!records) {
				return { };
			}
			std::vector<core::insights_review_pr> pulls;
			while ((status = sqlite3_step(records->Value)) == SQLITE_ROW) {
				core::insights_review_pr pull;
				pull.Host       = review_text(records->Value, 0);
				pull.Repository = review_text(records->Value, 1);
				pull.Number     = static_cast<std::uint64_t>(sqlite3_column_int64(records->Value, 2));
				pull.Reviews    = static_cast<std::uint64_t>(sqlite3_column_int64(records->Value, 3));
				pull.Files      = static_cast<std::uint64_t>(sqlite3_column_int64(records->Value, 4));
				pulls.push_back(std::move(pull));
			}
			if (status != SQLITE_DONE) {
				throw std::runtime_error{ "Insights review records unavailable" };
			}
			return core::build_insights_reviews(buckets, pulls, application::local_insights_day(*started), application::local_insights_day(aNow));
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

			void record(const std::vector<core::insights_event>& aEvents, const std::optional<core::insights_scope> aSelected) override
			{
				auto connection = open_database(Root);
				if (!connection) {
					return;
				}
				auto* db = connection->Database.get();
				if (!detail::execute(db, "BEGIN IMMEDIATE")) {
					return;
				}
				for (const auto& event : aEvents) {
					if (!valid_history(event) || !insert_event(db, event, 1)) {
						throw std::runtime_error{ "Insights history write failed" };
					}
				}
				if (aSelected) {
					auto query = detail::prepare(db, "INSERT INTO insights_github_selection VALUES(1,?1,?2) ON CONFLICT DO UPDATE SET host=excluded.host,account=excluded.account");
					if (!query) {
						throw std::runtime_error{ "Insights selection failed" };
					}
					sqlite3_bind_int64(query->Value, 1, static_cast<std::int64_t>(aSelected->Host));
					sqlite3_bind_int64(query->Value, 2, static_cast<std::int64_t>(aSelected->Account));
					if (sqlite3_step(query->Value) != SQLITE_DONE) {
						throw std::runtime_error{ "Insights selection failed" };
					}
				}
				if (!aEvents.empty()) {
					const auto now = aEvents.back().UnixSeconds;
					if (LastCompaction == 0 || now - LastCompaction >= 300) {
						compact_history(db, now);
						LastCompaction = now;
					}
				}

				if (!detail::execute(db, "COMMIT")) {
					throw std::runtime_error{ "Insights history commit failed" };
				}
			}
			void record_energy(const std::vector<core::insights_energy_sample>& aSamples) override
			{
				if (aSamples.empty()) {
					return;
				}
				auto connection = open_database(Root);
				if (!connection) {
					throw std::runtime_error{ "Energy store unavailable" };
				}
				auto* db = connection->Database.get();
				if (!detail::execute(db, "BEGIN IMMEDIATE")) {
					throw std::runtime_error{ "Energy transaction unavailable" };
				}
				for (const auto& sample : aSamples) {
					write_energy(db, sample);
				}
				const auto now = std::ranges::max(aSamples, { }, &core::insights_energy_sample::AtMicroseconds).AtMicroseconds / 1000000;
				if (LastEnergyCompaction == 0 || now - LastEnergyCompaction >= 300) {
					compact_energy(db, now);
					LastEnergyCompaction = now;
				}
				if (!detail::execute(db, "COMMIT")) {
					throw std::runtime_error{ "Energy commit failed" };
				}
			}
			auto energy(const std::int64_t aNow) -> core::insights_energy_snapshot override
			{
				auto connection = open_database(Root);
				if (!connection) {
					return { };
				}
				auto* db = connection->Database.get();
				if (!detail::execute(db, "BEGIN IMMEDIATE")) {
					return { };
				}
				compact_energy(db, aNow);
				if (!detail::execute(db, "COMMIT")) {
					throw std::runtime_error{ "Energy retention commit failed" };
				}
				return core::build_insights_energy(read_energy(db), aNow, application::local_insights_day(aNow));
			}

			void record_reviews(const std::vector<core::insights_review_event>& aEvents) override
			{
				if (aEvents.empty()) {
					return;
				}
				auto connection = open_database(Root);
				if (!connection) {
					throw std::runtime_error{ "Insights review store unavailable" };
				}
				auto* db = connection->Database.get();
				if (!detail::execute(db, "BEGIN IMMEDIATE")) {
					throw std::runtime_error{ "Insights review transaction unavailable" };
				}
				for (const auto& event : aEvents) {
					if (!insert_review(db, event)) {
						throw std::runtime_error{ "Insights review write failed" };
					}
				}
				if (!detail::execute(db, "COMMIT")) {
					throw std::runtime_error{ "Insights review commit failed" };
				}
			}
			auto reviews(const std::int64_t aNow) -> core::insights_reviews_snapshot override
			{
				auto connection = open_database(Root);
				return connection ? read_reviews(connection->Database.get(), aNow) : core::insights_reviews_snapshot{ };
			}

			auto github(const std::int64_t aNow) -> core::insights_github_snapshot override
			{
				auto connection = open_database(Root);
				if (!connection) {
					return { };
				}
				auto* db = connection->Database.get();
				if (LastCompaction == 0 || aNow - LastCompaction >= 300) {
					if (detail::execute(db, "BEGIN IMMEDIATE")) {
						compact_history(db, aNow);
						if (!detail::execute(db, "COMMIT")) {
							throw std::runtime_error{ "Insights retention commit failed" };
						}
						LastCompaction = aNow;
					}
				}
				auto                 query = detail::prepare(db, "SELECT host,account FROM insights_github_selection WHERE singleton=1");
				core::insights_scope scope;
				if (query && sqlite3_step(query->Value) == SQLITE_ROW) {
					scope.Host    = static_cast<std::uint64_t>(sqlite3_column_int64(query->Value, 0));
					scope.Account = static_cast<std::uint64_t>(sqlite3_column_int64(query->Value, 1));
				}
				return core::build_insights_github(read_history(db), scope, aNow);
			}

		private:
			std::int64_t                                                    LastCompaction       = 0;
			std::int64_t                                                    LastEnergyCompaction = 0;
			std::filesystem::path                                           Root;
			std::function<std::vector<application::insights_storage_row>()> Git;
			bool                                                            Available = false;
		};
	}

	auto make_insights_storage(const std::filesystem::path& aRoot, std::function<std::vector<application::insights_storage_row>()> aGitInventory) -> std::unique_ptr<application::insights_storage_port>
	{
		return std::make_unique<storage>(aRoot, std::move(aGitInventory));
	}

	void record_insights_review(const std::filesystem::path& aRoot, core::insights_review_event aEvent) noexcept
	{
		try {
			if (aEvent.UnixSeconds == 0) {
				aEvent.UnixSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				aEvent.LocalDay    = application::local_insights_day(aEvent.UnixSeconds);
			}
			auto store = make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
			store->record_reviews({ aEvent });
		} catch (...) { }
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
