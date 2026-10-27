#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP

#include "github_mutation_contract.hpp"

#include "bpr/application/review_mutation_port.hpp"

#include <expected>
#include <memory>
#include <stop_token>

namespace bpr::adapters::github {
	/// \brief Implements ordinary GitHub source-line review-comment mutation.
	///
	/// \details The provider owns no endpoint, credential, or network manager.
	/// Its immutable configuration and narrow transport are supplied by the
	/// composition root; every submit re-observes the exact target and acting
	/// identity immediately before the transport may perform a write.
	class live_review_mutation_provider final
		: public application::review_mutation_port
	{
	public:
		~live_review_mutation_provider() override = default;

		live_review_mutation_provider(const live_review_mutation_provider&) = delete;
		auto operator=(const live_review_mutation_provider&)
			-> live_review_mutation_provider&                          = delete;
		live_review_mutation_provider(live_review_mutation_provider&&) = delete;
		auto operator=(live_review_mutation_provider&&)
			-> live_review_mutation_provider& = delete;

		[[nodiscard]] auto observe_target()
			-> std::expected<core::review_mutation_target, application::review_mutation_failure> override;
		[[nodiscard]] auto observe_target(std::stop_token aStop)
			-> std::expected<core::review_mutation_target, application::review_mutation_failure> override;
		[[nodiscard]] auto submit(
			const application::review_mutation_request& aRequest)
			-> std::expected<application::review_mutation_receipt, application::review_mutation_failure> override;
		[[nodiscard]] auto submit_and_reconcile(
			const application::review_mutation_request& aRequest,
			std::stop_token                             aStop)
			-> application::review_mutation_submission override;
		[[nodiscard]] auto submit(
			const application::review_mutation_request& aRequest,
			std::stop_token                             aStop)
			-> std::expected<application::review_mutation_receipt, application::review_mutation_failure> override;
		[[nodiscard]] auto resolve_thread(
			const application::review_thread_resolution_request& aRequest,
			std::stop_token                                      aStop)
			-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure> override;
		/// [[BPR-1741]]. Adds or removes the viewer's reaction through
		/// the leased GraphQL mutation.
		[[nodiscard]] auto set_reaction(
			const application::review_reaction_request& aRequest,
			std::stop_token                             aStop)
			-> std::expected<application::review_reaction_receipt, application::review_mutation_failure> override;
		/// [[BPR-1774]]. Replaces the pull request's description through the
		/// leased GraphQL mutation while GitHub still has the original.
		[[nodiscard]] auto edit_description(
			const application::review_description_request& aRequest,
			std::stop_token                                aStop)
			-> std::expected<application::review_description_receipt, application::review_mutation_failure> override;
		/// [[BPR-1775]]. Replaces one of the viewer's published comments
		/// through the leased GraphQL mutation its kind takes while GitHub
		/// still has the original.
		[[nodiscard]] auto edit_comment(
			const application::review_comment_edit_request& aRequest,
			std::stop_token                                 aStop)
			-> std::expected<application::review_comment_edit_receipt, application::review_mutation_failure> override;
		[[nodiscard]] auto reconcile_publication(const application::review_mutation_request&, std::stop_token) -> application::review_mutation_submission override;
		void               repair_pending_publications(std::stop_token) override;
		[[nodiscard]] auto capabilities() const
			-> application::review_provider_capabilities override;

	private:
		friend auto make_live_review_mutation_provider(
			application::review_mutation_transport       aLease,
			application::review_mutation_target_binding  aTarget,
			application::review_mutation_acting_identity aActingIdentity,
			std::unique_ptr<application::review_mutation_recovery_port>
														  aRecovery,
			application::review_publication_lineage_port* aLineage, std::shared_ptr<application::diagnostic_log_port> aLog) noexcept
			-> std::expected<
				std::unique_ptr<live_review_mutation_provider>,
				application::review_mutation_failure>;

		live_review_mutation_provider(
			application::review_mutation_transport     aLease,
			detail::github_mutation_wire_transport&    aTransport,
			detail::live_review_mutation_configuration aConfiguration,
			std::unique_ptr<application::review_mutation_recovery_port>
														  aRecovery,
			application::review_publication_lineage_port* aLineage, std::shared_ptr<application::diagnostic_log_port> aLog) noexcept;

		detail::review_mutation_session Session;
	};

	/// \brief Constructs the contracted live provider for one fixed authority.
	[[nodiscard]] auto make_live_review_mutation_provider(
		application::review_mutation_transport       aLease,
		application::review_mutation_target_binding  aTarget,
		application::review_mutation_acting_identity aActingIdentity,
		std::unique_ptr<application::review_mutation_recovery_port>
													  aRecovery,
		application::review_publication_lineage_port* aLineage = nullptr, std::shared_ptr<application::diagnostic_log_port> aLog = { }) noexcept
		-> std::expected<
			std::unique_ptr<live_review_mutation_provider>,
			application::review_mutation_failure>;
} // namespace bpr::adapters::github

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP
