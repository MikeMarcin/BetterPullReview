#include "bpr/adapters/sqlite/review_cycle_store.hpp"

#include "bpr/core/sha256.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		using application::cycle_object_ref;
		using application::cycle_store_error;
		using application::cycle_store_failure;
		using application::cycle_store_result;
		using application::cycle_write_receipt;

		constexpr std::int64_t     application_id   = 0x42505243;
		constexpr std::int64_t     schema_version   = 3;
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
			table{ "revisions", "CREATE TABLE revisions(cycle INTEGER NOT NULL,revision INTEGER NOT NULL,parent INTEGER,scope_version INTEGER NOT NULL,material BLOB NOT NULL CHECK(length(material)=32),entries INTEGER NOT NULL,deletions INTEGER NOT NULL,unavailable INTEGER NOT NULL,coverage INTEGER NOT NULL,quality INTEGER NOT NULL,manifest_size INTEGER NOT NULL,workspace_version INTEGER NOT NULL,PRIMARY KEY(cycle,revision)) STRICT" },
			table{ "revision_objects", "CREATE TABLE revision_objects(cycle INTEGER NOT NULL,revision INTEGER NOT NULL,digest BLOB NOT NULL CHECK(length(digest)=32),PRIMARY KEY(cycle,revision,digest)) STRICT" },
			table{ "bindings", "CREATE TABLE bindings(cycle INTEGER PRIMARY KEY,project BLOB NOT NULL CHECK(length(project)=32),version INTEGER NOT NULL,kind INTEGER NOT NULL,location BLOB NOT NULL,root BLOB NOT NULL CHECK(length(root)=24),repository BLOB CHECK(length(repository)=24),worktree BLOB CHECK(length(worktree)=24),admission BLOB CHECK(length(admission)=40)) STRICT" },
			table{ "drafts", "CREATE TABLE drafts(cycle INTEGER NOT NULL,draft INTEGER NOT NULL,revision INTEGER NOT NULL,body BLOB NOT NULL,PRIMARY KEY(cycle,draft)) STRICT" },
			table{ "rounds", "CREATE TABLE rounds(cycle INTEGER NOT NULL,round INTEGER NOT NULL,draft INTEGER NOT NULL,draft_revision INTEGER NOT NULL,digest BLOB NOT NULL CHECK(length(digest)=32),payload_size INTEGER NOT NULL,PRIMARY KEY(cycle,round)) STRICT" },
			table{ "objects", "CREATE TABLE objects(digest BLOB PRIMARY KEY CHECK(length(digest)=32),size INTEGER NOT NULL,chunks INTEGER NOT NULL) STRICT" },
			table{ "object_chunks", "CREATE TABLE object_chunks(digest BLOB NOT NULL,ordinal INTEGER NOT NULL,bytes BLOB NOT NULL,PRIMARY KEY(digest,ordinal)) STRICT" },
			table{ "receipts", "CREATE TABLE receipts(operation BLOB PRIMARY KEY CHECK(length(operation)=32),request BLOB NOT NULL CHECK(length(request)=32),cycle INTEGER NOT NULL,revision INTEGER NOT NULL,allocated INTEGER NOT NULL,changed INTEGER NOT NULL) STRICT" },
			table{ "feedback", "CREATE TABLE feedback(cycle INTEGER NOT NULL,item INTEGER NOT NULL,version INTEGER NOT NULL,last_order INTEGER NOT NULL,lifecycle INTEGER NOT NULL,next_turn INTEGER NOT NULL,next_suggestion INTEGER NOT NULL,target BLOB NOT NULL,related BLOB NOT NULL,PRIMARY KEY(cycle,item)) STRICT" },
			table{ "feedback_revisions", "CREATE TABLE feedback_revisions(cycle INTEGER NOT NULL,item INTEGER NOT NULL,revision INTEGER NOT NULL,turn INTEGER NOT NULL,digest BLOB NOT NULL CHECK(length(digest)=32),author BLOB NOT NULL,completion INTEGER NOT NULL,lineage BLOB NOT NULL,body BLOB NOT NULL,PRIMARY KEY(cycle,item,revision)) STRICT" },
			table{ "suggestions", "CREATE TABLE suggestions(cycle INTEGER NOT NULL,item INTEGER NOT NULL,suggestion INTEGER NOT NULL,revision INTEGER NOT NULL,record BLOB NOT NULL,PRIMARY KEY(cycle,item,suggestion,revision)) STRICT" },
		};
		/// Tables each earlier schema lacks: the first had no bindings or
		/// content pins, and neither the first nor the second had feedback or
		/// suggestions.
		constexpr auto added_in_v2 = std::array<std::string_view, 2>{ "bindings", "revision_objects" };
		constexpr auto added_in_v3 = std::array<std::string_view, 3>{ "feedback", "feedback_revisions", "suggestions" };
		/// The first published schema differs only in `revisions`, which had no
		/// workspace version, and in having no `bindings` or `revision_objects`
		/// table.
		constexpr std::string_view revisions_v1 = "CREATE TABLE revisions(cycle INTEGER NOT NULL,revision INTEGER NOT NULL,parent INTEGER,scope_version INTEGER NOT NULL,material BLOB NOT NULL CHECK(length(material)=32),entries INTEGER NOT NULL,deletions INTEGER NOT NULL,unavailable INTEGER NOT NULL,coverage INTEGER NOT NULL,quality INTEGER NOT NULL,manifest_size INTEGER NOT NULL,PRIMARY KEY(cycle,revision)) STRICT";
		/// Objects some catalog row references. Everything else is reclaimable.
		constexpr std::string_view referenced_objects = "SELECT material AS digest FROM revisions UNION SELECT digest FROM revision_objects UNION SELECT digest FROM rounds";

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
			auto null(const int aColumn) -> query&
			{
				Good = Good && sqlite3_bind_null(Statement.Value, aColumn) == SQLITE_OK;
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
			auto identity(const core::workspace_object_identity& aIdentity) -> request_writer&
			{
				return u64(aIdentity.Volume).u64(aIdentity.ObjectHigh).u64(aIdentity.ObjectLow);
			}
			auto optional_identity(const std::optional<core::workspace_object_identity>& aIdentity) -> request_writer&
			{
				u8(aIdentity ? 1 : 0);
				return aIdentity ? identity(*aIdentity) : *this;
			}
			auto check(const std::optional<core::workspace_material_check>& aCheck) -> request_writer&
			{
				u8(aCheck ? 1 : 0);
				return aCheck ? u64(aCheck->Compared).u64(aCheck->Matching).u64(aCheck->Differing).u64(aCheck->Missing).u64(aCheck->Added) : *this;
			}
			auto root(const core::workspace_root_observation& aRoot) -> request_writer&
			{
				u8(static_cast<std::uint8_t>(aRoot.Kind)).text(aRoot.Location).identity(aRoot.Root);
				return optional_identity(aRoot.Repository).optional_identity(aRoot.Worktree);
			}
			auto optional_text(const std::optional<std::string>& aValue) -> request_writer&
			{
				u8(aValue ? 1 : 0);
				return aValue ? text(*aValue) : *this;
			}
			auto target(const core::cycle_feedback_target& aTarget) -> request_writer&
			{
				u8(static_cast<std::uint8_t>(aTarget.index()));
				if (const auto* module = std::get_if<core::cycle_module_target>(&aTarget)) {
					text(module->Path).u8(module->Revision ? 1 : 0).u64(module->Revision ? module->Revision->value() : 0U);
				} else if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
					u64(file->Revision.value()).text(file->Path).bytes(file->Blob.bytes());
				} else if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&aTarget)) {
					u64(anchor->Revision.value()).text(anchor->Path).bytes(anchor->Blob.bytes()).u64(anchor->Start).u64(anchor->End);
					text(anchor->Original).text(anchor->Before).text(anchor->After).optional_text(anchor->Symbol);
				}
				return *this;
			}
			auto related(const std::vector<core::cycle_related_reference>& aReferences) -> request_writer&
			{
				u64(aReferences.size());
				for (const auto& reference : aReferences) {
					target(reference.Target).u8(static_cast<std::uint8_t>(reference.Disclosure)).u8(reference.Rebinds ? 1 : 0).u64(reference.Rebinds.value_or(0U));
				}
				return *this;
			}
			auto author(const core::cycle_feedback_author& aAuthor) -> request_writer&
			{
				u8(static_cast<std::uint8_t>(aAuthor.Provenance));
				return optional_text(aAuthor.Adapter).optional_text(aAuthor.Provider).optional_text(aAuthor.Model).optional_text(aAuthor.Effort);
			}
			auto revision_ref(const core::cycle_feedback_revision_ref& aRef) -> request_writer&
			{
				return u64(aRef.Position.Turn.value()).u64(aRef.Position.Revision.value()).u64(aRef.Position.Order.value()).bytes(aRef.Digest.bytes());
			}
			auto anchor(const core::cycle_source_anchor& aAnchor) -> request_writer&
			{
				return target(core::cycle_feedback_target{ aAnchor });
			}
			auto suggestion(const core::cycle_suggestion& aSuggestion) -> request_writer&
			{
				u64(aSuggestion.Source.value()).text(aSuggestion.Path).bytes(aSuggestion.OriginalBlob.bytes()).u64(aSuggestion.Start).u64(aSuggestion.End);
				text(aSuggestion.Original).text(aSuggestion.Replacement).u8(static_cast<std::uint8_t>(aSuggestion.Intent)).u8(static_cast<std::uint8_t>(aSuggestion.State));
				return u8(aSuggestion.AppliedIn ? 1 : 0).u64(aSuggestion.AppliedIn ? aSuggestion.AppliedIn->value() : 0U);
			}
			auto suggestion_change(const core::cycle_suggestion_change& aChange) -> request_writer&
			{
				u8(static_cast<std::uint8_t>(aChange.index()));
				if (const auto* revise = std::get_if<core::revise_suggestion>(&aChange)) {
					return text(revise->Replacement).u8(static_cast<std::uint8_t>(revise->Intent));
				}
				if (const auto* revalidate = std::get_if<core::revalidate_suggestion>(&aChange)) {
					return anchor(revalidate->Candidate);
				}
				return u64(std::get<core::observe_suggestion_applied>(aChange).Result.value());
			}
			auto change(const core::cycle_feedback_change& aChange) -> request_writer&
			{
				u8(static_cast<std::uint8_t>(aChange.index()));
				if (const auto* append = std::get_if<core::append_feedback_turn>(&aChange)) {
					author(append->Author).text(append->Body).u8(static_cast<std::uint8_t>(append->Completion)).u8(append->ReplyTo ? 1 : 0);
					u64(append->ReplyTo ? append->ReplyTo->value() : 0U);
				} else if (const auto* edit = std::get_if<core::edit_feedback_turn>(&aChange)) {
					u64(edit->Latest.value()).text(edit->Body).u8(edit->AdoptIncomplete ? 1 : 0);
				} else if (const auto* reference = std::get_if<core::add_feedback_reference>(&aChange)) {
					related({ reference->Reference });
				} else {
					u8(static_cast<std::uint8_t>(std::get<core::set_feedback_lifecycle>(aChange).Lifecycle));
				}
				return *this;
			}
		};

		/// Reads what `request_writer` wrote, refusing anything short or long.
		class record_reader
		{
		public:
			explicit record_reader(const std::span<const std::uint8_t> aBytes) noexcept
				: Bytes{ aBytes }
			{
			}
			auto u8() -> std::uint8_t
			{
				if (!Good || Offset >= Bytes.size()) {
					Good = false;
					return 0;
				}
				return Bytes[Offset++];
			}
			auto u64() -> std::uint64_t
			{
				auto value = std::uint64_t{ 0 };
				for (auto index = 0; index < 8; ++index) {
					value = (value << 8U) | u8();
				}
				return value;
			}
			auto text() -> std::string
			{
				const auto size = u64();
				if (!Good || size > Bytes.size() - Offset) {
					Good = false;
					return { };
				}
				auto value = std::string{ reinterpret_cast<const char*>(Bytes.data() + Offset), static_cast<std::size_t>(size) };
				Offset += static_cast<std::size_t>(size);
				return value;
			}
			template <typename TDigest>
			auto digest() -> TDigest
			{
				const auto value = text();
				const auto bytes = std::span{ reinterpret_cast<const std::uint8_t*>(value.data()), value.size() };
				const auto found = Good ? digest_from<TDigest>(bytes) : std::nullopt;
				Good             = Good && found.has_value();
				return found.value_or(TDigest{ });
			}
			auto optional_text() -> std::optional<std::string>
			{
				return u8() != 0 ? std::optional{ text() } : std::nullopt;
			}
			auto target() -> core::cycle_feedback_target
			{
				switch (u8()) {
				case 0: return core::cycle_whole_target{ };
				case 1: {
					auto       module   = core::cycle_module_target{ .Path = text() };
					const auto present  = u8() != 0;
					const auto revision = u64();
					if (present) {
						module.Revision = core::cycle_source_revision_id{ revision };
					}
					return module;
				}
				case 2: {
					const auto revision = core::cycle_source_revision_id{ u64() };
					auto       path     = text();
					return core::cycle_file_target{ .Revision = revision, .Path = std::move(path), .Blob = digest<core::cycle_material_digest>() };
				}
				case 3: {
					auto anchor     = core::cycle_source_anchor{ .Revision = core::cycle_source_revision_id{ u64() } };
					anchor.Path     = text();
					anchor.Blob     = digest<core::cycle_material_digest>();
					anchor.Start    = u64();
					anchor.End      = u64();
					anchor.Original = text();
					anchor.Before   = text();
					anchor.After    = text();
					anchor.Symbol   = optional_text();
					return anchor;
				}
				default: Good = false; return core::cycle_whole_target{ };
				}
			}
			auto related() -> std::vector<core::cycle_related_reference>
			{
				const auto count      = u64();
				auto       references = std::vector<core::cycle_related_reference>{ };
				if (count > core::maximum_cycle_related_references) {
					Good = false;
					return references;
				}
				for (auto index = std::uint64_t{ 0 }; Good && index < count; ++index) {
					auto       reference  = core::cycle_related_reference{ .Target = target() };
					const auto disclosure = u8();
					const auto present    = u8() != 0;
					const auto rebinds    = u64();
					Good                  = Good && disclosure <= static_cast<std::uint8_t>(core::cycle_reference_disclosure::private_only) && rebinds <= core::maximum_cycle_related_references;
					reference.Disclosure  = static_cast<core::cycle_reference_disclosure>(disclosure);
					if (present) {
						reference.Rebinds = static_cast<std::uint32_t>(rebinds);
					}
					references.push_back(std::move(reference));
				}
				return references;
			}
			auto author() -> core::cycle_feedback_author
			{
				const auto provenance = u8();
				Good                  = Good && provenance <= static_cast<std::uint8_t>(core::authored_provenance::unknown);
				auto author           = core::cycle_feedback_author{ .Provenance = static_cast<core::authored_provenance>(provenance) };
				author.Adapter        = optional_text();
				author.Provider       = optional_text();
				author.Model          = optional_text();
				author.Effort         = optional_text();
				return author;
			}
			auto suggestion(const core::cycle_suggestion_ref& aId, const std::uint64_t aRevision) -> core::cycle_suggestion
			{
				auto value         = core::cycle_suggestion{ .Id = aId, .Revision = aRevision, .Source = core::cycle_source_revision_id{ u64() } };
				value.Path         = text();
				value.OriginalBlob = digest<core::cycle_material_digest>();
				value.Start        = u64();
				value.End          = u64();
				value.Original     = text();
				value.Replacement  = text();
				const auto intent  = u8();
				const auto state   = u8();
				Good               = Good && intent <= static_cast<std::uint8_t>(core::cycle_suggestion_intent::illustrative) && state <= static_cast<std::uint8_t>(core::cycle_suggestion_state::observed_already_applied);
				value.Intent       = static_cast<core::cycle_suggestion_intent>(intent);
				value.State        = static_cast<core::cycle_suggestion_state>(state);
				const auto applied = u8() != 0;
				const auto result  = u64();
				if (applied) {
					value.AppliedIn = core::cycle_source_revision_id{ result };
				}
				return value;
			}
			auto revision_ref(const core::cycle_feedback_item_ref& aItem) -> core::cycle_feedback_revision_ref
			{
				auto ref     = core::cycle_feedback_revision_ref{ .Item = aItem };
				ref.Position = { core::private_review_turn_id{ u64() }, core::private_review_turn_revision_id{ u64() }, core::private_vertex_order{ u64() } };
				ref.Digest   = digest<core::private_body_digest>();
				return ref;
			}
			[[nodiscard]] auto done() const noexcept -> bool
			{
				return Good && Offset == Bytes.size();
			}

		private:
			std::span<const std::uint8_t> Bytes;
			std::size_t                   Offset = 0;
			bool                          Good   = true;
		};

		auto span_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto encode_identity(const core::workspace_object_identity& aIdentity) -> std::vector<std::uint8_t>
		{
			return request_writer{ }.identity(aIdentity).Bytes;
		}
		auto encode_check(const core::workspace_material_check& aCheck) -> std::vector<std::uint8_t>
		{
			return request_writer{ }.u64(aCheck.Compared).u64(aCheck.Matching).u64(aCheck.Differing).u64(aCheck.Missing).u64(aCheck.Added).Bytes;
		}
		/// Big-endian words of a fixed-size blob, or nothing when the size is wrong.
		template <std::size_t Count>
		auto decode_words(const std::span<const std::uint8_t> aBytes) -> std::optional<std::array<std::uint64_t, Count>>
		{
			if (aBytes.size() != Count * 8U) {
				return std::nullopt;
			}
			auto words = std::array<std::uint64_t, Count>{ };
			for (std::size_t index = 0; index != aBytes.size(); ++index) {
				words.at(index / 8U) = (words.at(index / 8U) << 8U) | aBytes[index];
			}
			return words;
		}
		auto decode_identity(const std::span<const std::uint8_t> aBytes) -> std::optional<core::workspace_object_identity>
		{
			const auto words = decode_words<3>(aBytes);
			if (!words) {
				return std::nullopt;
			}
			return core::workspace_object_identity{ .Volume = (*words)[0], .ObjectHigh = (*words)[1], .ObjectLow = (*words)[2] };
		}

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
				if (const auto* bind = std::get_if<core::bind_cycle_workspace>(&aCommand.Change)) {
					request.root(bind->Root).check(bind->Check).u8(bind->Confirmed ? 1 : 0);
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
				const auto one       = std::array{ aBytes };
				auto       installed = install_all(one, aStop);
				if (!installed) {
					return std::unexpected(installed.error());
				}
				return installed->front();
			}

			auto install_all(const std::span<const std::span<const std::uint8_t>> aObjects, std::stop_token aStop) -> cycle_store_result<std::vector<cycle_object_ref>> override
			{
				auto objects = std::vector<cycle_object_ref>{ };
				objects.reserve(aObjects.size());
				for (const auto bytes : aObjects) {
					if (bytes.size() > application::maximum_cycle_object_bytes) {
						return fail(cycle_store_failure::bounds);
					}
					objects.push_back({ .Digest = core::cycle_material_digest{ core::sha256(bytes) }, .Size = bytes.size() });
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
				auto guard  = transaction{ database.Database };
				auto writes = std::vector<std::size_t>{ };
				auto growth = std::uint64_t{ 0 };
				for (std::size_t index = 0; index < objects.size(); ++index) {
					if (std::ranges::any_of(writes, [&](const std::size_t aEarlier) { return objects[aEarlier] == objects[index]; })) {
						continue;
					}
					const auto state = stored_state(database.Database, objects[index]);
					if (!state) {
						return std::unexpected(state.error());
					}
					if (*state == object_state::intact) {
						continue;
					}
					// A damaged copy is replaced by the verified bytes in hand; only
					// genuinely new bytes count against the budget.
					growth += *state == object_state::absent ? objects[index].Size : 0U;
					writes.push_back(index);
				}
				// Pinned and unreferenced bytes together are every object's bytes,
				// so the budget check needs one sum; the breakdown is computed only
				// to explain a refusal.
				if (growth > 0) {
					const auto total = detail::scalar_integer(database.Database, "SELECT coalesce(sum(size),0) FROM objects");
					if (!total || *total < 0) {
						return fail(cycle_store_failure::unavailable);
					}
					if (static_cast<std::uint64_t>(*total) + growth > Options.RetainedBudgetBytes) {
						const auto retained = read_inventory(database.Database);
						if (!retained) {
							return std::unexpected(retained.error());
						}
						return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::quota, .Inventory = *retained });
					}
				}
				for (const auto index : writes) {
					if (!replace_object(database.Database, objects[index], aObjects[index])) {
						return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
					}
				}
				if (!guard.commit(aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				// Verify the installed bytes before any catalog row may name them.
				for (const auto index : writes) {
					if (auto verified = read_bytes(database.Database, objects[index]); !verified) {
						return std::unexpected(verified.error());
					}
				}
				return objects;
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
				request.u64(capture.WorkspaceVersion).u64(aCommit.Content.size());
				for (const auto& object : aCommit.Content) {
					request.bytes(object.Digest.bytes()).u64(object.Size);
				}
				if (aCommit.Manifest.Digest != capture.Material) {
					return refuse(core::cycle_refusal::invalid_capture);
				}
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					if (!object_present(aDatabase, aCommit.Manifest)
						|| !std::ranges::all_of(aCommit.Content, [aDatabase](const cycle_object_ref& aObject) { return object_present(aDatabase, aObject); })) {
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
					if (outcome.Changed
						&& (!insert_revision(aDatabase, *outcome.Accepted.Latest, aCommit.Manifest.Size) || !pin_content(aDatabase, *outcome.Accepted.Latest, aCommit.Content)
							|| !store_cycle(aDatabase, outcome.Accepted, false))) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aCommit.Subject.Cycle, .Revision = outcome.Accepted.Revision, .Allocated = outcome.Captured ? outcome.Captured->Id.value() : 0U, .Changed = outcome.Changed };
				});
			}

			auto revision_holds(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision, const cycle_object_ref& aObject, std::stop_token aStop)
				-> cycle_store_result<bool> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<bool> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					const auto revision = read_revision(aDatabase, aSubject.Cycle, aRevision.value());
					if (!revision) {
						return fail(cycle_store_failure::not_found);
					}
					if (revision->Manifest == aObject) {
						return true;
					}
					// The pin is the revision's record; whether the bytes are still
					// present and intact is for the read to report.
					auto row = query{ aDatabase, "SELECT 1 FROM revision_objects WHERE cycle=?1 AND revision=?2 AND digest=?3" };
					row.id(1, aSubject.Cycle.value()).id(2, aRevision.value()).blob(3, aObject.Digest.bytes());
					const auto status = row.step();
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return status == SQLITE_ROW;
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

			auto begin_feedback(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const std::uint64_t aExpectedRevision,
								const core::begin_cycle_feedback& aCommand, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(9).subject(aSubject).u64(aExpectedRevision).target(aCommand.Primary).related(aCommand.Related).text(aCommand.Body);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					const auto begun = core::begin_feedback(*current, aExpectedRevision, aCommand);
					if (!begun) {
						return std::unexpected(*refusal(begun.error(), *current));
					}
					auto targets = std::vector<const core::cycle_feedback_target*>{ &aCommand.Primary };
					for (const auto& reference : aCommand.Related) {
						targets.push_back(&reference.Target);
					}
					const auto pinned = targets_pinned(aDatabase, aSubject.Cycle, targets);
					if (!pinned) {
						return std::unexpected(pinned.error());
					}
					if (!store_thread(aDatabase, begun->Thread, true) || !insert_feedback_revision(aDatabase, begun->Revision) || !store_cycle(aDatabase, begun->Cycle, false)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = begun->Cycle.Revision, .Allocated = begun->Thread.Id.Id.value(), .Changed = true };
				});
			}

			auto change_feedback(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem,
								 const std::uint64_t aExpectedVersion, const core::cycle_feedback_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(10).subject(aSubject).u64(aItem.Cycle.value()).u64(aItem.Id.value()).u64(aExpectedVersion).change(aChange);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					auto thread = read_thread(aDatabase, aSubject, aItem);
					if (!thread) {
						return std::unexpected(thread.error());
					}
					if (!*thread) {
						return fail(cycle_store_failure::not_found);
					}
					auto named = named_revision(aDatabase, **thread, aChange);
					if (!named) {
						return std::unexpected(named.error());
					}
					const auto outcome = core::apply_feedback(*current, **thread, aExpectedVersion, aChange, named->has_value() ? &**named : nullptr);
					if (!outcome) {
						return outcome.error() == core::cycle_refusal::conflict ? fail(cycle_store_failure::conflict) : refuse(outcome.error());
					}
					if (const auto* reference = std::get_if<core::add_feedback_reference>(&aChange)) {
						const auto pinned = targets_pinned(aDatabase, aSubject.Cycle, { &reference->Reference.Target });
						if (!pinned) {
							return std::unexpected(pinned.error());
						}
					}
					if (outcome->Changed && (!store_thread(aDatabase, outcome->Thread, false) || (outcome->Revision && !insert_feedback_revision(aDatabase, *outcome->Revision)))) {
						return fail(cycle_store_failure::unavailable);
					}
					const auto allocated = outcome->Revision ? outcome->Revision->Identity.Position.Revision.value() : 0U;
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = outcome->Thread.Lineage.Version, .Allocated = allocated, .Changed = outcome->Changed };
				});
			}

			auto load_feedback(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::stop_token aStop)
				-> cycle_store_result<std::optional<core::cycle_feedback_thread>> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<std::optional<core::cycle_feedback_thread>> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					return read_thread(aDatabase, aSubject, aItem);
				});
			}

			auto feedback(const core::local_cycle_subject& aSubject, const std::optional<core::cycle_feedback_item_id> aAfter, std::stop_token aStop)
				-> cycle_store_result<application::cycle_feedback_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_feedback_page> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					auto rows = query{ aDatabase, "SELECT item FROM feedback WHERE cycle=?1 AND item>?2 ORDER BY item LIMIT ?3" };
					rows.id(1, aSubject.Cycle.value()).id(2, aAfter ? aAfter->value() : 0U).id(3, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_feedback_page{ };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						if (page.Threads.size() == application::maximum_cycle_page_entries) {
							page.Next = page.Threads.back().Id.Id;
							break;
						}
						const auto id     = stored_id(rows.column_integer(0));
						auto       thread = id ? read_thread(aDatabase, aSubject, { .Cycle = aSubject.Cycle, .Id = core::cycle_feedback_item_id{ *id } })
											   : cycle_store_result<std::optional<core::cycle_feedback_thread>>{ std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::corrupt }) };
						if (!thread || !*thread) {
							return fail(thread ? cycle_store_failure::corrupt : thread.error().Failure);
						}
						page.Threads.push_back(std::move(**thread));
					}
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return page;
				});
			}

			auto feedback_revisions(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, const std::optional<core::private_review_turn_revision_id> aAfter,
									std::stop_token aStop) -> cycle_store_result<application::cycle_feedback_revision_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_feedback_revision_page> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					if (!core::belongs_to(aItem, aSubject)) {
						return refuse(core::cycle_refusal::invalid_identity);
					}
					auto rows = query{ aDatabase, "SELECT revision FROM feedback_revisions WHERE cycle=?1 AND item=?2 AND revision>?3 ORDER BY revision LIMIT ?4" };
					rows.id(1, aItem.Cycle.value()).id(2, aItem.Id.value()).id(3, aAfter ? aAfter->value() : 0U).id(4, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_feedback_revision_page{ };
					auto bytes  = std::size_t{ 0 };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						if (page.Revisions.size() == application::maximum_cycle_page_entries || (!page.Revisions.empty() && bytes >= application::maximum_cycle_feedback_page_bytes)) {
							page.Next = page.Revisions.back().Identity.Position.Revision;
							break;
						}
						const auto id       = stored_id(rows.column_integer(0));
						auto       revision = id ? read_feedback_revision(aDatabase, aItem, *id) : std::nullopt;
						if (!revision) {
							return fail(cycle_store_failure::corrupt);
						}
						bytes += revision->Body.size();
						page.Revisions.push_back(std::move(*revision));
					}
					if (status != SQLITE_ROW && status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return page;
				});
			}

			auto add_suggestion(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem,
								const std::uint64_t aExpectedVersion, const core::suggest_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(11).subject(aSubject).u64(aItem.Cycle.value()).u64(aItem.Id.value()).u64(aExpectedVersion).anchor(aChange.Selection).text(aChange.Replacement);
				request.u8(static_cast<std::uint8_t>(aChange.Intent));
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					auto thread = read_thread(aDatabase, aSubject, aItem);
					if (!thread) {
						return std::unexpected(thread.error());
					}
					if (!*thread) {
						return fail(cycle_store_failure::not_found);
					}
					const auto existing = count_suggestions(aDatabase, aItem);
					if (!existing) {
						return fail(cycle_store_failure::unavailable);
					}
					const auto added = core::add_suggestion(*current, **thread, aExpectedVersion, aChange, *existing);
					if (!added) {
						return added.error() == core::cycle_refusal::conflict ? fail(cycle_store_failure::conflict) : refuse(added.error());
					}
					const auto target = core::cycle_feedback_target{ aChange.Selection };
					if (const auto pinned = targets_pinned(aDatabase, aSubject.Cycle, { &target }); !pinned) {
						return std::unexpected(pinned.error());
					}
					if (!store_thread(aDatabase, added->Thread, false) || !insert_suggestion(aDatabase, added->Suggestion)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = added->Thread.Lineage.Version, .Allocated = added->Suggestion.Id.Id.value(), .Changed = true };
				});
			}

			auto change_suggestion(const application::cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_suggestion_ref& aSuggestion,
								   const std::uint64_t aExpectedRevision, const core::cycle_suggestion_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> override
			{
				auto request = request_writer{ };
				request.u8(12).subject(aSubject).u64(aSuggestion.Item.Cycle.value()).u64(aSuggestion.Item.Id.value()).u64(aSuggestion.Id.value()).u64(aExpectedRevision);
				request.suggestion_change(aChange);
				return write(aOperation, request, aStop, [&](sqlite3* aDatabase) -> cycle_store_result<cycle_write_receipt> {
					auto current = current_cycle(aDatabase, aSubject);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (current->Lifecycle == core::cycle_lifecycle::archived) {
						return refuse(core::cycle_refusal::archived);
					}
					auto thread = read_thread(aDatabase, aSubject, aSuggestion.Item);
					if (!thread) {
						return std::unexpected(thread.error());
					}
					if (!*thread) {
						return fail(cycle_store_failure::not_found);
					}
					if ((*thread)->Lineage.Lifecycle == core::private_thread_lifecycle::archived) {
						return refuse(core::cycle_refusal::closed);
					}
					auto latest = latest_suggestion(aDatabase, aSuggestion);
					if (!latest) {
						return std::unexpected(latest.error());
					}
					if (!*latest) {
						return fail(cycle_store_failure::not_found);
					}
					const auto changed = core::change_suggestion(**latest, aExpectedRevision, aChange);
					if (!changed) {
						return changed.error() == core::cycle_refusal::conflict ? fail(cycle_store_failure::conflict) : refuse(changed.error());
					}
					if (const auto* revalidate = std::get_if<core::revalidate_suggestion>(&aChange)) {
						const auto target = core::cycle_feedback_target{ revalidate->Candidate };
						if (!core::valid_target(*current, target)) {
							return refuse(core::cycle_refusal::invalid_target);
						}
						if (const auto pinned = targets_pinned(aDatabase, aSubject.Cycle, { &target }); !pinned) {
							return std::unexpected(pinned.error());
						}
					}
					if (const auto* applied = std::get_if<core::observe_suggestion_applied>(&aChange); applied != nullptr && applied->Result.value() >= current->Next.SourceRevision) {
						return refuse(core::cycle_refusal::invalid_target);
					}
					if (!insert_suggestion(aDatabase, *changed)) {
						return fail(cycle_store_failure::unavailable);
					}
					return cycle_write_receipt{ .Cycle = aSubject.Cycle, .Revision = changed->Revision, .Allocated = aSuggestion.Id.value(), .Changed = true };
				});
			}

			auto suggestions(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::stop_token aStop)
				-> cycle_store_result<std::vector<core::cycle_suggestion>> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<std::vector<core::cycle_suggestion>> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					if (!core::belongs_to(aItem, aSubject)) {
						return refuse(core::cycle_refusal::invalid_identity);
					}
					auto rows = query{ aDatabase, "SELECT suggestion,max(revision) FROM suggestions WHERE cycle=?1 AND item=?2 GROUP BY suggestion ORDER BY suggestion LIMIT ?3" };
					rows.id(1, aItem.Cycle.value()).id(2, aItem.Id.value()).id(3, core::maximum_cycle_suggestions);
					auto found  = std::vector<core::cycle_suggestion>{ };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						const auto id       = stored_id(rows.column_integer(0));
						const auto revision = stored_id(rows.column_integer(1));
						auto       value    = id && revision ? read_suggestion(aDatabase, { .Item = aItem, .Id = core::cycle_suggestion_id{ *id } }, *revision) : std::nullopt;
						if (!value) {
							return fail(cycle_store_failure::corrupt);
						}
						found.push_back(std::move(*value));
					}
					if (status != SQLITE_DONE) {
						return fail(cycle_store_failure::unavailable);
					}
					return found;
				});
			}

			auto suggestion_history(const core::local_cycle_subject& aSubject, const core::cycle_suggestion_ref& aSuggestion, const std::optional<std::uint64_t> aAfter, std::stop_token aStop)
				-> cycle_store_result<application::cycle_suggestion_history_page> override
			{
				return read_only(aStop, [&](sqlite3* aDatabase) -> cycle_store_result<application::cycle_suggestion_history_page> {
					if (auto current = current_cycle(aDatabase, aSubject); !current) {
						return std::unexpected(current.error());
					}
					if (!core::belongs_to(aSuggestion.Item, aSubject)) {
						return refuse(core::cycle_refusal::invalid_identity);
					}
					auto rows = query{ aDatabase, "SELECT revision FROM suggestions WHERE cycle=?1 AND item=?2 AND suggestion=?3 AND revision>?4 ORDER BY revision LIMIT ?5" };
					rows.id(1, aSuggestion.Item.Cycle.value()).id(2, aSuggestion.Item.Id.value()).id(3, aSuggestion.Id.value()).id(4, aAfter.value_or(0U));
					rows.id(5, application::maximum_cycle_page_entries + 1U);
					auto page   = application::cycle_suggestion_history_page{ };
					auto status = rows.step();
					for (; status == SQLITE_ROW; status = rows.step()) {
						if (page.Revisions.size() == application::maximum_cycle_page_entries) {
							page.Next = page.Revisions.back().Revision;
							break;
						}
						const auto revision = stored_id(rows.column_integer(0));
						auto       value    = revision ? read_suggestion(aDatabase, aSuggestion, *revision) : std::nullopt;
						if (!value) {
							return fail(cycle_store_failure::corrupt);
						}
						page.Revisions.push_back(std::move(*value));
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
					for (const auto sql : { "DELETE FROM revisions WHERE cycle=?1", "DELETE FROM revision_objects WHERE cycle=?1", "DELETE FROM drafts WHERE cycle=?1", "DELETE FROM rounds WHERE cycle=?1", "DELETE FROM bindings WHERE cycle=?1",
											"DELETE FROM feedback WHERE cycle=?1", "DELETE FROM feedback_revisions WHERE cycle=?1", "DELETE FROM suggestions WHERE cycle=?1",
											"DELETE FROM cycles WHERE cycle=?1" }) {
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

			/// The tables a schema version defines.
			static auto tables_of(const std::int64_t aVersion) -> std::vector<table>
			{
				const auto named  = [](const auto& aNames, const std::string_view aName) { return std::ranges::find(aNames, aName) != aNames.end(); };
				auto       tables = std::vector<table>{ };
				for (const auto& definition : schema) {
					if ((aVersion < 2 && named(added_in_v2, definition.Name)) || (aVersion < 3 && named(added_in_v3, definition.Name))) {
						continue;
					}
					tables.push_back(aVersion == 1 && definition.Name == "revisions" ? table{ definition.Name, revisions_v1 } : definition);
				}
				return tables;
			}

			static auto schema_valid(sqlite3* aDatabase, const std::int64_t aVersion = schema_version) -> bool
			{
				const auto count = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 32)");
				if (detail::scalar_integer(aDatabase, "PRAGMA application_id") != application_id || detail::scalar_integer(aDatabase, "PRAGMA user_version") != aVersion) {
					return false;
				}
				// Automatic primary-key indexes are named sqlite_autoindex_* and not counted.
				const auto tables = tables_of(aVersion);
				if (!count || *count != static_cast<std::int64_t>(tables.size())) {
					return false;
				}
				return std::ranges::all_of(tables, [aDatabase](const table& aTable) {
					auto definition = query{ aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name=?1" };
					definition.text(1, aTable.Name);
					return definition.step() == SQLITE_ROW && detail::canonical_sql(definition.column_text(0)) == detail::canonical_sql(aTable.Definition);
				});
			}

			/// Upgrades an earlier published schema in one transaction. Revisions
			/// captured under the first had no binding, so their version is zero;
			/// neither earlier schema held feedback.
			static auto migrate(sqlite3* aDatabase, const std::int64_t aFrom, std::stop_token aStop) -> cycle_store_result<void>
			{
				if (!detail::execute(aDatabase, "BEGIN IMMEDIATE", aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				auto       guard      = transaction{ aDatabase };
				const auto definition = [](const std::string_view aName) {
					return std::string{ std::ranges::find(schema, aName, &table::Name)->Definition };
				};
				auto steps = std::vector<std::string>{ };
				if (aFrom == 1) {
					steps.insert(steps.end(), {
												  "ALTER TABLE revisions RENAME TO revisions_v1",
												  definition("revisions"),
												  "INSERT INTO revisions SELECT cycle,revision,parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size,0 FROM revisions_v1",
												  "DROP TABLE revisions_v1",
											  });
					std::ranges::transform(added_in_v2, std::back_inserter(steps), definition);
				}
				std::ranges::transform(added_in_v3, std::back_inserter(steps), definition);
				steps.emplace_back("PRAGMA user_version=3");
				for (const auto& step : steps) {
					if (!detail::execute(aDatabase, step, aStop)) {
						return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
					}
				}
				if (!schema_valid(aDatabase) || !guard.commit(aStop)) {
					return fail(cancelled_or(aStop, cycle_store_failure::unavailable));
				}
				return { };
			}

			auto establish(sqlite3* aDatabase, std::stop_token aStop) -> cycle_store_result<void>
			{
				const auto empty = detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0 && detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
					&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema") == 0;
				const auto legacy = empty ? 0 : schema_valid(aDatabase, 1) ? 1
					: schema_valid(aDatabase, 2)                           ? 2
																		   : 0;
				if (!empty && legacy == 0 && !schema_valid(aDatabase)) {
					return fail(cycle_store_failure::schema_mismatch);
				}
				if (!detail::configure_connection(aDatabase)) {
					return fail(cycle_store_failure::unavailable);
				}
				if (legacy != 0) {
					return migrate(aDatabase, legacy, aStop);
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
						|| !detail::execute(aDatabase, "PRAGMA user_version=3", aStop) || !schema_valid(aDatabase) || !guard.commit(aStop)) {
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

			/// Every file or anchor target must name content its revision pinned.
			static auto targets_pinned(sqlite3* aDatabase, const core::review_cycle_id aCycle, const std::vector<const core::cycle_feedback_target*>& aTargets)
				-> cycle_store_result<void>
			{
				for (const auto* target : aTargets) {
					const auto* file   = std::get_if<core::cycle_file_target>(target);
					const auto* anchor = std::get_if<core::cycle_source_anchor>(target);
					if (file == nullptr && anchor == nullptr) {
						continue;
					}
					auto row = query{ aDatabase, "SELECT 1 FROM revision_objects WHERE cycle=?1 AND revision=?2 AND digest=?3" };
					row.id(1, aCycle.value()).id(2, file != nullptr ? file->Revision.value() : anchor->Revision.value()).blob(3, file != nullptr ? file->Blob.bytes() : anchor->Blob.bytes());
					const auto status = row.step();
					if (status == SQLITE_DONE) {
						return refuse(core::cycle_refusal::invalid_target);
					}
					if (status != SQLITE_ROW) {
						return fail(cycle_store_failure::unavailable);
					}
				}
				return { };
			}

			static auto read_thread(sqlite3* aDatabase, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem)
				-> cycle_store_result<std::optional<core::cycle_feedback_thread>>
			{
				if (!core::belongs_to(aItem, aSubject)) {
					return refuse(core::cycle_refusal::invalid_identity);
				}
				auto row = query{ aDatabase, "SELECT version,last_order,lifecycle,next_turn,next_suggestion,target,related FROM feedback WHERE cycle=?1 AND item=?2" };
				row.id(1, aItem.Cycle.value()).id(2, aItem.Id.value());
				const auto status = row.step();
				if (status == SQLITE_DONE) {
					return std::optional<core::cycle_feedback_thread>{ };
				}
				if (status != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				const auto version   = stored_id(row.column_integer(0));
				const auto order     = stored_id(row.column_integer(1));
				const auto lifecycle = row.column_integer(2);
				const auto nextTurn  = stored_id(row.column_integer(3));
				const auto nextSugg  = stored_id(row.column_integer(4));
				auto       target    = record_reader{ row.column_blob(5) };
				auto       related   = record_reader{ row.column_blob(6) };
				auto       thread    = core::cycle_feedback_thread{ .Id = aItem, .Primary = target.target(), .Related = related.related() };
				if (!version || !order || !nextTurn || !nextSugg || lifecycle < 0 || lifecycle > static_cast<std::int64_t>(core::private_thread_lifecycle::archived) || !target.done() || !related.done()) {
					return fail(cycle_store_failure::corrupt);
				}
				thread.Lineage        = { .Version = *version, .LastOrder = core::private_vertex_order{ *order }, .Lifecycle = static_cast<core::private_thread_lifecycle>(lifecycle) };
				thread.NextTurn       = *nextTurn;
				thread.NextSuggestion = *nextSugg;
				if (!core::valid(thread)) {
					return fail(cycle_store_failure::corrupt);
				}
				return std::optional{ std::move(thread) };
			}

			static auto store_thread(sqlite3* aDatabase, const core::cycle_feedback_thread& aThread, const bool aInsert) -> bool
			{
				const auto sql     = aInsert ? "INSERT INTO feedback(cycle,item,version,last_order,lifecycle,next_turn,next_suggestion,target,related) VALUES(?1,?2,?3,?4,?5,?6,?9,?7,?8)"
											 : "UPDATE feedback SET version=?3,last_order=?4,lifecycle=?5,next_turn=?6,next_suggestion=?9,target=?7,related=?8 WHERE cycle=?1 AND item=?2";
				auto       row     = query{ aDatabase, sql };
				const auto target  = request_writer{ }.target(aThread.Primary).Bytes;
				const auto related = request_writer{ }.related(aThread.Related).Bytes;
				row.id(1, aThread.Id.Cycle.value()).id(2, aThread.Id.Id.value()).id(3, aThread.Lineage.Version).id(4, aThread.Lineage.LastOrder.value());
				row.integer(5, static_cast<std::int64_t>(aThread.Lineage.Lifecycle)).id(6, aThread.NextTurn).blob(7, target).blob(8, related).id(9, aThread.NextSuggestion);
				return row.done() && sqlite3_changes(aDatabase) == 1;
			}

			static auto insert_feedback_revision(sqlite3* aDatabase, const core::cycle_feedback_revision& aRevision) -> bool
			{
				const auto& id      = aRevision.Identity;
				auto        lineage = request_writer{ };
				lineage.u8(aRevision.Predecessor ? 1 : 0);
				if (aRevision.Predecessor) {
					lineage.revision_ref(*aRevision.Predecessor);
				}
				lineage.u8(aRevision.ReplyTo ? 1 : 0);
				if (aRevision.ReplyTo) {
					lineage.revision_ref(*aRevision.ReplyTo);
				}
				lineage.u8(aRevision.EditDerivation ? static_cast<std::uint8_t>(static_cast<std::uint8_t>(*aRevision.EditDerivation) + 1U) : std::uint8_t{ 0 });
				auto row = query{ aDatabase, "INSERT INTO feedback_revisions(cycle,item,revision,turn,digest,author,completion,lineage,body) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)" };
				row.id(1, id.Item.Cycle.value()).id(2, id.Item.Id.value()).id(3, id.Position.Revision.value()).id(4, id.Position.Turn.value()).blob(5, id.Digest.bytes());
				row.blob(6, request_writer{ }.author(aRevision.Author).Bytes).integer(7, static_cast<std::int64_t>(aRevision.Completion)).blob(8, lineage.Bytes).blob(9, span_of(aRevision.Body));
				return row.done();
			}

			static auto read_feedback_revision(sqlite3* aDatabase, const core::cycle_feedback_item_ref& aItem, const std::uint64_t aRevision) -> std::optional<core::cycle_feedback_revision>
			{
				auto row = query{ aDatabase, "SELECT turn,digest,author,completion,lineage,body FROM feedback_revisions WHERE cycle=?1 AND item=?2 AND revision=?3" };
				row.id(1, aItem.Cycle.value()).id(2, aItem.Id.value()).id(3, aRevision);
				if (row.step() != SQLITE_ROW) {
					return std::nullopt;
				}
				const auto turn       = stored_id(row.column_integer(0));
				const auto digest     = digest_from<core::private_body_digest>(row.column_blob(1));
				auto       author     = record_reader{ row.column_blob(2) };
				const auto completion = row.column_integer(3);
				auto       lineage    = record_reader{ row.column_blob(4) };
				const auto body       = row.column_blob(5);
				if (!turn || !digest || completion < 0 || completion > static_cast<std::int64_t>(core::private_revision_completion::cancelled)) {
					return std::nullopt;
				}
				auto revision = core::cycle_feedback_revision{
					.Identity   = { .Item = aItem, .Position = { core::private_review_turn_id{ *turn }, core::private_review_turn_revision_id{ aRevision }, core::private_vertex_order{ aRevision } }, .Digest = *digest },
					.Author     = author.author(),
					.Body       = std::string{ reinterpret_cast<const char*>(body.data()), body.size() },
					.Completion = static_cast<core::private_revision_completion>(completion),
				};
				if (lineage.u8() != 0) {
					revision.Predecessor = lineage.revision_ref(aItem);
				}
				if (lineage.u8() != 0) {
					revision.ReplyTo = lineage.revision_ref(aItem);
				}
				const auto derivation = lineage.u8();
				if (derivation > static_cast<std::uint8_t>(core::private_derivation_kind::adopted_from_incomplete_run) + 1U) {
					return std::nullopt;
				}
				if (derivation != 0) {
					revision.EditDerivation = static_cast<core::private_derivation_kind>(derivation - 1U);
				}
				if (!author.done() || !lineage.done() || !core::valid(revision)) {
					return std::nullopt;
				}
				return revision;
			}

			/// The stored revision a change names: a reply's target, or the
			/// latest revision of the edited revision's turn.
			static auto named_revision(sqlite3* aDatabase, const core::cycle_feedback_thread& aThread, const core::cycle_feedback_change& aChange)
				-> cycle_store_result<std::optional<core::cycle_feedback_revision>>
			{
				auto named = std::optional<std::uint64_t>{ };
				if (const auto* append = std::get_if<core::append_feedback_turn>(&aChange); append != nullptr && append->ReplyTo) {
					named = append->ReplyTo->value();
				}
				const auto* edit = std::get_if<core::edit_feedback_turn>(&aChange);
				if (edit != nullptr) {
					named = edit->Latest.value();
				}
				if (!named || !core::valid_private_id(core::private_review_turn_revision_id{ *named })) {
					return std::optional<core::cycle_feedback_revision>{ };
				}
				auto revision = read_feedback_revision(aDatabase, aThread.Id, *named);
				if (!revision || edit == nullptr) {
					return revision;
				}
				auto latest = query{ aDatabase, "SELECT max(revision) FROM feedback_revisions WHERE cycle=?1 AND item=?2 AND turn=?3" };
				latest.id(1, aThread.Id.Cycle.value()).id(2, aThread.Id.Id.value()).id(3, revision->Identity.Position.Turn.value());
				const auto id = latest.step() == SQLITE_ROW ? stored_id(latest.column_integer(0)) : std::nullopt;
				if (!id) {
					return fail(cycle_store_failure::corrupt);
				}
				return *id == *named ? revision : read_feedback_revision(aDatabase, aThread.Id, *id);
			}

			static auto count_suggestions(sqlite3* aDatabase, const core::cycle_feedback_item_ref& aItem) -> std::optional<std::size_t>
			{
				auto row = query{ aDatabase, "SELECT count(DISTINCT suggestion) FROM suggestions WHERE cycle=?1 AND item=?2" };
				row.id(1, aItem.Cycle.value()).id(2, aItem.Id.value());
				if (row.step() != SQLITE_ROW || row.column_integer(0) < 0) {
					return std::nullopt;
				}
				return static_cast<std::size_t>(row.column_integer(0));
			}

			static auto insert_suggestion(sqlite3* aDatabase, const core::cycle_suggestion& aSuggestion) -> bool
			{
				auto row = query{ aDatabase, "INSERT INTO suggestions(cycle,item,suggestion,revision,record) VALUES(?1,?2,?3,?4,?5)" };
				row.id(1, aSuggestion.Id.Item.Cycle.value()).id(2, aSuggestion.Id.Item.Id.value()).id(3, aSuggestion.Id.Id.value()).id(4, aSuggestion.Revision);
				return row.blob(5, request_writer{ }.suggestion(aSuggestion).Bytes).done();
			}

			static auto read_suggestion(sqlite3* aDatabase, const core::cycle_suggestion_ref& aSuggestion, const std::uint64_t aRevision) -> std::optional<core::cycle_suggestion>
			{
				auto row = query{ aDatabase, "SELECT record FROM suggestions WHERE cycle=?1 AND item=?2 AND suggestion=?3 AND revision=?4" };
				row.id(1, aSuggestion.Item.Cycle.value()).id(2, aSuggestion.Item.Id.value()).id(3, aSuggestion.Id.value()).id(4, aRevision);
				if (row.step() != SQLITE_ROW) {
					return std::nullopt;
				}
				auto reader = record_reader{ row.column_blob(0) };
				auto value  = reader.suggestion(aSuggestion, aRevision);
				if (!reader.done() || !core::valid(value)) {
					return std::nullopt;
				}
				return value;
			}

			static auto latest_suggestion(sqlite3* aDatabase, const core::cycle_suggestion_ref& aSuggestion) -> cycle_store_result<std::optional<core::cycle_suggestion>>
			{
				auto row = query{ aDatabase, "SELECT max(revision) FROM suggestions WHERE cycle=?1 AND item=?2 AND suggestion=?3" };
				row.id(1, aSuggestion.Item.Cycle.value()).id(2, aSuggestion.Item.Id.value()).id(3, aSuggestion.Id.value());
				if (row.step() != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				if (row.column_null(0)) {
					return std::optional<core::cycle_suggestion>{ };
				}
				const auto revision = stored_id(row.column_integer(0));
				auto       value    = revision ? read_suggestion(aDatabase, aSuggestion, *revision) : std::nullopt;
				if (!value) {
					return fail(cycle_store_failure::corrupt);
				}
				return value;
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
				auto binding = read_binding(aDatabase, aCycle);
				if (!binding) {
					return std::unexpected(binding.error());
				}
				state.Workspace = std::move(*binding);
				return std::optional{ std::move(state) };
			}

			static auto read_binding(sqlite3* aDatabase, const core::review_cycle_id aCycle) -> cycle_store_result<std::optional<core::cycle_workspace_binding>>
			{
				auto row = query{ aDatabase, "SELECT project,version,kind,location,root,repository,worktree,admission FROM bindings WHERE cycle=?1" };
				row.id(1, aCycle.value());
				const auto status = row.step();
				if (status == SQLITE_DONE) {
					return std::optional<core::cycle_workspace_binding>{ };
				}
				if (status != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				const auto project      = digest_from<core::workspace_project_id>(row.column_blob(0));
				const auto version      = stored_id(row.column_integer(1));
				const auto kind         = row.column_integer(2);
				const auto location     = row.column_blob(3);
				const auto root         = decode_identity(row.column_blob(4));
				using nullable_identity = std::optional<core::workspace_object_identity>;
				const auto nullable     = [&](const int aColumn) -> std::optional<nullable_identity> {
					if (row.column_null(aColumn)) {
						return nullable_identity{ };
					}
					const auto decoded = decode_identity(row.column_blob(aColumn));
					return decoded ? std::optional<nullable_identity>{ decoded } : std::nullopt;
				};
				const auto repository = nullable(5);
				const auto worktree   = nullable(6);
				const auto admission  = row.column_null(7) ? std::optional<std::array<std::uint64_t, 5>>{ } : decode_words<5>(row.column_blob(7));
				if (!project || !version || kind < 0 || kind > static_cast<std::int64_t>(core::workspace_root_kind::git_linked_worktree) || !root
					|| !repository || !worktree || (!row.column_null(7) && !admission)) {
					return fail(cycle_store_failure::corrupt);
				}
				auto binding = core::cycle_workspace_binding{
					.Project = *project,
					.Version = *version,
					.Root    = {
						.Kind       = static_cast<core::workspace_root_kind>(kind),
						.Location   = std::string{ location.begin(), location.end() },
						.Root       = *root,
						.Repository = *repository,
						.Worktree   = *worktree,
					},
				};
				if (admission) {
					binding.Admission = core::workspace_material_check{ .Compared = (*admission)[0], .Matching = (*admission)[1], .Differing = (*admission)[2], .Missing = (*admission)[3], .Added = (*admission)[4] };
				}
				if (!core::valid(binding)) {
					return fail(cycle_store_failure::corrupt);
				}
				return std::optional{ std::move(binding) };
			}

			static auto store_binding(sqlite3* aDatabase, const core::review_cycle_state& aCycle) -> bool
			{
				if (!aCycle.Workspace) {
					auto erase = query{ aDatabase, "DELETE FROM bindings WHERE cycle=?1" };
					return erase.id(1, aCycle.Subject.Cycle.value()).done();
				}
				const auto& binding = *aCycle.Workspace;
				const auto& root    = binding.Root;
				auto        row     = query{ aDatabase, "INSERT OR REPLACE INTO bindings(cycle,project,version,kind,location,root,repository,worktree,admission) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)" };
				row.id(1, aCycle.Subject.Cycle.value()).blob(2, binding.Project.bytes()).id(3, binding.Version).integer(4, static_cast<std::int64_t>(root.Kind));
				row.blob(5, { reinterpret_cast<const std::uint8_t*>(root.Location.data()), root.Location.size() }).blob(6, encode_identity(root.Root));
				const auto bindNullable = [&row](const int aColumn, const std::optional<std::vector<std::uint8_t>>& aBytes) {
					if (aBytes) {
						row.blob(aColumn, *aBytes);
					} else {
						row.null(aColumn);
					}
				};
				bindNullable(7, root.Repository ? std::optional{ encode_identity(*root.Repository) } : std::nullopt);
				bindNullable(8, root.Worktree ? std::optional{ encode_identity(*root.Worktree) } : std::nullopt);
				bindNullable(9, binding.Admission ? std::optional{ encode_check(*binding.Admission) } : std::nullopt);
				return row.done();
			}

			static auto read_revision(sqlite3* aDatabase, const core::review_cycle_id aCycle, const std::uint64_t aRevision) -> std::optional<application::stored_cycle_revision>
			{
				auto row = query{ aDatabase, "SELECT parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size,workspace_version FROM revisions WHERE cycle=?1 AND revision=?2" };
				row.id(1, aCycle.value()).id(2, aRevision);
				if (row.step() != SQLITE_ROW) {
					return std::nullopt;
				}
				const auto parent   = row.column_null(0) ? std::optional<std::uint64_t>{ } : stored_id(row.column_integer(0));
				const auto scope    = stored_id(row.column_integer(1));
				const auto material = digest_from<core::cycle_material_digest>(row.column_blob(2));
				const auto coverage = row.column_integer(6);
				const auto quality  = row.column_integer(7);
				const auto counts   = std::array{ row.column_integer(3), row.column_integer(4), row.column_integer(5), row.column_integer(8), row.column_integer(9) };
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
							.WorkspaceVersion  = static_cast<std::uint64_t>(counts[4]),
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
				return row.done() && sqlite3_changes(aDatabase) == 1 && store_binding(aDatabase, aCycle);
			}

			static auto insert_revision(sqlite3* aDatabase, const core::cycle_source_revision& aRevision, const std::uint64_t aManifestSize) -> bool
			{
				const auto& capture = aRevision.Capture;
				auto        row     = query{ aDatabase, "INSERT INTO revisions(cycle,revision,parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size,workspace_version) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)" };
				row.id(1, aRevision.Id.Cycle.value()).id(2, aRevision.Id.Id.value()).optional_id(3, aRevision.Parent ? std::optional{ aRevision.Parent->value() } : std::nullopt);
				row.id(4, capture.ScopeVersion).blob(5, capture.Material.bytes()).id(6, capture.Entries).id(7, capture.ExplicitDeletions).id(8, capture.Unavailable);
				row.integer(9, static_cast<std::int64_t>(capture.Coverage)).integer(10, static_cast<std::int64_t>(capture.Quality)).id(11, aManifestSize).id(12, capture.WorkspaceVersion);
				return row.done();
			}

			/// Pins the content a revision's manifest references.
			static auto pin_content(sqlite3* aDatabase, const core::cycle_source_revision& aRevision, const std::vector<cycle_object_ref>& aContent) -> bool
			{
				return std::ranges::all_of(aContent, [&](const cycle_object_ref& aObject) {
					auto row = query{ aDatabase, "INSERT OR IGNORE INTO revision_objects(cycle,revision,digest) VALUES(?1,?2,?3)" };
					return row.id(1, aRevision.Id.Cycle.value()).id(2, aRevision.Id.Id.value()).blob(3, aObject.Digest.bytes()).done();
				});
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

			enum class object_state : std::uint8_t {
				absent,
				intact,
				/// Present under this digest but unreadable or not these bytes.
				damaged,
			};

			/// Whether an object's stored bytes still hash to its digest.
			static auto stored_state(sqlite3* aDatabase, const cycle_object_ref& aObject) -> cycle_store_result<object_state>
			{
				auto row = query{ aDatabase, "SELECT 1 FROM objects WHERE digest=?1" };
				row.blob(1, aObject.Digest.bytes());
				const auto present = row.step();
				if (present == SQLITE_DONE) {
					return object_state::absent;
				}
				if (present != SQLITE_ROW) {
					return fail(cycle_store_failure::unavailable);
				}
				const auto verified = read_bytes(aDatabase, aObject);
				if (verified) {
					return object_state::intact;
				}
				if (verified.error().Failure == cycle_store_failure::corrupt || verified.error().Failure == cycle_store_failure::object_unavailable) {
					return object_state::damaged;
				}
				return std::unexpected(verified.error());
			}

			static auto replace_object(sqlite3* aDatabase, const cycle_object_ref& aObject, const std::span<const std::uint8_t> aBytes) -> bool
			{
				for (const auto sql : { "DELETE FROM object_chunks WHERE digest=?1", "DELETE FROM objects WHERE digest=?1" }) {
					auto erase = query{ aDatabase, sql };
					if (!erase.blob(1, aObject.Digest.bytes()).done()) {
						return false;
					}
				}
				return insert_object(aDatabase, aObject, aBytes);
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
				const auto feedbackCount = count("SELECT count(*) FROM feedback WHERE cycle=?1");
				const auto released      = count(
					"SELECT coalesce(sum(size),0) FROM objects WHERE digest IN (SELECT material FROM revisions WHERE cycle=?1 UNION SELECT digest FROM revision_objects WHERE cycle=?1 "
					"UNION SELECT digest FROM rounds WHERE cycle=?1) AND digest NOT IN (SELECT material FROM revisions WHERE cycle<>?1 UNION SELECT digest FROM revision_objects "
					"WHERE cycle<>?1 UNION SELECT digest FROM rounds WHERE cycle<>?1)");
				if (!revisionCount || !draftCount || !roundCount || !feedbackCount || !released) {
					return fail(cycle_store_failure::unavailable);
				}
				auto result = application::cycle_removal_preview{
					.Subject       = aSubject,
					.Revision      = current->Revision,
					.Revisions     = *revisionCount,
					.Drafts        = *draftCount,
					.Rounds        = *roundCount,
					.Feedback      = *feedbackCount,
					.ReleasedBytes = *released,
				};
				auto token = request_writer{ };
				token.u8(8).subject(aSubject).u64(result.Revision).u64(result.Revisions).u64(result.Drafts).u64(result.Rounds).u64(result.Feedback).u64(result.ReleasedBytes);
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
