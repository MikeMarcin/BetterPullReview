#ifndef BPR_SRC_ADAPTERS_QT_SRC_SIDE_CHAT_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SIDE_CHAT_PARTS_HPP

#include <QColor>
#include <QObject>
#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QVBoxLayout;

/// \file
/// \brief [[BPR-2322]]. The parts every side chat is built from, as the Tour
/// tab's side chat ([[BPR-1925]], [[BPR-1995]]) first drew them: the seam down
/// the column's left edge, the author's bubble, a proposal's frame, Return to
/// send, and how bubbles and replies fit the column's width. The tour's chat
/// and the side chat about one review item (`item_side_chat`) both use these,
/// so the two read as one interface.
namespace bpr::qt::side_chat {
	/// \brief `aFrom` moved `aAmount` of the way to `aTo`.
	[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, double aAmount) -> QColor;
	/// \brief `aLabel` a little smaller than its font.
	auto small(QLabel* aLabel) -> QLabel*;
	/// \brief Plain, wrapping, selectable words.
	[[nodiscard]] auto words(const QString& aText, QWidget* aParent) -> QLabel*;
	/// \brief `aLabel` in the placeholder's quiet colour.
	auto quiet(QLabel* aLabel) -> QLabel*;
	/// \brief Empties `aLayout`, hiding and deleting what it held.
	void clear(QVBoxLayout* aLayout);
	/// \brief Paints the seam with what the column sits beside: a line of the
	/// agent's accent down its left edge, and the faintest wash beside it.
	void paint_seam(QWidget& aColumn);
	/// \brief A short bubble keeps to one line, a long one wraps; at most
	/// four fifths of `aAvailable`.
	void fit_bubble(QWidget& aBubble, int aAvailable);

	/// \brief Return sends; Shift+Return starts a new line. `aSend` is told
	/// whether Cmd+Return (Ctrl+Return off macOS) was pressed.
	class send_keys final : public QObject
	{
	public:
		send_keys(QObject* aParent, std::function<void(bool aOpposite)> aSend);

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		std::function<void(bool)> Send;
	};

	/// \brief The author's words: a quiet rounded bubble, right-aligned.
	class bubble_frame final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief A proposal waiting in the chat: the agent accent's rule down its
	/// left, as the proposal under Draft the tour is drawn.
	class proposal_frame final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};
} // namespace bpr::qt::side_chat

#endif
