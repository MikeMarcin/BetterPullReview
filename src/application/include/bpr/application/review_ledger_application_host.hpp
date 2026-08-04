#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP

#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstdint>

namespace bpr::application {
	/// \brief Records directly observable ledger-host activity.
	struct review_ledger_host_statistics
	{
		std::uint64_t Commands            = 0U;
		std::uint64_t Rejected            = 0U;
		std::uint64_t Unchanged           = 0U;
		std::uint64_t PersistenceRequests = 0U;
		std::uint64_t Completions         = 0U;
		std::uint64_t StoreFailures       = 0U;
		std::uint64_t Publications        = 0U;
		std::uint64_t FailurePublications = 0U;

		auto operator<=>(const review_ledger_host_statistics&) const = default;
	};

	/// \brief Turns receipt commands into durable state and published projections.
	///
	/// \details The order is the contract. A command is decided by the core, the
	/// decision becomes a persistence request, the store answers with a typed
	/// completion, and only then is the accepted projection published. A
	/// projection published before the commit would show a reader a state the
	/// database does not hold — and would survive exactly until the next
	/// restart contradicted it.
	class review_ledger_application_host final
	{
	public:
		review_ledger_application_host(
			review_ledger_store_port& aStore,
			receipt_projection_port&  aProjection) noexcept;

		/// \brief Seeds the retained receipt for one file.
		///
		/// \details The host holds one file's receipt at a time, because the
		/// core decides against the receipt the reader is acting on. It does
		/// not hold the ledger.
		void retain(const core::file_receipt& aReceipt);

		/// \brief Decides, commits, and publishes one receipt command.
		///
		/// \returns The core's outcome. A refused command never reaches the
		/// store, so a rejection here means nothing was written.
		auto submit(const core::file_receipt_command& aCommand)
			-> core::file_receipt_outcome;

		[[nodiscard]] auto retained() const noexcept -> const core::file_receipt&;
		[[nodiscard]] auto statistics() const noexcept
			-> review_ledger_host_statistics;

	private:
		review_ledger_store_port*     Store      = nullptr;
		receipt_projection_port*      Projection = nullptr;
		core::file_receipt            Retained;
		review_ledger_host_statistics Statistics;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
