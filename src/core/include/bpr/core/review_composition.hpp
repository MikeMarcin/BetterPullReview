#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMPOSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMPOSITION_HPP

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/private_review.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

namespace bpr::core {
	struct private_turn_source
	{
		private_review_thread_id        Thread;
		private_review_turn_id          Turn;
		private_review_turn_revision_id Revision;
		auto                            operator<=>(const private_turn_source&) const = default;
	};
	struct private_finding_source
	{
		agent_run_id  Run;
		std::uint32_t Ordinal                                          = 0;
		auto          operator<=>(const private_finding_source&) const = default;
	};
	struct private_composition_source
	{
		review_composition_id          Composition;
		review_composition_revision_id Revision;
		auto                           operator<=>(const private_composition_source&) const = default;
	};
	using private_source_key = std::variant<private_turn_source, private_finding_source, private_composition_source>;
	/// \brief Immutable source address; the store supplies existence and complete-body facts.
	struct private_source_reference
	{
		private_review_scope Scope;
		private_source_key   Key;
		private_vertex_order Order;
		private_body_digest  Digest;
		std::uint64_t        BodyBytes                                          = 0;
		auto                 operator<=>(const private_source_reference&) const = default;
	};
	/// \brief Historical selection is explicit and still revalidates the displayed tip.
	struct private_source_selection
	{
		private_source_reference Source;
		private_source_key       ExpectedLatest;
		bool                     Historical                                         = false;
		auto                     operator<=>(const private_source_selection&) const = default;
	};
	/// \brief Exact inline target with its retained revision; grants no mutation authority.
	struct private_composition_target
	{
		private_review_binding Binding;
		source_line_ref        Line;
		auto                   operator<=>(const private_composition_target&) const = default;
	};
	struct private_copy_body
	{
		private_body_digest Digest;
		std::uint64_t       Bytes                                       = 0;
		auto                operator<=>(const private_copy_body&) const = default;
	};
	/// \brief One bounded store observation; no loaded display fragment is an eligible body.
	struct private_source_observation
	{
		private_source_reference                  Source;
		private_source_key                        Latest;
		private_review_actor                      Actor;
		private_revision_completion               Completion   = private_revision_completion::incomplete;
		private_subject_availability              Availability = private_subject_availability::unplaceable;
		std::optional<private_composition_target> Target;
		std::optional<private_copy_body>          CopyBody;
		std::optional<agent_finding_verdict>      FindingVerdict = { };
		std::optional<private_review_binding>     Binding        = { };
	};
	[[nodiscard]] auto source_of(const private_review_revision& aRevision) -> private_source_reference;
	[[nodiscard]] auto valid_private_source(const private_source_reference& aSource) noexcept -> bool;
	[[nodiscard]] auto make_composition_target(const private_review_binding& aBinding, source_line_ref aLine,
											   const diff_semantic_index& aIndex) -> std::expected<private_composition_target, private_review_refusal>;
	/// \brief Versioned, length-framed identity digest in canonical source-key order.
	[[nodiscard]] auto private_selection_digest(std::span<const private_source_selection> aSelected)
		-> std::expected<private_body_digest, private_review_refusal>;
	/// \brief Revalidates exact sets, digest, tip, scope, completion and explicit history.
	[[nodiscard]] auto admit_composition_sources(const private_review_scope&               aScope,
												 std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved)
		-> std::expected<private_body_digest, private_review_refusal>;

	[[nodiscard]] auto admit_private_run_sources(const private_review_binding&, std::span<const private_source_selection>, std::span<const private_source_observation>) -> std::expected<private_body_digest, private_review_refusal>;
	/// \brief RFC-BPR-0012's comment workshop as an RFC-BPR-0023 private run ([[BPR-1245]]).
	/// A press asks about exactly one source: the complete, current reviewer revision that
	/// recorded the draft's text at the press. No source, a second one, an agent's turn, a
	/// finding, a composition or an explicitly historical revision is refused.
	[[nodiscard]] auto admit_workshop_run_sources(std::span<const private_source_selection>, std::span<const private_source_observation>) -> std::expected<void, private_review_refusal>;

	struct private_derivation_edge
	{
		private_derivation_kind  Kind = private_derivation_kind::composed_from;
		private_source_reference Source;
		auto                     operator<=>(const private_derivation_edge&) const = default;
	};
	/// \brief Admits edges only with a new destination and strictly older observed vertices.
	/// A single per-scope ordinal orders every vertex kind; no graph traversal is needed.
	[[nodiscard]] auto admit_private_derivation(const private_source_reference& aDestination,
												bool aDestinationExists, std::span<const private_derivation_edge> aEdges,
												std::span<const private_source_reference> aExistingSources)
		-> std::expected<std::vector<private_derivation_edge>, private_review_refusal>;

