#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PREPARATION_AREA_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PREPARATION_AREA_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QToolButton;

namespace bpr::qt {
	/// \brief RFC-BPR-0110. The gestures the Description and Tour areas send.
	struct local_draft_preparation_dispatch
	{
		std::function<void(const std::string&, const std::string&)>                                                         SaveDescription;
		std::function<void(const std::string&, const std::string&)>                                                         SaveBrief;
		std::function<void(const std::string&)>                                                                             Transfer;
		std::function<void(const std::string&, std::uint32_t, std::uint32_t, const std::string&, const std::string&)>       AddStop;
		std::function<void(std::uint64_t, const std::string&, const std::string&, const std::string&, std::uint32_t, std::uint32_t)> SaveStop;
		std::function<void(std::uint64_t)>                                                                                   AcceptStop;
		std::function<void(std::uint64_t)>                                                                                   RemoveStop;
		std::function<void(std::uint64_t, std::size_t)>                                                                      MoveStop;
		std::function<void(std::uint64_t, std::uint64_t, bool)>                                                              Cite;
		std::function<void(std::uint64_t, bool)>                                                                             DecideProposal;
		/// The host fills in the agent and its prompt purpose.
		std::function<void(application::local_draft_question)> Ask;
		std::function<void()>                                   StopAsking;
		std::function<void(std::uint64_t)>                      NewSession;
	};

	/// \brief Lines chosen in the Changes view: a file of the latest
	/// candidate and its new-side lines.
	struct local_draft_lines
	{
		std::string   Path;
		std::uint32_t First = 0;
		std::uint32_t Last  = 0;
	};

	/// \brief What the page lends both areas: the Changes view's selection,
	/// and arriving at lines there.
	struct local_draft_page_links
	{
		std::function<std::optional<local_draft_lines>()>                Selection;
		std::function<void(const std::string&, std::uint32_t, std::uint32_t)> Reveal;
	};

	/// \brief The agent bar pinned under an area: the transcript of the
	/// open session, proposals beside it, the session line, the running
	/// block and the composer (RFC-BPR-0100, 0101, 0103 as fitted by X07,
	/// X10, X13).
	class local_draft_chat final : public QWidget
	{
	public:
		/// \param aTargets Which proposals this bar lists.
		local_draft_chat(bool aDescription, QWidget* aParent = nullptr);

		void set_dispatch(local_draft_preparation_dispatch* aDispatch, local_draft_page_links* aLinks);
		/// \brief The thread and subject the next question is on.
		void set_subject(std::string aThread, core::local_subject aSubject, std::uint64_t aStop);
		void present(const application::local_draft_workbench_projection& aProjection);

		[[nodiscard]] auto transcript() const noexcept -> QTextBrowser*;
		[[nodiscard]] auto proposals() const noexcept -> QListWidget*;
		[[nodiscard]] auto session_line() const noexcept -> QLabel*;
		[[nodiscard]] auto session_menu() const noexcept -> QPushButton*;
		[[nodiscard]] auto composer() const noexcept -> QLineEdit*;
		[[nodiscard]] auto ask_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto accept_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto reject_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto running() const noexcept -> QLabel*;
		/// \brief The conversation shown: the open one, or one from history.
		[[nodiscard]] auto shown_conversation() const noexcept -> std::uint64_t { return Shown; }
		void               show_conversation(std::uint64_t aConversation);

	private:
		void send();
		void present_transcript();
		void present_proposals();
		void present_session();

		bool                                          Description = false;
		local_draft_preparation_dispatch*             Dispatch    = nullptr;
		local_draft_page_links*                       Links       = nullptr;
		application::local_draft_workbench_projection Projection;
		std::string                                   Thread{ core::local_change_thread };
		core::local_subject                           Subject = core::local_subject::whole_change;
		std::uint64_t                                 Stop    = 0;
		/// The history conversation shown, or 0 for the open one.
		std::uint64_t Shown = 0;

		QTextBrowser* Transcript = nullptr;
		QListWidget*  Proposals  = nullptr;
		QPushButton*  Accept     = nullptr;
		QPushButton*  Reject     = nullptr;
		QLabel*       Running    = nullptr;
		QLabel*       Failure    = nullptr;
		QLabel*       Session    = nullptr;
		QPushButton*  SessionMenu = nullptr;
		QLineEdit*    Composer   = nullptr;
		QPushButton*  Ask        = nullptr;
	};

