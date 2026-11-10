#ifndef BPR_CORE_REVIEW_TOUR_DESTINATION_HPP
#define BPR_CORE_REVIEW_TOUR_DESTINATION_HPP
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/review_tour.hpp"
#include <optional>
#include <span>
#include <string_view>
namespace bpr::core {
	enum class tour_subject : std::uint8_t { unknown,
											 current_diff,
											 current_off_hunk,
											 historical,
											 unchanged_file };
	enum class tour_operation_support : std::uint8_t { unknown,
													   supported,
													   unsupported,
													   denied,
													   rate_limited,
													   indeterminate };
	enum class tour_representation : std::uint8_t { unresolved,
													native_line,
													file_permalink,
													conversation_permalink };
	/// Evidence belongs to these exact operations, source range and acting scope.
	/// Standalone line creation and website support never qualify a pending batch.
	struct tour_destination_observation
	{
		tour_code_destination     Source;
		tour_subject              Subject                = tour_subject::unknown;
		bool                      SourceAvailable        = false;
		review_body_compatibility Body                   = review_body_compatibility::portable;
		tour_operation_support    PendingLineBatch       = tour_operation_support::unknown;
		tour_operation_support    PendingOffHunkBatch    = tour_operation_support::unknown;
		tour_operation_support    PendingFileThread      = tour_operation_support::unknown;
		tour_operation_support    StandaloneFile         = tour_operation_support::unknown;
		tour_operation_support    Conversation           = tour_operation_support::unknown;
		bool                      CompatibilityRequested = false;
	};
	struct tour_destination_plan
	{
		tour_representation Representation                                         = tour_representation::unresolved;
		bool                JoinsPendingReview                                     = false;
		bool                RequiresCompatibility                                  = false;
		bool                NeedsQualification                                     = true;
		bool                IncompatibleBody                                       = false;
		auto                operator==(const tour_destination_plan&) const -> bool = default;
	};
	/// \brief Semantic reasons shared by presentation and tour-agent context.
	enum class tour_status_reason : std::uint8_t {
		lines_unconfirmed,
		source_changed,
		source_ambiguous,
		source_missing,
		source_unavailable,
		title_missing,
		explanation_missing,
		generated_unvisited,
		generated_not_ready,
		preparation_attention,
		explanation_not_reaffirmed,
		read_only,
		invalid_tour,
		problem_missing,
		solution_missing,
		no_stops,
		too_many_stops,
		remote_not_owned,
		remote_stale,
		edit_conflict,
		baseline_missing,
		destination_unknown,
		incompatible_body,
		compatibility_required,
		destination_unsupported,
		body_edits_unqualified,
		stop_not_owned,
		stop_baseline_missing,
		source_not_visible,
	};
	/// \brief Confirmation, preparation and publication are separate facts; no path implies confirmation.
	struct tour_stop_status
	{
		bool                            ConfirmedLines     = false;
		bool                            SourceAttention    = false;
		bool                            ReadyEligible      = false;
		bool                            ExplanationCurrent = false;
		std::vector<tour_status_reason> SourceReasons, ReadyReasons, PublicationReasons;
		std::vector<std::string>        AvailableActions;
	};
	[[nodiscard]] auto tour_reason_name(tour_status_reason aReason) noexcept -> std::string_view;
	[[nodiscard]] auto tour_mapping_name(tour_mapping aMapping) noexcept -> std::string_view;
	/// \brief Current stop facts. Attention is the explicit observed editor/selected-source state.
	[[nodiscard]] auto project_tour_stop_status(const review_tour& aTour, const tour_stop& aStop, const tour_attention& aAttention = { }) -> tour_stop_status;
	/// \brief Complete tour-level blockers from the same policy as assess_tour_publication/update.
	[[nodiscard]] auto tour_publication_reasons(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility, tour_operation_support aBodyEdits = tour_operation_support::unknown) -> std::vector<tour_status_reason>;
	enum class tour_publication_readiness : std::uint8_t { incomplete,
														   unavailable,
														   compatibility_choice,
														   single_review,
														   mixed_comments,
														   compatibility_comments };
	[[nodiscard]] auto assess_tour_update(const review_tour&, std::span<const tour_destination_plan>, bool, tour_operation_support, bool aMixed = false) -> tour_publication_readiness;
	[[nodiscard]] auto assess_tour_publication(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility, bool aMixed = false) noexcept -> tour_publication_readiness;
	[[nodiscard]] auto plan_tour_destination(const tour_destination_observation& aObservation) noexcept -> tour_destination_plan;
	/// \brief The source a stop publishes from: the author's replacement, its
	/// original selection, or its remote address; nothing when it has none.
	[[nodiscard]] auto publication_source(const tour_stop& aStop) -> std::optional<tour_code_destination>;
	/// \brief What tour publication observes of one source in a tour whose
	/// pull request's head is `aHead` and base `aBase`: the current diff when
	/// the source is that side's commit, historical otherwise. `aQualified`
	/// is the host's operation qualification as the caller observed it.
	[[nodiscard]] auto publication_observation(const tour_code_destination& aSource, const git_commit_oid& aHead, const git_commit_oid& aBase, std::string_view aBody, bool aCompatibility,
											   bool aQualified) noexcept -> tour_destination_observation;
}
#endif
