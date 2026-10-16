#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_LINE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EPOCH_LINE_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/update_line.hpp"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QContextMenuEvent;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPalette;
class QRect;

namespace bpr::qt {
	/// \brief Reports one anchor the reviewer chose on the line.
	using epoch_line_dispatch = std::function<void(
		core::comparison_side,
		core::comparison_anchor_identity)>;

	/// \brief One place a handle can rest on: the pull request's base, or an
	/// update's base or head. The core decides the places ([[BPR-1557]]).
	using epoch_line_stop = core::update_line_place;

	/// \brief RFC-BPR-0028's epoch line: one node per update, two handles.
	///
	/// \details [[BPR-0646]], [[BPR-0743]]. Nodes carry evidenced update
	/// occurrences, with a separately labeled provisional current snapshot.
	/// Supplied display order never asserts a known push sequence.
	/// The two compared anchors are bracket handles with the accent span
	/// between them; an amber tick marks "my last review" with only a
	/// tooltip; the dates sit in their own row beneath the nodes. The line
	/// decides nothing: every gesture leaves as one exact anchor choice
	/// through the dispatch, and what is drawn is what was published back.
	///
	/// Gestures: click a node to compare from it (its head on the left),
	/// Cmd-click to compare to it (its head on the right), right-click for
	/// compare from here, compare to here, show this update alone and copy
	/// head commit, drag either handle, and -- because dense histories make
	/// precise mouse selection hard -- Left and Right step the left handle
	/// one stop at a time while the line has focus, Shift+Left and
	/// Shift+Right step the right handle.
	///
	/// [[BPR-1557]]. The line starts with the pull request's base, the anchor
	/// Whole PR compares from, which the core publishes with each resolution.
	/// It has no update of its own, so it is a post at the track's start, the
	/// left handle's first stop and, for Whole PR, where the left handle sits,
	/// with the right handle on the newest update and the span between them.
	/// Its click, menu and keys compare from it, as a node's do.
	///
	/// [[BPR-1130]]. RFC-BPR-0052 takes the permanent caption away: the
	/// history statement ("Known updates · display order is not push order")
	/// is the line's tooltip and accessible description, and a mark at the
	/// line's start says when history is incomplete or ambiguous, the
	/// statement RFC-BPR-0029 requires the line to make. The row is the
	/// nodes and their dates alone.
	class epoch_line final : public QWidget
	{
	public:
		explicit epoch_line(QWidget* aParent = nullptr);

		static constexpr int height_pixels = 34;

		/// \brief [[BPR-1130]]. Paints the incomplete-history mark: a ring
		/// with an exclamation point, in the palette's text colour, within
		/// `aBounds`. The review bar's Updates disclosure paints the same mark
		/// while the line is folded.
		static void paint_history_mark(QPainter& aPainter, const QRect& aBounds, const QPalette& aPalette);
		/// \brief [[BPR-1130]]. The mark's side, in pixels.
		static constexpr int history_mark_size = 11;

		/// \brief Adopts legacy local records in the supplied order.
		void set_updates(std::vector<application::epoch_row_projection> aOldestFirst);
		/// \brief Presents host-ordered occurrence evidence without inferring push order.
		void               set_occurrences(const core::epoch_occurrence_history&                   aHistory,
										   const std::optional<application::epoch_row_projection>& aCurrentEpoch          = std::nullopt,
										   const std::vector<application::epoch_row_projection>&   aLocalRows             = { },
										   bool                                                    aAllowOccurrenceImport = false);
		[[nodiscard]] auto history_text() const -> QString;
		/// \brief [[BPR-1130]]. Whether history is incomplete or ambiguous,
		/// so the line carries its mark.
		[[nodiscard]] auto history_marked() const noexcept -> bool;
		/// \brief [[BPR-1130]]. What the line's tooltip says away from a node:
		/// the history statement, or why there are no updates.
		[[nodiscard]] auto statement() const -> QString;
		/// \brief Names current loading or refusal when there are no update nodes.
		void               set_empty_status(QString aStatus);
		[[nodiscard]] auto empty_status() const -> QString;
		/// \brief [[BPR-1557]]. Adopts the pull request's base the core
		/// published, and when this install first saw it, if known.
		void set_pull_request_base(std::optional<core::comparison_anchor>         aBase,
								   std::optional<core::observed_utc_milliseconds> aObservedAt = std::nullopt);
		/// \brief Whether the line starts with the pull request's base.
		[[nodiscard]] auto has_pull_request_base() const noexcept -> bool;
		/// \brief [[BPR-1557]]. What the base says under its post, and its tooltip.
		[[nodiscard]] auto base_label() const -> QString;
		[[nodiscard]] auto base_tooltip() const -> QString;
		/// \brief Adopts the compared anchors the core published.
		void set_selection(
			std::optional<core::comparison_anchor_identity> aLeft,
			std::optional<core::comparison_anchor_identity> aRight);
		/// \brief Adopts the head commit of my last review, if any.
		void set_last_review(std::optional<core::git_commit_oid> aCommit);
		/// \brief Whether the core currently offers anchor selection.
		void set_selection_offered(bool aOffered);
		void set_dispatch(epoch_line_dispatch aDispatch);
		/// \brief Emits a portable occurrence choice before retained material exists.
		void set_occurrence_dispatch(std::function<void(core::comparison_side, core::epoch_occurrence_id)> aDispatch);