	inline constexpr std::size_t maximum_selected_private_source_bytes = 2U * 1024U * 1024U;
	inline constexpr std::size_t maximum_runtime_fact_bytes            = 256;
	inline constexpr std::size_t maximum_remote_run_id_bytes           = 1024;
	inline constexpr std::size_t maximum_runtime_reports               = 16;
	inline constexpr std::size_t maximum_runtime_metadata_bytes        = 16U * 1024U;
	/// \brief Protocol facts retain the exact field/event basis; conflicting reports coexist.
	struct agent_reported_facts
	{
		std::string                Basis;
		agent_setting_facts        Settings;
		std::optional<std::string> ToolVersion;
		std::optional<std::string> RemoteSession;
		std::optional<std::string> RemoteThread;
		std::optional<std::string> RemoteTurn;
		std::optional<std::string> RemoteRun;
		auto                       operator<=>(const agent_reported_facts&) const = default;
	};
	struct agent_runtime_metadata
	{
		agent_setting_facts               Requested;
		agent_setting_facts               Configured;
		std::vector<agent_reported_facts> ProtocolReported;
		agent_setting_facts               ContentSelfReported;
		auto                              operator<=>(const agent_runtime_metadata&) const = default;
	};
	enum class agent_fact_basis : std::uint8_t { requested,
												 configured,
												 protocol_reported,
												 content_self_reported };
	enum class agent_fact_field : std::uint8_t { model,
												 effort,
												 tool_version,
												 remote_session,
												 remote_thread,
												 remote_turn,
												 remote_run };
	struct agent_fact_location
	{
		agent_fact_basis Basis;
		std::size_t      Report                                        = 0;
		auto             operator<=>(const agent_fact_location&) const = default;
	};
	struct agent_provenance_conflict
	{
		agent_fact_field    Field;
		agent_fact_location First;
		agent_fact_location Second;
		auto                operator<=>(const agent_provenance_conflict&) const = default;
	};
	/// \brief Checks bounded scalar facts and returns conflicts without selecting a winner.
	[[nodiscard]] auto inspect_runtime_metadata(const agent_runtime_metadata& aMetadata)
		-> std::expected<std::vector<agent_provenance_conflict>, private_review_refusal>;
	/// \brief Coalesces identical reports; conflicting values are retained, never overwritten.
	[[nodiscard]] auto append_runtime_report(const agent_runtime_metadata& aMetadata, const agent_reported_facts& aReport)
		-> std::expected<agent_runtime_metadata, private_review_refusal>;

	enum class private_run_purpose : std::uint8_t { conversation,
													composition,
													findings };
	enum class private_run_outcome : std::uint8_t { admitted,
													succeeded,
													failed,
													cancelled,
													timed_out,
													protocol_failure,
													interrupted };
	/// \brief One invocation snapshot independent of the prunable session catalogue.
	struct agent_run_provenance
	{
		agent_run_id                          Run;
		agent_session_id                      Session;
		private_vertex_order                  Order;
		agent_profile_id                      Profile;
		std::string                           ProfileDisplayName;
		agent_protocol                        Protocol   = agent_protocol::external_terminal;
		agent_entry_point                     EntryPoint = agent_entry_point::ask_about_selection;
		private_run_purpose                   Purpose    = private_run_purpose::conversation;
		private_review_binding                Binding;
		agent_runtime_metadata                Metadata;
		std::vector<private_source_selection> SelectedPrivateSources;
		private_body_digest                   InputSelectionDigest;
		std::optional<private_body_digest>    OutputDigest;
		private_run_outcome                   Outcome    = private_run_outcome::admitted;
		private_revision_completion           Completion = private_revision_completion::incomplete;
		/// Ordered byte lengths preserve item boundaries without duplicating assistant text.
		std::vector<std::uint64_t> OutputItemBytes                                = { };
		auto                       operator<=>(const agent_run_provenance&) const = default;
	};
	[[nodiscard]] auto valid_run_provenance(const agent_run_provenance& aRun) -> bool;
	/// \brief Authorizes only a conversation result under its exact admitted entry point.
	[[nodiscard]] auto authorize_private_run(const agent_run_provenance& aRun)
		-> std::expected<private_run_authorization, private_review_refusal>;

	/// \brief Validated structured finding, including original verdict and analyzed revisions.
	struct retained_agent_finding
	{
		private_source_reference Source;
		agent_finding            Finding;
		agent_finding_verdict    Verdict;
		auto                     operator<=>(const retained_agent_finding&) const = default;
	};
	/// \brief Revalidates persisted finding shape and canonical digest without reinterpreting its historical verdict.
	[[nodiscard]] auto valid_retained_agent_finding(const retained_agent_finding& aFinding) -> bool;
	[[nodiscard]] auto retain_agent_finding(const agent_run_provenance& aRun, std::uint32_t aOrdinal,
											private_vertex_order aOrder, const agent_finding& aFinding, const agent_finding_snapshot& aSnapshot)
		-> std::expected<retained_agent_finding, private_review_refusal>;

