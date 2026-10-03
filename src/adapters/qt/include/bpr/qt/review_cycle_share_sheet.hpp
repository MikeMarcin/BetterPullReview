#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_SHEET_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_SHEET_HPP

#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/core/review_cycle_command.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QDialog>
#include <QFont>
#include <QPlainTextEdit>
#include <QPointer>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief The gestures the share sheet sends to its host.
	struct review_cycle_share_dispatch
	{
		/// Cancel, Done or Escape: the sheet closes and the draft stays as saved.
		std::function<void()>                         Back;
		std::function<void(core::cycle_compose_edit)> Edit;
		/// The composition chooses the file for Save as File and Save as Bundle.
		std::function<void(core::cycle_share_route)> Share;
		std::function<void(bool)>                    Replace;
		/// The larger view of one note's code, and closing it.
		std::function<void(core::cycle_feedback_item_id)> ShowCode;
		std::function<void()>                             CloseCode;
	};

	/// \brief A note's code, read-only: one row per excerpt row, its line
	/// number and a mark before the text (`•` a line the note is about, `-`
	/// a line a suggested change replaces, `+` one it puts in its place), in
	/// the code font, each kind on the review panes' row fill.
	class note_code_view final : public QPlainTextEdit
	{
	public:
		explicit note_code_view(QWidget* aParent = nullptr);

		/// \brief Shows `aExcerpt`; nothing changes when it is already shown.
		void show_excerpt(const core::cycle_note_excerpt& aExcerpt);
		void set_colours(const code_role_colors& aColours);
		/// \brief Fits the height to the rows, at most `aRows` of them.
		void fit_rows(int aRows);
		/// \brief The first row the note is about: its first noted or changed row.
		[[nodiscard]] auto anchor_row() const -> int;
		[[nodiscard]] auto excerpt() const noexcept -> const core::cycle_note_excerpt&;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void               paint_bands();
		[[nodiscard]] auto fitted_height(int aRows) const -> int;

		core::cycle_note_excerpt Shown;
		int                      FitRows = 0;
		code_role_colors         Colours{ };
		bool                     Empty = true;
	};

	/// \brief The larger view over the share sheet: one note's whole file, or
	/// the round's exact text, in a window-modal panel most of the window's
	/// size. Escape or Done closes it and the sheet is as it was.
	class review_cycle_share_viewer final : public QDialog
	{
	public:
		explicit review_cycle_share_viewer(QWidget* aParent);

		/// \brief One note's code; `aWhole` says whether `aExcerpt` is the whole
		/// file yet or still the sheet's short excerpt while the file is read.
		void show_code(const QString& aHeading, const QString& aDetail, const core::cycle_note_excerpt& aExcerpt, bool aWhole);
		/// \brief The round's exact text, as Copy and Save send it.
		void show_text(const QString& aHeading, const QString& aText, const QString& aFoot);
		void set_code_font(const QFont& aFont);
		void set_colours(const code_role_colors& aColours);
		/// \brief Previous note and Next note: offered when there is one.
		void set_steps(bool aPrevious, bool aNext);

		std::function<void(int)> Step;
		std::function<void()>    Closed;

		[[nodiscard]] auto heading() const noexcept -> QLabel*;
		[[nodiscard]] auto detail() const noexcept -> QLabel*;
		[[nodiscard]] auto code() const noexcept -> note_code_view*;
		[[nodiscard]] auto text() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto foot() const noexcept -> QLabel*;
		[[nodiscard]] auto previous_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto next_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto done_button() const noexcept -> QPushButton*;
		/// \brief Whether it shows a note's code (else the exact text).
		[[nodiscard]] auto showing_code() const noexcept -> bool;

	protected:
		void done(int aResult) override;
		void showEvent(QShowEvent* aEvent) override;

	private:
		QLabel*         Heading  = nullptr;
		QLabel*         Detail   = nullptr;
		note_code_view* Code     = nullptr;
		QPlainTextEdit* Text     = nullptr;
		QLabel*         Foot     = nullptr;
		QPushButton*    Previous = nullptr;
		QPushButton*    Next     = nullptr;
		QPushButton*    Done     = nullptr;
		bool            Centred  = false;
	};

	/// \brief Share notes (RFC-BPR-0061, BPR-1161): one round, shared from a
	/// sheet over the code (owner choice of direction A, 2026-10-02).
	///
	/// \details A window-modal dialog, attached as a sheet on macOS and placed
	/// under the review bar elsewhere, so the file the reviewer came from
	/// stays behind it. It says what a round is and what sharing does, asks
	/// two things (what to ask for, and an optional note for the whole round),
	/// lists the notes in the order they will be read, and offers one filled
	/// button, Copy round N, the default, with Save as File beside it;
	/// Read the exact text, Save as Bundle and any reachable optional route
	/// sit in its foot. A route policy refuses is shown refused, never hidden
	/// or replaced.
	///
	/// Each note is two lines by default: what it says first, where it is
	/// and what it is. Its disclosure (or Right and Left on it) shows the
	/// rest of its text and its code: the lines it is about and each
	/// suggested change in place, with two unchanged lines either side, cut
	/// at `core::cycle_excerpt_rows` rows. Show the whole file opens the
	/// larger view over the sheet: the note's whole file at the revision it
	/// names, each change in place, scrolled to the note, with Previous note
	/// and Next note. Expansion is the sheet's own state, kept per note across
	/// refreshes; Show all code opens or closes every note at once.
	///
	/// A note's check box includes it or leaves it out; its grip is dragged,
	/// or Option+Up and Option+Down (Alt elsewhere) move the focused note.
	/// After a share the sheet says what happened and what comes next, lists
	/// every delivery observation in its own words under what none of them
	/// means, and Done becomes the default. Return presses the default
	/// button; Escape (and Control-C on macOS, as in the code) is Cancel,
	/// which leaves the draft as saved. It decides nothing and performs
	/// nothing: gestures leave through `review_cycle_share_dispatch`.
	class review_cycle_share_sheet final : public QDialog
	{
	public:
		explicit review_cycle_share_sheet(QWidget* aParent);

		void set_dispatch(review_cycle_share_dispatch aDispatch);
		/// \brief Shows the host's share view; `aNotes` names the notes.
		void present(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes, bool aIdle);
		void set_code_font(const QFont& aFont);
		void set_code_colours(const code_role_colors& aColours);
		/// \brief Where the sheet hangs: the global point under the middle of
		/// the review bar, used where the window system does not attach it.
		void set_anchor(QPoint aGlobal);
		/// \brief Gives focus to the default button.
		void focus_first();

		/// \brief What the round note holds that the draft does not (BPR-1168):
		/// text entered but not yet saved, or an input method still composing.
		[[nodiscard]] auto draft_editor() const -> core::cycle_draft_editor;
		/// \brief Saves what the round note holds, as Return would; composing text is not saved.
		void save_draft_editor();

		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto intro() const noexcept -> QLabel*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		/// \brief The button for a route, when the sheet offers or refuses it.
		[[nodiscard]] auto route_button(core::cycle_share_route aRoute) const noexcept -> QPushButton*;
		/// \brief The words beside Copy or Save: when to use it, or why it is refused.
		[[nodiscard]] auto route_words(core::cycle_share_route aRoute) const noexcept -> QLabel*;
		[[nodiscard]] auto action_button(core::cycle_round_action aAction) const noexcept -> QPushButton*;
		[[nodiscard]] auto action_words() const noexcept -> QLabel*;
		[[nodiscard]] auto goal() const noexcept -> QLineEdit*;
		[[nodiscard]] auto notes_heading() const noexcept -> QLabel*;
		[[nodiscard]] auto notes_area() const noexcept -> QScrollArea*;
		[[nodiscard]] auto all_code_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto exact_text_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto deliveries() const noexcept -> QListWidget*;
		[[nodiscard]] auto meaning() const noexcept -> QLabel*;
		[[nodiscard]] auto invocation() const noexcept -> QLineEdit*;
		[[nodiscard]] auto replace_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto keep_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto replace_words() const noexcept -> QLabel*;
		[[nodiscard]] auto viewer() const noexcept -> review_cycle_share_viewer*;

		/// \brief One listed note's controls, in the order shown.
		struct note_row
		{
			core::cycle_feedback_item_id Item;
			bool                         Included = false;
			QWidget*                     Row      = nullptr;
			QLabel*                      Grip     = nullptr;
			QCheckBox*                   Include  = nullptr;
			QToolButton*                 Disclose = nullptr;
			QLabel*                      Number   = nullptr;
			QLabel*                      Heading  = nullptr;
			QLabel*                      Kind     = nullptr;
			QLabel*                      Where    = nullptr;
			QWidget*                     Detail   = nullptr;
			QLabel*                      Body     = nullptr;
			note_code_view*              Code     = nullptr;
			QLabel*                      Facts    = nullptr;
			QPushButton*                 Whole    = nullptr;
		};
		[[nodiscard]] auto rows() const noexcept -> const std::vector<note_row>&;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void done(int aResult) override;
		void showEvent(QShowEvent* aEvent) override;

	private:
		void rebuild_rows(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes);
		auto make_row(core::cycle_feedback_item_id aItem) -> note_row;
		void fill_row(note_row& aRow, std::size_t aNumber, const application::review_cycle_note_entry& aNote, const application::review_cycle_note_context* aContext);
		void set_expanded(note_row& aRow, bool aExpanded);
		void refresh_all_code();
		void reveal(core::cycle_feedback_item_id aItem);
		void rebuild_deliveries(const application::review_cycle_share_view& aShare);
		void refresh_defaults();
		void place();
		/// Moves the included note at `aFrom` to before the included note at
		/// `aTo` (`aTo` may be one past the last), one step at a time.
		void move_note(std::size_t aFrom, std::size_t aTo);
		/// The row holding `aWidget`, if any.
		[[nodiscard]] auto row_of(const QWidget* aWidget) -> note_row*;
		void               open_code(core::cycle_feedback_item_id aItem);
		void               refresh_viewer();
		void               step_viewer(int aDirection);
		void               open_text();

		review_cycle_share_dispatch                             Dispatch;
		application::review_cycle_share_view                    Shown;
		std::vector<application::review_cycle_note_entry>       ShownNotes;
		QLabel*                                                 Title = nullptr;
		QLabel*                                                 Intro = nullptr;
		std::array<QPushButton*, 3>                             Actions{ };
		QButtonGroup*                                           ActionGroup  = nullptr;
		QLabel*                                                 ActionWords  = nullptr;
		QLineEdit*                                              Goal         = nullptr;
		QLabel*                                                 NotesHeading = nullptr;
		QToolButton*                                            AllCode      = nullptr;
		QScrollArea*                                            NotesArea    = nullptr;
		QWidget*                                                NotesList    = nullptr;
		QVBoxLayout*                                            NotesLayout  = nullptr;
		QFrame*                                                 DropMark     = nullptr;
		std::vector<note_row>                                   Rows;
		std::set<core::cycle_feedback_item_id>                  Expanded;
		QLabel*                                                 Status      = nullptr;
		QWidget*                                                ReplaceRow  = nullptr;
		QLabel*                                                 ReplaceText = nullptr;
		QPushButton*                                            ReplaceIt   = nullptr;
		QPushButton*                                            KeepIt      = nullptr;
		QLineEdit*                                              Invocation  = nullptr;
		QWidget*                                                Shared      = nullptr;
		QListWidget*                                            Deliveries  = nullptr;
		QLabel*                                                 Meaning     = nullptr;
		std::array<QPushButton*, core::cycle_share_route_count> Routes{ };
		std::array<QLabel*, 2>                                  RouteWords{ };
		QPushButton*                                            ExactText = nullptr;
		QPushButton*                                            Cancel    = nullptr;
		review_cycle_share_viewer*                              Viewer    = nullptr;
		/// What had focus when the larger view opened; it gets focus back.
		QPointer<QWidget> ReturnFocus;
		/// The note the larger view shows, while it is open on a note.
		std::optional<core::cycle_feedback_item_id> Viewing;
		code_role_colors                            Colours{ };
		/// The review panes' code font, once the appearance gives it.
		std::optional<QFont>  CodeFont;
		std::optional<QPoint> Anchor;
		/// A grip being dragged: the included row it started on and where it was pressed.
		std::optional<std::size_t> Dragging;
		QPoint                     DragStart;
		bool                       DragMoved  = false;
		bool                       Idle       = true;
		bool                       Presenting = false;
		/// The goal the draft holds, as last shown.
		QString ShownGoal;
		/// An input method is composing in the round note: preedit text, not text yet.
		bool GoalComposing = false;
	};
}

#endif
