#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string_view>
#include <utility>
namespace bpr::adapters::sqlite {
	namespace {
		using application::window_review_snapshot;
		using application::window_review_store_failure;
		using application::window_review_store_result;
		[[nodiscard]] auto fail(window_review_store_failure aFailure)
		{
			return std::unexpected(application::window_review_store_error{ aFailure, { } });
		}
		constexpr std::int64_t     ApplicationId       = 0x42505252;
		constexpr std::string_view MetaSchema          = "CREATE TABLE restoration_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision TEXT NOT NULL) STRICT";
		constexpr std::string_view LegacyRecordsSchema = "CREATE TABLE restoration_records(window_id TEXT PRIMARY KEY,mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL) STRICT";
		constexpr std::string_view RecordsSchema       = "CREATE TABLE restoration_records(window_id TEXT PRIMARY KEY,mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,position BLOB) STRICT";
		// [[BPR-0733]]. The per-profile recently viewed list: ordinal 0 is the most
		// recent, and the CHECK holds core::maximum_recent_reviews in the file itself.
		constexpr std::string_view RecentSchema = "CREATE TABLE recent_reviews(ordinal INTEGER PRIMARY KEY CHECK(ordinal BETWEEN 0 AND 9),mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,title TEXT NOT NULL,viewed_at INTEGER NOT NULL,position BLOB) STRICT";
		// Version 5 binds every destination to its accepted viewer. Older rows
		// retain unknown ownership (zero), never the account that opens the file.
		constexpr std::string_view AccountRecordsSchema = "CREATE TABLE restoration_records(window_id TEXT PRIMARY KEY,mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,position BLOB,account TEXT NOT NULL) STRICT";
		constexpr std::string_view AccountRecentSchema  = "CREATE TABLE recent_reviews(ordinal INTEGER PRIMARY KEY CHECK(ordinal BETWEEN 0 AND 9),mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,title TEXT NOT NULL,viewed_at INTEGER NOT NULL,position BLOB,account TEXT NOT NULL) STRICT";
		constexpr std::string_view DraftRecentSchema    = "CREATE TABLE recent_reviews(ordinal INTEGER PRIMARY KEY CHECK(ordinal BETWEEN 0 AND 9),mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,title TEXT NOT NULL,viewed_at INTEGER NOT NULL,position BLOB,account TEXT NOT NULL,draft INTEGER CHECK(draft IN(0,1))) STRICT";
		static_assert(core::maximum_recent_reviews == 10U, "the recent_reviews CHECK and the seed's LIMIT spell this bound");
		// [[BPR-1131]]. Version 4: a window's document tabs, for its record
		// (scope 0, owner its window identity) and for a recently viewed entry
		// (scope 1, owner its ordinal). The set holds the window's comparison and
		// the active tab; each tab row is one exact view in the position format.
		constexpr std::string_view TabSetsSchema = "CREATE TABLE review_tab_sets(scope INTEGER NOT NULL CHECK(scope IN(0,1)),owner TEXT NOT NULL,window_comparison BLOB NOT NULL,active INTEGER CHECK(active BETWEEN 0 AND 15),PRIMARY KEY(scope,owner)) STRICT";
		constexpr std::string_view TabsSchema    = "CREATE TABLE review_tabs(scope INTEGER NOT NULL CHECK(scope IN(0,1)),owner TEXT NOT NULL,ordinal INTEGER NOT NULL CHECK(ordinal BETWEEN 0 AND 15),view BLOB NOT NULL,PRIMARY KEY(scope,owner,ordinal)) STRICT";
		static_assert(core::maximum_document_tabs == 16U, "the review_tabs and review_tab_sets CHECKs spell this bound");
		// RFC-BPR-0089 ([[BPR-1688]]). Version 7: a window's commit and file
		// filters, for its record and for its recently viewed entry, keyed as
		// tab sets are.
		constexpr std::string_view FiltersSchema = "CREATE TABLE review_filters(scope INTEGER NOT NULL CHECK(scope IN(0,1)),owner TEXT NOT NULL,filters BLOB NOT NULL,PRIMARY KEY(scope,owner)) STRICT";
		constexpr std::int64_t     record_scope  = 0;
		constexpr std::int64_t     recent_scope  = 1;
		// Seeds the list from the retained rows: one entry per destination, newest
		// window first, carrying that exact row's saved reading position.
		constexpr std::string_view RecentSeed = "INSERT INTO recent_reviews SELECT row_number() OVER (ORDER BY length(window_id) DESC,window_id DESC)-1,mode,hostname,owner,repository,number,'',0,position FROM (SELECT window_id,mode,hostname,owner,repository,number,position,row_number() OVER (PARTITION BY mode,hostname,owner,repository,number ORDER BY length(window_id) DESC,window_id DESC) AS recency FROM restoration_records) WHERE recency=1 ORDER BY 1 LIMIT 10";
		// Version 1 payload: fixed-width identities, big-endian integers, strict flags.
		class position_codec final
		{
		public:
			std::vector<std::uint8_t>     Bytes;
			std::span<const std::uint8_t> Input;
			bool                          Reading = false;
			bool                          Good    = true;
			void                          byte(std::uint8_t& aValue)
			{
				if (!Reading) {
					Bytes.push_back(aValue);
					return;
				}
				if (Input.empty()) {
					Good = false;
					return;
				}
				aValue = Input.front();
				Input  = Input.subspan(1);
			}
			void integer(std::uint64_t& aValue, unsigned aWidth)
			{
				auto result = std::uint64_t{ 0 };
				for (unsigned index = 0; index < aWidth; ++index) {
					auto value = static_cast<std::uint8_t>(aValue >> ((aWidth - index - 1U) * 8U));
					byte(value);
					result = (result << 8U) | value;
				}
				if (Reading) {
					aValue = result;
				}
			}
			template <typename TEnum>
			void enumeration(TEnum& aValue)
			{
				auto value = static_cast<std::uint8_t>(aValue);
				byte(value);
				aValue = static_cast<TEnum>(value);
			}
			template <typename TIdentity>
			void identity(TIdentity& aValue)
			{
				auto bytes = aValue.bytes();
				for (auto& value : bytes) {
					byte(value);
				}
				aValue = TIdentity{ bytes };
			}
			template <typename TValue, typename TFunction>
			void optional(std::optional<TValue>& aValue, TFunction aFunction)
			{
				auto present = static_cast<std::uint8_t>(aValue.has_value());
				byte(present);
				if (present > 1U) {
					Good = false;
					return;
				}
				if (present == 0U) {
					aValue.reset();
					return;
				}
				if (!aValue) {
					aValue.emplace();
				}
				aFunction(*aValue);
			}
			void anchor(core::comparison_anchor& aAnchor)
			{
				identity(aAnchor.Identity.Authority);
				identity(aAnchor.Identity.Incarnation);
				enumeration(aAnchor.Identity.Kind);
				auto epoch = static_cast<std::uint64_t>(aAnchor.Identity.Epoch.value());
				integer(epoch, 8);
				if (epoch > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					Good = false;
				}
				aAnchor.Identity.Epoch = core::epoch_ordinal{ static_cast<std::int64_t>(epoch) };
				identity(aAnchor.Identity.Review);
				auto algorithm = aAnchor.Commit.algorithm();
				enumeration(algorithm);
				auto bytes = aAnchor.Commit.bytes();
				for (auto& value : bytes) {
					byte(value);
				}
				aAnchor.Commit = core::git_commit_oid{ algorithm, bytes };
				enumeration(aAnchor.Availability);
			}
			void position(core::window_review_position& aPosition)
			{
				auto version = std::uint8_t{ 1 };
				byte(version);
				Good = Good && version == 1U;
				anchor(aPosition.Comparison.Left);
				anchor(aPosition.Comparison.Right);
				optional(aPosition.File, [this](auto& aValue) { this->identity(aValue); });
				optional(aPosition.Selection, [this](auto& aSelection) {
					identity(aSelection.Comparison);
					enumeration(aSelection.Kind);
					identity(aSelection.PreferredLine.Comparison);
					enumeration(aSelection.PreferredLine.Side);
					auto line = static_cast<std::uint64_t>(aSelection.PreferredLine.Line);
					integer(line, 4);
					aSelection.PreferredLine.Line = static_cast<std::uint32_t>(line);
					optional(aSelection.Segment, [this](auto& aValue) { this->identity(aValue); });
					optional(aSelection.Group, [this](auto& aValue) { this->identity(aValue); });
				});
			}
		};
		[[nodiscard]] auto bind_position(sqlite3_stmt* aStatement, const int aParameter, const std::optional<core::window_review_position>& aPosition) -> bool
		{
			if (!aPosition) {
				return sqlite3_bind_null(aStatement, aParameter) == SQLITE_OK;
			}
			auto codec    = position_codec{ };
			auto position = *aPosition;
			codec.position(position);
			return codec.Good && sqlite3_bind_blob(aStatement, aParameter, codec.Bytes.data(), static_cast<int>(codec.Bytes.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		[[nodiscard]] auto read_position(sqlite3_stmt* aStatement, const int aColumn, std::optional<core::window_review_position>& aPosition) -> bool
		{
			if (sqlite3_column_type(aStatement, aColumn) == SQLITE_NULL) {
				return true;
			}
			const auto size = sqlite3_column_bytes(aStatement, aColumn);
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB || size <= 0 || size > 512) {
				return false;
			}
			const auto* bytes = static_cast<const std::uint8_t*>(sqlite3_column_blob(aStatement, aColumn));
			if (!bytes) {
				return false;
			}
			auto codec = position_codec{ { }, { bytes, static_cast<std::size_t>(size) }, true, true };
			aPosition.emplace();
			codec.position(*aPosition);
			return codec.Good && codec.Input.empty() && core::valid(*aPosition);
		}
		// These connection-local limits perform no SQL and cannot change a refused file.
		void restrict_reads(sqlite3* aDatabase)
		{
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 4'096));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 8'192));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10'000));
		}

