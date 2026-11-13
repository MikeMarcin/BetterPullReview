#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGEST_CHANGE_CONTROL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGEST_CHANGE_CONTROL_HPP

#include <QFrame>
#include <QString>
#include <QToolButton>

#include <cstdint>

class QAbstractButton;
class QLabel;
class QPlainTextEdit;

/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change on a draft
/// comment: the control in the field's tool row that asks the agent for the
/// change on the draft's lines, and the panel under the field that says what
/// came of it. Nothing here runs an agent, edits code or writes the draft:
/// the window says what to show and where each button goes.
namespace bpr::qt {
	/// \brief Where one Suggest the change stands, as the panel shows it.
	enum class agent_suggestion_phase : std::uint8_t {
		/// Nothing asked, or the panel was closed.
		idle,
		/// The agent is writing the change.
		running,
		/// The change is open in the code, unsaved: Keep change or Discard.
		proposed,
		/// The change is in the comment: Undo or Done.
		kept,
		/// The reply was not shown as a suggestion: Show reply or Dismiss.
		refused,
	};

	/// \brief What the draft's composer shows of Suggest the change.
	struct agent_suggestion_presentation
	{
		/// The draft's lines can take a suggestion, so the control is offered.
		bool Offered = false;
		/// The agent is set up; otherwise the control says so when pressed.
		bool Ready = true;
		agent_suggestion_phase Phase = agent_suggestion_phase::idle;
		/// The panel's one sentence.
		QString Line;
		/// A refused reply, as the agent wrote it.
		QString Reply;

		auto operator==(const agent_suggestion_presentation&) const -> bool = default;
	};

	/// \brief The control: the words "Suggest change" in the Agent accent,
	/// at one width whether it asks or stops, so nothing beside it moves.
	class suggest_change_button final : public QToolButton
	{
	public:
		explicit suggest_change_button(QWidget* aParent);
		/// \brief Shows a run going (Stop, ordinary roles) or not.
		void               set_running(bool aRunning);
		[[nodiscard]] auto running() const noexcept -> bool { return Running; }
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void restyle();

		bool Running   = false;
		bool Restyling = false;
	};

	/// \brief Under the field: one sentence, the refused reply when asked
	/// for, and one row of two buttons of one width that keep their places
	/// from the agent's answer to the panel closing. A thin rule in the Agent
	/// accent at its leading edge says whose answer it is.
	class suggest_change_panel final : public QFrame
	{
	public:
		explicit suggest_change_panel(QWidget* aParent);
		/// \brief Takes the host's two buttons, first then second.
		void set_buttons(QAbstractButton* aFirst, QAbstractButton* aSecond);
		/// \brief Shows one presentation; idle hides the panel.
		void present(const agent_suggestion_presentation& aPresentation);
		/// \brief Shows or hides the refused reply as text.
		void               toggle_reply();
		[[nodiscard]] auto line_text() const -> QString;
		[[nodiscard]] auto reply_shown() const -> bool;
		[[nodiscard]] auto reply_view() const noexcept -> QPlainTextEdit* { return Reply; }
		[[nodiscard]] auto first_button() const noexcept -> QAbstractButton* { return First; }
		[[nodiscard]] auto second_button() const noexcept -> QAbstractButton* { return Second; }
		[[nodiscard]] auto phase() const noexcept -> agent_suggestion_phase { return Phase; }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void fit_buttons();

		QLabel*                Line   = nullptr;
		QPlainTextEdit*        Reply  = nullptr;
		QAbstractButton*       First  = nullptr;
		QAbstractButton*       Second = nullptr;
		agent_suggestion_phase Phase  = agent_suggestion_phase::idle;
	};
} // namespace bpr::qt

#endif
