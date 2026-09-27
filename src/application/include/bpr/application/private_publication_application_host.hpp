#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_PUBLICATION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_PUBLICATION_APPLICATION_HOST_HPP
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/review_mutation_port.hpp"
namespace bpr::application {
	/// \brief Local side of the logical publication join, accessed on the serialized store lane.
	/// The provider owns request compilation, remote journal and exact server reconciliation.
	/// This owner has only storage capabilities and can neither publish nor retry a POST.
	/// The store and this owner must outlive the borrowing provider and its drained effects;
	/// publication and private-store effects share the same serialized execution lane.
	class private_publication_application_host final : public review_publication_lineage_port
	{
	public:
		explicit private_publication_application_host(private_review_store_port& aStore)
			: Store(aStore)
		{ }
		/// \brief Loads an already publishing durable draft into the existing semantic proposal.
		[[nodiscard]] auto proposal(core::review_draft_id, std::uint64_t aExpectedRevision) -> private_run_result<core::review_publication_proposal>;
		[[nodiscard]] auto prepare(const review_mutation_request&, std::string_view, std::string_view) -> bool override;
		[[nodiscard]] auto complete(const review_mutation_request&, std::string_view, std::string_view, const review_mutation_receipt&) -> bool override;
		[[nodiscard]] auto complete_observed(const private_publication_record&, std::string_view, const review_mutation_receipt&) -> bool override;
		[[nodiscard]] auto pending(const core::review_mutation_target&, core::github_account_id) -> std::vector<private_publication_record> override;
		[[nodiscard]] auto for_draft(core::review_draft_id, std::optional<private_publication_cursor> = { }) -> private_run_result<private_publication_page>;
		/// \brief One bounded advisory page; returned lineage names canonical server IDs.
		[[nodiscard]] auto duplicates(core::review_composition_id, std::optional<private_publication_cursor> = { }) -> private_run_result<private_publication_page>;
		[[nodiscard]] auto last_error() const noexcept -> const std::optional<private_run_error>& { return Error; }

	private:
		private_review_store_port&       Store;
		std::optional<private_run_error> Error;
	};
}
#endif
