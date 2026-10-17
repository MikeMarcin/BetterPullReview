#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/core/reference_page.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QFrame>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

/// \file
/// \brief [[BPR-1083]]. The selected authoring controls of a comment editor:
/// the label chip and its menu and the decoration toggles (owner, BPR-1079
/// authoring A and v2; RFC-BPR-0048 R09, R10). The leading `:xx:` codes and
/// their completion were removed by [[BPR-1565]] (owner, 2026-10-16): a code
/// typed now stays the text it is.
/// \details Every change is one edit of the editor's own text, decided by
/// `core::decide_feedback_composition_edit` against the exact text on screen
/// and applied as one undoable step. The editor's existing typed-edit path
/// then carries the whole body to the draft owner, whose revision check
/// still guards it, so a label chosen here and one typed by hand are the same
/// draft. Nothing here stores a label: the text is the only authority, and
/// the controls are recomputed from it.
namespace bpr::qt {
	/// \brief The chip's reading of one body while it is being written.
	struct feedback_composition_state
	{
		std::optional<core::feedback_label> Label;
		/// Which of blocking, non-blocking and if-minor are written.
		bool Blocking    = false;
		bool NonBlocking = false;
		bool IfMinor     = false;
		/// The custom decorations written, as written, each once ([[BPR-1083]]
		/// v004: any decoration is accepted and kept exactly as authored).
		QStringList Custom;
		/// What the chip's mark shows of them.
		feedback_mark_emphasis Emphasis;
		/// A header with no subject yet (`issue: `). The recognizer reads it as
		/// `malformed` (`empty_subject`); the chip still shows its label, and
		/// says it is waiting for a subject (BPR-1083's settlement of the
		/// BPR-1082 open point).
		bool AwaitingSubject = false;
		/// The `QString` length of the prefix to highlight: the header through
		/// the separator before the subject. Zero when plain.
		qsizetype PrefixLength = 0;
	};
	/// \brief Reads one body the way the chip shows it.
	[[nodiscard]] auto feedback_composition_state_of(const QString& aBody) -> feedback_composition_state;

	/// \brief Directly observable control activity.
	/// \details [[BPR-1566]]. `References` counts the reference pages asked
	/// for from the menu.
	struct feedback_label_control_statistics
	{
		std::uint64_t Edits                                                       = 0U;
		std::uint64_t Refusals                                                    = 0U;
		std::uint64_t MenuOpens                                                   = 0U;
		std::uint64_t References                                                  = 0U;
		auto          operator<=>(const feedback_label_control_statistics&) const = default;
	};

	class feedback_label_menu;

	/// \brief The label tool row of one comment editor.
	class feedback_label_controls final : public QWidget
	{
	public:
		/// \brief Controls for `aBody`, whose text they edit.
		explicit feedback_label_controls(QPlainTextEdit* aBody, QWidget* aParent = nullptr);
		~feedback_label_controls() override;

		/// \brief The tints of the theme on screen and the mark style.
		void               set_theme(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto theme() const noexcept -> const feedback_mark_theme&;
		/// \brief Whether the blocking, non-blocking and if-minor toggles sit
		/// beside the chip (the default). Off, they stay in the chip's menu
		/// under "Adds" and the chip's mark still shows them: the drawer's
		/// in-place reply, which is too narrow for the row ([[BPR-1290]]).
		void set_decoration_toggles(bool aShown);
		/// \brief Where a refused edit's reason is said.
		void set_status_sink(std::function<void(const QString&)> aSink);
		/// \brief [[BPR-1566]]. Where the menu's About Conventional Comments
		/// goes: the window, which hands the named page to the application.
		/// \details Unset, the menu does not offer the item. The controls
		/// never hold an address; they name the page.
		void set_reference_dispatch(std::function<void(core::reference_page)> aDispatch);
		/// \brief Whether a reference dispatch is set.
		[[nodiscard]] auto reference_offered() const noexcept -> bool;
		/// \brief [[BPR-1566]]. Asks for one reference page. It edits nothing.
		void open_reference(core::reference_page aPage);

		/// \brief Applies one header edit to the text on screen as one undo step.
		/// \returns False when the core refused it; the text is unchanged.
		auto apply(const core::feedback_header_edit& aEdit) -> bool;
		/// \brief Re-reads the body: chip, toggles, highlight and enablement.
		void refresh();
		/// \brief Opens the label menu under the chip (the chip's click, or ⌘L
		/// while the body has focus).
		void open_menu();
		/// \brief Gives focus back to the body with the caret after the prefix.
		void return_focus();

		[[nodiscard]] auto state() const noexcept -> const feedback_composition_state&;
		[[nodiscard]] auto chip() const noexcept -> QToolButton*;
		[[nodiscard]] auto blocking_toggle() const noexcept -> QToolButton*;
		[[nodiscard]] auto non_blocking_toggle() const noexcept -> QToolButton*;
		[[nodiscard]] auto if_minor_toggle() const noexcept -> QToolButton*;
		[[nodiscard]] auto menu() const noexcept -> feedback_label_menu*;
		[[nodiscard]] auto last_refusal() const noexcept -> std::optional<core::feedback_edit_refusal>;
		[[nodiscard]] auto statistics() const noexcept -> feedback_label_control_statistics;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void on_text_changed();
		void toggle_decoration(const QString& aToken, bool aWritten);
		void refresh_icons();
		void place_caret_after_prefix();

		QPlainTextEdit*                            Body        = nullptr;
		QToolButton*                               Chip        = nullptr;
		QToolButton*                               Blocking    = nullptr;
		QToolButton*                               NonBlocking = nullptr;
		QToolButton*                               IfMinor     = nullptr;
		feedback_label_menu*                       Menu        = nullptr;
		feedback_mark_theme                        Theme;
		bool                                       Toggles = true;
		bool                                       Editing = false;
		feedback_composition_state                 State;
		std::optional<core::feedback_edit_refusal> Refusal;
		std::function<void(const QString&)>        StatusSink;
		std::function<void(core::reference_page)>  ReferenceDispatch;
		feedback_label_control_statistics          Statistics;
	};

