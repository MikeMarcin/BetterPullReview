#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_INFO_GLYPH_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_INFO_GLYPH_HPP

#include <QString>
#include <QToolButton>

class QEvent;
class QFocusEvent;

namespace bpr::qt {
	/// \brief A settings row's info glyph: the row's consequence, said on demand.
	///
	/// \details RFC-BPR-0032's row language as RFC-BPR-0034 amends it: a row is
	/// one line of label, info glyph and control, and the consequence sentence
	/// lives in the glyph's popover, shown on hover and on keyboard focus. The
	/// sentence is also the glyph's accessible description, so it is never
	/// reachable by pointer only. First built by [[BPR-0774]] for Accounts &
	/// hosts; the Appearance rows ([[BPR-0790]]) share it.
	///
	/// Presentation only: the glyph says a sentence it is given and performs
	/// nothing.
	class info_glyph final : public QToolButton
	{
	public:
		explicit info_glyph(QString aConsequence, QWidget* aParent = nullptr);

		/// \brief Adopts the sentence the popover and the accessible description say.
		void               set_consequence(QString aConsequence);
		[[nodiscard]] auto consequence() const noexcept -> const QString&;
		/// \brief Shows the popover now, as hovering, focusing or pressing the glyph does.
		void show_popover();

	protected:
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void apply_glyph();

		QString Consequence;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_INFO_GLYPH_HPP
