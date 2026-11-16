#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_PENDING_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_PENDING_LIST_HPP

#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

/// \file
/// \brief RFC-BPR-0126 ([[BPR-2151]], [[BPR-2154]]). The pending list one
/// session's queue shows in its chat: the Agent tab's, a tour part's side
/// chat, a local draft's discussion. Looks prompts-v001 to v004 drew it in
/// the Agent tab; it was made reusable for the tour chats as it was.
namespace bpr::qt {
	/// \brief One waiting message as the pending list shows it.
	struct agent_pending_row
	{
		std::uint64_t Id = 0U;
		QString       Label;
		/// A typed message's text; empty for a quick ask, which is not
		/// edited in place.
		QString Text;
		bool    Held    = false;
		bool    Sending = false;
		/// RFC-BPR-0126 AC04. Released before a crash or quit, and whether
		/// the agent took it is not known: offered Discard.
		bool Uncertain = false;
		/// RFC-BPR-0126 AC08 ([[BPR-2152]]). Steered into the running turn
		/// and acknowledged by the agent's runtime: shown as Sent to agent
		/// until the run's end settles it.
		bool Sent = false;
	};

	/// \brief What the pending list's controls ask of the session's queue.
	struct agent_pending_dispatch
	{
		std::function<void(std::uint64_t)>                 Edit;
		std::function<void(std::uint64_t, const QString&)> Save;
		std::function<void(std::uint64_t)>                 Cancel;
		std::function<void(std::uint64_t)>                 Remove;
		std::function<void(std::uint64_t, int)>            Move;
		/// Steer the running turn with this message instead of waiting.
		std::function<void(std::uint64_t)> Steer;
		std::function<void()>              Pause;
		std::function<void()>              Resume;
		/// Discard a message that may not have been delivered.
		std::function<void(std::uint64_t)> Discard;
	};

	/// \brief The bordered panel of bordered cards: a grip to drag a card
	/// (Option-Up and Option-Down move it, Delete removes it), the message on
	/// one line, a pencil, Steer and a trash can; Pause queue or Resume queue
	/// at its head with the queue's state in words. Hidden while empty.
	class agent_pending_list final : public QWidget
	{
	public:
		explicit agent_pending_list(QWidget* aParent = nullptr);

		void set_dispatch(agent_pending_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
		/// \brief The session's waiting messages, in order, with the queue's
		/// state ("waiting its turn", "paused: Stop") and whether it is paused.
		void present(std::vector<agent_pending_row> aRows, const QString& aState, bool aPaused);
		/// \brief A waiting card dropped at `aAt` among the waiting cards (0
		/// is above the first): the queue is asked to move it there.
		void               land(std::uint64_t aId, int aAt);
		[[nodiscard]] auto rows() const noexcept -> std::size_t { return Rows; }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void dragEnterEvent(QDragEnterEvent* aEvent) override;
		void dragMoveEvent(QDragMoveEvent* aEvent) override;
		void dragLeaveEvent(QDragLeaveEvent* aEvent) override;
		void dropEvent(QDropEvent* aEvent) override;

	private:
		[[nodiscard]] auto movable() const -> std::vector<QWidget*>;
		[[nodiscard]] auto landing(int aY) const -> std::pair<int, int>;

		agent_pending_dispatch       Dispatch;
		std::optional<std::uint64_t> Editing;
		std::size_t                  Rows = 0U;
		int                          Line = -1;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_PENDING_LIST_HPP