	enum class review_composition_mode : std::uint8_t { reviewer_authored,
														exact_copy,
														agent_assisted };
	enum class review_composition_lifecycle : std::uint8_t { editing,
															 accepted_as_draft,
															 abandoned };
	struct composition_target_partition
	{
		private_composition_target Target;
		std::vector<std::size_t>   Sources;
	};
	struct composition_target_partitions
	{
		std::vector<composition_target_partition> Assigned;
		std::vector<std::size_t>                  Unassigned;
	};
	/// \brief Partitions every source by exact target; unassigned members stay explicit.
	[[nodiscard]] auto partition_composition_targets(std::span<const private_source_observation> aSources)
		-> std::expected<composition_target_partitions, private_review_refusal>;

	/// \brief Compact workspace state; immutable bodies and source arrays remain observations.
	struct review_composition_state
	{
		review_composition_id                     Id;
		private_review_binding                    Binding;
		review_composition_mode                   Mode      = review_composition_mode::reviewer_authored;
		review_composition_lifecycle              Lifecycle = review_composition_lifecycle::editing;
		private_body_digest                       SourceSetDigest;
		std::size_t                               SourceCount = 0;
		std::optional<private_composition_target> Target;
		std::optional<private_source_reference>   Candidate;
	};
	struct review_composition_revision
	{
		private_source_reference             Identity;
		private_body_digest                  SourceSetDigest;
		private_review_actor                 Actor;
		std::string                          Body;
		std::vector<private_derivation_edge> Derivation;
	};
	struct composition_revision_proposal
	{
		review_composition_state    State;
		review_composition_revision Revision;
	};
	/// \brief Snapshots one selected set and proposes only a unique exact target, if any.
	[[nodiscard]] auto create_review_composition(review_composition_id aId, const private_review_binding& aBinding,
												 review_composition_mode aMode, std::span<const private_source_selection> aSelected,
												 std::span<const private_source_observation> aObserved, std::optional<private_composition_target> aExplicitTarget = { })
		-> std::expected<review_composition_state, private_review_refusal>;
	/// \brief Appends one authored/copy/agent candidate, or a reviewer edit of the exact tip.
	[[nodiscard]] auto propose_composition_revision(const review_composition_state& aState,
													review_composition_revision_id aRevision, private_vertex_order aOrder, std::string_view aBody,
													std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved,
													const agent_run_provenance* aRun = nullptr, const review_composition_revision* aPredecessor = nullptr)
		-> std::expected<composition_revision_proposal, private_review_refusal>;
	/// \brief Local-only intent for the existing draft owner to persist atomically with acceptance.
	struct accept_composition_as_draft_request
	{
		private_source_reference       CompositionRevision;
		private_composition_target     Target;
		std::string                    Body;
		std::optional<review_draft_id> ExistingDraft;
		std::uint64_t                  ExpectedDraftRevision = 0;
	};
	/// \brief Revalidates sources/tip/target and yields a local request; no publish command exists.
	[[nodiscard]] auto accept_composition_as_draft(const review_composition_state&    aState,
												   const review_composition_revision& aCandidate, std::span<const private_source_selection> aSelected,
												   std::span<const private_source_observation> aObserved, const diff_semantic_index& aTargetIndex,
												   const std::optional<revisioned_review_draft>& aExistingDraft = { })
		-> std::expected<accept_composition_as_draft_request, private_review_refusal>;
	/// \brief Applies a durable acceptance acknowledgement only for the exact current candidate.
	[[nodiscard]] auto acknowledge_composition_draft(const review_composition_state& aState,
													 const private_source_reference& aAcceptedCandidate) -> std::expected<review_composition_state, private_review_refusal>;
	/// \brief Starts a reviewer successor while preserving previously accepted immutable candidates.
	[[nodiscard]] auto reopen_review_composition(const review_composition_state&) -> std::expected<review_composition_state, private_review_refusal>;
	[[nodiscard]] auto abandon_review_composition(const review_composition_state& aState)
		-> std::expected<review_composition_state, private_review_refusal>;
	struct composition_publication_observation
	{
		private_review_scope       Scope;
		private_body_digest        SourceSetDigest;
		private_body_digest        BodyDigest;
		private_composition_target Target;
	};
	/// \brief Advisory only; intentional repeated publication remains a separate explicit flow.
	[[nodiscard]] auto duplicate_composition_publication(const review_composition_state&    aState,
														 const review_composition_revision& aCandidate, std::span<const composition_publication_observation> aPublished) noexcept -> bool;
}
#endif
