#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_COORDINATOR_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_COORDINATOR_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>

namespace bpr::application {
	/// \brief Records directly observable coordinator activity.
	struct review_mutation_statistics
	{
		std::uint64_t Attempts            = 0U;
		std::uint64_t TargetObservations  = 0U;
		std::uint64_t Submissions         = 0U;
		std::uint64_t RefusedBeforeSubmit = 0U;
		std::uint64_t Duplicates          = 0U;

		auto operator<=>(const review_mutation_statistics&) const = default;
	};

	/// \brief Performs review mutations against exactly one designated target.
	///
	/// \details The designated target is fixed when the coordinator is built
	/// and can never be replaced. Everything this type does is in service of a
	/// single rule: a mutation reaches the provider only if the server still
	/// reports the pull request this coordinator was designated for.
	///
	/// The target is observed **twice** — once to decide whether publication is
	/// permitted, and again immediately before submitting. A check performed
	/// only at the first point would pass for a target that changed afterwards,
	/// which is precisely the window an automated run could slip through.
	class review_mutation_coordinator final
	{
	public:
		review_mutation_coordinator(
			review_mutation_port&               aProvider,
			const core::review_mutation_target& aDesignated,
			core::github_account_id             aActingIdentity) noexcept;

		/// \brief Builds a coordinator with the complete live authority binding.
		review_mutation_coordinator(
			review_mutation_port&           aProvider,
			review_mutation_target_binding  aDesignated,
			review_mutation_acting_identity aActingIdentity) noexcept;

		/// \brief Publishes one proposal, or explains why it did not.
		///
		/// \param aAcknowledged Whether the reviewer explicitly accepted a
		/// moved head. It can never authorise a different target.
		[[nodiscard]] auto publish(
			const core::review_publication_proposal& aProposal,
			bool                                     aAcknowledged,
			std::stop_token                          aStop = { })
			-> std::expected<review_mutation_receipt, review_mutation_failure>;

		/// \brief Publishes and retains the provider's post-attempt server refresh.
		[[nodiscard]] auto publish_attempt(
			const core::review_publication_proposal&      aProposal,
			review_mutation_anchor_binding                aAnchor,
			bool                                          aAcknowledged,
			std::stop_token                               aStop   = { },
			std::optional<review_mutation_source_binding> aSource = std::nullopt)
			-> review_mutation_submission;

		/// \brief Publishes after binding the semantic anchor to exact source data.
		[[nodiscard]] auto publish(
			const core::review_publication_proposal& aProposal,
			review_mutation_anchor_binding           aAnchor,
			bool                                     aAcknowledged,
			std::stop_token                          aStop = { })
			-> std::expected<review_mutation_receipt, review_mutation_failure>;

		/// \brief [[BPR-0657]]. Resolves or unresolves one thread through the provider,
		/// bound to the designated target and the acting identity.
		///
		/// \details [[BPR-0879]]. The target is confirmed against the server
		/// first, as it is for a publication, so one rule governs every write
		/// this coordinator makes. A thread identity is still a free
		/// identifier at this level; binding it to a pull request needs a
		/// server observation only the provider can make.
		[[nodiscard]] auto resolve_thread(
			const std::string& aThread,
			bool               aResolve,
			std::stop_token    aStop = { })
			-> std::expected<review_thread_resolution_receipt, review_mutation_failure>;
		/// \brief [[BPR-1741]]. Adds or removes the viewer's reaction on
		/// one comment through the provider, after confirming the target as a
		/// resolution does.
		[[nodiscard]] auto set_reaction(
			const std::string&     aSubject,
			core::reaction_content aContent,
			bool                   aAdd,
			std::stop_token        aStop = { })
			-> std::expected<review_reaction_receipt, review_mutation_failure>;
		/// \brief [[BPR-1774]]. Replaces the pull request's description
		/// through the provider while GitHub still has `aOriginal`, after
		/// confirming the target as a resolution does.
		[[nodiscard]] auto edit_description(
			const std::string& aOriginal,
			const std::string& aBody,
			std::stop_token    aStop = { })
			-> std::expected<review_description_receipt, review_mutation_failure>;
		[[nodiscard]] auto designated() const noexcept
			-> const core::review_mutation_target&;
		[[nodiscard]] auto capabilities() const -> review_provider_capabilities;
		[[nodiscard]] auto statistics() const noexcept
			-> review_mutation_statistics;

	private:
		[[nodiscard]] auto confirm_target(
			bool            aAcknowledged,
			std::stop_token aStop)
			-> std::expected<void, review_mutation_failure>;
		[[nodiscard]] auto publish_bound(
			const core::review_publication_proposal&      aProposal,
			std::optional<review_mutation_anchor_binding> aAnchor,
			bool                                          aAcknowledged,
			std::stop_token                               aStop,
			std::optional<review_mutation_source_binding> aSource = std::nullopt)
			-> review_mutation_submission;

		review_mutation_port*                          Provider = nullptr;
		core::review_mutation_target                   Designated;
		core::github_account_id                        ActingIdentity;
		std::optional<review_mutation_target_binding>  TargetBinding;
		std::optional<review_mutation_acting_identity> ActingIdentityBinding;
		review_mutation_statistics                     Statistics;
	};

	/// \brief Derives the idempotency key for one proposal against one target.
	///
	/// \details The key is a pure function of what is being said, where, and to
	/// which revision. Two retries of the same publication produce the same
	/// key; two genuinely different comments cannot collide, because the draft
	/// identity is part of it.
	[[nodiscard]] auto idempotency_key_for(
		const core::review_publication_proposal& aProposal,
		const core::review_mutation_target&      aTarget) -> std::string;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_COORDINATOR_HPP
