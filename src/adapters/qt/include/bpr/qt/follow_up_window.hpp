#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FOLLOW_UP_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FOLLOW_UP_WINDOW_HPP

#include "bpr/core/agent_follow_up.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/// \file
/// \brief [[BPR-2286]]. The follow-up window: one conversation with the agent
/// about one review item (a suggested comment, a draft, a thread), its rounds,
/// and the one thing it can apply (Update suggested comment, Update draft or
/// Draft reply) shown as a proposal beside the text it replaces, used only by
/// Use. It runs nothing and writes nothing: each press leaves as an intent,
/// and what it shows arrives as the core's state.
namespace bpr::qt {
	/// \brief What a press asks for.
	struct follow_up_intents
	{
		std::function<void(const QString& aQuestion)> Ask;
		std::function<void(core::follow_up_apply)>    Propose;
		std::function<void(std::uint64_t aProposal)>  Use;
		std::function<void(std::uint64_t aProposal)>  Discard;
		std::function<void()>                         Stop;
		std::function<void()>                         Closed;
	};

	/// \brief The words for one apply action's button: "Update suggested
	/// comment", "Update draft", "Draft reply".
	[[nodiscard]] auto follow_up_apply_words(core::follow_up_apply aApply) -> QString;
	/// \brief Why a step did nothing, in the person's words; empty for none.
	[[nodiscard]] auto follow_up_refusal_words(core::follow_up_refusal aRefusal) -> QString;

	class follow_up_window final : public QWidget
	{
	public:
		explicit follow_up_window(QWidget* aParent = nullptr);

		void set_intents(follow_up_intents aIntents);
		/// \brief Shows `aState`: the item, its rounds, a run going, a
		/// proposal, why the last step did nothing (`aFailure` when a run
		/// failed). `aAgent` names the agent in the running line.
		void present(const core::follow_up_state& aState, const QString& aAgent, const QString& aFailure);
		/// \brief Says what came of a Use (or why it was not used).
		void show_status(const QString& aText);

		[[nodiscard]] auto question_field() const noexcept -> QPlainTextEdit* { return Question; }
		[[nodiscard]] auto ask_button() const noexcept -> QPushButton* { return AskButton; }
		[[nodiscard]] auto apply_button() const noexcept -> QPushButton* { return ApplyButton; }
		[[nodiscard]] auto use_button() const noexcept -> QPushButton* { return UseButton; }
		[[nodiscard]] auto discard_button() const noexcept -> QPushButton* { return DiscardButton; }
		[[nodiscard]] auto proposal_panel() const noexcept -> QFrame* { return Proposal; }
		[[nodiscard]] auto proposal_text() const -> QString;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto heading_text() const -> QString;
		[[nodiscard]] auto item_text() const -> QString;
		/// \brief The answers shown, in order.
		[[nodiscard]] auto answers() const -> QStringList;
		[[nodiscard]] auto running() const noexcept -> bool { return Running; }
		[[nodiscard]] auto shown_key() const -> std::string { return Shown ? Shown->Subject.Key : std::string{ }; }

	protected:
		void closeEvent(QCloseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void ask();
		void rebuild_rounds(const core::follow_up_state& aState);
		void restyle();

		follow_up_intents                    Intents;
		std::optional<core::follow_up_state> Shown;
		bool                                 Running         = false;
		QLabel*                              Heading         = nullptr;
		QLabel*                              ItemTitle       = nullptr;
		QLabel*                              ItemText        = nullptr;
		QFrame*                              Item            = nullptr;
		QScrollArea*                         Scroll          = nullptr;
		QWidget*                             RoundsHolder    = nullptr;
		QVBoxLayout*                         Rounds          = nullptr;
		QLabel*                              Working         = nullptr;
		QFrame*                              Proposal        = nullptr;
		QLabel*                              ProposalHeading = nullptr;
		QLabel*                              ProposalBefore  = nullptr;
		QLabel*                              ProposalAfter   = nullptr;
		QPushButton*                         UseButton       = nullptr;
		QPushButton*                         DiscardButton   = nullptr;
		QLabel*                              Status          = nullptr;
		QPlainTextEdit*                      Question        = nullptr;
		QPushButton*                         AskButton       = nullptr;
		QPushButton*                         ApplyButton     = nullptr;
		std::size_t                          RoundsShown     = 0U;
	};
} // namespace bpr::qt

#endif
