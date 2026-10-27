#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP

#include "bpr/application/commit_filter_projection.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/application/pull_request_overview.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/progress_mark.hpp"

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QModelIndex>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class QAbstractButton;
class QAction;
class QFont;
class QHBoxLayout;
class QKeyEvent;
class QMenu;
class QLabel;
class QPaintEvent;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QToolButton;
class QTreeView;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace bpr::qt {
	class overview_status_drawer;
	class commit_filter_chip;
	class generated_files_row;
	class overview_status_splitter;
	class drawer_meter;
	class drawer_section_header;
	class identity_avatar_view;
	class appearance_profile;
	class overview_description_view;
	class overview_summary_splitter;
	class markdown_body_view;
	class scoped_prose_editor;
	class steady_columns;

	/// \brief The colours the overview's marks are drawn in.
	///
	/// \details Taken from the appearance profile's state roles once one is
	/// adopted; before that, the built-in light values, so the page never
	/// paints a mark in no colour.
	struct overview_colours
	{
		QColor Unread{ 0x09, 0x69, 0xDA };
		QColor Accepted{ 0x1A, 0x7F, 0x37 };
		QColor Defect{ 0xCF, 0x22, 0x2E };
		/// "My last review" amber, the same mark the epoch line uses.
		QColor Marker{ 0xD2, 0x99, 0x22 };
		/// [[BPR-1405]]. Home's Waiting on others tone, for who is waited on.
		QColor Waiting{ 0x09, 0x69, 0xDA };
		/// [[BPR-1405]]. Mergeable, but something is still not passing.
		QColor Caution{ 0xBF, 0x87, 0x00 };
		/// [[BPR-1089]]. The label marks' tints and style, and the four round
		/// state marks' colours, for the Comments column's chips.
		feedback_mark_theme    Marks;
		feedback_state_colours States;
		/// [[BPR-1331]] v003. The read column's marks, the review bar's
		/// family, in the theme's colours; built-in light until adopted.
		progress_mark_colours Progress = builtin_progress_mark_colours(core::appearance_variant::light);
	};

	/// \brief [[BPR-1089]]. One chip of a Comments cell, where it is drawn.
	struct overview_comment_chip
	{
		enum class kind : std::uint8_t {
			/// A dedicated label: its mark and its count.
			label,
			/// The catch-all, or every conversation when no chip is chosen.
			other,
			/// "+n": the chips that did not fit; its menu names them.
			more,
			/// The round qualifier: units the count leaves out or cannot
			/// classify, or a count that is not exact.
			uncertain,
		};
		kind                                Kind = kind::label;
		std::optional<core::feedback_label> Label;
		std::size_t                         Count = 0U;
		core::feedback_state_counts         States{ };
		/// For `more`: the folded chips, in order.
		std::vector<overview_comment_chip> Folded;
		QRectF                             Box;
	};

	/// \brief [[BPR-1089]]. The chips of one Comments cell, laid out in it: the
	/// non-zero chips in the Settings order, Other last, "+n" when the fixed
	/// column cannot hold them all, and the qualifier at the right edge when
	/// anything is uncertain. Painting, tooltips and pointer activation all read
	/// this one layout.
	[[nodiscard]] auto overview_comment_chips(const core::feedback_counter_projection& aCounts, const QRect& aCell, const QFont& aFont)
		-> std::vector<overview_comment_chip>;

	/// \brief [[BPR-1089]]. The chip the keyboard is on: Tab steps from a row
	/// onto its chips, and the row and the chip's kind and label name it, so it
	/// survives a refresh that moves the row or a chip.
	struct overview_chip_focus
	{
		QPersistentModelIndex               Row;
		overview_comment_chip::kind         Kind = overview_comment_chip::kind::label;
		std::optional<core::feedback_label> Label;
	};

	/// \brief [[BPR-1089]]. How a file's operation mark is drawn: a coloured
	/// tile for added, removed and renamed; for modified, a quiet tile mixed
	/// from the row's ground and text, with an outline and a secondary-ink
	/// letter, and on a selected row only an outline and a letter in the
	/// selection's own text colour, so no dark square sits on the highlight.
	struct overview_operation_mark
	{
		/// The tile, or no colour for an outline only.
		QColor Fill;
		/// The outline, or no colour for a tile without one.
		QColor Outline;
		QColor Letter;
	};
	[[nodiscard]] auto overview_operation_mark_colours(application::overview_file_operation aOperation, const QPalette& aPalette, const overview_colours& aColours,
													   bool aSelected, QPalette::ColorGroup aGroup = QPalette::Active) -> overview_operation_mark;

	/// \brief [[BPR-1585]]. How far the Files table's band stands from its
	/// ground, as a luminance contrast ratio: a step the eye follows across a
	/// wide row and nothing reads as a mark.
	inline constexpr double overview_row_band_contrast = static_cast<double>(core::quiet_band_contrast_hundredths) / 100.0;