		[[nodiscard]] auto update_count() const noexcept -> std::size_t;
		[[nodiscard]] auto left_stop() const noexcept -> std::optional<epoch_line_stop>;
		[[nodiscard]] auto right_stop() const noexcept -> std::optional<epoch_line_stop>;
		[[nodiscard]] auto last_review_update() const noexcept -> std::optional<std::size_t>;
		[[nodiscard]] auto selection_offered() const noexcept -> bool;
		/// \brief Centre of an update's node, in widget coordinates.
		[[nodiscard]] auto node_centre(std::size_t aUpdate) const -> QPoint;
		/// \brief Where a stop's handle is drawn, in widget coordinates.
		[[nodiscard]] auto stop_position(const epoch_line_stop& aStop) const -> QPoint;
		[[nodiscard]] auto label_text(std::size_t aUpdate) const -> QString;
		/// \brief [[BPR-1129]]. The dates of the two compared updates, "Feb 12"
		/// and "Feb 15", for the review bar to name beside the comparison.
		///
		/// \details Empty unless both handles sit on updates with a known date.
		[[nodiscard]] auto selection_dates() const -> std::optional<std::pair<QString, QString>>;
		/// \brief [[BPR-1132]]. The dates of any pair's two updates, for a
		/// document tab's label; empty unless both are on the line with a date.
		[[nodiscard]] auto dates_of(
			const core::comparison_anchor_identity& aLeft,
			const core::comparison_anchor_identity& aRight) const -> std::optional<std::pair<QString, QString>>;
		[[nodiscard]] auto label_shown(std::size_t aUpdate) const -> bool;
		[[nodiscard]] auto node_tooltip(std::size_t aUpdate) const -> QString;
		/// \brief The exact anchor a place stands for, when it is retained.
		[[nodiscard]] auto identity_of(const epoch_line_stop& aStop) const
			-> std::optional<core::comparison_anchor_identity>;
		/// \brief The handle the arrow keys move without Shift.
		[[nodiscard]] auto active_side() const noexcept -> core::comparison_side;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;

	private:
		/// \brief Where the track starts: past the mark when there is one.
		[[nodiscard]] auto left_inset() const noexcept -> int;
		[[nodiscard]] auto node_x(std::size_t aUpdate) const -> int;
		[[nodiscard]] auto base_x() const -> int;
		[[nodiscard]] auto update_at(const QPoint& aPoint) const -> std::optional<std::size_t>;
		[[nodiscard]] auto base_at(const QPoint& aPoint) const -> bool;
		[[nodiscard]] auto facts() const -> core::update_line_facts;
		/// \brief The update's place among the line's labelled slots, the
		/// base first when the line has one.
		[[nodiscard]] auto slot_of(std::size_t aUpdate) const noexcept -> std::size_t;
		[[nodiscard]] auto handle_text(const std::optional<epoch_line_stop>& aStop) const -> QString;
		/// \brief Where a label centred on `aCentre` is drawn in the dates' row.
		[[nodiscard]] auto label_rect(const QString& aText, int aCentre) const -> QRect;
		/// \brief Which updates' labels are drawn, by the labels' own extents.
		[[nodiscard]] auto shown_labels() const -> std::vector<bool>;
		void               refresh_name();
		void               adopt_line_updates();
		[[nodiscard]] auto handle_at(const QPoint& aPoint) const -> std::optional<core::comparison_side>;
		[[nodiscard]] auto nearest_stop(core::comparison_side aSide, int aX) const
			-> std::optional<epoch_line_stop>;
		[[nodiscard]] auto stop_for(const core::comparison_anchor_identity& aIdentity) const
			-> std::optional<epoch_line_stop>;
		void choose(core::comparison_side aSide, const epoch_line_stop& aStop);
		void step(core::comparison_side aSide, int aDirection);

		struct line_row
		{
			std::optional<core::review_epoch_id> Id;
			core::git_commit_oid                 Head;
			QString                              Label;
			/// [[BPR-1129]]. The bare date, "Feb 12"; empty when unknown.
			QString                                  Date;
			QString                                  Details;
			bool                                     HasBase = false;
			std::optional<core::epoch_occurrence_id> ImportOccurrence;
		};
		std::vector<line_row> Updates;
		/// [[BPR-1557]]. What the core's line decisions read, kept with `Updates`.
		std::vector<core::update_line_update>                                 LineUpdates;
		std::optional<core::comparison_anchor>                                PullRequestBase;
		std::optional<core::observed_utc_milliseconds>                        BaseObservedAt;
		QString                                                               HistoryText;
		bool                                                                  Marked      = false;
		QString                                                               EmptyStatus = QStringLiteral("No updates loaded.");
		std::optional<core::comparison_anchor_identity>                       Left;
		std::optional<core::comparison_anchor_identity>                       Right;
		std::optional<core::git_commit_oid>                                   LastReview;
		epoch_line_dispatch                                                   Dispatch;
		std::function<void(core::comparison_side, core::epoch_occurrence_id)> OccurrenceDispatch;
		std::optional<core::comparison_side>                                  Dragging;
		std::optional<epoch_line_stop>                                        DragStop;
		core::comparison_side                                                 ActiveSide = core::comparison_side::left;
		bool                                                                  Offered    = false;
	};
} // namespace bpr::qt

#endif
