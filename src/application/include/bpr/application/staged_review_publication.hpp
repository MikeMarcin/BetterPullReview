#ifndef BPR_APPLICATION_STAGED_REVIEW_PUBLICATION_HPP
#define BPR_APPLICATION_STAGED_REVIEW_PUBLICATION_HPP
#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/application/staged_review_contract.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <functional>
namespace bpr::application {
	struct staged_tour_stop
	{
		core::tour_stop_id          Id;
		core::tour_code_destination Source;
		std::string                 Title, Narrative, SourceLink;
		auto                        operator==(const staged_tour_stop&) const -> bool = default;
	};
	struct staged_description_edit
	{
		std::string Baseline, Desired;
		bool        AppendIndexLink                                          = false;
		auto        operator==(const staged_description_edit&) const -> bool = default;
	};
	/// Immutable user-previewed plan. Only returned URLs may fill navigation slots.
	struct staged_review_plan
	{
		review_submission_request              Submission;
		core::tour_owner                       Owner;
		std::uint64_t                          Generation = 0;
		std::vector<staged_tour_stop>          Stops;
		bool                                   Compatibility = false;
		std::optional<staged_description_edit> Description;
		/// Exact observed root/owned baselines and desired route for a non-atomic maintenance plan.
		std::optional<core::review_tour> Update;
		auto                             operator==(const staged_review_plan&) const -> bool = default;
	};
	/// One durable current step plus confirmed ordered identities. Cleared steps stay
	/// confirmed in the body/identity prefix. A witness cannot be reset or replayed.
	struct staged_publication_record
	{
		staged_review_plan                   Plan;
		std::uint64_t                        Revision = 1;
		std::vector<std::uint64_t>           BaselineIds;
		std::optional<staged_review_entity>  Review;
		std::optional<staged_review_comment> Index;
		std::vector<staged_review_comment>   Comments;
		std::size_t                          Patched         = 0;
		bool                                 Published       = false;
		bool                                 DescriptionDone = false;
		std::optional<staged_review_request> Pending;
		bool                                 PossiblyTransmitted                                        = false;
		std::size_t                          Updated                                                    = 0;
		bool                                 DestinationsPublished                                      = false;
		auto                                 operator==(const staged_publication_record&) const -> bool = default;
	};
	/// Exact next-step authorization is consumed at the transport witness.
	using staged_child_authorizer = std::function<bool(const staged_publication_record&)>;
	enum class staged_publication_state : std::uint8_t { unavailable,
														 ready,
														 staged,
														 outcome_unknown,
														 conflict,
														 published,
														 description_attention,
														 partial };
	struct staged_publication_result
	{
		staged_publication_state                 State = staged_publication_state::unavailable;
		std::optional<staged_publication_record> Record;
	};
	[[nodiscard]] auto staged_update_bodies(const staged_publication_record&) -> std::optional<core::tour_update_bodies>;
	[[nodiscard]] auto staged_update_request(const staged_publication_record&) -> std::optional<staged_review_request>;
	[[nodiscard]] auto valid_staged_update(const staged_publication_record&) -> bool;
	[[nodiscard]] auto confirm_staged_update(const staged_publication_record&) -> staged_publication_record;
	[[nodiscard]] auto published_tour(const staged_publication_record&, const core::review_tour&) -> std::optional<core::review_tour>;
	[[nodiscard]] auto valid(const staged_review_plan& aPlan) -> bool;
	[[nodiscard]] auto valid(const staged_publication_record& aRecord) -> bool;
	[[nodiscard]] auto valid_staged_transition(const std::optional<staged_publication_record>& aBefore, const staged_publication_record& aAfter) -> bool;
	[[nodiscard]] auto staged_initial_request(const staged_review_plan& aPlan) -> std::optional<staged_review_request>;
	[[nodiscard]] auto staged_next_request(const staged_publication_record& aRecord) -> std::optional<staged_review_request>;
	/// \brief [[BPR-1368]]. The edit of the record's next unpatched comment,
	/// `aRecord.Comments[aRecord.Patched]`, while its own review is still
	/// pending: GraphQL, by the comment's node ID, never REST.
	[[nodiscard]] auto staged_pending_edit(const staged_publication_record& aRecord, std::string aBody) -> staged_review_request;
	[[nodiscard]] auto staged_final_bodies(const staged_publication_record& aRecord) -> std::optional<std::pair<std::string, std::vector<std::string>>>;
}
#endif
