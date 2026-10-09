#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_COMPOSITION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_COMPOSITION_APPLICATION_HOST_HPP
#include "bpr/application/private_review_application_host.hpp"
namespace bpr::application {
	/// \brief A bounded local workspace request. Bodies and provenance are reloaded by identity.
	struct review_composition_request
	{
		core::private_review_binding                    Binding;
		core::review_composition_mode                   Mode = core::review_composition_mode::reviewer_authored;
		std::vector<core::private_source_selection>     Sources;
		std::optional<core::private_composition_target> Target;
		std::string                                     Body;
	};
	struct review_composition_acceptance
	{
		core::review_composition_id          Composition;
		std::uint64_t                        ExpectedVersion = 0;
		core::private_source_reference       Candidate;
		std::optional<core::review_draft_id> Draft                 = { };
		std::uint64_t                        ExpectedDraftRevision = 0;
	};
	/// \brief Current host-owned material for each exact target in a split acceptance.
	struct composition_target_observation
	{
		core::private_review_binding Binding;
		core::diff_semantic_index    Index;
	};
	/// \brief Store acknowledgement carried from the storage lane to the existing shared draft owner.
	/// Runtime scopes and publication grants are never persisted or restored.
	class durable_draft_commit
	{
	public:
		[[nodiscard]] auto scope() const noexcept -> const core::private_review_scope& { return Scope; }
		[[nodiscard]] auto records() const noexcept -> std::span<const private_draft_record> { return Records; }

	private:
		friend class review_composition_application_host;
		friend class private_publication_application_host;
		core::private_review_scope        Scope;
		std::vector<private_draft_record> Records;
	};
	struct review_composition_split
	{
		std::vector<private_composition_record>     Compositions;
		std::vector<core::private_source_selection> Unassigned;
	};
	/// \brief Local-only storage-lane orchestration. The caller posts results to the decision lane.
	/// The store outlives operations; this owner has no process, mutation, projection or executor port.
	class review_composition_application_host final
	{
	public:
		explicit review_composition_application_host(private_review_store_port& aStore)
			: Store(aStore)
		{ }
		[[nodiscard]] auto create(const review_composition_request&) -> private_run_result<private_composition_record>;
		/// \brief Splits all exact target partitions atomically; unanchored sources are returned explicitly.
		[[nodiscard]] auto split(const review_composition_request&) -> private_run_result<review_composition_split>;
		[[nodiscard]] auto revise(core::review_composition_id, std::uint64_t aExpectedVersion, std::string aBody) -> private_run_result<private_composition_record>;
		[[nodiscard]] auto target(core::review_composition_id, std::uint64_t aExpectedVersion, const core::private_composition_target&, const core::diff_semantic_index&) -> private_run_result<private_composition_record>;
		/// \brief Snapshots the exact unproposed workspace for one explicit existing invocation.
		[[nodiscard]] auto agent_intent(core::review_composition_id, std::uint64_t aExpectedVersion) -> private_run_result<private_run_intent>;
		/// \brief Accepts the whole bounded batch, or no workspace/draft changes. Current target is host-owned.
		[[nodiscard]] auto accept(std::span<const review_composition_acceptance>, const core::private_review_binding& aCurrent, const core::diff_semantic_index&, core::review_draft_scope_id aRuntimeScope) -> private_run_result<durable_draft_commit>;
		[[nodiscard]] auto accept(std::span<const review_composition_acceptance>, std::span<const composition_target_observation>, core::review_draft_scope_id aRuntimeScope) -> private_run_result<durable_draft_commit>;
		/// \brief Allocates an ordinary inline draft from the same durable identity frontier.
		[[nodiscard]] auto allocate_draft(const core::private_composition_target&, const core::diff_semantic_index&, std::string aBody = { }) -> private_run_result<durable_draft_commit>;
		/// \brief Ordinary replies may use either exact side; they carry no composition acceptance.
		[[nodiscard]] auto allocate_reply(const core::private_review_binding&, core::source_line_ref, const core::diff_semantic_index&, std::string aThread) -> private_run_result<durable_draft_commit>;
		[[nodiscard]] auto restore(std::optional<core::git_commit_oid> aHead = { }, std::optional<core::review_draft_id> aDraft = { }) -> private_run_result<durable_draft_commit>;
		/// \brief Uses the existing draft reducer; linked edits atomically retain a reviewer successor.
		[[nodiscard]] auto change_draft(const core::revisioned_review_draft_command&) -> private_run_result<durable_draft_commit>;
		/// \brief [[BPR-1345]]. Applies every command through the same draft
		/// reducer in one store transaction: all of them, or none. A review's
		/// Submit holds its pending drafts this way, and its answer completes
		/// or releases them. Linked prose edits are refused here.
		[[nodiscard]] auto change_drafts(std::span<const core::revisioned_review_draft_command>) -> private_run_result<durable_draft_commit>;

	private:
		private_review_store_port& Store;
	};
}
#endif
