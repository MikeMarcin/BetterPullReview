#ifndef BPR_APP_TOUR_FEEDBACK_HPP
#define BPR_APP_TOUR_FEEDBACK_HPP
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "tour_cli_types.hpp"
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <optional>
#include <string_view>
/// \file
/// \brief [[BPR-1092]]. `bpr tour labels`: the Conventional Comments catalog, a body's
/// annotation, an exact label edit of a tour stop and the Overview's label counts, each
/// decided by the core and application services the desktop calls (RFC-BPR-0051 R38, R39).
/// \details Nothing here parses a label, keeps a draft or reads a provider: the catalog and
/// annotations are the core's, the edit is `core::plan_feedback_tour_stop_edit` applied
/// by the tour owner, and the counts are `application::feedback_counter_model` and
/// `application::project_overview_feedback` over an accepted snapshot. This file only
/// names their results for the terminal and the JSON envelope.
namespace bpr::app {
	/// \brief One command's data and human text.
	struct tour_feedback_output
	{
		QJsonObject Data;
		QString     Human;
	};
	/// \brief The catalog, with the grammar and catalog revisions and the standard decorations.
	[[nodiscard]] auto tour_label_catalog() -> tour_feedback_output;
	/// \brief One body as the shared recognizer reads it: an ordinary body, or with
	/// \p aPublished a published post, whose user text `core::published_feedback_segment` maps.
	[[nodiscard]] auto tour_label_annotation(std::string_view aBody, bool aPublished) -> tour_feedback_output;
	/// \brief The core annotation alone, as the edit result and the parse share it.
	[[nodiscard]] auto tour_label_annotation_json(std::string_view aBody, const core::feedback_annotation& aAnnotation) -> QJsonObject;
	/// \brief One explicit header edit from `--label ID|plain`, `--expectation`, `--decoration`
	/// and `--remove-decoration`; refuses a request with nothing to change or two weights at once.
	[[nodiscard]] auto parse_tour_label_edit(const QMap<QString, QString>& aOptions) -> tour_cli_result<core::feedback_header_edit>;
	/// \brief The CLI error for a plan the core refused; never called for `none`.
	[[nodiscard]] auto tour_label_edit_refusal(const core::feedback_tour_stop_edit_plan& aPlan) -> tour_cli_error;
	/// \brief The columns and mode a count was asked with, where each was not taken from Settings.
	struct tour_counts_options
	{
		std::optional<core::feedback_label_columns>   Columns;
		std::optional<core::feedback_resolution_mode> Mode;
	};
	/// \brief `--columns ID,ID|none` (exact stable IDs, no repeat) and `--mode open|all`.
	[[nodiscard]] auto parse_tour_counts_options(const QMap<QString, QString>& aOptions) -> tour_cli_result<tour_counts_options>;
	/// \brief The Overview's counts for one accepted snapshot, as the desktop window decides them.
	struct tour_feedback_counts
	{
		application::overview_feedback_projection Projection;
		core::feedback_collection                 Collection          = core::feedback_collection::partial;
		bool                                      ColumnsFromOption   = false;
		bool                                      ModeFromOption      = false;
		std::size_t                               TourIndexReferences = 0U;
	};
	/// \brief Observes \p aSnapshot into a fresh counter model under the target's exact review
	/// key and projects \p aRequest over its listed files, as `github_application` does.
	[[nodiscard]] auto count_tour_feedback(const application::tour_saved_target& aTarget, const application::github_pull_request_snapshot& aSnapshot,
										   const application::overview_feedback_request& aRequest) -> tour_cli_result<tour_feedback_counts>;
	/// \brief Every row with a conversation, the pull request and the remainder, each with its
	/// breakdown, limits and the number of quiet files whose zero is or is not exact.
	[[nodiscard]] auto tour_feedback_counts_output(const tour_feedback_counts& aCounts) -> tour_feedback_output;
}
#endif