		class bounded_read_scope final
		{
		public:
			bounded_read_scope(sqlite3* aDatabase, std::stop_token aStop)
				: Stop{ aStop }
				, Handler{ aDatabase, 100, progress, this }
			{
			}
			bounded_read_scope(const bounded_read_scope&)                    = delete;
			auto operator=(const bounded_read_scope&) -> bounded_read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<bounded_read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps >= 200U ? 1 : 0;
			}
			std::stop_token                Stop;
			std::size_t                    Steps = 0U;
			detail::progress_handler_scope Handler;
		};

		[[nodiscard]] auto translate(const detail::failure aFailure) -> window_review_store_failure
		{
			return aFailure == detail::failure::cancelled ? window_review_store_failure::cancelled : window_review_store_failure::unavailable;
		}
		[[nodiscard]] auto text_of(sqlite3_stmt* aStatement, const int aColumn) -> std::string_view
		{
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT) {
				return { };
			}
			const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(aStatement, aColumn));
			return text == nullptr ? std::string_view{ } : std::string_view{ text, static_cast<std::size_t>(sqlite3_column_bytes(aStatement, aColumn)) };
		}
		[[nodiscard]] auto id_of(sqlite3_stmt* aStatement, const int aColumn) -> std::uint64_t
		{
			const auto text = text_of(aStatement, aColumn);
			if (text.empty() || text.size() > 20U || text.front() == '0') {
				return 0U;
			}
			std::uint64_t result = 0U;
			const auto    parsed = std::from_chars(text.data(), text.data() + text.size(), result);
			return parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() ? result : 0U;
		}
		[[nodiscard]] auto read_account(sqlite3_stmt* aStatement, int aColumn, core::github_account_id& aAccount) -> bool
		{
			const auto value = id_of(aStatement, aColumn);
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT || (value == 0 && text_of(aStatement, aColumn) != "0")) {
				return false;
			}
			aAccount = core::github_account_id{ value };
			return true;
		}
		[[nodiscard]] auto bind_text(sqlite3_stmt* aStatement, const int aColumn, const std::string_view aText) -> bool
		{
			return sqlite3_bind_text(aStatement, aColumn, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		[[nodiscard]] auto run(sqlite3* aDatabase, const std::string_view aSql, const std::stop_token aStop = { }) -> window_review_store_result<void>
		{
			const auto result = detail::execute(aDatabase, aSql, aStop);
			return result ? window_review_store_result<void>{ } : fail(translate(result.error()));
		}

		class transaction_scope final
		{
		public:
			explicit transaction_scope(sqlite3* aDatabase)
				: Database{ aDatabase }
			{ }
			~transaction_scope() { static_cast<void>(sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr)); }
			transaction_scope(const transaction_scope&)                    = delete;
			auto operator=(const transaction_scope&) -> transaction_scope& = delete;

		private:
			sqlite3* Database;
		};

		/// \brief The collection a tab row names: a window's record or a recently
		/// viewed entry, by its scope and owner text.
		[[nodiscard]] auto tab_owner(window_review_snapshot& aSnapshot, const std::int64_t aScope, const std::string_view aOwner)
			-> std::optional<core::window_review_tabs>*
		{
			if (aScope == record_scope) {
				const auto window = std::ranges::find_if(aSnapshot.Records, [aOwner](const core::window_review_record& aRecord) {
					return std::to_string(aRecord.Window.value()) == aOwner;
				});
				return window == aSnapshot.Records.end() ? nullptr : &window->Tabs;
			}
			if (aScope == recent_scope && aOwner.size() == 1U && aOwner.front() >= '0' && aOwner.front() <= '9') {
				const auto ordinal = static_cast<std::size_t>(aOwner.front() - '0');
				return ordinal < aSnapshot.Recent.size() ? &aSnapshot.Recent[ordinal].Tabs : nullptr;
			}
			return nullptr;
		}
		/// \brief RFC-BPR-0089 ([[BPR-1688]]). Version 1 filters payload: the
		/// hidden commits (format, 32 OID bytes, headline, author) and the
		/// filtered paths, each text a big-endian length then its bytes.
		class filters_codec final
		{
		public:
			std::vector<std::uint8_t>     Bytes;
			std::span<const std::uint8_t> Input;
			bool                          Good = true;

			void put(const std::uint64_t aValue, const unsigned aWidth)
			{
				for (unsigned index = 0U; index < aWidth; ++index) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> ((aWidth - index - 1U) * 8U)));
				}
			}
			void put_text(const std::string_view aText)
			{
				put(aText.size(), 4U);
				Bytes.insert(Bytes.end(), aText.begin(), aText.end());
			}
			[[nodiscard]] auto take(const unsigned aWidth) -> std::uint64_t
			{
				auto result = std::uint64_t{ 0U };
				for (unsigned index = 0U; index < aWidth; ++index) {
					if (Input.empty()) {
						Good = false;
						return 0U;
					}
					result = (result << 8U) | Input.front();
					Input  = Input.subspan(1U);
				}
				return result;
			}
			[[nodiscard]] auto take_text(const std::size_t aBound) -> std::string
			{
				const auto size = take(4U);
				if (!Good || size > aBound || size > Input.size()) {
					Good = false;
					return { };
				}
				auto result = std::string{ reinterpret_cast<const char*>(Input.data()), static_cast<std::size_t>(size) };
				Input       = Input.subspan(static_cast<std::size_t>(size));
				return result;
			}
			void encode(const core::window_review_filters& aFilters)
			{
				put(1U, 1U);
				put(aFilters.Commits.Hidden.size(), 4U);
				for (const auto& hidden : aFilters.Commits.Hidden) {
					put(static_cast<std::uint64_t>(hidden.Oid.algorithm()), 1U);
					Bytes.insert(Bytes.end(), hidden.Oid.bytes().begin(), hidden.Oid.bytes().end());
					put_text(hidden.Headline);
					put_text(hidden.Author);
				}
				put(aFilters.Files.Hidden.size(), 4U);
				for (const auto& path : aFilters.Files.Hidden) {
					put_text(path);
				}
			}
			[[nodiscard]] auto decode() -> std::optional<core::window_review_filters>
			{
				auto result = core::window_review_filters{ };
				if (take(1U) != 1U) {
					return std::nullopt;
				}
				const auto commits = take(4U);
				if (!Good || commits > core::maximum_filter_hidden_commits) {
					return std::nullopt;
				}
				for (std::uint64_t index = 0U; Good && index < commits; ++index) {
					const auto algorithm = take(1U);
					auto       bytes     = std::array<std::uint8_t, 32>{ };
					for (auto& byte : bytes) {
						byte = static_cast<std::uint8_t>(take(1U));
					}
					auto hidden     = core::filter_hidden_commit{ };
					hidden.Oid      = core::git_commit_oid{ static_cast<core::git_hash_algorithm>(algorithm), bytes };
					hidden.Headline = take_text(core::maximum_filter_headline_bytes);
					hidden.Author   = take_text(core::maximum_filter_author_bytes);
					result.Commits.Hidden.push_back(std::move(hidden));
				}
				const auto paths = take(4U);
				if (!Good || paths > core::maximum_window_review_filter_paths) {
					return std::nullopt;
				}
				for (std::uint64_t index = 0U; Good && index < paths; ++index) {
					result.Files.Hidden.push_back(take_text(core::maximum_identified_path_bytes));
				}
				return Good && Input.empty() && core::valid(result) ? std::optional{ std::move(result) } : std::nullopt;
			}
		};
		/// \brief RFC-BPR-0089 ([[BPR-1688]]). The filters a row names: a
		/// window's record or a recently viewed entry, as `tab_owner` finds them.
		[[nodiscard]] auto filters_owner(window_review_snapshot& aSnapshot, const std::int64_t aScope, const std::string_view aOwner)
			-> std::optional<core::window_review_filters>*
		{
			if (aScope == record_scope) {
				const auto window = std::ranges::find_if(aSnapshot.Records, [aOwner](const core::window_review_record& aRecord) {
					return std::to_string(aRecord.Window.value()) == aOwner;
				});
				return window == aSnapshot.Records.end() ? nullptr : &window->Filters;
			}
			if (aScope == recent_scope && aOwner.size() == 1U && aOwner.front() >= '0' && aOwner.front() <= '9') {
				const auto ordinal = static_cast<std::size_t>(aOwner.front() - '0');
				return ordinal < aSnapshot.Recent.size() ? &aSnapshot.Recent[ordinal].Filters : nullptr;
			}
			return nullptr;
		}
		[[nodiscard]] auto read_filters(sqlite3* aDatabase, const std::stop_token aStop, window_review_snapshot& aSnapshot)
			-> window_review_store_result<void>
		{
			auto rows = detail::prepare(aDatabase, "SELECT scope,owner,filters FROM review_filters LIMIT 43");
			if (!rows) {
				return fail(window_review_store_failure::corrupt);
			}
			for (std::size_t count = 0U;; ++count) {
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				const auto step = sqlite3_step(rows->Value);
				if (step == SQLITE_DONE) {
					break;
				}
				if (step != SQLITE_ROW || count >= core::maximum_bpr_windows_per_profile + core::maximum_recent_reviews
					|| sqlite3_column_type(rows->Value, 0) != SQLITE_INTEGER || sqlite3_column_type(rows->Value, 2) != SQLITE_BLOB) {
					return fail(window_review_store_failure::corrupt);
				}
				auto* const owner = filters_owner(aSnapshot, sqlite3_column_int64(rows->Value, 0), text_of(rows->Value, 1));
				const auto* bytes = static_cast<const std::uint8_t*>(sqlite3_column_blob(rows->Value, 2));
				const auto  size  = sqlite3_column_bytes(rows->Value, 2);
				auto        codec = filters_codec{ };
				codec.Input       = std::span<const std::uint8_t>{ bytes, bytes == nullptr ? 0U : static_cast<std::size_t>(size) };
				auto decoded      = owner == nullptr || owner->has_value() ? std::nullopt : codec.decode();
				if (!decoded) {
					return fail(window_review_store_failure::corrupt);
				}
				owner->emplace(std::move(*decoded));
			}
			return { };
		}
		[[nodiscard]] auto write_filters(sqlite3* aDatabase, const std::int64_t aScope, const std::string& aOwner, const core::window_review_filters& aFilters) -> bool
		{
			auto codec = filters_codec{ };
			codec.encode(aFilters);
			auto row = detail::prepare(aDatabase, "INSERT INTO review_filters VALUES(?1,?2,?3)");
			return row && sqlite3_bind_int64(row->Value, 1, aScope) == SQLITE_OK && bind_text(row->Value, 2, aOwner)
				&& sqlite3_bind_blob(row->Value, 3, codec.Bytes.data(), static_cast<int>(codec.Bytes.size()), SQLITE_TRANSIENT) == SQLITE_OK
				&& sqlite3_step(row->Value) == SQLITE_DONE;
		}
		/// \brief [[BPR-1131]]. Attaches every stored tab collection to its record
		/// or recently viewed entry; a set whose owner is absent, a gap in a set's
		/// tabs or a row of no set is corrupt.
		[[nodiscard]] auto read_tabs(sqlite3* aDatabase, const std::stop_token aStop, window_review_snapshot& aSnapshot)
			-> window_review_store_result<void>
		{
			auto sets = detail::prepare(aDatabase, "SELECT scope,owner,window_comparison,active FROM review_tab_sets LIMIT 43");
			if (!sets) {
				return fail(window_review_store_failure::corrupt);
			}
			for (std::size_t count = 0U;; ++count) {
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				const auto step = sqlite3_step(sets->Value);
				if (step == SQLITE_DONE) {
					break;
				}
				if (step != SQLITE_ROW || count >= core::maximum_bpr_windows_per_profile + core::maximum_recent_reviews
					|| sqlite3_column_type(sets->Value, 0) != SQLITE_INTEGER) {
					return fail(window_review_store_failure::corrupt);
				}
				auto* const owner = tab_owner(aSnapshot, sqlite3_column_int64(sets->Value, 0), text_of(sets->Value, 1));
				auto        pair  = std::optional<core::window_review_position>{ };
				if (owner == nullptr || owner->has_value() || !read_position(sets->Value, 2, pair) || !pair || pair->File || pair->Selection) {
					return fail(window_review_store_failure::corrupt);
				}
				auto collection             = core::window_review_tabs{ };
				collection.WindowComparison = pair->Comparison;
				if (sqlite3_column_type(sets->Value, 3) == SQLITE_INTEGER) {
					collection.Active = static_cast<std::uint32_t>(sqlite3_column_int64(sets->Value, 3));
				} else if (sqlite3_column_type(sets->Value, 3) != SQLITE_NULL) {
					return fail(window_review_store_failure::corrupt);
				}
				owner->emplace(std::move(collection));
			}
			auto rows = detail::prepare(aDatabase, "SELECT scope,owner,ordinal,view FROM review_tabs ORDER BY scope,owner,ordinal LIMIT 673");
			if (!rows) {
				return fail(window_review_store_failure::corrupt);
			}
			for (std::size_t count = 0U;; ++count) {
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				const auto step = sqlite3_step(rows->Value);
				if (step == SQLITE_DONE) {
					break;
				}
				if (step != SQLITE_ROW || count >= (core::maximum_bpr_windows_per_profile + core::maximum_recent_reviews) * core::maximum_document_tabs
					|| sqlite3_column_type(rows->Value, 0) != SQLITE_INTEGER || sqlite3_column_type(rows->Value, 2) != SQLITE_INTEGER) {
					return fail(window_review_store_failure::corrupt);
				}
				auto* const owner = tab_owner(aSnapshot, sqlite3_column_int64(rows->Value, 0), text_of(rows->Value, 1));
				auto        view  = std::optional<core::window_review_position>{ };
				if (owner == nullptr || !owner->has_value() || sqlite3_column_int64(rows->Value, 2) != static_cast<std::int64_t>((*owner)->Tabs.size())
					|| !read_position(rows->Value, 3, view) || !view) {
					return fail(window_review_store_failure::corrupt);
				}
				(*owner)->Tabs.push_back(std::move(*view));
			}
			return { };
		}
		[[nodiscard]] auto read_snapshot(sqlite3* aDatabase, std::stop_token aStop) -> window_review_store_result<window_review_snapshot>
		{
			if (aStop.stop_requested()) {
				return fail(window_review_store_failure::cancelled);
			}
			const auto scope    = bounded_read_scope{ aDatabase, aStop };
			const auto version  = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto identity = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			const auto count    = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 6)");
			const auto meta     = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='restoration_meta'");
			const auto records  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='restoration_records'");
			const auto recent   = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='recent_reviews'");
			const auto tabSets  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='review_tab_sets'");
			const auto tabs     = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='review_tabs'");
			const auto filters  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='review_filters'");
			// Versions 1 to 4 are read only to migrate them; anything else, this
			// build's schema included, is refused before a single byte is written.
			const auto tables = version && *version >= 7 ? 6 : version && *version >= 4 ? 5
				: version && *version == 3                                              ? 3
																						: 2;
			if (!version || *version < 1 || *version > 7 || !identity || *identity != ApplicationId || !count || *count != tables
				|| (*version >= 7 && (!filters || detail::canonical_sql(*filters) != detail::canonical_sql(FiltersSchema)))
				|| !meta || detail::canonical_sql(*meta) != detail::canonical_sql(MetaSchema)
				|| !records || detail::canonical_sql(*records) != detail::canonical_sql(*version == 1 ? LegacyRecordsSchema : *version < 5 ? RecordsSchema
																																		   : AccountRecordsSchema)
				|| (*version >= 3 && (!recent || detail::canonical_sql(*recent) != detail::canonical_sql(*version < 5 ? RecentSchema : *version == 5 ? AccountRecentSchema
																																					 : DraftRecentSchema)))
				|| (*version >= 4 && (!tabSets || detail::canonical_sql(*tabSets) != detail::canonical_sql(TabSetsSchema) || !tabs || detail::canonical_sql(*tabs) != detail::canonical_sql(TabsSchema)))) {
				return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::schema_mismatch);
			}
			auto revision = detail::prepare(aDatabase, "SELECT singleton,revision FROM restoration_meta LIMIT 2");
			if (!revision || sqlite3_step(revision->Value) != SQLITE_ROW || sqlite3_column_int64(revision->Value, 0) != 1) {
				return fail(window_review_store_failure::corrupt);
			}
			auto result     = window_review_snapshot{ };
			result.Revision = id_of(revision->Value, 1);
			if (sqlite3_step(revision->Value) != SQLITE_DONE) {
				return fail(window_review_store_failure::corrupt);
			}
			auto rows = detail::prepare(aDatabase, *version == 1 ? "SELECT window_id,mode,hostname,owner,repository,number FROM restoration_records LIMIT 33" : *version < 5 ? "SELECT window_id,mode,hostname,owner,repository,number,position FROM restoration_records LIMIT 33"
																																											 : "SELECT window_id,mode,hostname,owner,repository,number,position,account FROM restoration_records LIMIT 33");
			if (!rows) {
				return fail(window_review_store_failure::corrupt);
			}
			for (;;) {
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				const auto step = sqlite3_step(rows->Value);
				if (step == SQLITE_DONE) {
					break;
				}
				if (step != SQLITE_ROW || result.Records.size() >= core::maximum_bpr_windows_per_profile) {
					return fail(window_review_store_failure::corrupt);
				}
				const auto mode   = sqlite3_column_int64(rows->Value, 1);
				const auto number = id_of(rows->Value, 5);
				if (sqlite3_column_type(rows->Value, 1) != SQLITE_INTEGER || mode < 0 || mode > 1 || number > core::maximum_pull_request_number) {
					return fail(window_review_store_failure::corrupt);
				}
				result.Records.push_back({ core::bpr_window_id{ id_of(rows->Value, 0) },
										   { static_cast<core::github_access_mode>(mode), std::string{ text_of(rows->Value, 2) }, { std::string{ text_of(rows->Value, 3) }, std::string{ text_of(rows->Value, 4) } }, core::pull_request_number{ static_cast<std::uint32_t>(number) } } });
				if ((*version != 1 && !read_position(rows->Value, 6, result.Records.back().Position))
					|| (*version >= 5 && !read_account(rows->Value, 7, result.Records.back().Destination.Account))) {
					return fail(window_review_store_failure::corrupt);
				}
			}
			std::ranges::sort(result.Records, { }, &core::window_review_record::Window);
			// A version 1 or 2 file has no list; the migration derives one.
			if (*version >= 3) {
				auto entries = detail::prepare(aDatabase, *version < 5 ? "SELECT ordinal,mode,hostname,owner,repository,number,title,viewed_at,position FROM recent_reviews ORDER BY ordinal LIMIT 11" : *version == 5 ? "SELECT ordinal,mode,hostname,owner,repository,number,title,viewed_at,position,account FROM recent_reviews ORDER BY ordinal LIMIT 11"
																																																					   : "SELECT ordinal,mode,hostname,owner,repository,number,title,viewed_at,position,account,draft FROM recent_reviews ORDER BY ordinal LIMIT 11");
				if (!entries) {
					return fail(window_review_store_failure::corrupt);
				}
				for (;;) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					const auto step = sqlite3_step(entries->Value);
					if (step == SQLITE_DONE) {
						break;
					}
					if (step != SQLITE_ROW || result.Recent.size() >= core::maximum_recent_reviews) {
						return fail(window_review_store_failure::corrupt);
					}
					const auto ordinal = sqlite3_column_int64(entries->Value, 0);
					const auto mode    = sqlite3_column_int64(entries->Value, 1);
					const auto number  = id_of(entries->Value, 5);
					const auto viewed  = sqlite3_column_int64(entries->Value, 7);
					if (sqlite3_column_type(entries->Value, 1) != SQLITE_INTEGER || sqlite3_column_type(entries->Value, 7) != SQLITE_INTEGER
						|| ordinal != static_cast<std::int64_t>(result.Recent.size()) || mode < 0 || mode > 1 || number > core::maximum_pull_request_number || viewed < 0) {
						return fail(window_review_store_failure::corrupt);
					}
					result.Recent.push_back({ { static_cast<core::github_access_mode>(mode), std::string{ text_of(entries->Value, 2) }, { std::string{ text_of(entries->Value, 3) }, std::string{ text_of(entries->Value, 4) } }, core::pull_request_number{ static_cast<std::uint32_t>(number) } },
											  std::string{ text_of(entries->Value, 6) },
											  viewed,
											  std::nullopt });
					if (*version >= 6 && sqlite3_column_type(entries->Value, 10) != SQLITE_NULL) {
						const auto draft = sqlite3_column_int64(entries->Value, 10);
						if (sqlite3_column_type(entries->Value, 10) != SQLITE_INTEGER || (draft != 0 && draft != 1)) {
							return fail(window_review_store_failure::corrupt);
						}
						result.Recent.back().Draft = draft != 0;
					}
					if (!read_position(entries->Value, 8, result.Recent.back().Position)
						|| (*version >= 5 && !read_account(entries->Value, 9, result.Recent.back().Destination.Account))) {
						return fail(window_review_store_failure::corrupt);
					}
				}
			}
			if (*version >= 4) {
				if (auto read = read_tabs(aDatabase, aStop, result); !read) {
					return std::unexpected(read.error());
				}
			}
			if (*version >= 7) {
				if (auto read = read_filters(aDatabase, aStop, result); !read) {
					return std::unexpected(read.error());
				}
			}
			if (!application::valid(result)) {
				return fail(window_review_store_failure::corrupt);
			}
			if (aStop.stop_requested()) {
				return fail(window_review_store_failure::cancelled);
			}
			return result;
		}
		/// \brief [[BPR-1131]]. Writes one collection: its set, then its tabs in order.
		[[nodiscard]] auto write_tabs(sqlite3* aDatabase, const std::int64_t aScope, const std::string& aOwner, const core::window_review_tabs& aTabs) -> bool
		{
			auto set = detail::prepare(aDatabase, "INSERT INTO review_tab_sets VALUES(?1,?2,?3,?4)");
			if (!set || sqlite3_bind_int64(set->Value, 1, aScope) != SQLITE_OK || !bind_text(set->Value, 2, aOwner)
				|| !bind_position(set->Value, 3, core::window_review_position{ .Comparison = aTabs.WindowComparison, .File = std::nullopt, .Selection = std::nullopt })
				|| (aTabs.Active ? sqlite3_bind_int64(set->Value, 4, static_cast<std::int64_t>(*aTabs.Active)) : sqlite3_bind_null(set->Value, 4)) != SQLITE_OK
				|| sqlite3_step(set->Value) != SQLITE_DONE) {
				return false;
			}
			for (std::size_t index = 0U; index < aTabs.Tabs.size(); ++index) {
				auto row = detail::prepare(aDatabase, "INSERT INTO review_tabs VALUES(?1,?2,?3,?4)");
				if (!row || sqlite3_bind_int64(row->Value, 1, aScope) != SQLITE_OK || !bind_text(row->Value, 2, aOwner)
					|| sqlite3_bind_int64(row->Value, 3, static_cast<std::int64_t>(index)) != SQLITE_OK
					|| !bind_position(row->Value, 4, aTabs.Tabs[index]) || sqlite3_step(row->Value) != SQLITE_DONE) {
					return false;
				}
			}
			return true;
		}
		[[nodiscard]] auto pristine(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			const auto scope = bounded_read_scope{ aDatabase, aStop };
			return detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0
				&& detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
				&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema LIMIT 1)") == 0;
		}
		class sqlite_window_review_store final : public application::window_review_restoration_port
		{
		public:
			explicit sqlite_window_review_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			~sqlite_window_review_store() override
			{
				if (Database) {
					sqlite3_close_v2(Database);
				}
			}
			auto load(std::stop_token aStop) -> window_review_store_result<window_review_snapshot> override
			{
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (auto begin = run(Database, "BEGIN", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = transaction_scope{ Database };
				return read_snapshot(Database, aStop);
			}
			auto save(const window_review_snapshot& aCandidate, std::uint64_t aExpectedRevision, std::stop_token aStop) -> window_review_store_result<window_review_snapshot> override
			{
				if (!application::valid(aCandidate) || aExpectedRevision == 0 || aExpectedRevision == std::numeric_limits<std::uint64_t>::max() || aCandidate.Revision != aExpectedRevision + 1U) {
					return fail(window_review_store_failure::corrupt);
				}
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = transaction_scope{ Database };
				const auto current     = read_snapshot(Database, aStop);
				if (!current) {
					return std::unexpected(current.error());
				}
				if (current->Revision != aExpectedRevision) {
					return std::unexpected(application::window_review_store_error{ window_review_store_failure::conflict, *current });
				}
				if (auto erased = run(Database, "DELETE FROM restoration_records", aStop); !erased) {
					return std::unexpected(erased.error());
				}
				for (const auto& record : aCandidate.Records) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					auto row = detail::prepare(Database, "INSERT INTO restoration_records VALUES(?1,?2,?3,?4,?5,?6,?7,?8)");
					if (!row || !bind_text(row->Value, 1, std::to_string(record.Window.value()))
						|| sqlite3_bind_int(row->Value, 2, static_cast<int>(record.Destination.Mode)) != SQLITE_OK
						|| !bind_text(row->Value, 3, record.Destination.Hostname)
						|| !bind_text(row->Value, 4, record.Destination.Repository.Owner)
						|| !bind_text(row->Value, 5, record.Destination.Repository.Name)
						|| !bind_text(row->Value, 6, std::to_string(record.Destination.Number.value()))
						|| !bind_position(row->Value, 7, record.Position)
						|| !bind_text(row->Value, 8, std::to_string(record.Destination.Account.value()))
						|| sqlite3_step(row->Value) != SQLITE_DONE) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				if (auto cleared = run(Database, "DELETE FROM recent_reviews", aStop); !cleared) {
					return std::unexpected(cleared.error());
				}
				for (std::size_t index = 0; index < aCandidate.Recent.size(); ++index) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					const auto& entry = aCandidate.Recent[index];
					auto        seat  = detail::prepare(Database, "INSERT INTO recent_reviews VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)");
					if (!seat || sqlite3_bind_int64(seat->Value, 1, static_cast<std::int64_t>(index)) != SQLITE_OK
						|| sqlite3_bind_int(seat->Value, 2, static_cast<int>(entry.Destination.Mode)) != SQLITE_OK
						|| !bind_text(seat->Value, 3, entry.Destination.Hostname)
						|| !bind_text(seat->Value, 4, entry.Destination.Repository.Owner)
						|| !bind_text(seat->Value, 5, entry.Destination.Repository.Name)
						|| !bind_text(seat->Value, 6, std::to_string(entry.Destination.Number.value()))
						|| !bind_text(seat->Value, 7, entry.Title)
						|| sqlite3_bind_int64(seat->Value, 8, entry.ViewedAt) != SQLITE_OK
						|| !bind_position(seat->Value, 9, entry.Position)
						|| !bind_text(seat->Value, 10, std::to_string(entry.Destination.Account.value()))
						|| (entry.Draft ? sqlite3_bind_int(seat->Value, 11, *entry.Draft ? 1 : 0) : sqlite3_bind_null(seat->Value, 11)) != SQLITE_OK
						|| sqlite3_step(seat->Value) != SQLITE_DONE) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				for (const auto sql : { std::string_view{ "DELETE FROM review_tab_sets" }, std::string_view{ "DELETE FROM review_tabs" } }) {
					if (auto cleared = run(Database, sql, aStop); !cleared) {
						return std::unexpected(cleared.error());
					}
				}
				for (const auto& record : aCandidate.Records) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					if (record.Tabs && !write_tabs(Database, record_scope, std::to_string(record.Window.value()), *record.Tabs)) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				for (std::size_t index = 0; index < aCandidate.Recent.size(); ++index) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					if (aCandidate.Recent[index].Tabs && !write_tabs(Database, recent_scope, std::to_string(index), *aCandidate.Recent[index].Tabs)) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				if (auto cleared = run(Database, "DELETE FROM review_filters", aStop); !cleared) {
					return std::unexpected(cleared.error());
				}
				for (const auto& record : aCandidate.Records) {
					if (record.Filters && !write_filters(Database, record_scope, std::to_string(record.Window.value()), *record.Filters)) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				for (std::size_t index = 0; index < aCandidate.Recent.size(); ++index) {
					if (aCandidate.Recent[index].Filters && !write_filters(Database, recent_scope, std::to_string(index), *aCandidate.Recent[index].Filters)) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				auto meta = detail::prepare(Database, "UPDATE restoration_meta SET revision=?1 WHERE singleton=1");
				if (!meta || !bind_text(meta->Value, 1, std::to_string(aCandidate.Revision)) || sqlite3_step(meta->Value) != SQLITE_DONE) {
					return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
				}
				if (auto commit = run(Database, "COMMIT", aStop); !commit) {
					return std::unexpected(commit.error());
				}
				return aCandidate;
			}

		private:
			[[nodiscard]] auto establish(std::stop_token aStop) -> window_review_store_result<void>
			{
				if (!pristine(Database, aStop)) {
					const auto current = read_snapshot(Database, aStop);
					if (!current) {
						return std::unexpected(current.error());
					}
				}
				if (auto configured = detail::configure_connection(Database); !configured) {
					return fail(translate(configured.error()));
				}
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return begin;
				}
				const auto transaction = transaction_scope{ Database };
				if (!pristine(Database, aStop)) {
					const auto current = read_snapshot(Database, aStop);
					if (!current) {
						return std::unexpected(current.error());
					}
					const auto stamped = detail::scalar_integer(Database, "PRAGMA user_version");
					if (stamped == 1) {
						// Rebuild inside the transaction: ALTER's internal quick-check view
						// is unavailable on this deliberately view-disabled connection.
						for (const auto sql : { std::string_view{ "CREATE TEMP TABLE restoration_migration AS SELECT * FROM restoration_records" }, std::string_view{ "DROP TABLE restoration_records" }, RecordsSchema, std::string_view{ "INSERT INTO restoration_records SELECT *,NULL FROM restoration_migration" }, std::string_view{ "DROP TABLE restoration_migration" }, std::string_view{ "PRAGMA user_version=2" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					if (stamped == 1 || stamped == 2) {
						// [[BPR-0733]]. The retained rows become the first list: the old
						// row survives as the newest entry rather than as an auto-open.
						for (const auto sql : { RecentSchema, RecentSeed, std::string_view{ "PRAGMA user_version=3" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					if (stamped == 1 || stamped == 2 || stamped == 3) {
						// [[BPR-1131]]. No older file has tabs; the tables start empty.
						for (const auto sql : { TabSetsSchema, TabsSchema, std::string_view{ "PRAGMA user_version=4" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					if (stamped && *stamped < 5) {
						for (const auto sql : {
								 std::string_view{ "CREATE TEMP TABLE account_records_migration AS SELECT * FROM restoration_records" },
								 std::string_view{ "DROP TABLE restoration_records" }, AccountRecordsSchema,
								 std::string_view{ "INSERT INTO restoration_records SELECT *,'0' FROM account_records_migration" },
								 std::string_view{ "DROP TABLE account_records_migration" },
								 std::string_view{ "CREATE TEMP TABLE account_recent_migration AS SELECT * FROM recent_reviews" },
								 std::string_view{ "DROP TABLE recent_reviews" }, AccountRecentSchema,
								 std::string_view{ "INSERT INTO recent_reviews SELECT *,'0' FROM account_recent_migration" },
								 std::string_view{ "DROP TABLE account_recent_migration" },
								 std::string_view{ "PRAGMA user_version=5" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					if (stamped && *stamped < 6) {
						for (const auto sql : { std::string_view{ "CREATE TEMP TABLE draft_recent_migration AS SELECT * FROM recent_reviews" }, std::string_view{ "DROP TABLE recent_reviews" }, DraftRecentSchema, std::string_view{ "INSERT INTO recent_reviews SELECT *,NULL FROM draft_recent_migration" }, std::string_view{ "DROP TABLE draft_recent_migration" }, std::string_view{ "PRAGMA user_version=6" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					if (stamped && *stamped < 7) {
						// RFC-BPR-0089 ([[BPR-1688]]). No older file has filters.
						for (const auto sql : { FiltersSchema, std::string_view{ "PRAGMA user_version=7" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					return run(Database, "COMMIT", aStop);
				}
				for (const auto sql : { MetaSchema, AccountRecordsSchema, DraftRecentSchema, TabSetsSchema, TabsSchema, FiltersSchema, std::string_view{ "INSERT INTO restoration_meta VALUES(1,'1')" }, std::string_view{ "PRAGMA application_id=1112560210" }, std::string_view{ "PRAGMA user_version=7" }, std::string_view{ "COMMIT" } }) {
					if (auto result = run(Database, sql, aStop); !result) {
						return result;
					}
				}
				return { };
			}

			[[nodiscard]] auto ensure_open(const std::stop_token aStop) -> window_review_store_result<void>
			{
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				if (Database != nullptr) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return fail(window_review_store_failure::unavailable);
				}
				Directory.emplace(std::move(*directory));
				auto opened = detail::open_private_store_database(*Directory, "window-restoration.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return fail(window_review_store_failure::unavailable);
				}
				Database = *opened;
				restrict_reads(Database);
				auto result = establish(aStop);
				if (!result) {
					sqlite3_close_v2(Database);
					Database = nullptr;
				}
				return result;
			}
			std::filesystem::path                  Root;
			std::optional<detail::state_directory> Directory;
			sqlite3*                               Database = nullptr;
		};
	}
	auto make_window_review_restoration_store(std::filesystem::path aStorageRoot) -> std::unique_ptr<application::window_review_restoration_port> { return std::make_unique<sqlite_window_review_store>(std::move(aStorageRoot)); }
}
