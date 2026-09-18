#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_OVERVIEW_PAGE_HPP

#include "bpr/application/pull_request_overview.hpp"

#include <QAbstractItemModel>
#include <QColor>
#include <QModelIndex>
#include <QString>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QHBoxLayout;
class QLabel;
class QPaintEvent;
class QPlainTextEdit;
class QPushButton;
class QTreeView;
class QVBoxLayout;

namespace bpr::qt {
	class appearance_profile;

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
	/// \details A two-level tree: one row per folder, its files beneath it,
	/// in the order the projection already sorted them. Columns follow the
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
		struct folder
		{
			QString                                     Name;
			std::vector<application::overview_file_row> Files;
		};

		std::vector<folder> Folders;
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

	private:
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
	/// leaves through a semantic dispatch: the window turns it into the core's
	/// file selection and the entry or file-row modality.
	class pull_request_overview_page final : public QWidget
	{
	public:
		explicit pull_request_overview_page(QWidget* aParent = nullptr);

		void               present(const application::pull_request_overview_projection& aProjection);
		void               clear();
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
		/// \brief A file row activated: open it in Read.
		void set_file_dispatch(overview_path_dispatch aDispatch);

		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto summary_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_view() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto files_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto files_view() const noexcept -> QTreeView*;
		[[nodiscard]] auto file_model() const noexcept -> overview_file_model*;
		[[nodiscard]] auto participant_rows() const -> std::vector<overview_participant_row*>;
		[[nodiscard]] auto since_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto value_label(overview_value aValue) const noexcept -> QLabel*;
		[[nodiscard]] auto entry_button() const noexcept -> QPushButton*;
		/// \brief No minimum: a hidden page must not widen the centre's stack.
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		void set_value(overview_value aValue, const QString& aText, bool aHighlight);
		void rebuild_participants(const std::vector<application::overview_participant>& aParticipants);

		std::optional<application::pull_request_overview_projection> Projection;
		overview_colours                                             Colours;
		overview_entry_dispatch                                      EntryDispatch;
		overview_path_dispatch                                       FileDispatch;
		QHBoxLayout*                                                 TechnicalRow = nullptr;
		QLabel*                                                      Title        = nullptr;
		QLabel*                                                      Summary      = nullptr;
		QPlainTextEdit*                                              Body         = nullptr;
		QLabel*                                                      FilesHeading = nullptr;
		QTreeView*                                                   Files        = nullptr;
		overview_file_model*                                         Model        = nullptr;
		QVBoxLayout*                                                 Participants = nullptr;
		QLabel*                                                      SinceHeading = nullptr;
		QLabel*                                                      Values[overview_value_count]{ };
		QPushButton*                                                 Entry = nullptr;
	};
} // namespace bpr::qt

#endif
