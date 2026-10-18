#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "private_review_codec.hpp"
#include "private_review_database.hpp"
#include "review_ledger_database.hpp"
#include <algorithm>
#include <set>
#include <sqlite3.h>
#include <utility>
namespace bpr::adapters::sqlite::detail {
	namespace {
		using namespace core;
		using namespace application;
		using error = private_store_failure;
		struct store_error
		{
			error Reason;
		};
		[[noreturn]] void refuse(error aReason) { throw store_error{ aReason }; }
		void              require(bool aCondition, error aReason = error::invalid_record)
		{
			if (!aCondition) {
				refuse(aReason);
			}
		}
		struct query
		{
			statement Statement;
			sqlite3*  Database;
			query(sqlite3* aDatabase, std::string_view aSql)
				: Database(aDatabase)
			{
				auto prepared = prepare(aDatabase, aSql);
				if (!prepared) {
					refuse(error::unavailable);
				}
				Statement = std::move(*prepared);
			}
			void bind(int aIndex, std::int64_t aValue) { require(sqlite3_bind_int64(Statement.Value, aIndex, aValue) == SQLITE_OK, error::unavailable); }
			void bind(int aIndex, std::uint64_t aValue)
			{
				require(aValue <= maximum_private_identity);
				bind(aIndex, static_cast<std::int64_t>(aValue));
			}
			void bind(int aIndex, int aValue) { bind(aIndex, static_cast<std::int64_t>(aValue)); }
			void bind(int aIndex, std::string_view aValue) { require(sqlite3_bind_text(Statement.Value, aIndex, aValue.data(), static_cast<int>(aValue.size()), SQLITE_TRANSIENT) == SQLITE_OK, error::unavailable); }
			void bind(int aIndex, const std::string& aValue) { bind(aIndex, std::string_view{ aValue }); }
			void bind(int aIndex, std::span<const std::uint8_t> aValue)
			{
				static constexpr std::uint8_t empty = 0;
				require(sqlite3_bind_blob(Statement.Value, aIndex, aValue.empty() ? &empty : aValue.data(), static_cast<int>(aValue.size()), SQLITE_TRANSIENT) == SQLITE_OK, error::unavailable);
			}
			template <std::size_t N>
			void bind(int aIndex, const std::array<std::uint8_t, N>& aValue) { bind(aIndex, std::span<const std::uint8_t>{ aValue }); }
			void bind(int aIndex, const std::vector<std::uint8_t>& aValue) { bind(aIndex, std::span<const std::uint8_t>{ aValue }); }
			void bind(int aIndex, std::nullopt_t) { require(sqlite3_bind_null(Statement.Value, aIndex) == SQLITE_OK, error::unavailable); }
			template <typename T>
			void bind(int aIndex, const std::optional<T>& aValue)
			{
				if (aValue) {
					bind(aIndex, *aValue);
				} else {
					bind(aIndex, std::nullopt);
				}
			}
			template <typename... T>
			void bind_all(const T&... aValues)
			{
				int index = 1;
				(bind(index++, aValues), ...);
			}
			bool row()
			{
				int result = sqlite3_step(Statement.Value);
				if (result == SQLITE_ROW) {
					return true;
				}
				if (result == SQLITE_DONE) {
					return false;
				}
				refuse(result == SQLITE_CONSTRAINT || (result & 255) == SQLITE_CONSTRAINT ? error::integrity : error::unavailable);
			}
			void          done() { require(!row(), error::integrity); }
			std::uint64_t integer(int aColumn)
			{
				require(sqlite3_column_type(Statement.Value, aColumn) == SQLITE_INTEGER, error::integrity);
				auto n = sqlite3_column_int64(Statement.Value, aColumn);
				require(n >= 0, error::integrity);
				return static_cast<std::uint64_t>(n);
			}
			bool                          null(int aColumn) { return sqlite3_column_type(Statement.Value, aColumn) == SQLITE_NULL; }
			std::span<const std::uint8_t> bytes(int aColumn)
			{
				auto size = sqlite3_column_bytes(Statement.Value, aColumn);
				auto data = static_cast<const std::uint8_t*>(sqlite3_column_blob(Statement.Value, aColumn));
				require(size >= 0 && (size == 0 || data), error::integrity);
				return { data, static_cast<std::size_t>(size) };
			}
			std::string text(int aColumn)
			{
				auto b = bytes(aColumn);
				if (b.empty()) {
					return { };
				}
				return { reinterpret_cast<const char*>(b.data()), b.size() };
			}
			template <typename T>
			T record(int aColumn)
			{
				auto decoded = decode_private<T>(bytes(aColumn));
				require(decoded.has_value(), error::integrity);
				return std::move(*decoded);
			}
			template <typename T>
			T digest(int aColumn)
			{
				auto b = bytes(aColumn);
				require(b.size() == 32, error::integrity);
				std::array<std::uint8_t, 32> data{ };
				std::ranges::copy(b, data.begin());
				return T{ data };
			}
		};
		std::span<const std::uint8_t> body_bytes(std::string_view aBody) { return { reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() }; }
		template <typename T>
		auto encoded(const T& aValue) -> std::vector<std::uint8_t>
		{
			auto result = encode_private(aValue);
			require(result.has_value());
			return std::move(*result);
		}
		struct transaction
		{
			sqlite3* Database;
			bool     Committed = false;
			explicit transaction(sqlite3* aDatabase, bool aWrite = true)
				: Database(aDatabase)
			{
				require(sqlite3_get_autocommit(Database) != 0, error::busy);
				const auto result = sqlite3_exec(Database, aWrite ? "BEGIN IMMEDIATE" : "BEGIN", nullptr, nullptr, nullptr);
				require(result == SQLITE_OK, (result & 255) == SQLITE_BUSY || (result & 255) == SQLITE_LOCKED ? error::busy : error::unavailable);
			}
			~transaction()
			{
				if (!Committed) {
					(void)sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr);
				}
			}
			void commit()
			{
				require(execute(Database, "COMMIT").has_value(), error::unavailable);
				Committed = true;
			}
		};
		template <typename F>
		auto guarded(F aFunction) -> private_store_result<decltype(aFunction())>
		{
			try {
				if constexpr (std::is_void_v<decltype(aFunction())>) {
					aFunction();
					return { };
				} else {
					return aFunction();
				}
			} catch (const store_error& e) {
				return std::unexpected(e.Reason);
			} catch (...) {
				return std::unexpected(error::unavailable);
			}
		}
		struct state
		{
			std::uint64_t                 Last = 0, Generation = 1, NoteRevision = 0;
			std::array<std::uint64_t, 10> Counts{ };
		};
		constexpr std::array<std::uint64_t, 10> Limits{ 4096, 65536, 65536, 65536, 4096, 65536, 4096, 65536, 67108864, 67108864 };
		class private_database
		{
		public:
			sqlite3*             Handle;
			std::int64_t         Scope;
			private_review_scope Identity;
			private_database(sqlite3* aHandle, std::int64_t aScope, private_review_scope aIdentity = { })
				: Handle(aHandle)
				, Scope(aScope)
				, Identity(aIdentity)
			{ }
			template <typename... T>
			auto sql(std::string_view aSql, const T&... aValues) -> query
			{
				query q{ Handle, aSql };
				q.bind_all(aValues...);
				return q;
			}
			template <typename... T>
			void write(std::string_view aSql, const T&... aValues)
			{
				auto q = sql(aSql, aValues...);
				q.done();
			}
			state read_state()
			{
				auto q = sql("SELECT last_identity,generation,note_revision,threads,revisions,runs,findings,compositions,candidates,drafts,publications,thread_bytes,payload_bytes FROM private_scope_state WHERE scope_id=?1", Scope);
				require(q.row(), error::integrity);
				state s{ q.integer(0), q.integer(1), q.integer(2) };
				for (std::size_t i = 0; i < s.Counts.size(); ++i) {
					s.Counts[i] = q.integer(static_cast<int>(i + 3));
				}
				q.done();
				return s;
			}
			void ensure_state()
			{
				auto q = sql("SELECT 1 FROM private_scope_state WHERE scope_id=?1", Scope);
				if (q.row()) {
					return;
				}
				auto retained = sql("SELECT 1 FROM private_thread WHERE scope_id=?1 LIMIT 1", Scope);
				require(!retained.row(), error::integrity);
				write("INSERT INTO private_scope_state(scope_id) VALUES(?1)", Scope);
			}
			void save_state(const state& aState)
			{
				require(aState.Generation < maximum_private_identity, error::quota);
				for (std::size_t i = 0; i < Limits.size(); ++i) {
					require(aState.Counts[i] <= Limits[i], error::quota);
				}
				write("UPDATE private_scope_state SET last_identity=?2,generation=?3,note_revision=?4,threads=?5,revisions=?6,runs=?7,findings=?8,compositions=?9,candidates=?10,drafts=?11,publications=?12,thread_bytes=?13,payload_bytes=?14 WHERE scope_id=?1", Scope, aState.Last, aState.Generation + 1, aState.NoteRevision, aState.Counts[0], aState.Counts[1], aState.Counts[2], aState.Counts[3], aState.Counts[4], aState.Counts[5], aState.Counts[6], aState.Counts[7], aState.Counts[8], aState.Counts[9]);
			}
			static std::uint64_t next(state& aState)
			{
				require(aState.Last < maximum_private_identity, error::quota);
				return ++aState.Last;
			}
			private_review_thread thread(std::uint64_t aId)
			{
				auto q = sql("SELECT version,last_order,lifecycle,binding,file_identity,comparison_id FROM private_thread WHERE scope_id=?1 AND id=?2", Scope, aId);
				require(q.row(), error::missing);
				private_review_thread t{ private_review_thread_id{ aId }, { }, q.integer(0), private_vertex_order{ q.integer(1) }, static_cast<private_thread_lifecycle>(q.integer(2)) };
				if (q.null(3)) {
					legacy_retained_subject legacy{ Identity, { } };
					if (!q.null(4)) {
						legacy.Attachment = private_file_subject{ q.digest<file_identity>(4), q.digest<file_comparison_digest>(5) };
					}
					t.Binding = legacy;
				} else {
					t.Binding = q.record<private_thread_binding>(3);
					require(scope_of(t.Binding) == Identity, error::integrity);
				}
				q.done();
				return t;
			}
			void write_thread(const private_review_thread& aThread, bool aInsert)
			{
				std::optional<std::vector<std::uint8_t>>    binding;
				std::optional<std::array<std::uint8_t, 32>> file, comparison;
				if (auto* legacy = std::get_if<legacy_retained_subject>(&aThread.Binding)) {
					if (legacy->Attachment) {
						file       = legacy->Attachment->File.bytes();
						comparison = legacy->Attachment->Comparison.bytes();
					}
				} else {
					binding = encoded(aThread.Binding);
				}
				write(aInsert ? "INSERT INTO private_thread(scope_id,id,version,last_order,lifecycle,binding,file_identity,comparison_id) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)" : "UPDATE private_thread SET version=?3,last_order=?4,lifecycle=?5,binding=?6,file_identity=?7,comparison_id=?8 WHERE scope_id=?1 AND id=?2", Scope, aThread.Id.value(), aThread.Version, aThread.LastOrder.value(), static_cast<int>(aThread.Lifecycle), binding, file, comparison);
			}
			std::optional<local_note_record> note(std::uint64_t aId)
			{
				auto q = sql("SELECT n.revision,t.file_identity,t.comparison_id,c.body,p.path,l.side,l.line,l.blob FROM private_legacy_note n JOIN private_thread t ON t.scope_id=n.scope_id AND t.id=n.thread_id JOIN private_turn u ON u.scope_id=t.scope_id AND u.thread_id=t.id JOIN private_content c ON c.scope_id=u.scope_id AND c.id=u.latest_revision LEFT JOIN private_note_path p ON p.scope_id=t.scope_id AND p.thread_id=t.id LEFT JOIN private_note_line l ON l.scope_id=t.scope_id AND l.thread_id=t.id WHERE n.scope_id=?1 AND n.note_id=?2 ORDER BY u.id LIMIT 1", Scope, aId);
				if (!q.row()) {
					return { };
				}
				local_note_record result{ .Id = local_note_id{ aId }, .Note = core::supplement_note{ .Text = q.text(3) }, .Revision = local_note_revision{ q.integer(0) } };
				if (!q.null(1)) {
					result.Attachment = local_note_attachment{ q.digest<file_identity>(1), q.digest<file_comparison_digest>(2) };
				}
				if (!q.null(4)) {
					result.FilePath = q.text(4);
				}
				// [[BPR-1591]]. A line only ever with its file.
				if (!q.null(5)) {
					require(result.Attachment.has_value(), error::integrity);
					auto line = core::note_line_anchor{ };
					line.Side = q.integer(5) == 0U ? core::diff_side::left : core::diff_side::right;
					line.Line = static_cast<std::uint32_t>(q.integer(6));
					if (!q.null(7)) {
						const auto kept = q.bytes(7);
						require(kept.size() == 20U || kept.size() == 32U, error::integrity);
						std::array<std::uint8_t, 32> data{ };
						std::ranges::copy(kept, data.begin());
						line.Blob = core::git_blob_oid{ kept.size() == 20U ? core::git_hash_algorithm::sha1 : core::git_hash_algorithm::sha256, data };
					}
					result.Attachment->Line = line;
				}
				require(application::valid(result), error::integrity);
				q.done();
				return result;
			}
			void insert_revision(const private_review_revision& aRevision, state& aState)
			{
				auto metadata = aRevision;
				metadata.Body.clear();
				metadata.Identity.Scope = { };
				std::optional<std::uint64_t> run;
				if (auto* actor = std::get_if<private_agent_actor>(&aRevision.Actor)) {
					run = actor->Run.value();
				}
				auto payload = encoded(metadata);
				require(payload.size() <= 131072);
				write("INSERT INTO private_content(scope_id,id,kind,owner_id,sub_id,body,digest,record,run_id) VALUES(?1,?2,0,?3,?4,?5,?6,?7,?8)", Scope, aRevision.Identity.Revision.value(), aRevision.Identity.Thread.value(), aRevision.Identity.Turn.value(), body_bytes(aRevision.Body), aRevision.Identity.Digest.bytes(), payload, run);
				++aState.Counts[1];
				aState.Counts[8] += aRevision.Body.size();
				aState.Counts[9] += payload.size();
				write("INSERT INTO private_turn(scope_id,id,thread_id,latest_revision) VALUES(?1,?2,?3,?4) ON CONFLICT(scope_id,id) DO UPDATE SET latest_revision=excluded.latest_revision", Scope, aRevision.Identity.Turn.value(), aRevision.Identity.Thread.value(), aRevision.Identity.Revision.value());
			}
			void edge(std::uint64_t aDestination, const private_source_reference& aSource, private_derivation_kind aKind)
			{
				auto reference = aSource;
				if (reference.BodyBytes == 0) {
					auto q = sql("SELECT length(body) FROM private_content WHERE scope_id=?1 AND id=?2", Scope, reference.Order.value());
					require(q.row(), error::missing);
					reference.BodyBytes = q.integer(0);
				}
				auto observed = source(reference);
				(void)observed;
				require(aSource.Order.value() < aDestination, error::conflict);
				write("INSERT INTO private_edge(scope_id,destination_id,source_id,kind) VALUES(?1,?2,?3,?4)", Scope, aDestination, aSource.Order.value(), static_cast<int>(aKind));
			}
			private_source_observation source(const private_source_reference& aSource)
			{
				require(aSource.Scope == Identity && valid_private_source(aSource));
				auto q = sql("SELECT kind,owner_id,sub_id,length(body),digest,record FROM private_content WHERE scope_id=?1 AND id=?2", Scope, aSource.Order.value());
				require(q.row(), error::missing);
				auto                       kind = q.integer(0);
				private_source_observation o{ };
				o.Source       = aSource;
				o.Completion   = private_revision_completion::complete;
				o.Availability = private_subject_availability::current;
				if (kind == 0) {
					auto meta           = q.record<private_review_revision>(5);
					meta.Identity.Scope = Identity;
					require(std::holds_alternative<private_turn_source>(aSource.Key));
					auto key = std::get<private_turn_source>(aSource.Key);
					require(key.Thread.value() == q.integer(1) && key.Turn.value() == q.integer(2) && key.Revision.value() == aSource.Order.value(), error::missing);
					auto tip = sql("SELECT latest_revision FROM private_turn WHERE scope_id=?1 AND id=?2 AND thread_id=?3", Scope, key.Turn.value(), key.Thread.value());
					require(tip.row(), error::integrity);
					o.Latest     = private_turn_source{ key.Thread, key.Turn, private_review_turn_revision_id{ tip.integer(0) } };
					o.Actor      = meta.Actor;
					o.Completion = meta.Completion;
					auto header  = thread(key.Thread.value());
					if (auto* exact = std::get_if<exact_private_subject>(&header.Binding)) {
						o.Binding = exact->Binding;
						if (auto* line = std::get_if<source_line_ref>(&exact->Subject)) {
							o.Target = private_composition_target{ exact->Binding, *line };
						}
					}
				} else if (kind == 1) {
					auto finding = q.record<retained_agent_finding>(5);
					require(valid_retained_agent_finding(finding), error::integrity);
					require(aSource.Key == finding.Source.Key, error::missing);
					o.Actor          = private_agent_actor{ std::get<private_finding_source>(aSource.Key).Run };
					o.Latest         = aSource.Key;
					o.FindingVerdict = finding.Verdict;
					{
						auto run = sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", Scope, std::get<private_finding_source>(aSource.Key).Run.value());
						require(run.row(), error::integrity);
						auto provenance = run.record<agent_run_provenance>(0);
						o.Binding       = provenance.Binding;
						if (finding.Verdict.anchored() && finding.Finding.Anchor.Line && provenance.Binding.Comparison == finding.Finding.Anchor.Line->Comparison) {
							o.Target = private_composition_target{ provenance.Binding, *finding.Finding.Anchor.Line };
						}
					}

					if (finding.Finding.SuggestedComment) {
						o.CopyBody = private_copy_body{ private_digest(*finding.Finding.SuggestedComment), finding.Finding.SuggestedComment->size() };
					}
				} else {
					auto candidate = q.record<review_composition_revision>(5);
					require(aSource == candidate.Identity, error::conflict);
					o.Actor   = candidate.Actor;
					auto comp = composition(q.integer(1));
					require(comp.State.Candidate.has_value(), error::integrity);
					o.Latest  = comp.State.Candidate->Key;
					o.Target  = comp.State.Target;
					o.Binding = comp.State.Binding;
				}
				require(aSource.Digest == q.digest<private_body_digest>(4), error::conflict);
				if (kind != 1) {
					require(aSource.BodyBytes == q.integer(3), error::conflict);
					o.CopyBody = private_copy_body{ aSource.Digest, aSource.BodyBytes };
				} else {
					auto finding = q.record<retained_agent_finding>(5);
					require(aSource == finding.Source, error::conflict);
				}
				q.done();
				return o;
			}
			private_composition_record composition(std::uint64_t aId)
			{
				auto q = sql("SELECT record FROM private_composition WHERE scope_id=?1 AND id=?2", Scope, aId);
				require(q.row(), error::missing);
				auto result = q.record<private_composition_record>(0);
				q.done();
				require(result.State.Binding.Scope == Identity, error::integrity);
				return result;
			}
			void erase_thread(std::uint64_t aId, std::uint64_t aVersion, state& aState)
			{
				auto t = thread(aId);
				require(t.Version == aVersion, error::conflict);
				auto refs = sql("SELECT EXISTS(SELECT 1 FROM private_edge e JOIN private_content c ON c.scope_id=e.scope_id AND c.id=e.source_id JOIN private_content d ON d.scope_id=e.scope_id AND d.id=e.destination_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2 AND NOT(d.kind=0 AND d.owner_id=?2)) OR EXISTS(SELECT 1 FROM private_composition_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2) OR EXISTS(SELECT 1 FROM private_run_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2)", Scope, aId);
				require(refs.row(), error::integrity);
				require(refs.integer(0) == 0, error::references_present);
				auto totals = sql("SELECT COUNT(*),COALESCE(SUM(length(body)),0),COALESCE(SUM(length(record)),0) FROM private_content WHERE scope_id=?1 AND kind=0 AND owner_id=?2", Scope, aId);
				require(totals.row(), error::integrity);
				aState.Counts[0]--;
				aState.Counts[1] -= totals.integer(0);
				aState.Counts[8] -= totals.integer(1);
				aState.Counts[9] -= totals.integer(2);
				totals.done();
				write("DELETE FROM private_edge WHERE scope_id=?1 AND destination_id IN(SELECT id FROM private_content WHERE scope_id=?1 AND kind=0 AND owner_id=?2)", Scope, aId);
				write("DELETE FROM private_legacy_note WHERE scope_id=?1 AND thread_id=?2", Scope, aId);
				write("DELETE FROM private_turn WHERE scope_id=?1 AND thread_id=?2", Scope, aId);
				write("DELETE FROM private_note_path WHERE scope_id=?1 AND thread_id=?2", Scope, aId);
				write("DELETE FROM private_note_line WHERE scope_id=?1 AND thread_id=?2", Scope, aId);
				write("DELETE FROM private_thread WHERE scope_id=?1 AND id=?2", Scope, aId);
				write("DELETE FROM private_content WHERE scope_id=?1 AND kind=0 AND owner_id=?2", Scope, aId);
			}
		};
	}
	namespace {
		auto add_note(private_database& aDb, state& aState, const local_note_record& aNote, bool aMigrating, std::optional<local_note_record> aCurrent = { }) -> local_note_record
		{
			auto accepted = aNote;
			if (!aMigrating) {
				require(aState.NoteRevision < maximum_private_identity, error::quota);
				accepted.Revision = local_note_revision{ ++aState.NoteRevision };
			}
			private_review_thread                     header{ };
			std::uint64_t                             turn = 0;
			std::optional<private_revision_reference> predecessor;
			if (aCurrent) {
				require(aCurrent->Attachment == aNote.Attachment, error::conflict);
				auto mapped = aDb.sql("SELECT n.thread_id,u.id,u.latest_revision FROM private_legacy_note n JOIN private_turn u ON u.scope_id=n.scope_id AND u.thread_id=n.thread_id WHERE n.scope_id=?1 AND n.note_id=?2 ORDER BY u.id LIMIT 1", aDb.Scope, aNote.Id.value());
				require(mapped.row(), error::integrity);
				header   = aDb.thread(mapped.integer(0));
				turn     = mapped.integer(1);
				auto old = mapped.integer(2);
				mapped.done();
				auto q = aDb.sql("SELECT digest FROM private_content WHERE scope_id=?1 AND id=?2", aDb.Scope, old);
				require(q.row(), error::integrity);
				predecessor = private_revision_reference{ aDb.Identity, header.Id, private_review_turn_id{ turn }, private_review_turn_revision_id{ old }, private_vertex_order{ old }, q.digest<private_body_digest>(0) };
				require(header.Version < maximum_private_identity, error::quota);
				++header.Version;
			} else {
				auto id = private_database::next(aState);
				turn    = private_database::next(aState);
				legacy_retained_subject binding{ aDb.Identity, { } };
				if (aNote.Attachment) {
					binding.Attachment = private_file_subject{ aNote.Attachment->File, aNote.Attachment->Comparison };
				}
				header = { private_review_thread_id{ id }, binding, 1, { } };
				++aState.Counts[0];
			}
			auto id          = private_database::next(aState);
			header.LastOrder = private_vertex_order{ id };
			private_review_revision revision{ { aDb.Identity, header.Id, private_review_turn_id{ turn }, private_review_turn_revision_id{ id }, private_vertex_order{ id }, private_digest(aNote.Note.Text) }, aMigrating ? private_review_actor{ private_legacy_unknown_actor{ } } : private_review_actor{ private_reviewer_actor{ } }, aNote.Note.Text, private_revision_completion::complete, predecessor, { }, predecessor ? std::optional{ private_derivation_kind::edited_from } : std::nullopt };
			aDb.write_thread(header, !aCurrent);
			aDb.insert_revision(revision, aState);
			if (predecessor) {
				aDb.write("INSERT INTO private_edge(scope_id,destination_id,source_id,kind) VALUES(?1,?2,?3,1)", aDb.Scope, id, predecessor->Revision.value());
			}
			aDb.write("INSERT INTO private_legacy_note(scope_id,note_id,thread_id,revision) VALUES(?1,?2,?3,?4) ON CONFLICT(scope_id,note_id) DO UPDATE SET revision=excluded.revision", aDb.Scope, aNote.Id.value(), header.Id.value(), accepted.Revision.value());
			// [[BPR-1210]]. The path is written with the note. An edit that
			// supplies none keeps the one already kept.
			if (aNote.FilePath && !aMigrating) {
				aDb.write("INSERT INTO private_note_path(scope_id,thread_id,path) VALUES(?1,?2,?3) ON CONFLICT(scope_id,thread_id) DO UPDATE SET path=excluded.path", aDb.Scope, header.Id.value(), *aNote.FilePath);
			} else if (aCurrent && aCurrent->FilePath) {
				accepted.FilePath = aCurrent->FilePath;
			}
			// [[BPR-1591]]. The line is written once, with a new note; an edit
			// keeps the attachment, and so its line, exactly.
			if (!aCurrent && !aMigrating && aNote.Attachment && aNote.Attachment->Line) {
				const auto& line = *aNote.Attachment->Line;
				auto        blob = std::optional<std::vector<std::uint8_t>>{ };
				if (line.Blob) {
					blob = std::vector<std::uint8_t>(line.Blob->bytes().begin(), line.Blob->bytes().begin() + static_cast<std::ptrdiff_t>(line.Blob->byte_count()));
				}
				aDb.write("INSERT INTO private_note_line(scope_id,thread_id,side,line,blob) VALUES(?1,?2,?3,?4,?5)", aDb.Scope, header.Id.value(), line.Side == core::diff_side::left ? 0 : 1,
						  static_cast<std::int64_t>(line.Line), blob);
			}
			return accepted;
		}
		auto legacy_failure(error aError, std::optional<local_note_record> aCurrent = { }) -> local_note_failure
		{
			auto reason = ledger_store_failure::unavailable;
			switch (aError) {
			case error::quota:              reason = ledger_store_failure::quota_exceeded; break;
			case error::invalid_record:     reason = ledger_store_failure::invalid_record; break;
			case error::integrity:          reason = ledger_store_failure::integrity_failure; break;
			case error::conflict:
			case error::references_present: reason = ledger_store_failure::conflict; break;
			default:                        break;
			}
			return { reason, std::move(aCurrent) };
		}
	}
	auto migrate_private_review(sqlite3* aHandle) -> std::expected<void, failure>
	{
		try {
			for (const auto& table : PrivateSchemaTables) {
				require(execute(aHandle, table.Sql).has_value(), error::integrity);
			}
			require(execute(aHandle, "INSERT INTO private_store_identity VALUES(1,randomblob(32))").has_value(), error::unavailable);
			query scopes{ aHandle, "SELECT s.scope_id,COALESCE(n.last_revision,0) FROM review_scope s LEFT JOIN local_note_sequence n ON n.scope_id=s.scope_id ORDER BY s.scope_id" };
			while (scopes.row()) {
				private_database db{ aHandle, static_cast<std::int64_t>(scopes.integer(0)) };
				db.ensure_state();
				auto current         = db.read_state();
				current.NoteRevision = scopes.integer(1);
				auto notes           = db.sql("SELECT note_id,file_identity,comparison_id,note_text,revision FROM local_note WHERE scope_id=?1 ORDER BY note_id", db.Scope);
				while (notes.row()) {
					// Filled member by member: GCC 15 at -O3 reads the designated
					// aggregate as leaving the note's text uninitialized.
					local_note_record note{ };
					note.Id        = local_note_id{ notes.integer(0) };
					note.Note.Text = notes.text(3);
					note.Revision  = local_note_revision{ notes.integer(4) };
					if (!notes.null(1)) {
						note.Attachment = local_note_attachment{ notes.digest<file_identity>(1), notes.digest<file_comparison_digest>(2) };
					}
					require(application::valid(note) && note.Revision.value() <= current.NoteRevision, error::integrity);
					(void)add_note(db, current, note, true);
				}
				db.save_state(current);
			}
			require(execute(aHandle, "DROP TABLE local_note").has_value(), error::integrity);
			require(execute(aHandle, "DROP TABLE local_note_sequence").has_value(), error::integrity);
			return { };
		} catch (const store_error& e) {
			return std::unexpected(e.Reason == error::quota ? failure::quota_exceeded : failure::integrity_failure);
		} catch (...) {
			return std::unexpected(failure::unavailable);
		}
	}
	auto migrate_private_note_paths(sqlite3* aHandle) -> std::expected<void, failure>
	{
		// Notes written before schema 8 keep no path: they stay honestly
		// unnamed rather than being given a guessed one.
		if (!execute(aHandle, PrivateNotePathTable.Sql).has_value()) {
			return std::unexpected(failure::integrity_failure);
		}
		return { };
	}
	auto migrate_private_note_lines(sqlite3* aHandle) -> std::expected<void, failure>
	{
		// Notes written before schema 10 keep no line: they stay on their
		// file or the pull request, as they were written.
		if (!execute(aHandle, PrivateNoteLineTable.Sql).has_value()) {
			return std::unexpected(failure::integrity_failure);
		}
		return { };
	}
	auto private_store_note(sqlite3* aHandle, std::int64_t aScope, const local_note_record& aNote, std::optional<local_note_revision> aExpected) -> std::expected<local_note_record, local_note_failure>
	{
		std::optional<local_note_record> current;
		try {
			require(application::valid(aNote) && aScope > 0 && (!aExpected || valid_private_id(*aExpected)));
			transaction      tx{ aHandle };
			private_database db{ aHandle, aScope };
			db.ensure_state();
			auto s  = db.read_state();
			current = db.note(aNote.Id.value());
			require(!current || current->Revision.value() <= s.NoteRevision, error::integrity);
			require(aExpected ? (current && current->Revision == *aExpected) : !current, error::conflict);
			auto accepted = add_note(db, s, aNote, false, current);
			db.save_state(s);
			tx.commit();
			return accepted;
		} catch (const store_error& e) {
			return std::unexpected(legacy_failure(e.Reason, current));
		} catch (...) {
			return std::unexpected(legacy_failure(error::unavailable));
		}
	}
	auto private_erase_note(sqlite3* aHandle, std::int64_t aScope, local_note_id aId, local_note_revision aExpected) -> std::expected<void, local_note_failure>
	{
		std::optional<local_note_record> current;
		try {
			require(valid_private_id(aId) && valid_private_id(aExpected));
			transaction      tx{ aHandle };
			private_database db{ aHandle, aScope };
			db.ensure_state();
			current = db.note(aId.value());
			require(!current || current->Revision.value() <= db.read_state().NoteRevision, error::integrity);
			require(current && current->Revision == aExpected, error::conflict);
			auto q = db.sql("SELECT n.thread_id,t.version FROM private_legacy_note n JOIN private_thread t ON t.scope_id=n.scope_id AND t.id=n.thread_id WHERE n.scope_id=?1 AND n.note_id=?2", aScope, aId.value());
			require(q.row(), error::integrity);
			auto id = q.integer(0), version = q.integer(1);
			q.done();
			auto s = db.read_state();
			db.erase_thread(id, version, s);
			db.save_state(s);
			tx.commit();
			return { };
		} catch (const store_error& e) {
			return std::unexpected(legacy_failure(e.Reason, current));
		} catch (...) {
			return std::unexpected(legacy_failure(error::unavailable));
		}
	}
	auto private_load_notes(sqlite3* aHandle, std::int64_t aScope) -> std::expected<std::vector<local_note_record>, failure>
	{
		try {
			private_database               db{ aHandle, aScope };
			auto                           q = db.sql("SELECT note_id FROM private_legacy_note WHERE scope_id=?1 ORDER BY note_id LIMIT 4097", aScope);
			std::vector<local_note_record> result;
			while (q.row()) {
				require(result.size() < maximum_private_notes, error::quota);
				auto note = db.note(q.integer(0));
				require(note.has_value(), error::integrity);
				result.push_back(std::move(*note));
			}
			return result;
		} catch (const store_error& e) {
			return std::unexpected(e.Reason == error::quota ? failure::quota_exceeded : failure::integrity_failure);
		} catch (...) {
			return std::unexpected(failure::unavailable);
		}
	}
	namespace {
		/// \brief Tracks every newly allocated object once; failed transactions consume nothing.
		class allocation
		{
		public:
			std::uint64_t           First;
			std::set<std::uint64_t> Used;
			explicit allocation(std::uint64_t aFirst)
				: First(aFirst)
			{ }
			void add(std::uint64_t aId)
			{
				require(First <= maximum_private_identity, error::quota);
				require(aId >= First && aId <= maximum_private_identity && Used.insert(aId).second, error::conflict);
			}
			void finish(state& aState)
			{
				auto expected = First;
				for (auto id : Used) {
					require(id == expected++, error::conflict);
				}
				if (!Used.empty()) {
					aState.Last = *Used.rbegin();
				}
			}
		};
		std::size_t record_size(private_database& aDb, std::string_view aTable, std::uint64_t aId)
		{
			auto q = aDb.sql("SELECT length(record) FROM " + std::string{ aTable } + " WHERE scope_id=?1 AND id=?2", aDb.Scope, aId);
			require(q.row(), error::missing);
			return static_cast<std::size_t>(q.integer(0));
		}
		void guards(private_database& aDb, std::span<const private_source_selection> aGuards)
		{
			require(aGuards.size() <= maximum_private_sources);
			std::set<private_source_key> seen;
			for (const auto& selected : aGuards) {
				require(seen.insert(selected.Source.Key).second);
				auto observed = aDb.source(selected.Source);
				require(observed.Latest == selected.ExpectedLatest, error::conflict);
				require(selected.Historical || selected.Source.Key == observed.Latest, error::conflict);
			}
		}
		void store_runs(private_database& aDb, state& aState, allocation& aIds, std::span<const agent_run_provenance> aRuns)
		{
			for (const auto& run : aRuns) {
				require(valid_run_provenance(run) && run.Binding.Scope == aDb.Identity && run.Run.value() == run.Order.value());
				auto payload = encoded(run);
				require(payload.size() <= 131072);
				auto q      = aDb.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", aDb.Scope, run.Run.value());
				bool update = q.row();
				if (update) {
					auto previous            = q.record<agent_run_provenance>(0);
					auto oldSize             = q.bytes(0).size();
					auto identity            = run;
					identity.Metadata        = previous.Metadata;
					identity.Outcome         = previous.Outcome;
					identity.Completion      = previous.Completion;
					identity.OutputDigest    = previous.OutputDigest;
					identity.OutputItemBytes = previous.OutputItemBytes;
					require(encoded(identity) == encoded(previous) && previous.Outcome == private_run_outcome::admitted && run.Outcome != private_run_outcome::admitted, error::conflict);
					require(run.Metadata.Requested == previous.Metadata.Requested && run.Metadata.Configured == previous.Metadata.Configured, error::conflict);
					require(run.Metadata.ProtocolReported.size() >= previous.Metadata.ProtocolReported.size() && std::equal(previous.Metadata.ProtocolReported.begin(), previous.Metadata.ProtocolReported.end(), run.Metadata.ProtocolReported.begin()), error::conflict);
					aState.Counts[9] -= oldSize;
				} else {
					guards(aDb, run.SelectedPrivateSources);
					require(run.Outcome == private_run_outcome::admitted);
					aIds.add(run.Run.value());
					++aState.Counts[2];
				}
				aState.Counts[9] += payload.size();
				aDb.write("INSERT INTO private_run(scope_id,id,record) VALUES(?1,?2,?3) ON CONFLICT(scope_id,id) DO UPDATE SET record=excluded.record", aDb.Scope, run.Run.value(), payload);
				if (!update) {
					std::uint64_t position = 0;
					for (const auto& source : run.SelectedPrivateSources) {
						aDb.write("INSERT INTO private_run_source VALUES(?1,?2,?3,?4)", aDb.Scope, run.Run.value(), position++, source.Source.Order.value());
					}
				}
			}
		}
		void store_threads(private_database& aDb, state& aState, allocation& aIds, std::span<const private_thread_write> aThreads)
		{
			for (const auto& write : aThreads) {
				const auto& t = write.Thread;
				require(valid_private_id(t.Id) && valid_private_id(t.LastOrder) && scope_of(t.Binding) == aDb.Identity && static_cast<unsigned>(t.Lifecycle) <= 2);
				if (auto* exact = std::get_if<exact_private_subject>(&t.Binding)) {
					require(valid_private_binding(exact->Binding) && !std::holds_alternative<std::monostate>(exact->Subject));
				}
				if (write.ExpectedVersion) {
					auto old = aDb.thread(t.Id.value());
					require(old.Version == write.ExpectedVersion && t.Version == old.Version + 1 && t.Binding == old.Binding && t.LastOrder >= old.LastOrder, error::conflict);
					auto mapped = aDb.sql("SELECT 1 FROM private_legacy_note WHERE scope_id=?1 AND thread_id=?2", aDb.Scope, t.Id.value());
					if (mapped.row()) {
						require(aState.NoteRevision < maximum_private_identity, error::quota);
						aDb.write("UPDATE private_legacy_note SET revision=?3 WHERE scope_id=?1 AND thread_id=?2", aDb.Scope, t.Id.value(), ++aState.NoteRevision);
					}
				} else {
					require(t.Version == 1);
					aIds.add(t.Id.value());
					++aState.Counts[0];
				}
				aDb.write_thread(t, write.ExpectedVersion == 0);
			}
		}
		void store_revisions(private_database& aDb, state& aState, allocation& aIds, std::span<const private_review_revision> aRevisions, std::span<const private_thread_write> aThreads)
		{
			for (const auto& r : aRevisions) {
				require(valid_private_revision(r) && r.Identity.Scope == aDb.Identity && r.Identity.Order.value() == r.Identity.Revision.value());
				require(!std::holds_alternative<private_legacy_unknown_actor>(r.Actor));
				require(std::ranges::any_of(aThreads, [&](const auto& aItem) { return aItem.Thread.Id == r.Identity.Thread; }), error::conflict);
				aIds.add(r.Identity.Revision.value());
				auto t = aDb.thread(r.Identity.Thread.value());
				require(t.LastOrder == r.Identity.Order, error::conflict);
				auto turn = aDb.sql("SELECT thread_id,latest_revision FROM private_turn WHERE scope_id=?1 AND id=?2", aDb.Scope, r.Identity.Turn.value());
				if (turn.row()) {
					require(turn.integer(0) == r.Identity.Thread.value() && r.Predecessor && r.Predecessor->Revision.value() == turn.integer(1), error::conflict);
				} else {
					require(!r.Predecessor);
					aIds.add(r.Identity.Turn.value());
				}
				if (auto* actor = std::get_if<private_agent_actor>(&r.Actor)) {
					auto q = aDb.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", aDb.Scope, actor->Run.value());
					require(q.row(), error::missing);
					auto run  = q.record<agent_run_provenance>(0);
					auto auth = authorize_private_run(run);
					require(auth.has_value() && auth->OutputDigest == r.Identity.Digest && auth->Completion == r.Completion, error::conflict);
					if (!run.OutputItemBytes.empty()) {
						std::uint64_t bytes = 0;
						for (const auto size : run.OutputItemBytes) {
							bytes += size;
						}
						require(bytes == r.Body.size(), error::conflict);
					}
					auto* exact = std::get_if<exact_private_subject>(&t.Binding);
					require(exact && exact->Binding == run.Binding, error::conflict);
				}
				if (r.Predecessor) {
					require(r.Predecessor->Thread == r.Identity.Thread && r.Predecessor->Turn == r.Identity.Turn && r.EditDerivation, error::conflict);
				}
				aDb.insert_revision(r, aState);
				if (t.Version == 1) {
					if (const auto* exact = std::get_if<exact_private_subject>(&t.Binding)) {
						if (const auto* linked = std::get_if<private_finding_reference>(&exact->Subject)) {
							auto source = aDb.sql("SELECT record FROM private_content WHERE scope_id=?1 AND kind=1 AND owner_id=?2 AND sub_id=?3", aDb.Scope, linked->Run.value(), static_cast<std::uint64_t>(linked->Ordinal));
							require(source.row(), error::missing);
							auto finding = source.record<retained_agent_finding>(0);
							require(finding.Source.Digest == linked->Digest, error::conflict);
							auto provenance = aDb.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", aDb.Scope, linked->Run.value());
							require(provenance.row() && provenance.record<agent_run_provenance>(0).Binding == exact->Binding, error::conflict);
							aDb.edge(r.Identity.Order.value(), finding.Source, private_derivation_kind::promoted_from_finding);
						}
					}
				}

				if (r.Predecessor) {
					aDb.edge(r.Identity.Order.value(), { r.Predecessor->Scope, private_turn_source{ r.Predecessor->Thread, r.Predecessor->Turn, r.Predecessor->Revision }, r.Predecessor->Order, r.Predecessor->Digest, 0 }, *r.EditDerivation);
				}
				if (r.ReplyTo) {
					aDb.edge(r.Identity.Order.value(), { r.ReplyTo->Scope, private_turn_source{ r.ReplyTo->Thread, r.ReplyTo->Turn, r.ReplyTo->Revision }, r.ReplyTo->Order, r.ReplyTo->Digest, 0 }, private_derivation_kind::replies_to);
				}
			}
		}
		void store_findings(private_database& aDb, state& aState, allocation& aIds, std::span<const retained_agent_finding> aFindings)
		{
			for (const auto& finding : aFindings) {
				require(valid_retained_agent_finding(finding) && finding.Source.Scope == aDb.Identity && std::holds_alternative<private_finding_source>(finding.Source.Key));
				const auto key = std::get<private_finding_source>(finding.Source.Key);
				auto       run = aDb.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", aDb.Scope, key.Run.value());
				require(run.row(), error::missing);
				auto provenance = run.record<agent_run_provenance>(0);
				require(provenance.Outcome == private_run_outcome::succeeded && provenance.Binding.Scope == finding.Source.Scope && finding.Finding.Provider == provenance.Profile && finding.Finding.AnalyzedBaseOid == provenance.Binding.Revision.BaseOid && finding.Finding.AnalyzedHeadOid == provenance.Binding.Revision.HeadOid && finding.Source.Order > provenance.Order);
				auto duplicate = aDb.sql("SELECT 1 FROM private_content WHERE scope_id=?1 AND kind=1 AND owner_id=?2 AND sub_id=?3", aDb.Scope, key.Run.value(), static_cast<std::uint64_t>(key.Ordinal));
				require(!duplicate.row(), error::conflict);
				auto payload = encoded(finding);
				require(payload.size() <= 131072);
				aIds.add(finding.Source.Order.value());
				++aState.Counts[3];
				aState.Counts[9] += payload.size();
				aDb.write("INSERT INTO private_content(scope_id,id,kind,owner_id,sub_id,body,digest,record,run_id) VALUES(?1,?2,1,?3,?4,?5,?6,?7,?3)", aDb.Scope, finding.Source.Order.value(), key.Run.value(), static_cast<std::uint64_t>(key.Ordinal), body_bytes(""), finding.Source.Digest.bytes(), payload);
			}
		}
		void store_compositions(private_database& aDb, state& aState, allocation& aIds, std::span<const private_composition_write> aCompositions)
		{
			for (const auto& item : aCompositions) {
				const auto& c = item.Record;
				require(valid_private_id(c.State.Id) && valid_private_binding(c.State.Binding) && c.State.Binding.Scope == aDb.Identity && c.Sources.size() == c.State.SourceCount);
				auto digest = private_selection_digest(c.Sources);
				require(digest && *digest == c.State.SourceSetDigest);
				guards(aDb, c.Sources);
				std::vector<private_source_observation> observed;
				observed.reserve(c.Sources.size());
				for (const auto& source : c.Sources) {
					observed.push_back(aDb.source(source.Source));
				}
				require(admit_composition_sources(aDb.Identity, c.Sources, observed).has_value());
				require(static_cast<unsigned>(c.State.Mode) <= 2 && static_cast<unsigned>(c.State.Lifecycle) <= 2);
				if (c.State.Target) {
					require(c.State.Target->Binding == c.State.Binding);
				}
				if (item.ExpectedVersion) {
					auto old = aDb.composition(c.State.Id.value());
					require(old.Version == item.ExpectedVersion && c.Version == old.Version + 1 && old.Sources == c.Sources && old.State.Binding == c.State.Binding && old.State.Mode == c.State.Mode, error::conflict);
					aState.Counts[9] -= record_size(aDb, "private_composition", c.State.Id.value());
				} else {
					require(c.Version == 1);
					aIds.add(c.State.Id.value());
					++aState.Counts[4];
				}
				auto payload = encoded(c);
				require(payload.size() <= 131072);
				aState.Counts[9] += payload.size();
				aDb.write("INSERT INTO private_composition(scope_id,id,version,record) VALUES(?1,?2,?3,?4) ON CONFLICT(scope_id,id) DO UPDATE SET version=excluded.version,record=excluded.record", aDb.Scope, c.State.Id.value(), c.Version, payload);
				if (!item.ExpectedVersion) {
					std::uint64_t position = 0;
					for (const auto& source : c.Sources) {
						aDb.write("INSERT INTO private_composition_source VALUES(?1,?2,?3,?4)", aDb.Scope, c.State.Id.value(), position++, source.Source.Order.value());
					}
				}
			}
		}
		void store_candidates(private_database& aDb, state& aState, allocation& aIds, std::span<const review_composition_revision> aCandidates)
		{
			for (const auto& c : aCandidates) {
				require(valid_private_source(c.Identity) && c.Identity.Scope == aDb.Identity && std::holds_alternative<private_composition_source>(c.Identity.Key) && c.Body.size() <= maximum_private_reviewer_body_bytes && !c.Body.empty() && private_digest(c.Body) == c.Identity.Digest && c.Body.size() == c.Identity.BodyBytes && c.Derivation.size() <= maximum_private_sources);
				auto key = std::get<private_composition_source>(c.Identity.Key);
				require(key.Revision.value() == c.Identity.Order.value());
				auto composition = aDb.composition(key.Composition.value());
				require(composition.State.Candidate == c.Identity && composition.State.SourceSetDigest == c.SourceSetDigest, error::conflict);
				std::optional<std::uint64_t> run;
				const bool                   exactCopy = composition.State.Mode == review_composition_mode::exact_copy && c.Derivation.size() == 1 && c.Derivation.front().Kind == private_derivation_kind::copied_from;
				if (exactCopy) {
					auto source = aDb.source(c.Derivation.front().Source);
					require(source.Actor == c.Actor && source.CopyBody && source.CopyBody->Digest == c.Identity.Digest && source.CopyBody->Bytes == c.Body.size(), error::conflict);
					if (auto* actor = std::get_if<private_agent_actor>(&c.Actor)) {
						run = actor->Run.value();
					}
				} else if (auto* actor = std::get_if<private_agent_actor>(&c.Actor)) {
					run    = actor->Run.value();
					auto q = aDb.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", aDb.Scope, *run);
					require(q.row(), error::missing);
					auto p = q.record<agent_run_provenance>(0);
					require(p.Binding == composition.State.Binding && p.Purpose == private_run_purpose::composition && p.OutputDigest == c.Identity.Digest && p.Outcome == private_run_outcome::succeeded, error::conflict);
				} else {
					require(std::holds_alternative<private_reviewer_actor>(c.Actor));
				}
				auto metadata = c;
				metadata.Body.clear();
				auto payload = encoded(metadata);
				require(payload.size() <= 131072);
				aIds.add(key.Revision.value());
				++aState.Counts[5];
				aState.Counts[9] += payload.size() + c.Body.size();
				aDb.write("INSERT INTO private_content(scope_id,id,kind,owner_id,sub_id,body,digest,record,run_id) VALUES(?1,?2,2,?3,0,?4,?5,?6,?7)", aDb.Scope, key.Revision.value(), key.Composition.value(), body_bytes(c.Body), c.Identity.Digest.bytes(), payload, run);
				for (const auto& edge : c.Derivation) {
					aDb.edge(key.Revision.value(), edge.Source, edge.Kind);
				}
			}
		}
		/// [[BPR-1345]]. A pending mark is a new comment's, on the head the
		/// draft was written on.
		[[nodiscard]] auto valid_review_mark(const private_draft_record& aRecord) -> bool
		{
			const auto& draft = aRecord.Draft.Draft;
			return !draft.InReview || (valid(*draft.InReview) && !draft.ReplyToThread && draft.InReview->Head == aRecord.Binding.Revision.HeadOid);
		}
		private_draft_record read_draft(private_database& aDb, std::uint64_t aId)
		{
			auto q = aDb.sql("SELECT d.record,d.body,c.body FROM private_draft d LEFT JOIN private_content c ON c.scope_id=d.scope_id AND c.id=d.content_id WHERE d.scope_id=?1 AND d.id=?2", aDb.Scope, aId);
			require(q.row(), error::missing);
			auto record             = q.record<private_draft_record>(0);
			record.Draft.Draft.Body = q.text(q.null(1) ? 2 : 1);
			q.done();
			require(record.Binding.Scope == aDb.Identity && record.Draft.Scope.value() == 0 && valid_comment_range(record.Draft.Draft) && valid_review_mark(record), error::integrity);
			if (record.Draft.Composition) {
				require(record.Draft.Composition->BodyDigest == private_digest(record.Draft.Draft.Body), error::integrity);
			}
			return record;
		}
		[[nodiscard]] auto active_draft(const core::revisioned_review_draft& aDraft) -> bool
		{
			return (aDraft.Draft.State == core::review_draft_state::drafting || aDraft.Draft.State == core::review_draft_state::publishing) && !core::unused_review_draft(aDraft);
		}
		void store_drafts(private_database& aDb, state& aState, allocation& aIds, std::span<const private_draft_write> aDrafts, bool& aAdmitsActive)
		{
			for (const auto& item : aDrafts) {
				const auto& d = item.Record;
				require(valid_private_binding(d.Binding) && d.Binding.Scope == aDb.Identity && valid_private_id(d.Draft.Draft.Id) && d.Draft.Revision > 0 && d.Draft.Scope.value() == 0 && d.Draft.Draft.Body.size() <= maximum_shared_draft_body_bytes && static_cast<unsigned>(d.Draft.Draft.State) <= 3);
				require(d.Binding.Comparison == d.Draft.Draft.Anchor.Comparison && supported_comment_target(d.Draft.Draft.Anchor.Side) && valid_comment_range(d.Draft.Draft) && valid_review_mark(d));
				if (d.Draft.Draft.ReplyToThread) {
					require(d.Draft.Draft.ReplyToThread->size() <= maximum_shared_draft_reply_bytes);
				}
				bool unchangedLink = false;
				bool wasActive     = false;
				if (item.ExpectedRevision) {
					auto old  = read_draft(aDb, d.Draft.Draft.Id.value());
					wasActive = active_draft(old.Draft);
					require(old.Draft.Revision == item.ExpectedRevision && d.Draft.Revision == old.Draft.Revision + 1 && d.Binding == old.Binding && d.Draft.Draft.Anchor == old.Draft.Draft.Anchor, error::conflict);
					require(!old.Draft.Composition || (d.Draft.Composition && old.Draft.Composition->Scope == d.Draft.Composition->Scope && old.Draft.Composition->Composition == d.Draft.Composition->Composition && old.Draft.Composition->Revision <= d.Draft.Composition->Revision), error::conflict);
					unchangedLink = old.Draft.Composition == d.Draft.Composition && old.Draft.Draft.Body == d.Draft.Draft.Body;
					aState.Counts[9] -= record_size(aDb, "private_draft", d.Draft.Draft.Id.value()) + (old.Draft.Composition ? 0 : old.Draft.Draft.Body.size());
				} else {
					require(d.Draft.Revision == 1);
					aIds.add(d.Draft.Draft.Id.value());
					++aState.Counts[6];
				}
				aAdmitsActive = aAdmitsActive || (!wasActive && active_draft(d.Draft));
				std::optional<std::uint64_t>             content;
				std::optional<std::vector<std::uint8_t>> body;
				if (d.Draft.Composition) {
					const auto& link = *d.Draft.Composition;
					require(link.Scope == aDb.Identity);
					auto                     c = aDb.composition(link.Composition.value());
					private_source_reference source{ link.Scope, private_composition_source{ link.Composition, link.Revision }, private_vertex_order{ link.Revision.value() }, link.BodyDigest, d.Draft.Draft.Body.size() };
					require(c.State.Target && c.State.Target->Binding == d.Binding && c.State.Target->Line == d.Draft.Draft.Anchor && private_digest(d.Draft.Draft.Body) == link.BodyDigest, error::conflict);
					if (!unchangedLink) {
						guards(aDb, c.Sources);
						require(c.State.Lifecycle == review_composition_lifecycle::accepted_as_draft && c.State.Candidate == source, error::conflict);
					}
					(void)aDb.source(source);
					content = link.Revision.value();
				} else {
					body = std::vector<std::uint8_t>{ d.Draft.Draft.Body.begin(), d.Draft.Draft.Body.end() };
				}
				auto metadata = d;
				metadata.Draft.Draft.Body.clear();
				auto payload = encoded(metadata);
				require(payload.size() <= 4096);
				aState.Counts[9] += payload.size() + (body ? body->size() : 0);
				aDb.write("INSERT INTO private_draft(scope_id,id,revision,record,body,content_id) VALUES(?1,?2,?3,?4,?5,?6) ON CONFLICT(scope_id,id) DO UPDATE SET revision=excluded.revision,record=excluded.record,body=excluded.body,content_id=excluded.content_id", aDb.Scope, d.Draft.Draft.Id.value(), d.Draft.Revision, payload, body, content);
			}
		}
		void store_publications(private_database& aDb, state& aState, allocation& aIds, std::span<const private_publication_record> aPublications)
		{
			for (const auto& p : aPublications) {
				require(valid_private_id(p.Id) && valid_private_id(p.Draft) && p.DraftRevision > 0 && p.Target.Binding.Scope == aDb.Identity && valid_private_binding(p.Target.Binding) && valid_private_id(p.Actor) && p.ApprovedProseDigest.valid() && p.WireBodyDigest.valid());
				require(!p.LogicalPublication.empty() && p.LogicalPublication.size() <= 1024 && !p.Representation.empty() && p.Representation.size() <= 64 && p.ServerComment.has_value() == p.ReconciledIdentity.has_value());
				require(p.ReceiptReserve <= 4096 && (!p.ServerComment || p.ReceiptReserve == 0));
				require(p.IdempotencyKey.size() <= 64 && p.ActorNode.size() <= 256 && p.ActorLogin.size() <= 256 && p.Path.size() <= 4096);
				if (p.ServerComment) {
					require(!p.ServerComment->empty() && p.ServerComment->size() <= 1024 && !p.ReconciledIdentity->empty() && p.ReconciledIdentity->size() <= 1024);
				}
				auto q = aDb.sql("SELECT record FROM private_publication WHERE scope_id=?1 AND id=?2", aDb.Scope, p.Id.value());
				if (q.row()) {
					auto previous = q.record<private_publication_record>(0);
					if (encoded(previous) == encoded(p)) {
						continue;
					}
					auto next               = p;
					next.ServerComment      = previous.ServerComment;
					next.ReconciledIdentity = previous.ReconciledIdentity;
					next.ReceiptReserve     = previous.ReceiptReserve;
					// Only the provider's same logical parent may advance a pending native
					// representation to its authorized file child, or attach its receipt.
					if (!previous.ServerComment && previous.Representation == "native_line" && p.Representation == "file_comment_with_permalink") {
						next.Representation = previous.Representation;
						next.WireBodyDigest = previous.WireBodyDigest;
					}
					require(encoded(previous) == encoded(next) && !previous.ServerComment, error::conflict);
					require(p.ReceiptReserve == (p.ServerComment ? 0 : previous.ReceiptReserve), error::conflict);
					aState.Counts[9] -= q.bytes(0).size() + previous.ReceiptReserve;
				} else {
					auto draft = read_draft(aDb, p.Draft.value());
					require(draft.Draft.Revision == p.DraftRevision && draft.Binding == p.Target.Binding && draft.Draft.Draft.Anchor == p.Target.Line && private_digest(draft.Draft.Draft.Body) == p.ApprovedProseDigest && draft.Draft.Composition == p.Composition, error::conflict);
					aIds.add(p.Id.value());
					++aState.Counts[7];
				}
				auto payload = encoded(p);
				require(payload.size() <= 8192);
				aState.Counts[9] += payload.size() + p.ReceiptReserve;
				std::optional<std::uint64_t> content;
				if (p.Composition) {
					content = p.Composition->Revision.value();
				}
				aDb.write("INSERT INTO private_publication(scope_id,id,draft_id,content_id,logical_id,server_id,record) VALUES(?1,?2,?3,?4,?5,?6,?7) ON CONFLICT(scope_id,id) DO UPDATE SET server_id=excluded.server_id,record=excluded.record", aDb.Scope, p.Id.value(), p.Draft.value(), content, p.LogicalPublication, p.ServerComment, payload);
			}
		}
		class sqlite_private_review_store final : public private_review_store_port
		{
			state_directory        Directory;
			review_ledger_database Ledger;
			std::int64_t           Scope;
			private_review_scope   Identity;
			auto                   database() -> private_database { return { Ledger.handle(), Scope, Identity }; }
			auto                   current(private_database& aDb) -> private_store_frontier
			{
				auto s = aDb.read_state();
				return { Identity, s.Last == maximum_private_identity ? 0 : s.Last + 1, s.Generation };
			}

		public:
			sqlite_private_review_store(state_directory aDirectory, review_ledger_database aLedger, std::int64_t aScope, private_review_scope aIdentity)
				: Directory(std::move(aDirectory))
				, Ledger(std::move(aLedger))
				, Scope(aScope)
				, Identity(aIdentity)
			{ }
			auto claim_sources(std::span<const private_source_selection> aSelected) -> private_store_result<std::unique_ptr<private_source_use>> override
			{
				return guarded([&]() -> std::unique_ptr<private_source_use> {
					struct claim final : private_source_use
					{
						transaction Transaction;
						explicit claim(sqlite3* aDatabase)
							: Transaction(aDatabase)
						{ }
					};
					auto held = std::make_unique<claim>(Ledger.handle());
					auto db   = database();
					guards(db, aSelected);
					return held;
				});
			}
			auto list_runs(std::optional<agent_run_id> aAfter) -> private_store_result<private_run_page> override
			{
				return guarded([&] {
					auto             db = database();
					auto             q  = db.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id>?2 ORDER BY id LIMIT 17", Scope, aAfter ? aAfter->value() : 0);
					private_run_page page;
					std::size_t      bytes = 0;
					while (q.row()) {
						require(q.bytes(0).size() <= 131072, error::integrity);
						if (page.Runs.size() == maximum_private_detail_runs || bytes + q.bytes(0).size() > 256U * 1024U) {
							page.Next = page.Runs.back().Run;
							break;
						}
						bytes += q.bytes(0).size();
						auto run = q.record<agent_run_provenance>(0);
						require(valid_run_provenance(run) && run.Binding.Scope == Identity, error::integrity);
						page.Runs.push_back(std::move(run));
					}
					return page;
				});
			}
			auto frontier() -> private_store_result<private_store_frontier> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					return current(db);
				});
			}
			auto commit(const private_store_write& aWrite) -> private_store_result<private_store_frontier> override
			{
				return guarded([&] {
					const auto count = aWrite.Threads.size() + aWrite.Revisions.size() + aWrite.Runs.size() + aWrite.Findings.size() + aWrite.Compositions.size() + aWrite.Candidates.size() + aWrite.Drafts.size() + aWrite.RemoveUnusedDrafts.size() + aWrite.Publications.size();
					require(count > 0 && count <= maximum_private_sources);
					transaction tx{ Ledger.handle() };
					auto        db = database();
					auto        s  = db.read_state();
					require(aWrite.ExpectedNextIdentity == (s.Last == maximum_private_identity ? 0 : s.Last + 1), error::conflict);
					allocation ids{ s.Last + 1 };
					guards(db, aWrite.SourceGuards);
					store_runs(db, s, ids, aWrite.Runs);
					store_threads(db, s, ids, aWrite.Threads);
					store_revisions(db, s, ids, aWrite.Revisions, aWrite.Threads);
					store_findings(db, s, ids, aWrite.Findings);
					store_compositions(db, s, ids, aWrite.Compositions);
					store_candidates(db, s, ids, aWrite.Candidates);
					for (const auto id : aWrite.RemoveUnusedDrafts) {
						const auto record = read_draft(db, id.value());
						require(core::unused_review_draft(record.Draft), error::conflict);
						auto references = db.sql("SELECT 1 FROM private_publication WHERE scope_id=?1 AND draft_id=?2 LIMIT 1", Scope, id.value());
						require(!references.row(), error::references_present);
						s.Counts[9] -= record_size(db, "private_draft", id.value());
						--s.Counts[6];
						db.write("DELETE FROM private_draft WHERE scope_id=?1 AND id=?2", Scope, id.value());
					}
					bool admitsActive = false;
					store_drafts(db, s, ids, aWrite.Drafts, admitsActive);
					// A completed record remains publication evidence without consuming
					// an editable slot. No authored or lineage row expires here.
					if (admitsActive) {
						auto        drafts   = db.sql("SELECT id FROM private_draft WHERE scope_id=?1 ORDER BY id LIMIT 4097", Scope);
						std::size_t retained = 0, active = 0;
						while (drafts.row()) {
							require(++retained <= maximum_retained_private_drafts, error::quota);
							const auto record = read_draft(db, drafts.integer(0));
							if (active_draft(record.Draft)) {
								++active;
							}
						}
						require(active <= maximum_private_drafts, error::quota);
					}
					store_publications(db, s, ids, aWrite.Publications);
					for (const auto& changed : aWrite.Threads) {
						auto t = db.thread(changed.Thread.Id.value());
						auto q = db.sql("SELECT 1 FROM private_content WHERE scope_id=?1 AND id=?2 AND kind=0 AND owner_id=?3", Scope, t.LastOrder.value(), t.Id.value());
						require(q.row(), error::conflict);
					}
					for (const auto& changed : aWrite.Compositions) {
						if (changed.Record.State.Candidate) {
							(void)db.source(*changed.Record.State.Candidate);
						}
					}
					ids.finish(s);
					db.save_state(s);
					tx.commit();
					return current(db);
				});
			}
			auto load_thread(private_review_thread_id aId) -> private_store_result<private_review_thread> override
			{
				return guarded([&] {
					require(valid_private_id(aId));
					auto db = database();
					return db.thread(aId.value());
				});
			}
			auto list_threads(bool aIncludeArchived, std::optional<private_catalog_cursor> aCursor) -> private_store_result<private_catalog_page> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					auto        s  = db.read_state();
					if (aCursor) {
						require(aCursor->Scope == Identity && aCursor->Generation == s.Generation && aCursor->IncludeArchived == aIncludeArchived, error::stale_cursor);
					}
					auto                 q = db.sql("SELECT id FROM private_thread WHERE scope_id=?1 AND id>?2 AND (?3=1 OR lifecycle!=2) ORDER BY id LIMIT 65", Scope, aCursor ? aCursor->After : 0, static_cast<int>(aIncludeArchived));
					private_catalog_page page;
					std::size_t          bytes = 0;
					while (q.row()) {
						auto id = q.integer(0);
						if (page.Threads.size() == maximum_private_page_rows) {
							page.Next = private_catalog_cursor{ Identity, s.Generation, page.Threads.back().Id.value(), aIncludeArchived };
							break;
						}
						auto header = db.thread(id);
						bytes += encoded(header).size();
						require(bytes <= 65536, error::quota);
						page.Threads.push_back(std::move(header));
					}
					return page;
				});
			}
			auto list_revisions(private_review_thread_id aThread, std::optional<private_revision_cursor> aCursor) -> private_store_result<private_revision_page> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					auto        t  = db.thread(aThread.value());
					if (aCursor) {
						require(aCursor->Scope == Identity && aCursor->Thread == aThread && aCursor->Version == t.Version, error::stale_cursor);
					}
					auto                  q = db.sql("SELECT id,length(body),record,digest FROM private_content WHERE scope_id=?1 AND kind=0 AND owner_id=?2 AND id>?3 ORDER BY id LIMIT 65", Scope, aThread.value(), aCursor ? aCursor->After : 0);
					private_revision_page page;
					std::size_t           bytes = 0;
					while (q.row()) {
						if (page.Revisions.size() == maximum_private_page_rows) {
							page.Next = private_revision_cursor{ Identity, aThread, t.Version, page.Revisions.back().Revision.Identity.Order.value() };
							break;
						}
						auto revision           = q.record<private_review_revision>(2);
						revision.Identity.Scope = Identity;
						if (revision.Predecessor) {
							revision.Predecessor->Scope = Identity;
						}
						if (revision.ReplyTo) {
							revision.ReplyTo->Scope = Identity;
						}
						auto size = q.integer(1);
						require(revision.Identity.Order.value() == q.integer(0) && revision.Identity.Thread == aThread && revision.Identity.Digest == q.digest<private_body_digest>(3), error::integrity);
						private_revision_page_entry entry{ revision, size, { } };
						if (size <= maximum_private_page_bytes - bytes) {
							auto body = db.sql("SELECT body FROM private_content WHERE scope_id=?1 AND id=?2", Scope, q.integer(0));
							require(body.row(), error::integrity);
							entry.Revision.Body = body.text(0);
							require(private_digest(entry.Revision.Body) == revision.Identity.Digest, error::integrity);
							bytes += entry.Revision.Body.size();
						} else {
							auto source            = source_of(revision);
							source.BodyBytes       = size;
							entry.BodyContinuation = private_body_cursor{ source, 0 };
						}
						page.Revisions.push_back(std::move(entry));
					}
					return page;
				});
			}
			auto read_revision_body(const private_body_cursor& aCursor) -> private_store_result<private_body_page> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					require(!std::holds_alternative<private_finding_source>(aCursor.Source.Key));
					(void)db.source(aCursor.Source);
					require(aCursor.Offset < aCursor.Source.BodyBytes, error::stale_cursor);
					auto q = db.sql("SELECT substr(body,?3,524288) FROM private_content WHERE scope_id=?1 AND id=?2", Scope, aCursor.Source.Order.value(), aCursor.Offset + 1);
					require(q.row(), error::missing);
					private_body_page page{ q.text(0), { } };
					require(!page.Bytes.empty() && page.Bytes.size() <= maximum_private_page_bytes && page.Bytes.size() <= aCursor.Source.BodyBytes - aCursor.Offset, error::integrity);
					auto next = aCursor.Offset + page.Bytes.size();
					if (next < aCursor.Source.BodyBytes) {
						page.Next = private_body_cursor{ aCursor.Source, next };
					}
					return page;
				});
			}
			auto load_run_details(std::span<const agent_run_id> aRuns) -> private_store_result<std::vector<agent_run_provenance>> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					require(aRuns.size() <= maximum_private_detail_runs);
					auto                              db = database();
					std::set<agent_run_id>            seen;
					std::vector<agent_run_provenance> result;
					std::size_t                       bytes = 0;
					for (auto id : aRuns) {
						require(valid_private_id(id) && seen.insert(id).second);
						auto q = db.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", Scope, id.value());
						require(q.row(), error::missing);
						bytes += q.bytes(0).size();
						require(bytes <= 256U * 1024U, error::quota);
						auto run = q.record<agent_run_provenance>(0);
						require(valid_run_provenance(run) && run.Binding.Scope == Identity, error::integrity);
						result.push_back(std::move(run));
					}
					return result;
				});
			}
			auto load_sources(std::span<const private_source_reference> aSources) -> private_store_result<std::vector<private_source_observation>> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					require(aSources.size() <= maximum_private_sources);
					auto                                    db = database();
					std::set<private_source_key>            seen;
					std::vector<private_source_observation> result;
					for (const auto& source : aSources) {
						require(seen.insert(source.Key).second);
						result.push_back(db.source(source));
					}
					return result;
				});
			}
			auto list_findings(std::optional<private_finding_cursor> aCursor) -> private_store_result<private_finding_page> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db    = database();
					auto        state = db.read_state();
					if (aCursor) {
						require(aCursor->Scope == Identity && aCursor->Generation == state.Generation, error::stale_cursor);
					}
					auto                 q = db.sql("SELECT id,record FROM private_content WHERE scope_id=?1 AND kind=1 AND id>?2 ORDER BY id LIMIT 65", Scope, aCursor ? aCursor->After : 0);
					private_finding_page page;
					while (q.row()) {
						if (page.Sources.size() == maximum_private_page_rows) {
							page.Next = private_finding_cursor{ Identity, state.Generation, page.Sources.back().Order.value() };
							break;
						}
						auto finding = q.record<retained_agent_finding>(1);
						require(valid_retained_agent_finding(finding) && finding.Source.Scope == Identity && finding.Source.Order.value() == q.integer(0), error::integrity);
						page.Sources.push_back(finding.Source);
					}
					return page;
				});
			}
			auto source_lineage(const private_source_reference& aSource) -> private_store_result<private_source_lineage> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					(void)db.source(aSource);
					auto q = db.sql("SELECT EXISTS(SELECT 1 FROM private_composition_source WHERE scope_id=?1 AND source_id=?2),EXISTS(SELECT 1 FROM private_composition_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.kind=2 AND c.owner_id=s.composition_id JOIN private_draft d ON d.scope_id=c.scope_id AND d.content_id=c.id WHERE s.scope_id=?1 AND s.source_id=?2),EXISTS(SELECT 1 FROM private_composition_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.kind=2 AND c.owner_id=s.composition_id JOIN private_publication p ON p.scope_id=c.scope_id AND p.content_id=c.id WHERE s.scope_id=?1 AND s.source_id=?2 AND p.server_id IS NOT NULL)", Scope, aSource.Order.value());
					require(q.row(), error::integrity);
					return private_source_lineage{ q.integer(0) != 0, q.integer(1) != 0, q.integer(2) != 0 };
				});
			}
			auto load_finding(const private_source_reference& aSource) -> private_store_result<retained_agent_finding> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					require(std::holds_alternative<private_finding_source>(aSource.Key));
					auto db = database();
					(void)db.source(aSource);
					auto q = db.sql("SELECT record FROM private_content WHERE scope_id=?1 AND id=?2", Scope, aSource.Order.value());
					require(q.row(), error::missing);
					return q.record<retained_agent_finding>(0);
				});
			}
			auto load_composition(review_composition_id aId) -> private_store_result<private_composition_record> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					return db.composition(aId.value());
				});
			}
			auto load_candidate(const private_source_reference& aSource) -> private_store_result<review_composition_revision> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					require(std::holds_alternative<private_composition_source>(aSource.Key));
					auto db = database();
					(void)db.source(aSource);
					auto q = db.sql("SELECT record,body FROM private_content WHERE scope_id=?1 AND id=?2", Scope, aSource.Order.value());
					require(q.row(), error::missing);
					auto c = q.record<review_composition_revision>(0);
					c.Body = q.text(1);
					require(private_digest(c.Body) == c.Identity.Digest, error::integrity);
					return c;
				});
			}
			auto load_draft(review_draft_id aId) -> private_store_result<private_draft_record> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					return read_draft(db, aId.value());
				});
			}
			auto list_drafts() -> private_store_result<std::vector<private_draft_record>> override
			{
				return guarded([&] {
					transaction                       snapshot{ Ledger.handle(), false };
					auto                              db = database();
					auto                              q  = db.sql("SELECT id FROM private_draft WHERE scope_id=?1 ORDER BY id LIMIT 4097", Scope);
					std::vector<private_draft_record> result;
					while (q.row()) {
						require(result.size() < maximum_retained_private_drafts, error::integrity);
						result.push_back(read_draft(db, q.integer(0)));
					}
					return result;
				});
			}
			auto lookup_publication(std::string_view aLogical) -> private_store_result<std::optional<private_publication_record>> override
			{
				return guarded([&]() -> std::optional<private_publication_record> {
					require(!aLogical.empty() && aLogical.size() <= 1024);
					auto db = database();
					auto q  = db.sql("SELECT record FROM private_publication WHERE scope_id=?1 AND logical_id=?2", Scope, std::string{ aLogical });
					if (!q.row()) {
						return { };
					}
					return q.record<private_publication_record>(0);
				});
			}
			auto find_publication(std::optional<std::string> aServer, std::optional<private_publication_cursor> aCursor) -> private_store_result<private_publication_page> override
			{
				return guarded([&] {
					if (aServer) {
						require(!aServer->empty() && aServer->size() <= 1024);
					}
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					auto        s  = db.read_state();
					if (aCursor) {
						require(aCursor->Scope == Identity && aCursor->Generation == s.Generation && aCursor->ServerComment == aServer, error::stale_cursor);
					}
					auto                     q = db.sql("SELECT id,record FROM private_publication WHERE scope_id=?1 AND id>?2 AND (?3 IS NULL OR server_id=?3) ORDER BY id LIMIT 65", Scope, aCursor ? aCursor->After : 0, aServer);
					private_publication_page page;
					while (q.row()) {
						if (page.Records.size() == maximum_private_page_rows) {
							page.Next = private_publication_cursor{ Identity, s.Generation, page.Records.back().Id.value(), aServer };
							break;
						}
						page.Records.push_back(q.record<private_publication_record>(1));
					}
					return page;
				});
			}
			auto erase_thread(private_review_thread_id aId, std::uint64_t aVersion) -> private_store_result<void> override
			{
				return guarded([&] {
					transaction tx{ Ledger.handle() };
					auto        db = database();
					auto        s  = db.read_state();
					db.erase_thread(aId.value(), aVersion, s);
					db.save_state(s);
					tx.commit();
				});
			}

			auto list_thread_dependents(private_review_thread_id aThread, std::optional<private_dependency_cursor> aCursor) -> private_store_result<private_dependency_page> override
			{
				return guarded([&] {
					transaction snapshot{ Ledger.handle(), false };
					auto        db = database();
					(void)db.thread(aThread.value());
					auto s = db.read_state();
					if (aCursor) {
						require(aCursor->Scope == Identity && aCursor->Thread == aThread && aCursor->Generation == s.Generation, error::stale_cursor);
					}
					auto                    q = db.sql("SELECT kind,id FROM (SELECT CASE d.kind WHEN 0 THEN 0 WHEN 1 THEN 2 ELSE 3 END AS kind,CASE d.kind WHEN 1 THEN d.id ELSE d.owner_id END AS id FROM private_edge e JOIN private_content c ON c.scope_id=e.scope_id AND c.id=e.source_id JOIN private_content d ON d.scope_id=e.scope_id AND d.id=e.destination_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2 AND NOT(d.kind=0 AND d.owner_id=?2) UNION SELECT 3,s.composition_id FROM private_composition_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2 UNION SELECT 1,s.run_id FROM private_run_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=0 AND c.owner_id=?2) WHERE (kind>?3 OR (kind=?3 AND id>?4)) ORDER BY kind,id LIMIT 65", Scope, aThread.value(), aCursor ? static_cast<int>(aCursor->After.Kind) : -1, aCursor ? aCursor->After.Id : 0);
					private_dependency_page page;
					while (q.row()) {
						if (page.Objects.size() == maximum_private_page_rows) {
							page.Next = private_dependency_cursor{ Identity, aThread, s.Generation, page.Objects.back() };
							break;
						}
						page.Objects.push_back({ static_cast<private_object_kind>(q.integer(0)), q.integer(1) });
					}
					return page;
				});
			}
			auto erase_local_object(private_object_reference aObject, std::uint64_t aExpectedVersion) -> private_store_result<void> override
			{
				return guarded([&] {
					require(aObject.Id > 0 && aObject.Id <= maximum_private_identity);
					transaction tx{ Ledger.handle() };
					auto        db = database();
					auto        s  = db.read_state();
					if (aObject.Kind == private_object_kind::thread) {
						db.erase_thread(aObject.Id, aExpectedVersion, s);
					} else if (aObject.Kind == private_object_kind::composition) {
						auto c = db.composition(aObject.Id);
						require(c.Version == aExpectedVersion, error::conflict);
						auto refs = db.sql("SELECT EXISTS(SELECT 1 FROM private_draft d JOIN private_content c ON c.scope_id=d.scope_id AND c.id=d.content_id WHERE c.scope_id=?1 AND c.kind=2 AND c.owner_id=?2) OR EXISTS(SELECT 1 FROM private_publication p JOIN private_content c ON c.scope_id=p.scope_id AND c.id=p.content_id WHERE c.scope_id=?1 AND c.kind=2 AND c.owner_id=?2) OR EXISTS(SELECT 1 FROM private_edge e JOIN private_content c ON c.scope_id=e.scope_id AND c.id=e.source_id JOIN private_content d ON d.scope_id=e.scope_id AND d.id=e.destination_id WHERE c.scope_id=?1 AND c.kind=2 AND c.owner_id=?2 AND NOT(d.kind=2 AND d.owner_id=?2)) OR EXISTS(SELECT 1 FROM private_composition_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=2 AND c.owner_id=?2) OR EXISTS(SELECT 1 FROM private_run_source s JOIN private_content c ON c.scope_id=s.scope_id AND c.id=s.source_id WHERE c.scope_id=?1 AND c.kind=2 AND c.owner_id=?2)", Scope, aObject.Id);
						require(refs.row() && refs.integer(0) == 0, error::references_present);
						auto totals = db.sql("SELECT COUNT(*),COALESCE(SUM(length(body)+length(record)),0) FROM private_content WHERE scope_id=?1 AND kind=2 AND owner_id=?2", Scope, aObject.Id);
						require(totals.row(), error::integrity);
						s.Counts[5] -= totals.integer(0);
						s.Counts[9] -= totals.integer(1) + record_size(db, "private_composition", aObject.Id);
						--s.Counts[4];
						db.write("DELETE FROM private_composition_source WHERE scope_id=?1 AND composition_id=?2", Scope, aObject.Id);
						db.write("DELETE FROM private_edge WHERE scope_id=?1 AND destination_id IN(SELECT id FROM private_content WHERE scope_id=?1 AND kind=2 AND owner_id=?2)", Scope, aObject.Id);
						db.write("DELETE FROM private_content WHERE scope_id=?1 AND kind=2 AND owner_id=?2", Scope, aObject.Id);
						db.write("DELETE FROM private_composition WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
					} else if (aObject.Kind == private_object_kind::draft) {
						auto d = read_draft(db, aObject.Id);
						require(d.Draft.Revision == aExpectedVersion, error::conflict);
						auto refs = db.sql("SELECT 1 FROM private_publication WHERE scope_id=?1 AND draft_id=?2 LIMIT 1", Scope, aObject.Id);
						require(!refs.row(), error::references_present);
						s.Counts[9] -= record_size(db, "private_draft", aObject.Id) + (d.Draft.Composition ? 0 : d.Draft.Draft.Body.size());
						--s.Counts[6];
						db.write("DELETE FROM private_draft WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
					} else if (aObject.Kind == private_object_kind::publication) {
						auto publication = db.sql("SELECT record FROM private_publication WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
						require(publication.row(), error::missing);
						s.Counts[9] -= publication.bytes(0).size() + publication.record<private_publication_record>(0).ReceiptReserve;
						--s.Counts[7];
						db.write("DELETE FROM private_publication WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
					} else if (aObject.Kind == private_object_kind::run) {
						auto currentRun = db.sql("SELECT record FROM private_run WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
						require(currentRun.row(), error::missing);
						require(currentRun.record<agent_run_provenance>(0).Outcome != private_run_outcome::admitted, error::references_present);
						auto refs = db.sql("SELECT 1 FROM private_content WHERE scope_id=?1 AND run_id=?2 LIMIT 1", Scope, aObject.Id);
						require(!refs.row(), error::references_present);
						s.Counts[9] -= record_size(db, "private_run", aObject.Id);
						--s.Counts[2];
						db.write("DELETE FROM private_run_source WHERE scope_id=?1 AND run_id=?2", Scope, aObject.Id);
						db.write("DELETE FROM private_run WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
					} else if (aObject.Kind == private_object_kind::finding) {
						auto row = db.sql("SELECT length(record) FROM private_content WHERE scope_id=?1 AND id=?2 AND kind=1", Scope, aObject.Id);
						require(row.row(), error::missing);
						auto refs = db.sql("SELECT EXISTS(SELECT 1 FROM private_edge WHERE scope_id=?1 AND source_id=?2) OR EXISTS(SELECT 1 FROM private_composition_source WHERE scope_id=?1 AND source_id=?2) OR EXISTS(SELECT 1 FROM private_run_source WHERE scope_id=?1 AND source_id=?2)", Scope, aObject.Id);
						require(refs.row() && refs.integer(0) == 0, error::references_present);
						s.Counts[9] -= row.integer(0);
						--s.Counts[3];
						db.write("DELETE FROM private_content WHERE scope_id=?1 AND id=?2", Scope, aObject.Id);
					} else {
						refuse(error::invalid_record);
					}
					db.save_state(s);
					tx.commit();
				});
			}
			auto purge() -> private_store_result<void> override
			{
				return guarded([&] {
					transaction tx{ Ledger.handle() };
					auto        db = database();
					auto        s  = db.read_state();
					// Explicit dependency order preserves the connection's zero trigger-depth policy.
					for (auto table : { "private_publication", "private_run_source", "private_draft", "private_composition_source", "private_composition", "private_edge", "private_turn", "private_legacy_note", "private_note_path", "private_note_line", "private_thread", "private_content", "private_run" }) {
						db.write("DELETE FROM " + std::string{ table } + " WHERE scope_id=?1", Scope);
					}
					s.Counts.fill(0);
					db.save_state(s);
					tx.commit();
				});
			}
		};
	}
}
namespace bpr::adapters::sqlite {
	auto make_private_review_store(const private_review_configuration& aConfiguration, std::stop_token aStop) -> application::private_store_result<std::unique_ptr<application::private_review_store_port>>
	{
		using namespace detail;
		return guarded([&]() -> std::unique_ptr<application::private_review_store_port> {
			auto directory = open_state_directory(aConfiguration.Ledger.StorageRoot);
			require(directory.has_value(), error::unavailable);
			auto ledger = review_ledger_database::open(*directory, "review-ledger.sqlite3", aStop);
			if (!ledger) {
				refuse(ledger.error() == failure::schema_mismatch || ledger.error() == failure::integrity_failure ? error::integrity : error::unavailable);
			}
			auto scope = ledger->scope_id({ aConfiguration.Ledger.Host, aConfiguration.Ledger.Account, aConfiguration.Ledger.Repository, aConfiguration.Ledger.PullRequest });
			require(scope.has_value(), error::unavailable);
			transaction      tx{ ledger->handle() };
			private_database db{ ledger->handle(), *scope };
			db.ensure_state();
			auto q = db.sql("SELECT incarnation FROM private_store_identity WHERE singleton=1");
			require(q.row(), error::integrity);
			core::private_review_scope identity{ aConfiguration.Profile, q.digest<core::private_ledger_incarnation>(0), aConfiguration.Host, aConfiguration.Account, aConfiguration.Repository, aConfiguration.PullRequest };
			require(core::valid_private_scope(identity));
			auto binding = db.sql("SELECT identity_binding FROM private_scope_state WHERE scope_id=?1", *scope);
			require(binding.row(), error::integrity);
			if (binding.null(0)) {
				db.write("UPDATE private_scope_state SET identity_binding=?2 WHERE scope_id=?1", *scope, encoded(identity));
			} else {
				require(binding.record<core::private_review_scope>(0) == identity, error::conflict);
			}
			tx.commit();
			return std::make_unique<sqlite_private_review_store>(std::move(*directory), std::move(*ledger), *scope, identity);
		});
	}
}