	/// \brief [[BPR-1585]]. The quiet band that alternate file rows of the
	/// Files table lie on: the palette's `Base` moved toward its `Text` until
	/// the two stand `overview_row_band_contrast` apart. One step in every
	/// theme, and neutral: it carries no hue of its own, so it never reads as
	/// a selection or a state. A palette whose text is that close to its
	/// ground has no band, and gets its `Base`.
	[[nodiscard]] auto overview_row_band(const QPalette& aPalette, QPalette::ColorGroup aGroup = QPalette::Active) -> QColor;

	/// \brief [[BPR-1089]]. The fixed width of the Comments column.
	inline constexpr int overview_comments_width = 216;

	/// \brief The overview's files, grouped by folder.
	///
	/// \details Flat and retained ([[BPR-1213]]): one row per folder that holds
	/// files, named by its whole path, with its files beneath it. A folder
	/// without files of its own takes no row. Siblings keep projection order
	/// and surviving items keep their indexes across refresh. Columns follow the
	/// accepted table: operation, file, added, removed, changed, read,
	/// comments, note. The model holds the projection's rows and decides
	/// nothing about them.
	class overview_file_model final : public QAbstractItemModel
	{
	public:
		enum column : int {
			operation_column,
			file_column,
			added_column,
			removed_column,
			changed_column,
			read_column,
			comments_column,
			note_column,
			column_count,
		};

		enum roles : int {
			path_role = Qt::UserRole + 1,
			folder_role,
			operation_role,
			changed_role,
			/// -1 without a receipt, else the `core::file_read_state` value.
			read_role,
			/// [[BPR-1339]]. -1 without a receipt, else the
			/// `core::file_progress_mark` the row shows.
			progress_role,
			threads_role,
			unresolved_role,
			/// [[BPR-1089]]. True on the "Not on a file" row.
			remainder_role,
			/// [[BPR-1363]]. On a folder row, the files it holds; the row
			/// shows the count right after its disclosure mark.
			file_count_role,
		};

		explicit overview_file_model(QObject* aParent = nullptr);

		void apply(const std::vector<application::overview_file_row>& aRows);
		/// \brief [[BPR-0972]]. Updates the rows of these files in place.
		void update_rows(std::span<const application::overview_file_row> aRows);

		[[nodiscard]] auto index(int aRow, int aColumn, const QModelIndex& aParent = { }) const
			-> QModelIndex override;
		[[nodiscard]] auto parent(const QModelIndex& aIndex) const -> QModelIndex override;
		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto columnCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRole) const -> QVariant override;
		[[nodiscard]] auto headerData(int aSection, Qt::Orientation aOrientation, int aRole) const
			-> QVariant override;

		[[nodiscard]] auto folder_count() const noexcept -> std::size_t;
		[[nodiscard]] auto file_count() const noexcept -> std::size_t;
		/// \brief [[BPR-1363]] v002. The most files any folder row holds, which
		/// sizes the slot every folder's count is drawn in.
		[[nodiscard]] auto largest_folder_file_count() const noexcept -> std::size_t;
		/// \brief The index of a file row, by path.
		[[nodiscard]] auto index_for_path(const QString& aPath) const -> QModelIndex;
		[[nodiscard]] auto row_at(const QModelIndex& aIndex) const
			-> const application::overview_file_row*;

		/// \brief [[BPR-1089]]. Takes the Comments column's counts, or drops
		/// them with none. Only that column changes; the "Not on a file" row
		/// comes and goes at the end.
		void               apply_feedback(std::optional<application::overview_feedback_projection> aFeedback);
		[[nodiscard]] auto feedback() const noexcept -> const std::optional<application::overview_feedback_projection>&;
		/// \brief The counts of a file, folder or remainder row, or none.
		[[nodiscard]] auto feedback_at(const QModelIndex& aIndex) const -> const core::feedback_counter_projection*;
		/// \brief The row a drill-down from this index names.
		[[nodiscard]] auto feedback_row(const QModelIndex& aIndex) const
			-> std::optional<std::pair<application::overview_feedback_row_kind, std::string>>;
		/// \brief The "Not on a file" row, when shown.
		[[nodiscard]] auto remainder_index(int aColumn = 0) const -> QModelIndex;

	private:
		struct node
		{
			QString                                       Path;
			std::optional<application::overview_file_row> File;
			node*                                         Parent = nullptr;
			std::vector<std::unique_ptr<node>>            Children;
			std::size_t                                   FileCount = 0U;
			int                                           Row       = 0;
			/// [[BPR-1089]]. The "Not on a file" row.
			bool Remainder = false;
		};

		static void        build_tree(node& aRoot, const std::vector<application::overview_file_row>& aRows);
		static void        update_rows(node& aParent, int aFirst, int aLast);
		void               reconcile(node& aParent, node& aDesired);
		void               update_file(node& aNode, const application::overview_file_row& aRow);
		void               rebuild_lookup(const std::vector<application::overview_file_row>& aRows);
		[[nodiscard]] auto node_at(const QModelIndex& aIndex) const -> const node*;
		[[nodiscard]] auto index_of(const node& aNode, int aColumn = 0) const -> QModelIndex;

		void               sync_remainder();
		[[nodiscard]] auto comments_text(const core::feedback_counter_projection& aCounts, bool aAccessible) const -> QString;

