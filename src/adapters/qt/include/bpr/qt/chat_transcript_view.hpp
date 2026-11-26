#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_TRANSCRIPT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_TRANSCRIPT_VIEW_HPP

#include "bpr/core/chat_transcript.hpp"
#include "bpr/qt/agent_run_status.hpp"

#include <QElapsedTimer>
#include <QScrollArea>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief The one transcript of a chat with an agent (RFC-BPR-0130 stage 3,
/// AC06).
/// \details A chat hands it a `core::chat_transcript` and it changes only the
/// turns `core::chat_transcript_delta` names: a turn appended is made where
/// it stands, a turn removed goes, a reply that grows is set again in its own
/// reader, a turn that settles shows its ending line. Every other turn keeps
/// its widgets. It is the chat's scroll area, kept at its end
/// (`stick_to_bottom`) and edged at its top while a turn is cut there
/// (`transcript_edge`). A turn is drawn in the side chats' look: the
/// reviewer's words in a bubble on the right, the reply as Markdown on the
/// left, then the one ending line with the turn's actions. What a chat hangs
/// on a turn (its attachments) is drawn by the factory the chat registers.
/// It decides nothing: the chat's projector says what each turn holds and
/// offers, and each press leaves as a gesture naming the turn.
namespace bpr::qt {
	class markdown_body_view;

	/// \brief A press on a turn's own action.
	enum class chat_turn_gesture : std::uint8_t {
		resend,
		remove,
	};

	class chat_transcript_view final : public QScrollArea
	{
	public:
		/// \brief The object names of the parts, each chat's own.
		struct names
		{
			QString Scroll;
			QString Bubble;
			QString Question;
			QString Steered;
			QString Reply;
			QString Failure;
			QString Ending;
			QString CopyReply;
			QString Resend;
			QString Delete;
			QString QuestionNote;
			QString Activity;
		};
		/// \brief The chat's part in drawing a turn.
		struct parts
		{
			/// Sets a new reader up as the chat reads Markdown (its code
			/// style, its links) before its body is set.
			std::function<void(markdown_body_view& aReader)> DressReader;
			/// Makes the widget for one attachment; null for none.
			std::function<QWidget*(const core::chat_turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aParent)> MakeAttachment;
			/// A press on a turn's action.
			std::function<void(const std::string& aTurn, chat_turn_gesture aGesture)> Act;
			/// Dresses a new bubble, or one whose question changed, above its
			/// words (what the question quotes, the prompt it was sent from);
			/// `aAbove` is empty each time. Null for none.
			std::function<void(const core::chat_turn& aTurn, QWidget& aBubble, QVBoxLayout& aAbove, QLabel& aQuestion)> DressBubble;
			/// The body a reply is shown with (its references as links); null
			/// shows the reply as written.
			std::function<std::string(const core::chat_turn& aTurn)> ReplyBody;
			/// Stop, from a running turn's quiet notice.
			std::function<void()> Stop;
		};

		chat_transcript_view(const names& aNames, QWidget* aParent);
		~chat_transcript_view() override;
		chat_transcript_view(const chat_transcript_view&)                    = delete;
		auto operator=(const chat_transcript_view&) -> chat_transcript_view& = delete;

		void set_parts(parts aParts);
		/// \brief The chat's own items above the turns (the item it is about)
		/// and under them.
		[[nodiscard]] auto lead() const noexcept -> QVBoxLayout* { return Lead; }
		[[nodiscard]] auto trail() const noexcept -> QVBoxLayout* { return Trail; }

		/// \brief Draws `aTranscript`, changing only the turns that differ
		/// from the one shown.
		void               present(const core::chat_transcript& aTranscript);
		[[nodiscard]] auto shown() const noexcept -> const core::chat_transcript& { return Shown; }
		/// \brief What the last `present` changed.
		[[nodiscard]] auto last_delta() const noexcept -> const core::chat_delta& { return LastDelta; }
		/// \brief What a running turn's line says, timed from `aStarted`.
		void present_live(const agent_run_status& aStatus, const QElapsedTimer& aStarted);

		/// \brief The turns shown, in order, and their parts; null where a
		/// turn has no such part or past the turns.
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Shown.Turns.size(); }
		[[nodiscard]] auto turn_widget(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto bubble(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto question(std::size_t aTurn) const -> QLabel*;
		[[nodiscard]] auto reply(std::size_t aTurn) const -> markdown_body_view*;
		[[nodiscard]] auto ending(std::size_t aTurn) const -> agent_run_status_line*;
		[[nodiscard]] auto copy_reply(std::size_t aTurn) const -> QToolButton*;
		[[nodiscard]] auto resend(std::size_t aTurn) const -> QPushButton*;
		[[nodiscard]] auto remove(std::size_t aTurn) const -> QPushButton*;
		[[nodiscard]] auto activity(std::size_t aTurn) const -> agent_activity_view*;
		/// \brief The widget the factory made for attachment `aKey` of a turn.
		[[nodiscard]] auto attachment(std::size_t aTurn, const std::string& aKey) const -> QWidget*;
		/// \brief Every reader of the transcript, for the chat's code colours.
		[[nodiscard]] auto readers() const -> std::vector<markdown_body_view*>;

		/// \brief Takes the transcript to its end and keeps it there.
		/// \brief Whether Resend takes a press now (nothing runs, the chat is
		/// ready).
		void set_resend_enabled(bool aEnabled);
		/// \brief Makes every attachment `aKey` again, as its turn holds it: its
		/// own controls go back to what the chat says (a grant declined).
		void remake_attachments(const std::string& aKey);
		void pin();
		/// \brief Lays bubbles and readers out for the width the column has.
		void fit();

	protected:
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		class turn_view;

		[[nodiscard]] auto view_of(std::size_t aTurn) const -> turn_view*;
		[[nodiscard]] auto available_width() const -> int;
		void               fit_soon();

		names                           Names;
		parts                           Parts;
		QVBoxLayout*                    Lead  = nullptr;
		QVBoxLayout*                    Turns = nullptr;
		QVBoxLayout*                    Trail = nullptr;
		core::chat_transcript           Shown;
		core::chat_delta                LastDelta;
		std::vector<turn_view*>         Views;
		std::optional<agent_run_status> Live;
		QElapsedTimer                   LiveClock;
		bool                            FitQueued     = false;
		bool                            ResendEnabled = true;
	};
}

#endif
