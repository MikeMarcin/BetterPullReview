#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_STORE_HPP
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/review_composition.hpp"
#include <memory>
namespace bpr::application {
	inline constexpr std::size_t maximum_private_threads     = 4096;
	inline constexpr std::size_t maximum_private_revisions   = 65536;
	inline constexpr std::size_t maximum_private_body_bytes  = 64U * 1024U * 1024U;
	inline constexpr std::size_t maximum_private_page_rows   = 64;
	inline constexpr std::size_t maximum_private_page_bytes  = 512U * 1024U;
	inline constexpr std::size_t maximum_private_detail_runs = 16;
	inline constexpr std::size_t maximum_private_drafts      = 64;
	/// Active authored drafts keep their64-row limit; completed history has its own bounded allowance.
	inline constexpr std::size_t maximum_retained_private_drafts = 4096;
	/// \brief Storage failures do not confer a workflow or remote authority.
	enum class private_store_failure : std::uint8_t { unavailable,
													  integrity,
													  invalid_record,
													  conflict,
													  missing,
													  stale_cursor,
													  quota,
													  references_present,
													  busy };
	template <typename T>
	using private_store_result = std::expected<T, private_store_failure>;
	/// \brief A scope-bound optimistic allocation frontier; committing consumes IDs atomically.
	struct private_store_frontier
	{
		core::private_review_scope Scope;
		std::uint64_t              NextIdentity = 1;
		std::uint64_t              Generation   = 1;
	};
	struct private_catalog_cursor
	{
		core::private_review_scope Scope;
		std::uint64_t              Generation      = 0;
		std::uint64_t              After           = 0;
		bool                       IncludeArchived = false;
	};
	struct private_catalog_page
	{
		std::vector<core::private_review_thread> Threads;
		std::optional<private_catalog_cursor>    Next;
	};
	struct private_revision_cursor
	{
		core::private_review_scope     Scope;
		core::private_review_thread_id Thread;
		std::uint64_t                  Version = 0;
		std::uint64_t                  After   = 0;
	};
	/// \brief Full immutable body identity; offset is a byte position, never a text coordinate.
	struct private_body_cursor
	{
		core::private_source_reference Source;
		std::uint64_t                  Offset = 0;
	};
	struct private_revision_page_entry
	{
		core::private_review_revision      Revision;
		std::uint64_t                      BodyBytes = 0;
		std::optional<private_body_cursor> BodyContinuation;
	};
	struct private_revision_page
	{
		std::vector<private_revision_page_entry> Revisions;
		std::optional<private_revision_cursor>   Next;
	};
	struct private_body_page
	{
		std::string                        Bytes;
		std::optional<private_body_cursor> Next;
	};
	struct private_thread_write
	{
		core::private_review_thread Thread;
		std::uint64_t               ExpectedVersion = 0;
	};
	struct private_composition_record
	{
		core::review_composition_state              State;
		std::uint64_t                               Version = 1;
		std::vector<core::private_source_selection> Sources;
	};
	struct private_composition_write
	{
		private_composition_record Record;
		std::uint64_t              ExpectedVersion = 0;
	};
	/// \brief Persistent fields of the existing draft; runtime scope and grants are never restored.
	struct private_draft_record
	{
		core::private_review_binding  Binding;
		core::revisioned_review_draft Draft;
	};
	struct private_draft_write
	{
		private_draft_record Record;
		std::uint64_t        ExpectedRevision = 0;
	};
	/// \brief A local journal join. Receipt absence means pending repair, never permission to post.
	struct private_publication_record
	{
		core::review_publication_lineage_id          Id;
		core::review_draft_id                        Draft;
		std::uint64_t                                DraftRevision = 0;
		std::optional<core::review_composition_link> Composition;
		core::private_composition_target             Target;
		core::github_account_id                      Actor;
		core::private_body_digest                    ApprovedProseDigest;
		core::private_body_digest                    WireBodyDigest;
		std::string                                  LogicalPublication;
		std::string                                  Representation;
		std::optional<std::string>                   ServerComment;
		std::optional<std::string>                   ReconciledIdentity;
		/// Version-two correlation; version-one rows retain unknown values.
		std::string          IdempotencyKey                                       = { };
		std::uint64_t        ReceiptReserve                                       = 0;
		std::string          ActorNode                                            = { };
		std::uint64_t        ActorDatabaseId                                      = 0;
		std::string          ActorLogin                                           = { };
		std::string          Path                                                 = { };
		core::git_commit_oid AcceptedHead                                         = { };
		auto                 operator<=>(const private_publication_record&) const = default;
	};
	struct private_publication_cursor
	{
		core::private_review_scope Scope;
		std::uint64_t              Generation = 0;
		std::uint64_t              After      = 0;
		std::optional<std::string> ServerComment;
	};
	struct private_publication_page
	{
		std::vector<private_publication_record>   Records;
		std::optional<private_publication_cursor> Next;
	};
	enum class private_object_kind : std::uint8_t { thread,
													run,
													finding,
													composition,
													draft,
													publication };
	struct private_object_reference
	{
		private_object_kind Kind;
		std::uint64_t       Id                                                 = 0;
		auto                operator<=>(const private_object_reference&) const = default;
	};
	struct private_dependency_cursor
	{
		core::private_review_scope     Scope;
		core::private_review_thread_id Thread;
		std::uint64_t                  Generation = 0;
		private_object_reference       After;
	};
	struct private_dependency_page
	{
		std::vector<private_object_reference>    Objects;
		std::optional<private_dependency_cursor> Next;
	};
	/// \brief One bounded atomic write, prepared by core/host policy against an observed frontier.
	/// New thread, turn, revision, run, finding-order, composition, candidate, draft and
	/// publication IDs must consume consecutive IDs starting at ExpectedNextIdentity.
	/// Each content vertex's ordinal equals its newly allocated identity. Existing headers
	/// and drafts carry CAS values. Source guards are checked in the same transaction.
	struct private_store_write
	{
		std::uint64_t                                  ExpectedNextIdentity = 0;
		std::vector<private_thread_write>              Threads;
		std::vector<core::private_review_revision>     Revisions;
		std::vector<core::agent_run_provenance>        Runs;
		std::vector<core::retained_agent_finding>      Findings;
		std::vector<private_composition_write>         Compositions;
		std::vector<core::review_composition_revision> Candidates;
		std::vector<private_draft_write>               Drafts;
		/// Explicit cleanup: exact never-edited empty records, checked transactionally.
		std::vector<core::review_draft_id>          RemoveUnusedDrafts;
		std::vector<private_publication_record>     Publications;
		std::vector<core::private_source_selection> SourceGuards;
	};
	struct private_finding_cursor
	{
		core::private_review_scope Scope;
		std::uint64_t              Generation = 0;
		std::uint64_t              After      = 0;
	};
	struct private_finding_page
	{
		std::vector<core::private_source_reference> Sources;
		std::optional<private_finding_cursor>       Next;
	};
	struct private_source_lineage
	{
		bool Composition = false;
		bool Draft       = false;
		bool Published   = false;
	};
	/// \brief Source guard owned and released on the store effect lane.
	class private_source_use
	{
	public:
		virtual ~private_source_use() = default;
	};
	struct private_run_page
	{
		std::vector<core::agent_run_provenance> Runs;
		std::optional<core::agent_run_id>       Next;
	};
	/// \brief One durable authority, accessed on the owning storage lane. No remote capabilities.
	class private_review_store_port
	{
	public:
		virtual ~private_review_store_port() = default;
		/// \brief Rechecks exact source tips and excludes concurrent writes through input delivery.
		[[nodiscard]] virtual auto claim_sources(std::span<const core::private_source_selection>) -> private_store_result<std::unique_ptr<private_source_use>> { return std::unexpected(private_store_failure::unavailable); }
		/// \brief Recovery scans bounded run pages; never resumes or launches a process.
		[[nodiscard]] virtual auto list_runs(std::optional<core::agent_run_id> = { }) -> private_store_result<private_run_page> { return std::unexpected(private_store_failure::unavailable); }
		/// \brief Observes the non-pruned allocation frontier; exhausted identity returns zero.
		[[nodiscard]] virtual auto frontier() -> private_store_result<private_store_frontier> = 0;
		/// \brief Commits one bounded batch or refuses it whole, including allocation and quota counters.
		[[nodiscard]] virtual auto commit(const private_store_write&) -> private_store_result<private_store_frontier> = 0;
		/// \brief Returns at most 64 headers and 64 KiB metadata; generation changes refuse old cursors.
		[[nodiscard]] virtual auto list_threads(bool aIncludeArchived = false, std::optional<private_catalog_cursor> aCursor = { }) -> private_store_result<private_catalog_page> = 0;
		/// \brief Loads one exact header without transcript or provenance aggregation.
		[[nodiscard]] virtual auto load_thread(core::private_review_thread_id) -> private_store_result<core::private_review_thread> = 0;
		/// \brief Returns at most 64 revisions and 512 KiB text, with immutable body continuations.
		[[nodiscard]] virtual auto list_revisions(core::private_review_thread_id, std::optional<private_revision_cursor> aCursor = { }) -> private_store_result<private_revision_page> = 0;
		/// \brief Reads at most 512 KiB exact bytes and advances the full-identity continuation.
		[[nodiscard]] virtual auto read_revision_body(const private_body_cursor&) -> private_store_result<private_body_page> = 0;
		/// \brief Loads at most 16 selected runs and 256 KiB encoded provenance.
		[[nodiscard]] virtual auto load_run_details(std::span<const core::agent_run_id>) -> private_store_result<std::vector<core::agent_run_provenance>> = 0;
		/// \brief Reobserves at most 256 exact source identities and their retained tips.
		[[nodiscard]] virtual auto load_sources(std::span<const core::private_source_reference>) -> private_store_result<std::vector<core::private_source_observation>> = 0;
		/// \brief At most 64 retained finding identities, including across restarts; generation-bound continuation.
		[[nodiscard]] virtual auto list_findings(std::optional<private_finding_cursor> = { }) -> private_store_result<private_finding_page> { return std::unexpected(private_store_failure::unavailable); }
		/// \brief Compact exact-source membership badges, no transcript/body aggregation.
		[[nodiscard]] virtual auto source_lineage(const core::private_source_reference&) -> private_store_result<private_source_lineage> { return std::unexpected(private_store_failure::unavailable); }
		/// \brief Loads one validated retained finding, never a revalidation against a new head.
		[[nodiscard]] virtual auto load_finding(const core::private_source_reference&) -> private_store_result<core::retained_agent_finding> = 0;
		/// \brief Loads one bounded source selection and compact composition state.
		[[nodiscard]] virtual auto load_composition(core::review_composition_id) -> private_store_result<private_composition_record> = 0;
		/// \brief Loads one immutable candidate body and its authored provenance.
		[[nodiscard]] virtual auto load_candidate(const core::private_source_reference&) -> private_store_result<core::review_composition_revision> = 0;
		/// \brief Loads the persistent shared-draft value, with no runtime publication grants.
		[[nodiscard]] virtual auto load_draft(core::review_draft_id) -> private_store_result<private_draft_record> = 0;
		/// \brief Loads the bounded collection of at most 64 durable drafts for reattachment.
		[[nodiscard]] virtual auto list_drafts() -> private_store_result<std::vector<private_draft_record>> = 0;
		/// \brief Exact indexed logical identity lookup, never a scan by body text.
		[[nodiscard]] virtual auto lookup_publication(std::string_view) -> private_store_result<std::optional<private_publication_record>> { return std::unexpected(private_store_failure::unavailable); }
		/// \brief Pages at most 64 local journal or receipt joins, optionally by server identity.
		[[nodiscard]] virtual auto find_publication(std::optional<std::string> aServerComment = { }, std::optional<private_publication_cursor> aCursor = { }) -> private_store_result<private_publication_page> = 0;
		/// \brief Deletes one expected thread version only when external retained references are absent.
		[[nodiscard]] virtual auto erase_thread(core::private_review_thread_id, std::uint64_t aExpectedVersion) -> private_store_result<void> = 0;
		/// \brief Pages at most 64 exact dependent identities for separately selected cleanup.
		[[nodiscard]] virtual auto list_thread_dependents(core::private_review_thread_id, std::optional<private_dependency_cursor> aCursor = { }) -> private_store_result<private_dependency_page> = 0;
		/// \brief Explicitly selected cleanup only; external references refuse, no cross-object cascade.
		[[nodiscard]] virtual auto erase_local_object(private_object_reference, std::uint64_t aExpectedVersion = 0) -> private_store_result<void> = 0;
		/// \brief Requires producer quiescence; retains incarnation and all high waters.
		[[nodiscard]] virtual auto purge() -> private_store_result<void> = 0;
	};
}
#endif
