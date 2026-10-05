#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QFrame>
#include <QString>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

/// \file
/// \brief [[BPR-1083]]. The selected authoring controls of a comment editor:
/// the label chip and its menu, the decoration toggles and the leading
/// `:xx:` shortcodes (owner, BPR-1079 authoring A and v2; RFC-BPR-0048 R09,
/// R10).
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
		core::feedback_expectation          Expectation = core::feedback_expectation::unspecified;
		/// Which of blocking, non-blocking and if-minor are written.
		bool Blocking    = false;
		bool NonBlocking = false;
		bool IfMinor     = false;
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
	struct feedback_label_control_statistics
	{
		std::uint64_t Edits                                                       = 0U;
		std::uint64_t Refusals                                                    = 0U;
		std::uint64_t Shortcodes                                                  = 0U;
		std::uint64_t MenuOpens                                                   = 0U;
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
		/// \brief The label helper: completion and shortcode replacement. Off
		/// by default (owner, Settings v1); a label typed in full counts
		/// either way.
		void               set_helper(bool aEnabled);
		[[nodiscard]] auto helper() const noexcept -> bool;
		/// \brief Where a refused edit's reason is said.
		void set_status_sink(std::function<void(const QString&)> aSink);

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
		[[nodiscard]] auto completion() const noexcept -> QListWidget*;
		[[nodiscard]] auto completion_labels() const -> std::vector<core::feedback_label>;
		[[nodiscard]] auto last_refusal() const noexcept -> std::optional<core::feedback_edit_refusal>;
		[[nodiscard]] auto statistics() const noexcept -> feedback_label_control_statistics;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void on_contents_change(int aPosition, int aRemoved, int aAdded);
		void on_text_changed();
		void replace_code(qsizetype aCodeLength, core::feedback_label aLabel);
		void show_completion(const std::vector<core::feedback_label>& aLabels);
		void hide_completion();
		void take_completion();
		void toggle_decoration(core::feedback_expectation aExpectation, bool aWritten);
		void refresh_icons();
		void place_caret_after_prefix();

		QPlainTextEdit*                            Body        = nullptr;
		QToolButton*                               Chip        = nullptr;
		QToolButton*                               Blocking    = nullptr;
		QToolButton*                               NonBlocking = nullptr;
		QToolButton*                               IfMinor     = nullptr;
		feedback_label_menu*                       Menu        = nullptr;
		QListWidget*                               Completion  = nullptr;
		feedback_mark_theme                        Theme;
		bool                                       Helper    = false;
		bool                                       Editing   = false;
		bool                                       Typed     = false;
		bool                                       Dismissed = false;
		feedback_composition_state                 State;
		std::optional<core::feedback_edit_refusal> Refusal;
		std::function<void(const QString&)>        StatusSink;
		feedback_label_control_statistics          Statistics;
	};

	/// \brief The label menu: Plain comment (⌘0), the 3×4 grid of the twelve
	/// marks with names and codes, and the decorations under "Adds".
	/// \details A popup, so ⌘0 reaches it before Home's window shortcut and is
	/// a key of the open menu only (contract, derived). Arrow keys move through
	/// the grid; Return or Space chooses; Esc closes. Focus returns to the body
	/// with the caret after the prefix when it closes.
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

	protected:
		auto event(QEvent* aEvent) -> bool override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;

	private:
		void choose(const core::feedback_header_edit& aEdit);
		void move_focus(int aRows, int aColumns);

		feedback_label_controls&                             Controls;
		QToolButton*                                         Plain = nullptr;
		std::array<QToolButton*, core::feedback_label_count> Labels{ };
		std::array<QToolButton*, 3U>                         Decorations{ };
		QLabel*                                              Hint = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_CONTROLS_HPP
