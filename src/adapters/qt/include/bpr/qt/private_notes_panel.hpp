#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_NOTES_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_NOTES_PANEL_HPP

#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/note_placement.hpp"
#include <QColor>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QAbstractButton;
class QAction;
class QLabel;
class QPlainTextEdit;
class QToolButton;

namespace bpr::qt {
	class drawer_card;
	class drawer_empty_state;

	/// \brief Exact file comparison offered for a new private note.
	///
	/// \details [[BPR-1592]]. A note written on a selected line also names
	/// that line, as `core::note_line_anchor_at` records it.
	struct private_note_target
	{
		core::file_identity                   File;
		core::file_comparison_digest          Comparison;
		std::optional<core::note_line_anchor> Line                                          = std::nullopt;
		auto                                  operator<=>(const private_note_target&) const = default;
	};
	/// \brief One accepted ledger record, with host-supplied presentation labels.
	struct private_note_item
	{
		std::uint64_t                      Id       = 0U;
		std::uint64_t                      Revision = 0U;
		std::optional<private_note_target> Attachment;
		std::string                        Label;
		std::string                        Text;
		auto                               operator<=>(const private_note_item&) const = default;
	};
	struct private_notes_projection
	{
		std::uint64_t                      Scope     = 0U;
		bool                               Available = false;
		std::optional<private_note_target> CurrentFile;
		std::string                        CurrentFileLabel;
		std::vector<private_note_item>     Notes;
	};
	enum class private_note_action : std::uint8_t { discard,
													save,
													erase,
													promote };
	/// \brief One gesture retains its original review scope and durable revision.
	struct private_note_intent
	{
		std::uint64_t                      Scope    = 0U;
		private_note_action                Action   = private_note_action::save;
		std::uint64_t                      Id       = 0U;
		std::uint64_t                      Revision = 0U;
		std::optional<private_note_target> Attachment;
		std::string                        Text;
	};
	using private_note_dispatch = std::function<void(private_note_intent)>;

	/// \brief [[BPR-1592]]. Where a zero-based line of the shown comparison
	/// is, in the panel's coordinates; none when it is not drawn.
	using private_note_line_locator = std::function<std::optional<int>(core::diff_side, std::uint32_t)>;

	/// \brief [[BPR-1592]]. A selected line of the shown comparison.
	struct private_note_line
	{
		core::diff_side Side                                        = core::diff_side::right;
		std::uint32_t   Line                                        = 0U;
		auto            operator<=>(const private_note_line&) const = default;
	};

	/// \brief [[BPR-1592]]. A noted line's mark in the code's gutter.
	struct private_note_mark
	{
		std::uint32_t Line = 0U;
		/// The open note's line: washed in the writing blue.
		bool Open = false;
		/// "Line 433: your private note".
		QString Description;
		auto    operator==(const private_note_mark&) const -> bool = default;
	};

	/// \brief [[BPR-1592]]. Notes, RFC-BPR-0085 D3: private notes hang at
	/// their lines beside the code and open in place, as threads do, drawn
	/// dashed with a lock; notes on the pull request, the whole file and other
	/// files wait in the edge chips. Presents ledger values and emits
	/// gestures; owns no effects. `core::place_note` decides where each note
	/// shows.
	class private_notes_panel final : public QWidget
	{
	public:
		explicit private_notes_panel(QWidget* aParent = nullptr);
		void set_dispatch(private_note_dispatch aDispatch);
		void present(private_notes_projection aProjection);
		/// \brief Whether Comment's current target can take a promoted note
		/// that has no line of its own, and why not.
		void set_promotion_target(bool aAvailable, const QString& aReason);
		void acknowledge(std::uint64_t aSavedId);
		void refuse(const QString& aReason);

		/// \brief Where the shown comparison's lines are.
		void set_line_locator(private_note_line_locator aLocator);
		/// \brief The comparison shown, or none.
		void set_comparison_source(std::function<const core::file_alignment*()> aSource);
		/// \brief The line selected in the code, where N writes a note.
		void set_selected_line_source(std::function<std::optional<private_note_line>()> aSource);
		/// \brief Shows a line of the shown comparison in the code.
		void set_line_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch);
		/// \brief Goes to another file's note: its file, then its line.
		void set_elsewhere_dispatch(std::function<void(const private_note_item&)> aDispatch);
		/// \brief The gutter marks changed.
		void set_marks_observer(std::function<void()> aObserver);
		/// \brief The notes' colours: what is new, and where you read.
		void set_colours(const QColor& aNew, const QColor& aRoute);
		/// \brief The code moved, or another comparison is shown.
		void refresh_geometry();

		/// \brief Opens one note in place, to read and write.
		void open_note(std::uint64_t aId);
		/// \brief Starts a new note on the selected line (N).
		void start_line_note();
		/// \brief Starts a new note on the whole pull request.
		void start_pull_request_note();
		/// \brief Closes the open note; changes not saved are kept until Esc
		/// asks again or Discard changes.
		void close_note();
		/// \brief Shows the notes on the pull request, the whole file and
		/// other files (true), or the notes at their lines (false).
		void               show_elsewhere(bool aShown);
		[[nodiscard]] auto elsewhere_shown() const noexcept -> bool;
		[[nodiscard]] auto open_note_id() const noexcept -> std::uint64_t;
		/// \brief The notes as last presented, newest first as the host sent them.
		[[nodiscard]] auto notes() const noexcept -> const std::vector<private_note_item>&;
		[[nodiscard]] auto writing() const noexcept -> bool;
		[[nodiscard]] auto placement(std::uint64_t aId) const -> core::note_placement;
		[[nodiscard]] auto line_marks(core::diff_side aSide) const -> std::vector<private_note_mark>;
		/// \brief The note at a line of the shown comparison, if one is placed there.
		[[nodiscard]] auto note_on_line(core::diff_side aSide, std::uint32_t aLine) const -> std::optional<std::uint64_t>;
		[[nodiscard]] auto note_rect(std::uint64_t aId) const -> QRect;
		[[nodiscard]] auto edge_chips() const -> std::vector<QAbstractButton*>;
		[[nodiscard]] auto open_card() const noexcept -> QWidget*;
		[[nodiscard]] auto empty_state() const noexcept -> drawer_empty_state*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief What the panel says while no note is open: why a gesture
		/// did nothing.
		[[nodiscard]] auto notice_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto new_line_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_review_action() const noexcept -> QAction*;
		[[nodiscard]] auto elsewhere_action() const noexcept -> QAction*;
		[[nodiscard]] auto save_action() const noexcept -> QAction*;
		[[nodiscard]] auto erase_action() const noexcept -> QAction*;
		[[nodiscard]] auto promote_action() const noexcept -> QAction*;
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		[[nodiscard]] auto close_action() const noexcept -> QAction*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void wheelEvent(QWheelEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		/// One note as drawn: where, how tall, and what it says of itself.
		struct laid_note
		{
			std::uint64_t        Id = 0U;
			core::note_placement Place;
			QRect                Rect;
			int                  TargetY = 0;
			bool                 Shown   = false;
			QString              Where;
			QString              Sentence;
		};
		void               begin_note(std::optional<private_note_target> aTarget, std::optional<private_note_line> aLine);
		void               load_editor();
		void               refresh_actions();
		void               emit_intent(private_note_action aAction);
		void               relayout();
		void               place_open_card(int aTop, std::optional<int> aNotch);
		void               build_open_card();
		void               refresh_open_card();
		void               set_marks(std::vector<private_note_mark> aLeft, std::vector<private_note_mark> aRight);
		[[nodiscard]] auto can_promote() const -> bool;
		[[nodiscard]] auto where_of(const private_note_item& aNote, const core::note_placement& aPlace) const -> QString;
		[[nodiscard]] auto sentence_of(const core::note_placement& aPlace) const -> QString;
		[[nodiscard]] auto closed_height(const private_note_item& aNote, const QString& aWhere, const QString& aSentence, int aWidth) const -> int;
		[[nodiscard]] auto item(std::uint64_t aId) const -> const private_note_item*;
		[[nodiscard]] auto laid(std::uint64_t aId) const -> const laid_note*;
		[[nodiscard]] auto hit(QPoint aPoint) const -> std::optional<std::uint64_t>;
		[[nodiscard]] auto order() const -> std::vector<std::uint64_t>;

		private_note_dispatch                               Dispatch;
		private_note_line_locator                           Locator;
		std::function<const core::file_alignment*()>        Comparison;
		std::function<std::optional<private_note_line>()>   SelectedLine;
		std::function<void(core::diff_side, std::uint32_t)> LineDispatch;
		std::function<void(const private_note_item&)>       ElsewhereDispatch;
		std::function<void()>                               MarksObserver;
		private_notes_projection                            Shown;
		std::optional<private_note_item>                    Editing;
		std::optional<private_note_target>                  NewTarget;
		std::optional<private_note_line>                    NewLine;
		std::uint64_t                                       Selected        = 0U;
		std::uint64_t                                       Current         = 0U;
		std::uint64_t                                       Hovered         = 0U;
		bool                                                Creating        = false;
		bool                                                Open            = false;
		bool                                                Dirty           = false;
		bool                                                Applying        = false;
		bool                                                Elsewhere       = false;
		bool                                                EscArmed        = false;
		bool                                                TargetAvailable = false;
		QString                                             TargetReason;
		QColor                                              New;
		QColor                                              Route;
		int                                                 ListScroll = 0;
		int                                                 ListBottom = 0;
		std::vector<laid_note>                              Laid;
		std::vector<private_note_mark>                      LeftMarks;
		std::vector<private_note_mark>                      RightMarks;
		std::vector<QAbstractButton*>                       Chips;
		QWidget*                                            ListHeader      = nullptr;
		QAbstractButton*                                    BackChip        = nullptr;
		QToolButton*                                        NewReviewButton = nullptr;
		drawer_card*                                        Card            = nullptr;
		QLabel*                                             CardWhere       = nullptr;
		QLabel*                                             CardState       = nullptr;
		QLabel*                                             Status          = nullptr;
		QLabel*                                             Notice          = nullptr;
		QLabel*                                             OnlyYou         = nullptr;
		QPlainTextEdit*                                     Body            = nullptr;
		QAbstractButton*                                    SaveButton      = nullptr;
		QAbstractButton*                                    DraftButton     = nullptr;
		QAbstractButton*                                    DeleteButton    = nullptr;
		QAbstractButton*                                    CloseButton     = nullptr;
		QAbstractButton*                                    DiscardButton   = nullptr;
		drawer_empty_state*                                 Empty           = nullptr;
		QAction*                                            NewLineAction   = nullptr;
		QAction*                                            NewReview       = nullptr;
		QAction*                                            ElsewhereAction = nullptr;
		QAction*                                            Save            = nullptr;
		QAction*                                            Erase           = nullptr;
		QAction*                                            Promote         = nullptr;
		QAction*                                            Discard         = nullptr;
		QAction*                                            Close           = nullptr;
	};
}
#endif
