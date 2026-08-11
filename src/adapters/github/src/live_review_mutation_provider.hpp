#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP

#include "github_mutation_contract.hpp"

#include "bpr/application/review_mutation_port.hpp"

#include <expected>
#include <memory>

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
		[[nodiscard]] auto submit(
			const application::review_mutation_request& aRequest)
			-> std::expected<application::review_mutation_receipt, application::review_mutation_failure> override;
		[[nodiscard]] auto capabilities() const
			-> application::review_provider_capabilities override;

	private:
		friend auto make_live_review_mutation_provider(
			detail::github_mutation_wire_transport&    aTransport,
			detail::live_review_mutation_configuration aConfiguration) noexcept
			-> std::expected<
				std::unique_ptr<live_review_mutation_provider>,
				application::review_mutation_failure>;

		live_review_mutation_provider(
			detail::github_mutation_wire_transport&    aTransport,
			detail::live_review_mutation_configuration aConfiguration) noexcept;

		detail::review_mutation_session Session;
	};

	/// \brief Constructs the contracted live provider for one fixed authority.
	[[nodiscard]] auto make_live_review_mutation_provider(
		detail::github_mutation_wire_transport&    aTransport,
		detail::live_review_mutation_configuration aConfiguration) noexcept
		-> std::expected<
			std::unique_ptr<live_review_mutation_provider>,
			application::review_mutation_failure>;
} // namespace bpr::adapters::github

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_LIVE_REVIEW_MUTATION_PROVIDER_HPP
