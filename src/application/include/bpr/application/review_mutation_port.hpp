#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief Names why a mutation could not be performed.
	enum class review_mutation_failure : std::uint8_t {
		unavailable,
		refused_by_server,
		undesignated_target,
		target_moved,
		stale_head,
		unsupported,
		cancelled,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_mutation_failure aFailure) noexcept
		-> std::string_view;

	/// \brief Reports what a provider can do natively.
	///
	/// \details A provider that cannot resolve threads or record viewed state
	/// must say so, so the degradation is reported rather than silently
	/// skipped. Silently skipping would leave a reviewer believing they had
	/// resolved something.
	struct review_provider_capabilities
	{
		bool ResolvesThreads    = true;
		bool RecordsViewedState = true;

		auto operator<=>(const review_provider_capabilities&) const = default;
	};

	/// \brief Carries one publication to the provider.
	///
	/// \details `IdempotencyKey` is what makes a retry safe. A request whose
	/// answer was lost is indistinguishable from one that never arrived, so the
	/// only way to retry without risking a second comment is for the provider
	/// to recognise the repeat.
	struct review_mutation_request
	{
		core::review_publication_proposal Proposal;
		core::review_mutation_target      Target;
		core::github_account_id           ActingIdentity;
		std::string                       IdempotencyKey;

		auto operator<=>(const review_mutation_request&) const = default;
	};

	/// \brief Reports what the provider did.
	struct review_mutation_receipt
	{
		std::string ServerCommentId;
		bool        Duplicate = false;

		auto operator<=>(const review_mutation_receipt&) const = default;
	};

	/// \brief The GitHub mutation surface, as the host sees it.
	class review_mutation_port
	{
	public:
		review_mutation_port()                                               = default;
		review_mutation_port(const review_mutation_port&)                    = delete;
		auto operator=(const review_mutation_port&) -> review_mutation_port& = delete;
		review_mutation_port(review_mutation_port&&)                         = delete;
		auto operator=(review_mutation_port&&) -> review_mutation_port&      = delete;
		virtual ~review_mutation_port()                                      = default;

		/// \brief Reads the target as the server currently reports it.
		[[nodiscard]] virtual auto observe_target()
			-> std::expected<core::review_mutation_target, review_mutation_failure>
			= 0;

		/// \brief Submits one publication.
		[[nodiscard]] virtual auto submit(const review_mutation_request& aRequest)
			-> std::expected<review_mutation_receipt, review_mutation_failure> = 0;

		[[nodiscard]] virtual auto capabilities() const
			-> review_provider_capabilities = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP
