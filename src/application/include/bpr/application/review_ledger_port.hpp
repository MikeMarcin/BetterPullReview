#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP

#include "bpr/core/review_ledger.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace bpr::application {
	/// \brief Names why the durable ledger refused one operation.
	///
	/// \details These are storage outcomes, not review outcomes. A receipt the
	/// core refused never reaches the store, so nothing here overlaps
	/// `core::file_receipt_rejection`.
	enum class ledger_store_failure : std::uint8_t {
		unavailable,
		conflict,
		integrity_failure,
		cancelled,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(ledger_store_failure aFailure) noexcept
		-> std::string_view;

	/// \brief Carries one accepted receipt to the durable ledger.
	///
	/// \details This is a *request*, not the write. The host holds it only long
	/// enough to hand it to the store, and publishes nothing until the store
	/// answers.
	struct receipt_persistence_request
	{
		core::file_receipt Receipt;

		auto operator<=>(const receipt_persistence_request&) const = default;
	};

	/// \brief The durable ledger, as the host sees it.
	///
	/// \details The interface is deliberately narrow. The host does not open,
	/// migrate, or close the database; it stores and loads receipts, and every
	/// answer is typed.
	class review_ledger_store_port
	{
	public:
		review_ledger_store_port()                                                   = default;
		review_ledger_store_port(const review_ledger_store_port&)                    = delete;
		auto operator=(const review_ledger_store_port&) -> review_ledger_store_port& = delete;
		review_ledger_store_port(review_ledger_store_port&&)                         = delete;
		auto operator=(review_ledger_store_port&&) -> review_ledger_store_port&      = delete;
		virtual ~review_ledger_store_port()                                          = default;

		/// \brief Commits one receipt, answering with a typed completion.
		[[nodiscard]] virtual auto commit(
			const receipt_persistence_request& aRequest)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Reads one receipt back from durable storage.
		[[nodiscard]] virtual auto load(const core::file_identity& aFile)
			-> std::expected<std::optional<core::file_receipt>, ledger_store_failure>
			= 0;
	};

	/// \brief Reports one accepted receipt to whatever presents it.
	struct receipt_projection
	{
		core::file_receipt Receipt;

		auto operator<=>(const receipt_projection&) const = default;
	};

	/// \brief Reports one receipt that was not accepted, and why.
	///
	/// \details Exactly one of the two reasons applies: the core refused the
	/// command, or the store refused the write. Carrying both in one record
	/// keeps a caller from having to guess which failure vocabulary to consult.
	struct receipt_failure_publication
	{
		core::file_receipt                  Retained;
		core::file_receipt_rejection        Rejection = core::file_receipt_rejection::none;
		std::optional<ledger_store_failure> Store;

		auto operator<=>(const receipt_failure_publication&) const = default;
	};

	/// \brief Receives accepted receipts and refusals.
	class receipt_projection_port
	{
	public:
		receipt_projection_port()                                                  = default;
		receipt_projection_port(const receipt_projection_port&)                    = delete;
		auto operator=(const receipt_projection_port&) -> receipt_projection_port& = delete;
		receipt_projection_port(receipt_projection_port&&)                         = delete;
		auto operator=(receipt_projection_port&&) -> receipt_projection_port&      = delete;
		virtual ~receipt_projection_port()                                         = default;

		virtual void publish(const receipt_projection& aProjection)    = 0;
		virtual void fail(const receipt_failure_publication& aFailure) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