		node                  Root;
		QHash<QString, node*> FileNodes;
		std::vector<node*>    FileOrder;
		std::size_t           FolderCount        = 0U;
		std::size_t           LargestFolderCount = 0U;
		/// [[BPR-1089]]. The counts, and each file's and folder's place in them.
		std::optional<application::overview_feedback_projection> Feedback;
		QHash<QString, std::size_t>                              FileFeedback;
		QHash<QString, std::size_t>                              FolderFeedback;
	};

	/// \brief Paints the overview table's marks: operation, changed, read, comments.
	class overview_row_delegate final : public QStyledItemDelegate
	{
	public:
		explicit overview_row_delegate(const overview_colours& aColours, QObject* aParent = nullptr);

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QSize override;

		/// \brief Where a folder row's first cell (`aOption.rect`) draws its
		/// disclosure mark, in view coordinates: at the row's left edge
		/// ([[BPR-1363]]).
		[[nodiscard]] static auto disclosure_rect(const QStyleOptionViewItem& aOption) -> QRect;

		/// \brief [[BPR-1363]] v002. Where a folder row's first cell
		/// (`aOption.rect`) draws its file count, right-aligned in tabular
		/// figures: a slot right after the disclosure mark, as wide as the
		/// largest count of the pull request, so every folder's count shares
		/// one column.
		[[nodiscard]] static auto count_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QRect;

		/// \brief [[BPR-1089]]. The chip to ring while the view has focus.
		void set_chip_focus(std::optional<overview_chip_focus> aFocus);

		/// \brief [[BPR-1585]]. Whether the row of `aIndex` lies on the band in
		/// `aView`. A folder header and the Not on a file row never do; each
		/// header starts its group on the ground, and the files under it take
		/// turns from there, the first on the band. Turns are counted over the
		/// rows the view shows: a hidden row takes none, and a collapsed
		/// folder or a sort elsewhere moves no other group's turns.
		[[nodiscard]] static auto banded(const QModelIndex& aIndex, const QWidget* aView) -> bool;

	protected:
		/// \brief [[BPR-1585]]. On a banded row, the cell's background is the
		/// band and the band is the cell's `Base`, the ground its marks mix
		/// with and cut their letters to. A selected row is the selection's.
		void initStyleOption(QStyleOptionViewItem* aOption, const QModelIndex& aIndex) const override;

	private:
		[[nodiscard]] static auto disclosure_width(const QStyleOptionViewItem& aOption) -> int;
		void                      paint_cell(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const;
		void                      paint_folder(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const;
		void                      paint_disclosure(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const;
		void                      paint_chips(QPainter* aPainter, const QStyleOptionViewItem& aOption, const core::feedback_counter_projection& aCounts, core::feedback_resolution_mode aMode) const;

		const overview_colours&            Colours;
		std::optional<overview_chip_focus> ChipFocus;
	};

	/// \brief One participant: avatar initial, login, role and standing.
	class overview_participant_row final : public QWidget
	{
	public:
		overview_participant_row(
			const application::overview_participant& aParticipant,
			const overview_colours&                  aColours,
			QWidget*                                 aParent = nullptr);

		[[nodiscard]] auto login() const -> QString;
		[[nodiscard]] auto standing_text() const -> QString;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		application::overview_participant Participant;
		const overview_colours&           Colours;
	};

	/// \brief [[BPR-1405]]. One outstanding review request: the participant
	/// row's anatomy, a square tile for a team, and code owner or requested
	/// at the right. A team's readable name is in the tooltip and the
	/// accessible name.
	class overview_request_row final : public QWidget
	{
	public:
		overview_request_row(
			const application::overview_review_request& aRequest,
			const overview_colours&                     aColours,
			QWidget*                                    aParent = nullptr);

		[[nodiscard]] auto request() const noexcept -> const application::overview_review_request&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		application::overview_review_request Request;
		const overview_colours&              Colours;
	};

	/// \brief [[BPR-1589]]. One person or team as a face in People's strip.
	struct overview_face
	{
		QString Identity;
		/// The initials' source when it is not the identity (a team's slug).
		QString InitialsFrom;
		bool    Team = false;
		/// The standing's mark at the face's corner; none when invalid.
		QColor Badge;

		auto operator==(const overview_face&) const -> bool = default;
	};

	/// \brief [[BPR-1589]]. People at a glance: a row of faces, each with its
	/// standing's mark, as many as fit.
	class overview_faces final : public QWidget
	{
	public:
		explicit overview_faces(QWidget* aParent = nullptr);

		void               set_faces(std::vector<overview_face> aFaces);
		[[nodiscard]] auto faces() const -> const std::vector<overview_face>&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		std::vector<overview_face> Faces;
	};

	/// \brief [[BPR-1589]] v002. My role at a glance: my face, the strongest
	/// role as a short label, and any other roles as small chips after it,
	/// wrapping when the drawer is narrow. Why a role is what it is (a team
	/// GitHub would not say I am in, say) is in the tooltip, not the line.
	class overview_role_line final : public QWidget
	{
	public:
		explicit overview_role_line(QWidget* aParent = nullptr);

		/// \brief The face's identity, the strongest role and the others.
		void               present(const QString& aViewer, const QString& aPrimary, const QStringList& aChips, const QString& aExplanation);
		[[nodiscard]] auto primary() const -> const QString&;
		[[nodiscard]] auto chips() const -> const QStringList&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		/// The primary label's box and each chip's, laid out in `aWidth`.
		[[nodiscard]] auto layout_boxes(int aWidth) const -> std::vector<QRect>;

		QString     Viewer;
		QString     Primary;
		QStringList Chips;
	};

	/// \brief [[BPR-1589]]. The Status drawer's sections, in their order.
	enum class overview_status_section : std::uint8_t {
		attention,
		my_review,
		checks,
		people,
	};

	inline constexpr std::size_t overview_status_section_count = 4U;

	/// \brief The values the overview's side column names.
	enum class overview_value : std::uint8_t {
		uploads,
		files_changed,
		new_comments,
		replies_to_me,
		my_unresolved_threads,
		checks,
		files_read,
		private_notes,
		drafted_comments,
	};

	inline constexpr std::size_t overview_value_count = 9U;

	using overview_path_dispatch = std::function<void(const QString&)>;
	/// [[BPR-1089]]. The view's chips, mode and sort, for the host to count.
	using overview_feedback_dispatch = std::function<void(const application::overview_feedback_request&)>;
	/// [[BPR-1089]]. One number of one row, activated.
	using overview_drill_down_dispatch = std::function<void(const application::overview_drill_down&)>;

	/// \brief [[BPR-1089]]. A count's words: "issue", "Other", "Comments",
	/// "Not counted".
	[[nodiscard]] auto overview_count_name(const core::feedback_count_target& aTarget, bool aAnyColumns) -> QString;

	class pending_veil;

	/// \brief [[BPR-1576]]. The Overview's sections as they wait on the read.
	struct overview_pending_veils
	{
		/// The title and its summary line.
		pending_veil* Header      = nullptr;
		pending_veil* Description = nullptr;
		/// The files table: the catalogue, then its comment counts.
		pending_veil* Files = nullptr;
		/// Merge, waiting on, participants, since my last review, checks.
		pending_veil* Side = nullptr;
	};

	/// \brief [[BPR-1774]]. Save pressed in the description's editor: the body
	/// editing began with, byte for byte, and the text to save. A gesture
	/// only; the composition decides whether anything is sent.
	struct overview_description_save
	{
		std::string Original;
		std::string Body;

		auto operator==(const overview_description_save&) const -> bool = default;
	};

	/// \brief RFC-BPR-0028's overview: what changed and who has signed off.
	///
	/// \details [[BPR-0647]]. The Overview modality's content, drawn from
	/// one `pull_request_overview_projection` the host decided. The main
	/// column is the pull request's identity, description and the files
	/// table; the side column is the participants, what happened since my
	/// last review, my review progress, and the one action. Every gesture
	/// for review navigation leaves through a semantic dispatch. Description
	/// expansion, focus and reading position remain local to this page.
	class pull_request_overview_page final : public QWidget
	{
	public:
		explicit pull_request_overview_page(QWidget* aParent = nullptr);

		void present(const application::pull_request_overview_projection& aProjection);
		/// \brief [[BPR-0972]]. Takes receipt changes to the presented overview.
		void apply_receipts(const application::overview_receipt_update& aUpdate);
		void apply_history(const application::overview_history_update& aUpdate);
		/// \brief Clears text and local reading state on the owner's exact authority invalidation.
		void clear();
		/// \brief Presents a pending overview without manufacturing accepted rows.
		void present_loading();
		void present_observation(const application::github_loading_overview_projection& aObservation);
		/// \brief Shows bounded historical metadata with explicit unavailable review content.
		void               present_cached(const application::github_cached_overview_projection& aOverview);
		void               present_cached_source(const application::cached_review_source_projection& aSource);
		void               set_cached_source_dispatch(overview_path_dispatch aDispatch);
		[[nodiscard]] auto presented() const noexcept -> bool;
		/// \brief [[BPR-0671]]. Takes the commit-oid disclosure the drawer's
		/// summary box used to carry, and puts it under the branches.
		void               adopt_technical_details(QWidget* aButton, QWidget* aDetails);
		[[nodiscard]] auto projection() const noexcept
			-> const std::optional<application::pull_request_overview_projection>&;

		/// \brief Adopts the state colours the appearance profile names.
		void               adopt_appearance(const appearance_profile& aProfile);
		[[nodiscard]] auto colours() const noexcept -> const overview_colours&;

		/// \brief Presents the window-owned private tour action from Overview.
		void set_prepare_tour_action(QAction* aAction);
		/// \brief [[BPR-1588]]. Puts the window's Open on GitHub on the Status
		/// drawer's tab row.
		void set_open_on_github_action(QAction* aAction);
		/// \brief RFC-BPR-0089. Presents the commit filter in the Commits tab
		/// and the chip above the files.
		void present_commit_filter(const application::commit_filter_projection& aProjection);
		/// \brief The Commits tab's and the chip's gestures.
		void set_commit_filter_dispatch(std::function<void(const application::commit_filter_gesture&)> aDispatch);
		/// \brief The Commits tab first showed.
		void set_commit_filter_shown_dispatch(std::function<void()> aDispatch);
		/// \brief Open on GitHub for one commit.
		void               set_commit_open_dispatch(std::function<void(const std::string&)> aDispatch);
		[[nodiscard]] auto commit_chip() const noexcept -> commit_filter_chip*;
		/// \brief RFC-BPR-0089 M2. "Hide files these commits changed" from the
		/// Commits tab.
		void set_file_filter_dispatch(std::function<void(const std::vector<std::string>&)> aDispatch);
		/// \brief RFC-BPR-0089 M2. Show as normal, Treat as generated, Review
		/// anyway and their reverses, from a file's context menu.
		void set_generated_dispatch(std::function<void(const application::generated_file_gesture&)> aDispatch);
		/// \brief The row that ends the file tree: generated files set aside.
		[[nodiscard]] auto generated_row() const noexcept -> generated_files_row*;
		/// \brief The context menu a file row opens: its comments, then its
		/// generated gestures; the caller owns it, none for a folder.
		[[nodiscard]] auto make_file_menu(const QModelIndex& aIndex) -> QMenu*;
		/// \brief [[BPR-1588]]. The Overview's right column, a real drawer.
		[[nodiscard]] auto status_drawer() const noexcept -> overview_status_drawer*;
		[[nodiscard]] auto status_splitter() const noexcept -> QSplitter*;
		/// \brief [[BPR-1588]]. The first unseen change's file, where the first
		/// entry into Review from here lands; empty when there is nothing to read.
		[[nodiscard]] auto entry_path() const -> std::optional<QString>;
		/// \brief A file row activated: open it in Read.
		void set_file_dispatch(overview_path_dispatch aDispatch);
		/// \brief [[BPR-1589]]. How many threads have posts I have not read,
		/// as Threads' Only new counts them; My review says so with Go to them.
		void               set_new_threads(std::size_t aCount);
		[[nodiscard]] auto new_threads() const noexcept -> std::size_t;
		/// \brief [[BPR-1589]]. Go to them: open Threads with Only new on.
		void               set_new_threads_dispatch(std::function<void()> aDispatch);
		[[nodiscard]] auto new_threads_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1589]]. New pull-request comments that are not on a
		/// file, once the ledger keeps their read state: "N new comments in
		/// the conversation" with Go to them, which opens Discuss. Absent or
		/// zero draws nothing; until that read state exists nothing sets it.
		void               set_new_conversation_comments(std::optional<std::size_t> aCount);
		void               set_new_conversation_dispatch(std::function<void()> aDispatch);
		[[nodiscard]] auto new_conversation_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1589]]. A Status section's header, its disclosure.
		[[nodiscard]] auto status_section_header(overview_status_section aSection) const noexcept -> drawer_section_header*;
		/// \brief [[BPR-1589]]. A Status section, header, body and details.
		[[nodiscard]] auto status_section(overview_status_section aSection) const noexcept -> QWidget*;
		/// \brief [[BPR-1589]]. My role's line.
		[[nodiscard]] auto role_line() const noexcept -> overview_role_line*;
		/// \brief [[BPR-1589]]. Each Needs attention row's title and its second
		/// line, if shown, joined by a newline.
		[[nodiscard]] auto attention_rows() const -> QStringList;
		[[nodiscard]] auto people_faces() const noexcept -> overview_faces*;
		[[nodiscard]] auto people_summary() const noexcept -> QLabel*;
		/// \brief Sends an explicit check destination gesture to the accepted host.
		void set_check_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch);

		/// \brief [[BPR-1089]]. Takes the host's counts for the view's request.
		/// \details A projection for another request than the view's current
		/// one is dropped; the next answer replaces it.
		void present_feedback(const application::overview_feedback_projection& aFeedback);
		/// \brief The reviewer's chips, in their order, from Settings.
		void set_label_columns(const core::feedback_label_columns& aColumns);
		/// \brief Where a pull request's Overview starts counting (Settings).
		/// \details The active mode is the view's own: this applies to the
		/// next pull request presented, or to this one before its mode was
		/// chosen.
		void set_default_mode(core::feedback_resolution_mode aMode);
		void set_label_marks(const feedback_mark_theme& aTheme);
		void set_attention_colour(const QColor& aColour);
		void set_feedback_dispatch(overview_feedback_dispatch aDispatch);
		void set_drill_down_dispatch(overview_drill_down_dispatch aDispatch);
		/// \brief The view's current request: its chips, mode and sort.
		[[nodiscard]] auto feedback_request() const -> application::overview_feedback_request;
		/// \brief Chooses the mode, as the switch does.
		void choose_mode(core::feedback_resolution_mode aMode);
		/// \brief Sorts the rows by one number, or by file with none.
		void choose_sort(std::optional<core::feedback_count_target> aSort);
		/// \brief The menu of one row's chips: what the context-menu key and a
		/// right-click open, and "+n". Null for a row with none. The caller
		/// owns it.
		[[nodiscard]] auto make_comments_menu(const QModelIndex& aIndex) -> QMenu*;
		/// \brief The Comments header's sort menu. The caller owns it.
		[[nodiscard]] auto make_sort_menu() -> QMenu*;
		[[nodiscard]] auto open_counts_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto all_counts_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto uncertain_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto label_key_button() const noexcept -> QToolButton*;
		/// \brief A count's mark as its chip draws it: the label's mark, the
		/// speech bubble (with dots for Other) or the ? state mark.
		[[nodiscard]] auto count_mark(const core::feedback_count_target& aTarget, bool aAnyColumns) const -> QPixmap;
		/// \brief [[BPR-1089]]. The chip the keyboard is on, laid out in its
		/// cell, or none: Tab steps from a row onto its chips.
		[[nodiscard]] auto focused_chip() const -> std::optional<overview_comment_chip>;
		/// \brief Opens the Label key, as Alt+K does.
		void               show_label_key();
		[[nodiscard]] auto label_key() const noexcept -> QWidget*;

		/// \brief [[BPR-1576]]. The sections' veils, for courses.
		[[nodiscard]] auto pending_veils() const noexcept -> overview_pending_veils;
		/// \brief [[BPR-1576]]. What the read still waits on, said by a section
		/// whose wait has become slow: "the changed files, page 3".
		void set_read_waiting(const QString& aWaiting);
		/// \brief [[BPR-1576]]. Motion, contrast and failure ink for the veils.
		void set_pending_appearance(bool aFullMotion, bool aHighContrast, const QColor& aFailureInk);

		/// \brief [[BPR-1774]]. Offers Edit on the description, or withdraws
		/// it. The composition offers it to the pull request's author alone;
		/// withdrawn while editing, the editor closes and nothing is sent.
		void               set_description_editable(bool aEditable);
		[[nodiscard]] auto description_editable() const noexcept -> bool;
		/// \brief [[BPR-1774]]. Where Save goes. Nothing is sent by the page.
		void set_description_save_dispatch(std::function<void(const overview_description_save&)> aDispatch);
		/// \brief [[BPR-1774]]. Opens the editor on the presented body, as
		/// Edit does; false when Edit is not offered.
		auto               begin_description_edit() -> bool;
		[[nodiscard]] auto description_editing() const noexcept -> bool;
		/// \brief [[BPR-1774]]. What the composition says beside the editor:
		/// a refusal or progress in words. `aSaving` holds the text and Save
		/// while GitHub is asked.
		void present_description_status(const QString& aText, bool aSaving = false);
		/// \brief [[BPR-1774]]. GitHub's body changed since editing began:
		/// offers to go on editing from `aNewer`. The typed text stays until
		/// the offer is taken.
		void offer_newer_description(std::string aNewer);
		/// \brief [[BPR-1774]]. Closes the editor, as after a save GitHub
		/// answered; the description then shows what the next read brings.
		void               close_description_editor();
		[[nodiscard]] auto description_edit_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto description_add_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto description_editor() const noexcept -> scoped_prose_editor*;
		[[nodiscard]] auto description_save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto description_cancel_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto description_newer_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto description_status() const noexcept -> QLabel*;
		/// \brief The body editing began with.
		[[nodiscard]] auto description_original() const noexcept -> const std::string&;
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto summary_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_view() const noexcept -> markdown_body_view*;
		/// \brief Window-owned preferred summary fraction, in ten-thousandths; none uses content-aware sizing.
		void               set_summary_fraction(std::optional<std::uint32_t> aFraction);
		[[nodiscard]] auto summary_fraction() const -> std::optional<std::uint32_t>;
		/// \brief Reports explicit pointer, keyboard or reset gestures, never a geometry clamp.
		void               set_summary_resize_dispatch(std::function<void(std::optional<std::uint32_t>)> aDispatch);
		[[nodiscard]] auto files_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto files_view() const noexcept -> QTreeView*;
		/// \brief [[BPR-1269]]. Collapses every folder row; files keep their order
		/// and nothing opens. A hidden current file leaves the current row on its
		/// folder, as a single collapse does.
		void collapse_all_folders();
		/// \brief [[BPR-1269]]. Expands every folder row.
		void               expand_all_folders();
		[[nodiscard]] auto collapse_folders_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto expand_folders_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto file_model() const noexcept -> overview_file_model*;
		[[nodiscard]] auto participant_rows() const -> std::vector<overview_participant_row*>;
		/// \brief [[BPR-1405]]. The Waiting on roster, in projection order.
		[[nodiscard]] auto request_rows() const -> std::vector<overview_request_row*>;
		[[nodiscard]] auto since_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto value_label(overview_value aValue) const noexcept -> QLabel*;
		/// \brief No minimum: a hidden page must not widen the centre's stack.
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		/// [[BPR-1576]]. Puts every section's veil in the look its facts give.
		void apply_pending();
		/// [[BPR-1774]]. Shows Edit, Add a description, the reader and the
		/// editor as the body, the offer and the editing state say.
		void present_description_controls();
		void set_description_expanded(bool aExpanded);
		void show_cached_source(bool aShow);
		void set_value(overview_value aValue, const QString& aText, bool aHighlight);
		void present_files_heading(const application::pull_request_overview_projection& aProjection);
		/// \brief [[BPR-1269]]. Shows the folder buttons beside a shown heading
		/// while there is a folder, and repaints their glyphs in the palette.
		void update_folder_controls();
		void refresh_folder_icons();
		void rebuild_participants(const std::vector<application::overview_participant>& aParticipants);
		void present_merge(const application::pull_request_overview_projection& aProjection);
		/// [[BPR-1589]]. My role, the sections' facts and their memory.
		void present_status(bool aSamePullRequest);
		void present_attention();
		void present_my_review();
		void present_people();
		void show_value(overview_value aValue, bool aShown);
		void remember_sections();
		void hide_status();
		/// [[BPR-1089]]. The rows in the order the sort or the file order gives.
		[[nodiscard]] auto ordered_rows() const -> std::vector<application::overview_file_row>;
		void               apply_rows();
		void               dispatch_feedback_request();
		void               present_feedback_controls();
		void               refresh_feedback_icons();
		void               drill(application::overview_feedback_row_kind aRow, const std::string& aPath, const core::feedback_count_target& aTarget);
		void               open_comments_menu(const QModelIndex& aIndex, const QPoint& aGlobal);
		/// \brief Opens the file whose row holds `aIndex`, if it is a file.
		void               open_file_at(const QModelIndex& aIndex);
		[[nodiscard]] auto chip_at(const QPoint& aViewportPosition) const -> std::optional<std::pair<QModelIndex, overview_comment_chip>>;
		/// [[BPR-1089]]. A row's chips, laid out in its Comments cell.
		[[nodiscard]] auto row_chips(const QModelIndex& aRow) const -> std::vector<overview_comment_chip>;
		/// The focused chip's place among the current row's chips.
		[[nodiscard]] auto focused_chip_position(const std::vector<overview_comment_chip>& aChips) const -> std::optional<std::size_t>;
		void               focus_chip(const QModelIndex& aRow, const overview_comment_chip* aChip);
		/// What a click on a chip does, and Enter or Space on a focused one.
		void activate_chip(const QModelIndex& aCell, const overview_comment_chip& aChip);
		/// The keyboard on the files: the tree's own keys, and Tab onto chips.
		[[nodiscard]] auto handle_files_key(const QKeyEvent& aKey) -> bool;
		/// [[BPR-1677]]. Makes the files' top or bottom shown row current and
		/// scrolls to that end; false when no row is shown.
		auto move_files_to(document_edge aEdge) -> bool;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

		std::optional<application::pull_request_overview_projection> Projection;
		void                                                         present_checks(bool aSamePullRequest);
		void                                                         present_selected_check();
		drawer_section_header*                                       ChecksDisclosure      = nullptr;
		QLabel*                                                      MergeState            = nullptr;
		QLabel*                                                      MergeConditions       = nullptr;
		QVBoxLayout*                                                 Requests              = nullptr;
		QLabel*                                                      RequestsNote          = nullptr;
		QLabel*                                                      RequirementDetails    = nullptr;
		QToolButton*                                                 RequirementDisclosure = nullptr;
		QWidget*                                                     CheckDetails          = nullptr;
		QLabel*                                                      ChecksNote            = nullptr;
		QTreeWidget*                                                 CheckList             = nullptr;
		QLabel*                                                      CheckHeading          = nullptr;
		QPlainTextEdit*                                              CheckDescription      = nullptr;
		QPushButton*                                                 CheckOpen             = nullptr;
		QHash<QString, QTreeWidgetItem*>                             CheckItems;
		std::function<void(const std::string&, const std::string&)>  CheckLinkDispatch;
		overview_colours                                             Colours;
		overview_path_dispatch                                       FileDispatch;
		overview_path_dispatch                                       CachedSourceDispatch;
		bool                                                         Cached = false;
		std::string                                                  CachedBase, CachedHead;
		QPlainTextEdit*                                              CachedSource     = nullptr;
		QPushButton*                                                 CachedSourceBack = nullptr;
		QHBoxLayout*                                                 TechnicalRow     = nullptr;
		QLabel*                                                      Title            = nullptr;
		QLabel*                                                      Summary          = nullptr;
		overview_summary_splitter*                                   SummarySplit     = nullptr;
		QWidget*                                                     SummaryPane      = nullptr;
		QWidget*                                                     FilesPane        = nullptr;
		overview_description_view*                                   Body             = nullptr;
		QWidget*                                                     Header           = nullptr;
		QWidget*                                                     Side             = nullptr;
		QLabel*                                                      EmptyBody        = nullptr;
		/// [[BPR-1774]]. The description's edit controls and editor.
		QWidget*                                              DescriptionCorner = nullptr;
		QToolButton*                                          DescriptionEdit   = nullptr;
		QPushButton*                                          DescriptionAdd    = nullptr;
		QWidget*                                              DescriptionPanel  = nullptr;
		scoped_prose_editor*                                  DescriptionText   = nullptr;
		QLabel*                                               DescriptionStatus = nullptr;
		QPushButton*                                          DescriptionNewer  = nullptr;
		QPushButton*                                          DescriptionCancel = nullptr;
		QPushButton*                                          DescriptionSave   = nullptr;
		std::function<void(const overview_description_save&)> DescriptionSaveDispatch;
		std::string                                           DescriptionOriginal;
		std::optional<std::string>                            DescriptionNewerText;
		bool                                                  DescriptionEditable  = false;
		bool                                                  DescriptionEditing   = false;
		bool                                                  DescriptionSaving    = false;
		QToolButton*                                          FullView             = nullptr;
		QLabel*                                               CachedThreadsHeading = nullptr;
		QPlainTextEdit*                                       CachedThreads        = nullptr;
		QPlainTextEdit*                                       CachedDiscussion     = nullptr;
		QLabel*                                               FilesHeading         = nullptr;
		QTreeView*                                            Files                = nullptr;
		QToolButton*                                          CollapseFolders      = nullptr;
		QToolButton*                                          ExpandFolders        = nullptr;
		overview_file_model*                                  Model                = nullptr;
		QVBoxLayout*                                          Participants         = nullptr;
		QLabel*                                               SinceHeading         = nullptr;
		QLabel*                                               Values[overview_value_count]{ };
		overview_status_splitter*                             StatusSplit  = nullptr;
		overview_status_drawer*                               StatusDrawer = nullptr;
		/// [[BPR-1589]]. The Status drawer's content.
		struct status_section_parts
		{
			drawer_section_header* Header  = nullptr;
			QWidget*               Frame   = nullptr;
			QWidget*               Details = nullptr;
		};
		status_section_parts       Sections[overview_status_section_count]{ };
		QLabel*                    StatusNote     = nullptr;
		overview_role_line*        RoleRow        = nullptr;
		QVBoxLayout*               AttentionList  = nullptr;
		drawer_meter*              ReadMeter      = nullptr;
		drawer_meter*              ChecksMeter    = nullptr;
		QWidget*                   NewThreadsRow  = nullptr;
		QLabel*                    NewThreadsText = nullptr;
		QToolButton*               NewThreadsGo   = nullptr;
		std::size_t                NewThreads     = 0U;
		std::function<void()>      NewThreadsDispatch;
		QWidget*                   NewConversationRow  = nullptr;
		QLabel*                    NewConversationText = nullptr;
		QToolButton*               NewConversationGo   = nullptr;
		std::optional<std::size_t> NewConversation;
		std::function<void()>      NewConversationDispatch;
		overview_faces*            Faces         = nullptr;
		QLabel*                    PeopleSummary = nullptr;
		/// Which sections are open, by pull request, for this window.
		QHash<QString, std::uint8_t> SectionMemory;
		QString                      SectionKey;
		// [[BPR-1089]]. The Comments column's view state and controls.
		core::feedback_label_columns                          Columns{ };
		core::feedback_resolution_mode                        DefaultMode = core::default_feedback_resolution_mode;
		core::feedback_resolution_mode                        Mode        = core::default_feedback_resolution_mode;
		bool                                                  ModeChosen  = false;
		std::optional<core::feedback_count_target>            Sort;
		std::optional<application::overview_feedback_request> AskedFeedback;
		overview_feedback_dispatch                            FeedbackDispatch;
		overview_drill_down_dispatch                          DrillDownDispatch;
		QToolButton*                                          OpenCounts    = nullptr;
		QToolButton*                                          AllCounts     = nullptr;
		QToolButton*                                          Uncertain     = nullptr;
		QToolButton*                                          LabelKey      = nullptr;
		QWidget*                                              LabelKeyPopup = nullptr;
		overview_row_delegate*                                RowDelegate   = nullptr;
		/// [[BPR-1363]]. The Files table's widths, from the whole tree.
		steady_columns*                    FileColumns = nullptr;
		std::optional<overview_chip_focus> ChipFocus;
		/// [[BPR-1576]]. A first read is under way: nothing accepted yet.
		bool Loading = false;
		/// [[BPR-1576]]. The read's provisional metadata is shown.
		bool                   Observed      = false;
		bool                   Observing     = false;
		std::size_t            ReportedFiles = 0U;
		QString                ReadWaiting;
		overview_pending_veils Veils;

		/// RFC-BPR-0089. The chip above the files, and the gestures it shares
		/// with the Commits tab.
		commit_filter_chip*                                            CommitChip = nullptr;
		std::function<void(const application::commit_filter_gesture&)> CommitFilterDispatch;
		/// RFC-BPR-0089 M2. The generated row, the last filter projection the
		/// file menus read their gestures from, and where gestures go.
		generated_files_row*                                            GeneratedRow = nullptr;
		std::optional<application::commit_filter_projection>            FilterProjection;
		std::function<void(const application::generated_file_gesture&)> GeneratedDispatch;
	};
} // namespace bpr::qt

#endif
