#include "bpr/adapters/sqlite/review_cycle_store.hpp"

#include "bpr/core/sha256.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include <algorithm>
#include <array>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		using application::cycle_object_ref;
		using application::cycle_store_error;
		using application::cycle_store_failure;
		using application::cycle_store_result;
		using application::cycle_write_receipt;

		constexpr std::int64_t     application_id   = 0x42505243;
		constexpr std::int64_t     schema_version   = 1;
		constexpr std::size_t      chunk_bytes      = 512U * 1024U;
		constexpr std::int64_t     maximum_receipts = 4096;
		constexpr std::string_view database_name    = "review-cycles.sqlite3";

		struct table
		{
			std::string_view Name;
			std::string_view Definition;
		};
		constexpr auto schema = std::array{
			table{ "catalog", "CREATE TABLE catalog(singleton INTEGER PRIMARY KEY CHECK(singleton=1),profile INTEGER NOT NULL,incarnation BLOB NOT NULL CHECK(length(incarnation)=32),next_cycle INTEGER NOT NULL) STRICT" },
			table{ "cycles", "CREATE TABLE cycles(cycle INTEGER PRIMARY KEY,generation INTEGER NOT NULL,revision INTEGER NOT NULL,title TEXT NOT NULL,lifecycle INTEGER NOT NULL,scope_version INTEGER NOT NULL,scope BLOB NOT NULL,initial INTEGER,latest INTEGER,next_revision INTEGER NOT NULL,next_item INTEGER NOT NULL,next_draft INTEGER NOT NULL,next_round INTEGER NOT NULL,next_response INTEGER NOT NULL,next_verification INTEGER NOT NULL,next_publication INTEGER NOT NULL) STRICT" },
			table{ "revisions", "CREATE TABLE revisions(cycle INTEGER NOT NULL,revision INTEGER NOT NULL,parent INTEGER,scope_version INTEGER NOT NULL,material BLOB NOT NULL CHECK(length(material)=32),entries INTEGER NOT NULL,deletions INTEGER NOT NULL,unavailable INTEGER NOT NULL,coverage INTEGER NOT NULL,quality INTEGER NOT NULL,manifest_size INTEGER NOT NULL,PRIMARY KEY(cycle,revision)) STRICT" },
			table{ "drafts", "CREATE TABLE drafts(cycle INTEGER NOT NULL,draft INTEGER NOT NULL,revision INTEGER NOT NULL,body BLOB NOT NULL,PRIMARY KEY(cycle,draft)) STRICT" },
			table{ "rounds", "CREATE TABLE rounds(cycle INTEGER NOT NULL,round INTEGER NOT NULL,draft INTEGER NOT NULL,draft_revision INTEGER NOT NULL,digest BLOB NOT NULL CHECK(length(digest)=32),payload_size INTEGER NOT NULL,PRIMARY KEY(cycle,round)) STRICT" },
			table{ "objects", "CREATE TABLE objects(digest BLOB PRIMARY KEY CHECK(length(digest)=32),size INTEGER NOT NULL,chunks INTEGER NOT NULL) STRICT" },
			table{ "object_chunks", "CREATE TABLE object_chunks(digest BLOB NOT NULL,ordinal INTEGER NOT NULL,bytes BLOB NOT NULL,PRIMARY KEY(digest,ordinal)) STRICT" },
			table{ "receipts", "CREATE TABLE receipts(operation BLOB PRIMARY KEY CHECK(length(operation)=32),request BLOB NOT NULL CHECK(length(request)=32),cycle INTEGER NOT NULL,revision INTEGER NOT NULL,allocated INTEGER NOT NULL,changed INTEGER NOT NULL) STRICT" },
		};
		/// Objects some catalog row references. Everything else is reclaimable.
		constexpr std::string_view referenced_objects = "SELECT material AS digest FROM revisions UNION SELECT digest FROM rounds";

		auto fail(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}
		auto refuse(const core::cycle_refusal aRefusal) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = aRefusal });
		}

		struct connection
		{
			sqlite3* Database = nullptr;
			explicit connection(sqlite3* aDatabase) noexcept
				: Database{ aDatabase }
			{
			}
			connection(const connection&)                    = delete;
			auto operator=(const connection&) -> connection& = delete;
			connection(connection&&)                         = delete;
			auto operator=(connection&&) -> connection&      = delete;
			~connection()
			{
				if (Database != nullptr) {
					sqlite3_close_v2(Database);
				}
			}
		};

		/// Rolls back unless the transaction was committed.
		struct transaction
		{
			sqlite3* Database;
			bool     Open = true;
			explicit transaction(sqlite3* aDatabase) noexcept
				: Database{ aDatabase }
			{
			}
			transaction(const transaction&)                    = delete;
			auto operator=(const transaction&) -> transaction& = delete;
			transaction(transaction&&)                         = delete;
			auto operator=(transaction&&) -> transaction&      = delete;
			~transaction()
			{
				if (Open) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				}
			}
			auto commit(std::stop_token aStop) -> bool
			{
				if (aStop.stop_requested()) {
					return false;
				}
				const auto progress  = detail::progress_scope{ Database, aStop };
				auto       statement = detail::prepare(Database, "COMMIT");
				Open                 = !(statement && sqlite3_step(statement->Value) == SQLITE_DONE);
				return !Open;
			}
		};

		/// One prepared statement with bounded typed accessors.
		class query
		{
		public:
			query(sqlite3* aDatabase, const std::string_view aSql)
			{
				if (auto prepared = detail::prepare(aDatabase, aSql)) {
					Statement = std::move(*prepared);
				} else {
					Good = false;
				}
			}
			auto integer(const int aColumn, const std::int64_t aValue) -> query&
			{
				Good = Good && sqlite3_bind_int64(Statement.Value, aColumn, aValue) == SQLITE_OK;
				return *this;
			}
			auto id(const int aColumn, const std::uint64_t aValue) -> query&
			{
				return integer(aColumn, static_cast<std::int64_t>(aValue));
			}
			auto blob(const int aColumn, const std::span<const std::uint8_t> aBytes) -> query&
			{
				// A zero-length blob binds as an empty blob, not NULL.
				static constexpr std::uint8_t empty = 0;
				const auto*                   data  = aBytes.empty() ? &empty : aBytes.data();
				Good                                = Good && sqlite3_bind_blob64(Statement.Value, aColumn, data, aBytes.size(), SQLITE_TRANSIENT) == SQLITE_OK;
				return *this;
			}
			auto text(const int aColumn, const std::string_view aText) -> query&
			{
				Good = Good && sqlite3_bind_text64(Statement.Value, aColumn, aText.data(), aText.size(), SQLITE_TRANSIENT, SQLITE_UTF8) == SQLITE_OK;
				return *this;
			}
			auto optional_id(const int aColumn, const std::optional<std::uint64_t> aValue) -> query&
			{
				if (aValue) {
					return id(aColumn, *aValue);
				}
				Good = Good && sqlite3_bind_null(Statement.Value, aColumn) == SQLITE_OK;
				return *this;
			}
			/// SQLITE_ROW, SQLITE_DONE or an error code.
			auto step() -> int
			{
				return Good ? sqlite3_step(Statement.Value) : SQLITE_ERROR;
			}
			auto done() -> bool
			{
				return step() == SQLITE_DONE;
			}
			[[nodiscard]] auto column_integer(const int aColumn) const -> std::int64_t
			{
				return sqlite3_column_int64(Statement.Value, aColumn);
			}
			[[nodiscard]] auto column_null(const int aColumn) const -> bool
			{
				return sqlite3_column_type(Statement.Value, aColumn) == SQLITE_NULL;
			}
			[[nodiscard]] auto column_blob(const int aColumn) const -> std::span<const std::uint8_t>
			{
				const auto size = sqlite3_column_bytes(Statement.Value, aColumn);
				if (sqlite3_column_type(Statement.Value, aColumn) != SQLITE_BLOB || size <= 0) {
					return { };
				}
				return { static_cast<const std::uint8_t*>(sqlite3_column_blob(Statement.Value, aColumn)), static_cast<std::size_t>(size) };
			}
			[[nodiscard]] auto column_text(const int aColumn) const -> std::string_view
			{
				const auto* value = sqlite3_column_text(Statement.Value, aColumn);
				const auto  size  = sqlite3_column_bytes(Statement.Value, aColumn);
				if (value == nullptr || size < 0) {
					return { };
				}
				return { reinterpret_cast<const char*>(value), static_cast<std::size_t>(size) };
			}

		private:
			detail::statement Statement;
			bool              Good = true;
		};

		template <typename TDigest>
		auto digest_from(const std::span<const std::uint8_t> aBytes) -> std::optional<TDigest>
		{
			if (aBytes.size() != 32U) {
				return std::nullopt;
			}
			auto bytes = std::array<std::uint8_t, 32>{ };
			std::ranges::copy(aBytes, bytes.begin());
			const auto value = TDigest{ bytes };
			return value.valid() ? std::optional{ value } : std::nullopt;
		}

		/// A positive stored identity inside the durable domain.
		auto stored_id(const std::int64_t aValue) -> std::optional<std::uint64_t>
		{
			if (aValue <= 0) {
				return std::nullopt;
			}
			return static_cast<std::uint64_t>(aValue);
		}

		/// Canonical request bytes; only their digest is stored.
		struct request_writer
		{
			std::vector<std::uint8_t> Bytes;
			auto                      u8(const std::uint8_t aValue) -> request_writer&
			{
				Bytes.push_back(aValue);
				return *this;
			}
			auto u64(const std::uint64_t aValue) -> request_writer&
			{
				for (auto shift = 56; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
				return *this;
			}
			auto bytes(const std::span<const std::uint8_t> aValue) -> request_writer&
			{
				u64(aValue.size());
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
				return *this;
			}
			auto text(const std::string_view aValue) -> request_writer&
			{
				return bytes({ reinterpret_cast<const std::uint8_t*>(aValue.data()), aValue.size() });
			}
			auto subject(const core::local_cycle_subject& aSubject) -> request_writer&
			{
				u64(aSubject.Authority.Profile.value()).bytes(aSubject.Authority.Incarnation.bytes());
				return u64(aSubject.Cycle.value()).u64(aSubject.Generation);
			}
		};

		auto encode_scope(const std::vector<std::string>& aPaths) -> std::vector<std::uint8_t>
		{
			auto bytes = std::vector<std::uint8_t>{ };
			for (const auto& path : aPaths) {
				if (!bytes.empty()) {
					bytes.push_back(0);
				}
				bytes.insert(bytes.end(), path.begin(), path.end());
			}
			return bytes;
		}
		auto decode_scope(const std::span<const std::uint8_t> aBytes) -> std::vector<std::string>
		{
			auto paths = std::vector<std::string>{ };
			if (aBytes.empty()) {
				return paths;
			}
			auto current = std::string{ };
			for (const auto byte : aBytes) {
				if (byte == 0) {
					paths.push_back(std::exchange(current, { }));
				} else {
					current.push_back(static_cast<char>(byte));
				}
			}
			paths.push_back(std::move(current));
			return paths;
		}

		class cycle_store final : public application::review_cycle_store
		{
		public:
			cycle_store(std::filesystem::path aRoot, const core::bpr_profile_id aProfile, const review_cycle_store_options aOptions)
				: Root{ std::move(aRoot) }
				, Profile{ aProfile }
				, Options{ aOptions }
			{
			}

			auto catalog(std::stop_token aStop) -> cycle_store_result<core::review_cycle_catalog> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) { return read_catalog(aDatabase); });
			}

			auto create(const application::cycle_operation_id& aOperation, const core::create_review_cycle& aCommand, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(1).u64(aCommand.Authority.Profile.value()).bytes(aCommand.Authority.Incarnation.bytes()).text(aCommand.Title);
				request.bytes(encode_scope(aCommand.Scope.Paths));
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					const auto known = read_catalog(aDatabase);
					if (!known) {
						return std::unexpected(known.error());
					}
					const auto created = core::create(*known, aCommand);
					if (!created) {
						return refuse(created.error());
					}
					auto update = query{ aDatabase, "UPDATE catalog SET next_cycle=?1 WHERE singleton=1" };
					if (!update.id(1, created->Catalog.NextCycle).done() || !store_cycle(aDatabase, created->Cycle, true)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = created->Cycle.Subject.Cycle, .Revision = created->Cycle.Revision, .Allocated = created->Cycle.Subject.Cycle.value(), .Changed = true };
				});
			}

			auto load(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::optional<core::review_cycle_state>> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<std::optional<core::review_cycle_state>> {
					auto current = read_cycle(aDatabase, aSubject.Authority, aSubject.Cycle);
					if (!current || !*current) {
						return current;
					}
					if ((*current)->Subject != aSubject) {
						return std::optional<core::review_cycle_state>{ };
					}
					return current;
				});
			}

			auto list(const std::optional<core::review_cycle_id> aAfter, std::stop_token aStop) -> cycle_store_result<application::cycle_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_page> {
					const auto known = read_catalog(aDatabase);
					if (!known) {
						return std::unexpected(known.error());
					}
					auto ids = query{ aDatabase, "SELECT cycle FROM cycles WHERE cycle>?1 ORDER BY cycle LIMIT ?2" };
					ids.id(1, aAfter ? aAfter->value() : 0U).id(2, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_page{ };
					auto status = ids.step();
					for (; status == SQLITE_ROW; status = ids.step()) {
						const auto id = stored_id(ids.column_integer(0));
						if (!id) {
							return fail(cycle_store_failure::corrupt);
						}
						if (page.Cycles.size() == application::maximum_cycle_page_entries) {
							page.Next = page.Cycles.back().Subject.Cycle;
							break;
						}
						const auto cycle = read_cycle(aDatabase, known->Authority, core::review_cycle_id{ *id });
						if (!cycle || !*cycle) {
							return fail(cycle ? cycle_store_failure::corrupt : cycle.error().Failure);
						}
						page.Cycles.push_back(core::project(**cycle));
					}
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return page;
				});
			}

			auto apply(const application::cycle_operation_id& aOperation, const core::review_cycle_command& aCommand, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				if (std::holds_alternative<core::record_cycle_capture>(aCommand.Change)) {
					return refuse(core::cycle_refusal::invalid_capture);
				}
				auto request = request_writer{ };
				request.u8(2).subject(aCommand.Subject).u64(aCommand.ExpectedRevision).u8(static_cast<std::uint8_t>(aCommand.Change.index()));
				if (const auto* rename = std::get_if<core::rename_review_cycle>(&aCommand.Change)) {
					request.text(rename->Title);
				}
				if (const auto* scope = std::get_if<core::change_cycle_scope>(&aCommand.Change)) {
					request.bytes(encode_scope(scope->Paths));
				}
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aCommand.Subject);
					if (!current) {
						return std::unexpected(current.error());
					}
					const auto outcome = core::apply(*current, aCommand);
					if (auto refused = refusal(outcome.Refusal, *current)) {
						return std::unexpected(*refused);
					}
					if (outcome.Changed && !store_cycle(aDatabase, outcome.Accepted, false)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aCommand.Subject.Cycle, .Revision = outcome.Accepted.Revision, .Changed = outcome.Changed };
				});
			}

			auto install(const std::span<const std::uint8_t> aBytes, std::stop_token aStop) -> cycle_store_result<cycle_object_ref> override
			{
				if (aBytes.size() > application::maximum_cycle_object_bytes) {
					return fail(cycle_store_failure::bounds);
				}
				const auto digest = core::cycle_material_digest{ core::sha256(aBytes) };
				const auto object = cycle_object_ref{ .Digest = digest, .Size = aBytes.size() };
				auto       opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				const auto progress = detail::progress_scope{ database.Database, aStop };
				if (!detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				auto guard  = transaction{ database.Database };
				auto exists = query{ database.Database, "SELECT size FROM objects WHERE digest=?1" };
				exists.blob(1, digest.bytes());
				const auto present = exists.step();
				if (present == SQLITE_ROW) {
					if (exists.column_integer(0) != static_cast<std::int64_t>(aBytes.size())) {
						return fail(cycle_store_failure::corrupt);
					}
					return object;
				}
				if (present != SQLITE_DONE) {
					return fail(cycle_store_failure::unavailable);
				}
				const auto retained = read_inventory(database.Database);
				if (!retained) {
					return std::unexpected(retained.error());
				}
				if (retained->PinnedBytes + retained->UnreferencedBytes + aBytes.size() > retained->Budget) {
					return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::quota, .Inventory = *retained });
				}
				if (!insert_object(database.Database, object, aBytes) || !guard.commit(aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				// Verify the installed bytes before any catalog row may name them.
				if (auto verified = read_bytes(database.Database, object); !verified) {
					return std::unexpected(verified.error());
				}
				return object;
			}

			auto read_object(const cycle_object_ref& aObject, std::stop_token aStop) -> cycle_store_result<std::vector<std::uint8_t>> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) { return read_bytes(aDatabase, aObject); });
			}

			auto record_capture(const application::cycle_operation_id& aOperation, const application::cycle_capture_commit& aCommit, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				const auto& capture = aCommit.Capture.Capture;
				auto        request = request_writer{ };
				request.u8(3).subject(aCommit.Subject).u64(aCommit.ExpectedRevision).u64(capture.ScopeVersion).bytes(capture.Material.bytes());
				request.u64(capture.Entries).u64(capture.ExplicitDeletions).u64(capture.Unavailable);
				request.u8(static_cast<std::uint8_t>(capture.Coverage)).u8(static_cast<std::uint8_t>(capture.Quality)).u8(aCommit.Capture.Deliberate ? 1 : 0).u64(aCommit.Manifest.Size);
				if (aCommit.Manifest.Digest != capture.Material) {
					return refuse(core::cycle_refusal::invalid_capture);
				}
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					if (!object_present(aDatabase, aCommit.Manifest)) {
						return fail(cycle_store_failure::object_unavailable);
					}
					auto current = current_cycle(aDatabase, aCommit.Subject);
					if (!current) {
						return std::unexpected(current.error());
					}
					const auto outcome = core::apply(*current, { .Subject = aCommit.Subject, .ExpectedRevision = aCommit.ExpectedRevision, .Change = aCommit.Capture });
					if (auto refused = refusal(outcome.Refusal, *current)) {
						return std::unexpected(*refused);
					}
					if (outcome.Changed && (!insert_revision(aDatabase, *outcome.Accepted.Latest, aCommit.Manifest.Size) || !store_cycle(aDatabase, outcome.Accepted, false))) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aCommit.Subject.Cycle, .Revision = outcome.Accepted.Revision, .Allocated = outcome.Captured ? outcome.Captured->Id.value() : 0U, .Changed = outcome.Changed };
				});
			}

			auto revisions(const core::local_cycle_subject& aSubject, const std::optional<core::cycle_source_revision_id> aAfter, std::stop_token aStop)
				-> cycle_store_result<application::cycle_revision_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_revision_page> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					auto rows = query{ aDatabase, "SELECT revision FROM revisions WHERE cycle=?1 AND revision>?2 ORDER BY revision LIMIT ?3" };
					rows.id(1, aSubject.Cycle.value()).id(2, aAfter ? aAfter->value() : 0U).id(3, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_revision_page{ };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						if (page.Revisions.size() == application::maximum_cycle_page_entries) {
							page.Next = page.Revisions.back().Revision.Id.Id;
							break;
						}
						const auto id       = stored_id(rows.column_integer(0));
						auto       revision = id ? read_revision(aDatabase, aSubject.Cycle, *id) : std::nullopt;
						if (!revision) {
							return fail(cycle_store_failure::corrupt);
						}
						page.Revisions.push_back(std::move(*revision));
					}
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return page;
				});
			}

			auto begin_draft(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const std::uint64_t aExpectedRevision, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(4).subject(aSubject).u64(aExpectedRevision);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					const auto begun = core::begin_round_draft(*current, aExpectedRevision);
					if (!begun) {
						return std::unexpected(*refusal(begun.error(), *current));
					}
					const auto& [cycle, draft] = *begun;
					auto insert                = query{ aDatabase, "INSERT INTO drafts(cycle,draft,revision,body) VALUES(?1,?2,?3,?4)" };
					insert.id(1, aSubject.Cycle.value()).id(2, draft.Id.Id.value()).id(3, draft.Revision).blob(4, { });
					if (!insert.done() || !store_cycle(aDatabase, cycle, false)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = cycle.Revision, .Allocated = draft.Id.Id.value(), .Changed = true };
				});
			}

			auto save_draft(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft,
							const std::uint64_t aExpectedDraftRevision, const std::span<const std::uint8_t> aBody, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				if (aBody.size() > application::maximum_cycle_draft_bytes) {
					return fail(cycle_store_failure::bounds);
				}
				auto request = request_writer{ };
				request.u8(5).subject(aSubject).u64(aDraft.Cycle.value()).u64(aDraft.Id.value()).u64(aExpectedDraftRevision).bytes(aBody);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (current->Lifecycle == core::cycle_lifecycle::archived) {
						return refuse(core::cycle_refusal::archived);
					}
					auto stored = read_draft(aDatabase, aSubject, aDraft);
					if (!stored) {
						return std::unexpected(stored.error());
					}
					if (!*stored) {
						return fail(cycle_store_failure::not_found);
					}
					const auto edited = core::edit_round_draft((*stored)->Draft, aExpectedDraftRevision);
					if (!edited) {
						return edited.error() == core::cycle_refusal::conflict ? fail(cycle_store_failure::conflict) : refuse(edited.error());
					}
					auto update = query{ aDatabase, "UPDATE drafts SET revision=?3,body=?4 WHERE cycle=?1 AND draft=?2" };
					update.id(1, aDraft.Cycle.value()).id(2, aDraft.Id.value()).id(3, edited->Revision).blob(4, aBody);
					if (!update.done()) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = edited->Revision, .Allocated = aDraft.Id.value(), .Changed = true };
				});
			}

			auto load_draft(const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::stop_token aStop)
				-> cycle_store_result<std::optional<application::stored_cycle_draft>> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<std::optional<application::stored_cycle_draft>> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					return read_draft(aDatabase, aSubject, aDraft);
				});
			}

			auto freeze(const application::cycle_operation_id& aOperation, const application::cycle_freeze_commit& aCommit, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(6).subject(aCommit.Subject).u64(aCommit.Draft.Cycle.value()).u64(aCommit.Draft.Id.value()).u64(aCommit.ExpectedDraftRevision);
				request.bytes(aCommit.Payload.Digest.bytes()).u64(aCommit.Payload.Size);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					if (!object_present(aDatabase, aCommit.Payload)) {
						return fail(cycle_store_failure::object_unavailable);
					}
					auto current = current_cycle(aDatabase, aCommit.Subject);
					if (!current) {
						return std::unexpected(current.error());
					}
					auto stored = read_draft(aDatabase, aCommit.Subject, aCommit.Draft);
					if (!stored) {
						return std::unexpected(stored.error());
					}
					if (!*stored) {
						return fail(cycle_store_failure::not_found);
					}
					auto prior = latest_round(aDatabase, aCommit.Subject.Cycle, aCommit.Draft);
					if (!prior) {
						return std::unexpected(prior.error());
					}
					const auto digest = core::cycle_round_digest{ aCommit.Payload.Digest.bytes() };
					const auto frozen = core::freeze_round(*current, (*stored)->Draft, aCommit.ExpectedDraftRevision, digest, *prior);
					if (!frozen) {
						return frozen.error() == core::cycle_refusal::conflict ? fail(cycle_store_failure::conflict) : refuse(frozen.error());
					}
					if (!frozen->Reused) {
						auto insert = query{ aDatabase, "INSERT INTO rounds(cycle,round,draft,draft_revision,digest,payload_size) VALUES(?1,?2,?3,?4,?5,?6)" };
						insert.id(1, aCommit.Subject.Cycle.value()).id(2, frozen->Round.Id.Id.value()).id(3, aCommit.Draft.Id.value()).id(4, frozen->Round.DraftRevision);
						insert.blob(5, digest.bytes()).id(6, aCommit.Payload.Size);
						if (!insert.done() || !store_cycle(aDatabase, frozen->Cycle, false)) {
							return fail(cycle_store_failure::unavailable);
						}
					}
					return cycle_write_receipt{ .Cycle = aCommit.Subject.Cycle, .Revision = frozen->Cycle.Revision, .Allocated = frozen->Round.Id.Id.value(), .Changed = !frozen->Reused };
				});
			}

			auto rounds(const core::local_cycle_subject& aSubject, const std::optional<core::cycle_round_id> aAfter, std::stop_token aStop)
				-> cycle_store_result<application::cycle_round_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_round_page> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					auto rows = query{ aDatabase, "SELECT round,draft,draft_revision,digest,payload_size FROM rounds WHERE cycle=?1 AND round>?2 ORDER BY round LIMIT ?3" };
					rows.id(1, aSubject.Cycle.value()).id(2, aAfter ? aAfter->value() : 0U).id(3, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_round_page{ };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						if (page.Rounds.size() == application::maximum_cycle_page_entries) {
							page.Next = page.Rounds.back().Round.Id.Id;
							break;
						}
						auto round = decode_round(rows, aSubject.Cycle);
						if (!round) {
							return fail(cycle_store_failure::corrupt);
						}
						page.Rounds.push_back(*round);
					}
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return page;
				});
			}

			auto inventory(std::stop_token aStop) -> cycle_store_result<application::cycle_retention_inventory> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) { return read_inventory(aDatabase); });
			}

			auto reclaim(std::stop_token aStop) -> cycle_store_result<application::cycle_retention_inventory> override
			{
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				const auto progress = detail::progress_scope{ database.Database, aStop };
				if (!detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				auto       guard  = transaction{ database.Database };
				const auto chunks = std::string{ "DELETE FROM object_chunks WHERE digest NOT IN (" } + std::string{ referenced_objects } + ")";
				const auto rows   = std::string{ "DELETE FROM objects WHERE digest NOT IN (" } + std::string{ referenced_objects } + ")";
				if (!detail::execute(database.Database, chunks, aStop) || !detail::execute(database.Database, rows, aStop) || !guard.commit(aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				return read_inventory(database.Database);
			}

			auto removal_preview(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<application::cycle_removal_preview> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) { return preview(aDatabase, aSubject); });
			}

			auto remove(const application::cycle_operation_id& aOperation, const application::cycle_removal_preview& aPreview, std::stop_token aStop)
				-> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(7).subject(aPreview.Subject).u64(aPreview.Revision).bytes(aPreview.Token.bytes());
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					const auto current = preview(aDatabase, aPreview.Subject);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (*current != aPreview) {
						return fail(cycle_store_failure::preview_stale);
					}
					for (const auto sql : { "DELETE FROM revisions WHERE cycle=?1", "DELETE FROM drafts WHERE cycle=?1", "DELETE FROM rounds WHERE cycle=?1", "DELETE FROM cycles WHERE cycle=?1" }) {
						auto erase = query{ aDatabase, sql };
						if (!erase.id(1, aPreview.Subject.Cycle.value()).done()) {
							return fail(cycle_store_failure::unavailable);
						}
					}
					return cycle_write_receipt{ .Cycle = aPreview.Subject.Cycle, .Revision = aPreview.Revision, .Changed = true };
				});
			}

		private:
			static auto cancelled_or(const std::stop_token& aStop, const cycle_store_failure aFailure) -> cycle_store_failure
			{
				return aStop.stop_requested() ? cycle_store_failure::cancelled : aFailure;
			}

			/// Maps a core refusal; conflicts carry the stored state.
			static auto refusal(const core::cycle_refusal aRefusal, const core::review_cycle_state& aCurrent) -> std::optional<cycle_store_error>
			{
				if (aRefusal == core::cycle_refusal::none) {
					return std::nullopt;
				}
				if (aRefusal == core::cycle_refusal::conflict) {
					return cycle_store_error{ .Failure = cycle_store_failure::conflict, .Current = aCurrent };
				}
				return cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = aRefusal };
			}

			auto open(std::stop_token aStop) -> cycle_store_result<sqlite3*>
			{
				if (aStop.stop_requested()) {
					return fail(cycle_store_failure::cancelled);
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return fail(cycle_store_failure::unavailable);
				}
				const auto opened = detail::open_private_store_database(*directory, database_name, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return fail(cycle_store_failure::unavailable);
				}
				if (auto established = establish(*opened, aStop); !established) {
					sqlite3_close_v2(*opened);
					return std::unexpected(established.error());
				}
				return *opened;
			}

			static auto schema_valid(sqlite3* aDatabase) -> bool
			{
				const auto count = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 32)");
				if (detail::scalar_integer(aDatabase, "PRAGMA application_id") != application_id || detail::scalar_integer(aDatabase, "PRAGMA user_version") != schema_version) {
					return false;
				}
				// Automatic primary-key indexes are named sqlite_autoindex_* and not counted.
				if (!count || *count != static_cast<std::int64_t>(schema.size())) {
					return false;
				}
				return std::ranges::all_of(schema, [aDatabase](const table& aTable) {
					auto definition = query{ aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name=?1" };
					definition.text(1, aTable.Name);
					return definition.step() == SQLITE_ROW && detail::canonical_sql(definition.column_text(0)) == detail::canonical_sql(aTable.Definition);
				});
			}

			auto establish(sqlite3* aDatabase, std::stop_token aStop) -> cycle_store_result<void>
			{
				const auto empty = detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0 && detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
					&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema") == 0;
				if (!empty && !schema_valid(aDatabase)) {
					return fail(cycle_store_failure::schema_mismatch);
				}
				if (!detail::configure_connection(aDatabase)) {
					return fail(cycle_store_failure::unavailable);
				}
				if (empty) {
					if (!detail::execute(aDatabase, "BEGIN IMMEDIATE", aStop)) {
						return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
					}
					auto guard = transaction{ aDatabase };
					for (const auto& definition : schema) {
						if (!detail::execute(aDatabase, definition.Definition, aStop)) {
							return fail(cycle_store_failure::unavailable);
						}
					}
					auto catalog = query{ aDatabase, "INSERT INTO catalog(singleton,profile,incarnation,next_cycle) VALUES(1,?1,randomblob(32),1)" };
					if (!catalog.id(1, Profile.value()).done() || !detail::execute(aDatabase, "PRAGMA application_id=1112560195", aStop)
						|| !detail::execute(aDatabase, "PRAGMA user_version=1", aStop) || !schema_valid(aDatabase) || !guard.commit(aStop)) {
						return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
					}
				}
				return { };
			}

			template <typename TWork>
			auto read_only(std::stop_token aStop, TWork&& aWork) -> decltype(aWork(std::declval<sqlite3*>()))
			{
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				const auto progress = detail::progress_scope{ database.Database, aStop };
				auto       result   = std::forward<TWork>(aWork)(database.Database);
				if (aStop.stop_requested()) {
					return fail(cycle_store_failure::cancelled);
				}
				return result;
			}

			/// Runs one deduplicated write in one immediate transaction.
			template <typename TWork>
			auto write(const application::cycle_operation_id& aOperation, const request_writer& aRequest, std::stop_token aStop, TWork&& aWork)
				-> cycle_store_result<cycle_write_receipt>
			{
				if (!aOperation.valid()) {
					return fail(cycle_store_failure::bounds);
				}
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				const auto progress = detail::progress_scope{ database.Database, aStop };
				if (!detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				auto       guard   = transaction{ database.Database };
				const auto request = core::sha256(aRequest.Bytes);
				auto       prior   = query{ database.Database, "SELECT request,cycle,revision,allocated,changed FROM receipts WHERE operation=?1" };
				prior.blob(1, aOperation.bytes());
				const auto found = prior.step();
				if (found == SQLITE_ROW) {
					if (!std::ranges::equal(prior.column_blob(0), request)) {
						return fail(cycle_store_failure::operation_reused);
					}
					const auto cycle = stored_id(prior.column_integer(1));
					if (!cycle) {
						return fail(cycle_store_failure::corrupt);
					}
					return cycle_write_receipt{
						.Cycle     = core::review_cycle_id{ *cycle },
						.Revision  = static_cast<std::uint64_t>(prior.column_integer(2)),
						.Allocated = static_cast<std::uint64_t>(prior.column_integer(3)),
						.Changed   = prior.column_integer(4) != 0,
						.Replayed  = true,
					};
				}
				if (found != SQLITE_DONE) {
					return fail(cycle_store_failure::unavailable);
				}
				auto receipt = std::forward<TWork>(aWork)(database.Database);
				if (!receipt) {
					return receipt;
				}
				auto record = query{ database.Database, "INSERT INTO receipts(operation,request,cycle,revision,allocated,changed) VALUES(?1,?2,?3,?4,?5,?6)" };
				record.blob(1, aOperation.bytes()).blob(2, request).id(3, receipt->Cycle.value()).id(4, receipt->Revision).id(5, receipt->Allocated).integer(6, receipt->Changed ? 1 : 0);
				auto trim = query{ database.Database, "DELETE FROM receipts WHERE rowid IN (SELECT rowid FROM receipts ORDER BY rowid LIMIT max(0,(SELECT count(*) FROM receipts)-?1))" };
				if (!record.done() || !trim.integer(1, maximum_receipts).done() || !guard.commit(aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				return receipt;
			}

			auto read_catalog(sqlite3* aDatabase) const -> cycle_store_result<core::review_cycle_catalog>
			{
				auto row = query{ aDatabase, "SELECT profile,incarnation,next_cycle FROM catalog WHERE singleton=1" };
				if (row.step() != SQLITE_ROW) {
					return fail(cycle_store_failure::corrupt);
				}
				const auto profile     = stored_id(row.column_integer(0));
				const auto incarnation = digest_from<core::cycle_catalog_incarnation>(row.column_blob(1));
				const auto next        = stored_id(row.column_integer(2));
				if (!profile || !incarnation || !next) {
					return fail(cycle_store_failure::corrupt);
				}
				if (core::bpr_profile_id{ *profile } != Profile) {
					return fail(cycle_store_failure::wrong_profile);
				}
				return core::review_cycle_catalog{ .Authority = { .Profile = Profile, .Incarnation = *incarnation }, .NextCycle = *next };
			}

			/// Loads the current cycle a command names, or the reason it cannot.
			auto current_cycle(sqlite3* aDatabase, const core::local_cycle_subject& aSubject) const -> cycle_store_result<core::review_cycle_state>
			{
				const auto known = read_catalog(aDatabase);
				if (!known) {
					return std::unexpected(known.error());
				}
				if (aSubject.Authority != known->Authority) {
					return refuse(core::cycle_refusal::invalid_identity);
				}
				auto current = read_cycle(aDatabase, known->Authority, aSubject.Cycle);
				if (!current) {
					return std::unexpected(current.error());
				}
				if (!*current) {
					return fail(cycle_store_failure::not_found);
				}
				if ((*current)->Subject != aSubject) {
					return refuse(core::cycle_refusal::invalid_identity);
				}
				return std::move(**current);
			}

			static auto read_cycle(sqlite3* aDatabase, const core::local_cycle_authority& aAuthority, const core::review_cycle_id aCycle)
				-> cycle_store_result<std::optional<core::review_cycle_state>>
			{
				auto row = query{ aDatabase, "SELECT generation,revision,title,lifecycle,scope_version,scope,initial,latest,next_revision,next_item,next_draft,next_round,next_response,next_verification,next_publication FROM cycles WHERE cycle=?1" };
				row.id(1, aCycle.value());
				const auto status = row.step();
				if (status == SQLITE_DONE) {
					return std::optional<core::review_cycle_state>{ };
				}
				if (status != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				auto counters = std::array<std::optional<std::uint64_t>, 9>{ };
				for (auto index = 0U; index != counters.size(); ++index) {
					static constexpr auto columns = std::array{ 0, 1, 4, 8, 9, 10, 11, 12, 13 };
					counters.at(index)            = stored_id(row.column_integer(columns.at(index)));
				}
				const auto lifecycle = row.column_integer(3);
				if (std::ranges::any_of(counters, [](const auto& aValue) { return !aValue.has_value(); }) || !stored_id(row.column_integer(14))
					|| lifecycle < 0 || lifecycle > static_cast<std::int64_t>(core::cycle_lifecycle::archived)) {
					return fail(cycle_store_failure::corrupt);
				}
				auto state = core::review_cycle_state{
					.Subject   = { .Authority = aAuthority, .Cycle = aCycle, .Generation = *counters[0] },
					.Revision  = *counters[1],
					.Title     = std::string{ row.column_text(2) },
					.Lifecycle = static_cast<core::cycle_lifecycle>(lifecycle),
					.Scope     = { .Version = *counters[2], .Paths = decode_scope(row.column_blob(5)) },
					.Next      = {
						.SourceRevision = *counters[3],
						.FeedbackItem   = *counters[4],
						.RoundDraft     = *counters[5],
						.Round          = *counters[6],
						.Response       = *counters[7],
						.Verification   = *counters[8],
						.Publication    = static_cast<std::uint64_t>(row.column_integer(14)),
					},
				};
				if (!core::valid(state.Subject) || state.Title.empty() || state.Title.size() > core::maximum_cycle_title_bytes
					|| core::validate_scope_paths(state.Scope.Paths) != core::cycle_refusal::none) {
					return fail(cycle_store_failure::corrupt);
				}
				for (const auto& [column, target] : { std::pair{ 6, &state.Initial }, std::pair{ 7, &state.Latest } }) {
					if (row.column_null(column)) {
						continue;
					}
					const auto id       = stored_id(row.column_integer(column));
					auto       revision = id && *id < state.Next.SourceRevision ? read_revision(aDatabase, aCycle, *id) : std::nullopt;
					if (!revision) {
						return fail(cycle_store_failure::corrupt);
					}
					*target = revision->Revision;
				}
				if (state.Initial.has_value() != state.Latest.has_value()) {
					return fail(cycle_store_failure::corrupt);
				}
				return std::optional{ std::move(state) };
			}

			static auto read_revision(sqlite3* aDatabase, const core::review_cycle_id aCycle, const std::uint64_t aRevision) -> std::optional<application::stored_cycle_revision>
			{
				auto row = query{ aDatabase, "SELECT parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size FROM revisions WHERE cycle=?1 AND revision=?2" };
				row.id(1, aCycle.value()).id(2, aRevision);
				if (row.step() != SQLITE_ROW) {
					return std::nullopt;
				}
				const auto parent   = row.column_null(0) ? std::optional<std::uint64_t>{ } : stored_id(row.column_integer(0));
				const auto scope    = stored_id(row.column_integer(1));
				const auto material = digest_from<core::cycle_material_digest>(row.column_blob(2));
				const auto coverage = row.column_integer(6);
				const auto quality  = row.column_integer(7);
				const auto counts   = std::array{ row.column_integer(3), row.column_integer(4), row.column_integer(5), row.column_integer(8) };
				if ((!row.column_null(0) && !parent) || !scope || !material || coverage < 0 || coverage > 1 || quality < 0 || quality > 1
					|| std::ranges::any_of(counts, [](const std::int64_t aValue) { return aValue < 0; })) {
					return std::nullopt;
				}
				return application::stored_cycle_revision{
					.Revision = {
						.Id      = { .Cycle = aCycle, .Id = core::cycle_source_revision_id{ aRevision } },
						.Parent  = parent ? std::optional{ core::cycle_source_revision_id{ *parent } } : std::nullopt,
						.Capture = {
							.ScopeVersion      = *scope,
							.Material          = *material,
							.Entries           = static_cast<std::uint64_t>(counts[0]),
							.ExplicitDeletions = static_cast<std::uint64_t>(counts[1]),
							.Unavailable       = static_cast<std::uint64_t>(counts[2]),
							.Coverage          = static_cast<core::cycle_capture_coverage>(coverage),
							.Quality           = static_cast<core::cycle_capture_quality>(quality),
						},
					},
					.Manifest = { .Digest = *material, .Size = static_cast<std::uint64_t>(counts[3]) },
				};
			}

			static auto store_cycle(sqlite3* aDatabase, const core::review_cycle_state& aCycle, const bool aInsert) -> bool
			{
				const auto sql      = aInsert
					? "INSERT INTO cycles(cycle,generation,revision,title,lifecycle,scope_version,scope,initial,latest,next_revision,next_item,next_draft,next_round,next_response,next_verification,next_publication) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16)"
					: "UPDATE cycles SET generation=?2,revision=?3,title=?4,lifecycle=?5,scope_version=?6,scope=?7,initial=?8,latest=?9,next_revision=?10,next_item=?11,next_draft=?12,next_round=?13,next_response=?14,next_verification=?15,next_publication=?16 WHERE cycle=?1";
				auto       row      = query{ aDatabase, sql };
				const auto revision = [](const std::optional<core::cycle_source_revision>& aRevision) {
					return aRevision ? std::optional{ aRevision->Id.Id.value() } : std::nullopt;
				};
				row.id(1, aCycle.Subject.Cycle.value()).id(2, aCycle.Subject.Generation).id(3, aCycle.Revision).text(4, aCycle.Title);
				row.integer(5, static_cast<std::int64_t>(aCycle.Lifecycle)).id(6, aCycle.Scope.Version).blob(7, encode_scope(aCycle.Scope.Paths));
				row.optional_id(8, revision(aCycle.Initial)).optional_id(9, revision(aCycle.Latest));
				const auto& next = aCycle.Next;
				row.id(10, next.SourceRevision).id(11, next.FeedbackItem).id(12, next.RoundDraft).id(13, next.Round).id(14, next.Response).id(15, next.Verification).id(16, next.Publication);
				return row.done() && sqlite3_changes(aDatabase) == 1;
			}

			static auto insert_revision(sqlite3* aDatabase, const core::cycle_source_revision& aRevision, const std::uint64_t aManifestSize) -> bool
			{
				const auto& capture = aRevision.Capture;
				auto        row     = query{ aDatabase, "INSERT INTO revisions(cycle,revision,parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)" };
				row.id(1, aRevision.Id.Cycle.value()).id(2, aRevision.Id.Id.value()).optional_id(3, aRevision.Parent ? std::optional{ aRevision.Parent->value() } : std::nullopt);
				row.id(4, capture.ScopeVersion).blob(5, capture.Material.bytes()).id(6, capture.Entries).id(7, capture.ExplicitDeletions).id(8, capture.Unavailable);
				row.integer(9, static_cast<std::int64_t>(capture.Coverage)).integer(10, static_cast<std::int64_t>(capture.Quality)).id(11, aManifestSize);
				return row.done();
			}

			static auto read_draft(sqlite3* aDatabase, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft)
				-> cycle_store_result<std::optional<application::stored_cycle_draft>>
			{
				if (!core::belongs_to(aDraft, aSubject)) {
					return refuse(core::cycle_refusal::invalid_identity);
				}
				auto row = query{ aDatabase, "SELECT revision,body FROM drafts WHERE cycle=?1 AND draft=?2" };
				row.id(1, aDraft.Cycle.value()).id(2, aDraft.Id.value());
				const auto status = row.step();
				if (status == SQLITE_DONE) {
					return std::optional<application::stored_cycle_draft>{ };
				}
				const auto revision = stored_id(row.column_integer(0));
				const auto body     = row.column_blob(1);
				if (status != SQLITE_ROW || !revision || body.size() > application::maximum_cycle_draft_bytes) {
					return fail(status == SQLITE_ROW ? cycle_store_failure::corrupt : cycle_store_failure::unavailable);
				}
				return std::optional{ application::stored_cycle_draft{ .Draft = { .Id = aDraft, .Revision = *revision }, .Body = { body.begin(), body.end() } } };
			}

			static auto decode_round(query& aRow, const core::review_cycle_id aCycle) -> std::optional<application::stored_cycle_round>
			{
				const auto round    = stored_id(aRow.column_integer(0));
				const auto draft    = stored_id(aRow.column_integer(1));
				const auto revision = stored_id(aRow.column_integer(2));
				const auto digest   = digest_from<core::cycle_round_digest>(aRow.column_blob(3));
				const auto size     = aRow.column_integer(4);
				if (!round || !draft || !revision || !digest || size < 0) {
					return std::nullopt;
				}
				return application::stored_cycle_round{
					.Round   = { .Id = { .Cycle = aCycle, .Id = core::cycle_round_id{ *round } }, .Draft = { .Cycle = aCycle, .Id = core::cycle_round_draft_id{ *draft } }, .DraftRevision = *revision, .Digest = *digest },
					.Payload = { .Digest = core::cycle_material_digest{ digest->bytes() }, .Size = static_cast<std::uint64_t>(size) },
				};
			}

			static auto latest_round(sqlite3* aDatabase, const core::review_cycle_id aCycle, const core::cycle_round_draft_ref& aDraft)
				-> cycle_store_result<std::optional<core::cycle_round>>
			{
				auto row = query{ aDatabase, "SELECT round,draft,draft_revision,digest,payload_size FROM rounds WHERE cycle=?1 AND draft=?2 ORDER BY round DESC LIMIT 1" };
				row.id(1, aCycle.value()).id(2, aDraft.Id.value());
				const auto status = row.step();
				if (status == SQLITE_DONE) {
					return std::optional<core::cycle_round>{ };
				}
				auto round = status == SQLITE_ROW ? decode_round(row, aCycle) : std::nullopt;
				if (!round) {
					return fail(status == SQLITE_ROW ? cycle_store_failure::corrupt : cycle_store_failure::unavailable);
				}
				return std::optional{ round->Round };
			}

			static auto object_present(sqlite3* aDatabase, const cycle_object_ref& aObject) -> bool
			{
				auto row = query{ aDatabase, "SELECT size FROM objects WHERE digest=?1" };
				row.blob(1, aObject.Digest.bytes());
				return row.step() == SQLITE_ROW && row.column_integer(0) == static_cast<std::int64_t>(aObject.Size);
			}

			static auto insert_object(sqlite3* aDatabase, const cycle_object_ref& aObject, const std::span<const std::uint8_t> aBytes) -> bool
			{
				const auto chunks = (aBytes.size() + chunk_bytes - 1U) / chunk_bytes;
				auto       row    = query{ aDatabase, "INSERT INTO objects(digest,size,chunks) VALUES(?1,?2,?3)" };
				if (!row.blob(1, aObject.Digest.bytes()).id(2, aBytes.size()).id(3, chunks).done()) {
					return false;
				}
				for (auto ordinal = std::size_t{ 0 }; ordinal != chunks; ++ordinal) {
					auto chunk = query{ aDatabase, "INSERT INTO object_chunks(digest,ordinal,bytes) VALUES(?1,?2,?3)" };
					if (!chunk.blob(1, aObject.Digest.bytes()).id(2, ordinal).blob(3, aBytes.subspan(ordinal * chunk_bytes, std::min(chunk_bytes, aBytes.size() - ordinal * chunk_bytes))).done()) {
						return false;
					}
				}
				return true;
			}

			static auto read_bytes(sqlite3* aDatabase, const cycle_object_ref& aObject) -> cycle_store_result<std::vector<std::uint8_t>>
			{
				if (aObject.Size > application::maximum_cycle_object_bytes) {
					return fail(cycle_store_failure::bounds);
				}
				auto header = query{ aDatabase, "SELECT size,chunks FROM objects WHERE digest=?1" };
				header.blob(1, aObject.Digest.bytes());
				const auto status = header.step();
				if (status == SQLITE_DONE) {
					return fail(cycle_store_failure::object_unavailable);
				}
				if (status != SQLITE_ROW || header.column_integer(0) != static_cast<std::int64_t>(aObject.Size)) {
					return fail(status == SQLITE_ROW ? cycle_store_failure::corrupt : cycle_store_failure::unavailable);
				}
				const auto chunks = header.column_integer(1);
				auto       bytes  = std::vector<std::uint8_t>{ };
				bytes.reserve(aObject.Size);
				auto rows = query{ aDatabase, "SELECT ordinal,bytes FROM object_chunks WHERE digest=?1 ORDER BY ordinal" };
				rows.blob(1, aObject.Digest.bytes());
				auto expected = std::int64_t{ 0 };
				auto step     = rows.step();
				for (; step == SQLITE_ROW; step = rows.step()) {
					const auto chunk = rows.column_blob(1);
					if (rows.column_integer(0) != expected++ || bytes.size() + chunk.size() > aObject.Size) {
						return fail(cycle_store_failure::corrupt);
					}
					bytes.insert(bytes.end(), chunk.begin(), chunk.end());
				}
				if (step != SQLITE_DONE || expected != chunks || bytes.size() != aObject.Size
					|| core::cycle_material_digest{ core::sha256(bytes) } != aObject.Digest) {
					return fail(step == SQLITE_DONE ? cycle_store_failure::corrupt : cycle_store_failure::unavailable);
				}
				return bytes;
			}

			auto read_inventory(sqlite3* aDatabase) const -> cycle_store_result<application::cycle_retention_inventory>
			{
				const auto sql = std::string{ "SELECT coalesce(sum(CASE WHEN digest IN (" } + std::string{ referenced_objects }
					+ ") THEN size END),0),coalesce(sum(CASE WHEN digest IN (" + std::string{ referenced_objects }
					+ ") THEN 1 END),0),coalesce(sum(CASE WHEN digest NOT IN (" + std::string{ referenced_objects }
					+ ") THEN size END),0),coalesce(sum(CASE WHEN digest NOT IN (" + std::string{ referenced_objects } + ") THEN 1 END),0) FROM objects";
				auto row = query{ aDatabase, sql };
				if (row.step() != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				return application::cycle_retention_inventory{
					.Budget              = Options.RetainedBudgetBytes,
					.PinnedBytes         = static_cast<std::uint64_t>(row.column_integer(0)),
					.PinnedObjects       = static_cast<std::uint64_t>(row.column_integer(1)),
					.UnreferencedBytes   = static_cast<std::uint64_t>(row.column_integer(2)),
					.UnreferencedObjects = static_cast<std::uint64_t>(row.column_integer(3)),
				};
			}

			auto preview(sqlite3* aDatabase, const core::local_cycle_subject& aSubject) const -> cycle_store_result<application::cycle_removal_preview>
			{
				const auto current = current_cycle(aDatabase, aSubject);
				if (!current) {
					return std::unexpected(current.error());
				}
				const auto count = [&](const char* aSql) -> std::optional<std::uint64_t> {
					auto row = query{ aDatabase, aSql };
					if (row.id(1, aSubject.Cycle.value()).step() != SQLITE_ROW || row.column_integer(0) < 0) {
						return std::nullopt;
					}
					return static_cast<std::uint64_t>(row.column_integer(0));
				};
				const auto revisionCount = count("SELECT count(*) FROM revisions WHERE cycle=?1");
				const auto draftCount    = count("SELECT count(*) FROM drafts WHERE cycle=?1");
				const auto roundCount    = count("SELECT count(*) FROM rounds WHERE cycle=?1");
				const auto released      = count(
					"SELECT coalesce(sum(size),0) FROM objects WHERE digest IN (SELECT material FROM revisions WHERE cycle=?1 UNION SELECT digest FROM rounds WHERE cycle=?1) "
					"AND digest NOT IN (SELECT material FROM revisions WHERE cycle<>?1 UNION SELECT digest FROM rounds WHERE cycle<>?1)");
				if (!revisionCount || !draftCount || !roundCount || !released) {
					return fail(cycle_store_failure::unavailable);
				}
				auto result = application::cycle_removal_preview{
					.Subject       = aSubject,
					.Revision      = current->Revision,
					.Revisions     = *revisionCount,
					.Drafts        = *draftCount,
					.Rounds        = *roundCount,
					.ReleasedBytes = *released,
				};
				auto token = request_writer{ };
				token.u8(8).subject(aSubject).u64(result.Revision).u64(result.Revisions).u64(result.Drafts).u64(result.Rounds).u64(result.ReleasedBytes);
				result.Token = core::cycle_material_digest{ core::sha256(token.Bytes) };
				return result;
			}

			std::filesystem::path      Root;
			core::bpr_profile_id       Profile;
			review_cycle_store_options Options;
		};
	}

	auto make_review_cycle_store(std::filesystem::path aRoot, const core::bpr_profile_id aProfile, const review_cycle_store_options aOptions)
		-> std::unique_ptr<application::review_cycle_store>
	{
		return std::make_unique<cycle_store>(std::move(aRoot), aProfile, aOptions);
	}
}
