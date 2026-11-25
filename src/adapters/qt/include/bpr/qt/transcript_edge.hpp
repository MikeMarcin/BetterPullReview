#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TRANSCRIPT_EDGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TRANSCRIPT_EDGE_HPP

#include <QColor>
#include <QPointer>
#include <QWidget>

class QEvent;
class QPaintEvent;
class QPalette;
class QScrollArea;

namespace bpr::qt {
	/// \brief The top edge of a chat's transcript, drawn while turns are
	/// scrolled under it.
	/// \details Owner, attended review 2026-11-25, of a refused question in a
	/// 280 pixel drawer: its first line was "partly hidden under the Native
	/// continuation line". Nothing lay over it. A transcript keeps itself at
	/// its end (`stick_to_bottom`), so one taller than its viewport cuts a
	/// turn at the viewport's top, wherever that falls; with no line between
	/// the header's words and the turns, the cut turn read as covered by them.
	///
	/// Attached to a transcript's scroll area, this is one hairline on the
	/// viewport's first row. It shows exactly while a turn is scrolled under
	/// it (the transcript is off its start by more than the blank margin its
	/// layout keeps over the first turn), as a window's toolbar shows its
	/// separator only while content is scrolled under it: the line says that
	/// the turn at the top goes on above, and a transcript at its start has
	/// no line over its first turn. It takes no room in any layout, so
	/// showing it moves nothing, and no pointer or keyboard event.
	class transcript_edge final : public QWidget
	{
	public:
		/// \brief Attaches to `aArea`, which owns it; again, the one attached.
		static auto attach(QScrollArea& aArea) -> transcript_edge&;
		/// \brief The one attached to `aArea`, if any.
		[[nodiscard]] static auto of(const QScrollArea& aArea) -> transcript_edge*;
		/// \brief The line's colour on `aPalette`'s window ground: the
		/// hairline the transcript's own cards are edged with.
		[[nodiscard]] static auto colour(const QPalette& aPalette) -> QColor;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		explicit transcript_edge(QScrollArea& aArea);

		/// \brief Lies on the viewport's first row and shows while a turn is
		/// scrolled under it.
		void follow();

		QPointer<QScrollArea> Area;
	};
} // namespace bpr::qt

#endif