	/// \brief The Description area: the pull request's full description, its
	/// title in the page's header, the exact public preview, and the agent bar.
	class local_draft_description_area final : public QWidget
	{
	public:
		explicit local_draft_description_area(QWidget* aParent = nullptr);
		void set_dispatch(local_draft_preparation_dispatch* aDispatch, local_draft_page_links* aLinks);
		void present(const application::local_draft_workbench_projection& aProjection);
		/// \brief The exact public preview's text, as Preview shows it.
		[[nodiscard]] auto preview_text() const -> QString;

		[[nodiscard]] auto body() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto preview_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto insert_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto notice() const noexcept -> QLabel*;
		[[nodiscard]] auto chat() const noexcept -> local_draft_chat*;

	private:
		void save();

		local_draft_preparation_dispatch*             Dispatch = nullptr;
		application::local_draft_workbench_projection Projection;
		QPlainTextEdit*                               Body    = nullptr;
		QLabel*                                       Notice  = nullptr;
		QPushButton*                                  Insert  = nullptr;
		QPushButton*                                  Preview = nullptr;
		QPushButton*                                  Save    = nullptr;
		local_draft_chat*                             Chat    = nullptr;
	};

	/// \brief The Tour area: the head (its stops and how many to accept; no
	/// Publish), the scrolling route (Problem, Solution, the stops and the
	/// selected stop's editor) and the pinned agent bar (X04, X05).
	class local_draft_tour_area final : public QWidget
	{
	public:
		explicit local_draft_tour_area(QWidget* aParent = nullptr);
		void set_dispatch(local_draft_preparation_dispatch* aDispatch, local_draft_page_links* aLinks);
		void present(const application::local_draft_workbench_projection& aProjection);

		[[nodiscard]] auto head() const noexcept -> QLabel*;
		[[nodiscard]] auto problem() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto solution() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto save_brief_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto stops() const noexcept -> QListWidget*;
		[[nodiscard]] auto add_stop_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto stop_title() const noexcept -> QLineEdit*;
		[[nodiscard]] auto stop_text() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto stop_lines() const noexcept -> QLabel*;
		[[nodiscard]] auto use_lines_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto show_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto accept_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto remove_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto notice() const noexcept -> QLabel*;
		[[nodiscard]] auto chat() const noexcept -> local_draft_chat*;
		[[nodiscard]] auto selected_stop() const -> std::uint64_t;

	private:
		void present_stops();
		void present_editor();
		void arrive();

		local_draft_preparation_dispatch*             Dispatch = nullptr;
		local_draft_page_links*                       Links    = nullptr;
		application::local_draft_workbench_projection Projection;
		/// Lines chosen with Use Selected Lines, not yet saved.
		std::optional<local_draft_lines> PendingLines;
		std::uint64_t                    Editing = 0;

		QLabel*           Head      = nullptr;
		QPlainTextEdit*   Problem   = nullptr;
		QPlainTextEdit*   Solution  = nullptr;
		QPushButton*      SaveBrief = nullptr;
		QListWidget*      Stops     = nullptr;
		QPushButton*      AddStop   = nullptr;
		QLineEdit*        StopTitle = nullptr;
		QPlainTextEdit*   StopText  = nullptr;
		QLabel*           StopLines = nullptr;
		QPushButton*      UseLines  = nullptr;
		QPushButton*      Show      = nullptr;
		QPushButton*      AcceptStop = nullptr;
		QPushButton*      SaveStop  = nullptr;
		QPushButton*      Remove    = nullptr;
		QPushButton*      Move      = nullptr;
		QLabel*           Notice    = nullptr;
		local_draft_chat* Chat      = nullptr;
	};

	/// \brief Words for a refused description or tour edit.
	[[nodiscard]] auto preparation_refusal_text(core::preparation_refusal aRefusal) -> QString;
	/// \brief Dollars and cents, "< $0.01" below a cent.
	[[nodiscard]] auto local_cost_text(std::uint64_t aMicros) -> QString;
}

#endif