	/// \brief The label menu: Plain comment (⌘0), the 3×4 grid of the twelve
	/// marks with their names, and the decorations under "Adds".
	/// \details The decorations combine ([[BPR-1083]] v004, after the owner's
	/// `decorations-v003`): blocking and non-blocking are one weight, so each
	/// takes the other out, and if-minor goes with either. Custom decorations
	/// written in the comment show as toggles too, and a field takes a new
	/// one, as in `security`, which is written as typed.
	/// \details A popup, so ⌘0 reaches it before Actual Size's window shortcut
	/// and is a key of the open menu only (contract, derived). Arrow keys move through
	/// the grid; Return or Space chooses; Esc closes. Focus returns to the body
	/// with the caret after the prefix when it closes.
	/// \details [[BPR-1566]]. The last item, under a rule, is About
	/// Conventional Comments: where the labels come from, opened in the
	/// browser. It is not a label choice, edits nothing, and closes the menu.
	/// Arrow keys reach it below the custom decoration field, passing rows
	/// that are disabled while the comment is plain.
	class feedback_label_menu final : public QFrame
	{
	public:
		explicit feedback_label_menu(feedback_label_controls& aControls);

		void               present(const feedback_composition_state& aState, const feedback_mark_theme& aTheme);
		[[nodiscard]] auto plain_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto label_button(core::feedback_label aLabel) const noexcept -> QToolButton*;
		[[nodiscard]] auto blocking_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto non_blocking_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto if_minor_button() const noexcept -> QToolButton*;
		/// \brief The toggles of the custom decorations written, in order.
		[[nodiscard]] auto custom_buttons() const -> std::vector<QToolButton*>;
		/// \brief The field that takes a new custom decoration, and its Add.
		[[nodiscard]] auto custom_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto custom_add_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1566]]. About Conventional Comments, the menu's last item.
		[[nodiscard]] auto about_button() const noexcept -> QToolButton*;

	protected:
		auto event(QEvent* aEvent) -> bool override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;

	private:
		void choose(const core::feedback_header_edit& aEdit);
		void decorate(const QString& aToken, bool aWritten);
		void add_custom();
		void move_focus(int aRows, int aColumns);

		feedback_label_controls&                             Controls;
		QToolButton*                                         Plain = nullptr;
		std::array<QToolButton*, core::feedback_label_count> Labels{ };
		std::array<QToolButton*, 3U>                         Decorations{ };
		QWidget*                                             CustomRow = nullptr;
		std::vector<QToolButton*>                            Custom;
		QLineEdit*                                           CustomField = nullptr;
		QToolButton*                                         CustomAdd   = nullptr;
		QLabel*                                              Hint        = nullptr;
		QFrame*                                              AboutRule   = nullptr;
		QToolButton*                                         About       = nullptr;
	};

	/// \brief [[BPR-1083]]. A few marks drawn in the style and tints on
	/// screen, beside the Label marks setting, so a reviewer sees Icons or
	/// Letters before meeting them in a thread (owner, mark-style-v001: "it'd
	/// be nice to see a preview").
	/// \details Presentation only: it paints what `set_theme` hands it and
	/// dispatches nothing. The marks show the three weights and the if-minor
	/// corner on two of them. Each mark has its own tooltip, its name and
	/// catalog description (owner, `mark-preview-v002`: "There should be
	/// individual tooltips instead of one giant tooltip."), right below the
	/// mark (owner, `preview-tooltips-v003`: "The tooltip feels strangely far
	/// away.").
	class feedback_mark_preview final : public QWidget
	{
	public:
		/// \brief One mark of the preview.
		struct entry
		{
			core::feedback_label   Label;
			feedback_mark_emphasis Emphasis;
		};

		explicit feedback_mark_preview(QWidget* aParent = nullptr);

		void               set_theme(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto theme() const noexcept -> const feedback_mark_theme&;
		/// \brief The marks drawn, in order.
		[[nodiscard]] static auto entries() noexcept -> std::span<const entry>;
		/// \brief Where one entry's mark is drawn, in the widget.
		[[nodiscard]] auto entry_rect(std::size_t aIndex) const -> QRectF;
		/// \brief The entry whose mark is under a point, if any; the gaps
		/// between marks belong to none.
		[[nodiscard]] auto entry_at(QPointF aPoint) const -> std::optional<std::size_t>;
		/// \brief One entry's tooltip: its name, as in "issue (blocking)", and
		/// its catalog description.
		[[nodiscard]] static auto entry_tooltip(const entry& aEntry) -> QString;
		[[nodiscard]] auto        sizeHint() const -> QSize override;

	protected:
		auto event(QEvent* aEvent) -> bool override;
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void name_for_theme();

		feedback_mark_theme Theme;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP
