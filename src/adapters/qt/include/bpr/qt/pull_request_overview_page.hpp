#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/pull_request_overview.hpp"

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QModelIndex>
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
#include <vector>

class QAction;
class QHBoxLayout;
class QLabel;
class QPaintEvent;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QTreeView;
class QVBoxLayout;

namespace bpr::qt {
	class appearance_profile;
	class overview_description_view;
	class markdown_body_view;

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
	};

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
			threads_role,
			unresolved_role,
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
		/// \brief The index of a file row, by path.
		[[nodiscard]] auto index_for_path(const QString& aPath) const -> QModelIndex;
		[[nodiscard]] auto row_at(const QModelIndex& aIndex) const
			-> const application::overview_file_row*;

	private:
		struct node
		{
			QString                                       Path;
			std::optional<application::overview_file_row> File;
			node*                                         Parent = nullptr;
			std::vector<std::unique_ptr<node>>            Children;
			std::size_t                                   FileCount = 0U;
			int                                           Row       = 0;
		};

		static void        build_tree(node& aRoot, const std::vector<application::overview_file_row>& aRows);
		static void        update_rows(node& aParent, int aFirst, int aLast);
		void               reconcile(node& aParent, node& aDesired);
		void               update_file(node& aNode, const application::overview_file_row& aRow);
		void               rebuild_lookup(const std::vector<application::overview_file_row>& aRows);
		[[nodiscard]] auto node_at(const QModelIndex& aIndex) const -> const node*;
		[[nodiscard]] auto index_of(const node& aNode, int aColumn = 0) const -> QModelIndex;

		node                  Root;
		QHash<QString, node*> FileNodes;
		std::vector<node*>    FileOrder;
		std::size_t           FolderCount = 0U;
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

		/// \brief Where a folder cell draws its disclosure mark, in view coordinates.
		[[nodiscard]] static auto disclosure_rect(const QStyleOptionViewItem& aOption) -> QRect;

	private:
		[[nodiscard]] static auto disclosure_width(const QStyleOptionViewItem& aOption) -> int;
		void                      paint_folder(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const;

		const overview_colours& Colours;
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
	using overview_entry_dispatch
		= std::function<void(application::overview_entry_kind, const QString&)>;

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

		/// \brief Continue review or Start review.
		void set_entry_dispatch(overview_entry_dispatch aDispatch);
		/// \brief Presents the window-owned private tour action from Overview.
		void set_prepare_tour_action(QAction* aAction);
		/// \brief A file row activated: open it in Read.
		void set_file_dispatch(overview_path_dispatch aDispatch);

		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto summary_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_view() const noexcept -> markdown_body_view*;
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
		[[nodiscard]] auto since_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto value_label(overview_value aValue) const noexcept -> QLabel*;
		[[nodiscard]] auto entry_button() const noexcept -> QPushButton*;
		/// \brief No minimum: a hidden page must not widen the centre's stack.
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		void set_description_expanded(bool aExpanded);
		void show_cached_source(bool aShow);
		void set_value(overview_value aValue, const QString& aText, bool aHighlight);
		void present_files_heading(const application::pull_request_overview_projection& aProjection);
		/// \brief [[BPR-1269]]. Shows the folder buttons beside a shown heading
		/// while there is a folder, and repaints their glyphs in the palette.
		void update_folder_controls();
		void refresh_folder_icons();
		void present_entry(const application::pull_request_overview_projection& aProjection);
		void rebuild_participants(const std::vector<application::overview_participant>& aParticipants);

		std::optional<application::pull_request_overview_projection> Projection;
		overview_colours                                             Colours;
		overview_entry_dispatch                                      EntryDispatch;
		overview_path_dispatch                                       FileDispatch;
		overview_path_dispatch                                       CachedSourceDispatch;
		bool                                                         Cached = false;
		std::string                                                  CachedBase, CachedHead;
		QPlainTextEdit*                                              CachedSource         = nullptr;
		QPushButton*                                                 CachedSourceBack     = nullptr;
		QHBoxLayout*                                                 TechnicalRow         = nullptr;
		QLabel*                                                      Title                = nullptr;
		QLabel*                                                      Summary              = nullptr;
		overview_description_view*                                   Body                 = nullptr;
		QWidget*                                                     Header               = nullptr;
		QWidget*                                                     Side                 = nullptr;
		QLabel*                                                      EmptyBody            = nullptr;
		QPushButton*                                                 ReadDescription      = nullptr;
		QPushButton*                                                 ReturnToFiles        = nullptr;
		QLabel*                                                      CachedThreadsHeading = nullptr;
		QPlainTextEdit*                                              CachedThreads        = nullptr;
		QPlainTextEdit*                                              CachedDiscussion     = nullptr;
		QLabel*                                                      FilesHeading         = nullptr;
		QTreeView*                                                   Files                = nullptr;
		QToolButton*                                                 CollapseFolders      = nullptr;
		QToolButton*                                                 ExpandFolders        = nullptr;
		overview_file_model*                                         Model                = nullptr;
		QVBoxLayout*                                                 Participants         = nullptr;
		QLabel*                                                      SinceHeading         = nullptr;
		QLabel*                                                      Values[overview_value_count]{ };
		QPushButton*                                                 Entry = nullptr;
	};
} // namespace bpr::qt

#endif
