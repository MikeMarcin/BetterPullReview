#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EXPLAIN_FILE_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EXPLAIN_FILE_CARD_HPP

#include "bpr/core/agent_activity.hpp"
#include "bpr/qt/agent_run_status.hpp"

#include <QElapsedTimer>
#include <QFrame>
#include <QPointer>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QPushButton;
class QScrollArea;

namespace bpr::qt {
	class markdown_body_view;

	/// \brief RFC-BPR-0117 ([[BPR-2076]]). One Explain this file turn, as the
	/// Agent tab knows it: the card at the file's header shows what it is told.
	struct explain_file_progress
	{
		/// The file explained.
		QString Path;
		/// The Agent tab turn it is; Continue in Agent tab opens it.
		std::size_t Turn = 0U;
		/// Live while the run is; once it ends, `Ending` says how.
		std::optional<agent_run_status> Running;
		QElapsedTimer                   Started;
		/// The runtime's activity while it reports one, and folded after.
		std::optional<core::agent_turn_activity> Activity;
		qint64                                   Seconds = 0;
		/// The answer as written, with its references as links.
		QString Answer;
		/// The answer as the agent wrote it, which Keep as note keeps.
		QString AnswerText;
		/// The turn's ending line once it ended; whether it ended answered.
		QString Ending;
		bool    Answered = false;
	};

	/// \brief RFC-BPR-0117 ([[BPR-2076]]). The answer to Explain this file, at
	/// the file's header in Review.
	///
	/// \details The run's line and its activity while it runs; then the
	/// answer, its references links, under a height of its own that scrolls.
	/// One row of actions, each the same size and in the same place from the
	/// press to the end: Keep as note (then Go to note), Continue in Agent tab
	/// and Dismiss. A thin rule in the Agent accent at its leading edge says
	/// whose answer it is. It decides nothing: the window says what to show
	/// and where each action goes.
	class explain_file_card final : public QFrame
	{
	public:
		explicit explain_file_card(QWidget* aParent = nullptr);

		void present(const explain_file_progress& aProgress);
		/// \brief Keep as note's outcome: Go to note once kept, or the
		/// refusal's words in the line.
		void present_note(bool aKept, const QString& aWhy, std::uint64_t aNote);
		void clear();

		void set_keep_dispatch(std::function<void(const QString& aPath, const QString& aAnswer)> aDispatch) { Keep = std::move(aDispatch); }
		void set_continue_dispatch(std::function<void(std::size_t aTurn)> aDispatch) { Continue = std::move(aDispatch); }
		void set_dismiss_dispatch(std::function<void()> aDispatch) { Dismiss = std::move(aDispatch); }
		void set_go_to_note(std::function<void(std::uint64_t aNote)> aDispatch) { GoToNote = std::move(aDispatch); }
		void set_link_dispatch(std::function<void(const QString& aLink)> aDispatch) { Link = std::move(aDispatch); }
		/// \brief How an answer's code is set: the Agent tab's own dress.
		void set_dresser(std::function<void(markdown_body_view&)> aDresser);

		[[nodiscard]] auto path() const noexcept -> const QString& { return Shown.Path; }
		[[nodiscard]] auto progress() const noexcept -> const explain_file_progress& { return Shown; }
		[[nodiscard]] auto note() const noexcept -> std::uint64_t { return Note; }
		[[nodiscard]] auto keep_button() const noexcept -> QPushButton* { return KeepButton; }
		[[nodiscard]] auto continue_button() const noexcept -> QPushButton* { return ContinueButton; }
		[[nodiscard]] auto dismiss_button() const noexcept -> QPushButton* { return DismissButton; }
		[[nodiscard]] auto answer_view() const noexcept -> markdown_body_view* { return Answer; }
		[[nodiscard]] auto status_line() const noexcept -> agent_run_status_line* { return Status; }
		[[nodiscard]] auto activity_view() const noexcept -> agent_activity_view* { return ActivityView; }

		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void fit_answer();
		void fit_buttons();

		explain_file_progress                                         Shown;
		std::uint64_t                                                 Note     = 0U;
		bool                                                          Keeping  = false;
		QString                                                       Refusal;
		std::function<void(const QString&, const QString&)>           Keep;
		std::function<void(std::size_t)>                              Continue;
		std::function<void()>                                         Dismiss;
		std::function<void(std::uint64_t)>                            GoToNote;
		std::function<void(const QString&)>                           Link;
		QPointer<agent_run_status_line>                               Status;
		QPointer<agent_activity_view>                                 ActivityView;
		QPointer<QScrollArea>                                         Scroll;
		QPointer<markdown_body_view>                                  Answer;
		QPointer<QPushButton>                                         KeepButton;
		QPointer<QPushButton>                                         ContinueButton;
		QPointer<QPushButton>                                         DismissButton;
		std::uint64_t                                                 Serial = 0U;
	};
} // namespace bpr::qt

#endif
