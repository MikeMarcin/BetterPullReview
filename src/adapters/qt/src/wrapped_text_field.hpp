#ifndef BPR_SRC_ADAPTERS_QT_SRC_WRAPPED_TEXT_FIELD_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_WRAPPED_TEXT_FIELD_HPP

#include <QObject>
#include <QPlainTextEdit>
#include <QScrollBar>

/// \file
/// \brief A drawer's field of wrapped prose has no sideways range. No drawer
/// pans sideways (owner, look workflow-v002).
namespace bpr::qt::detail {
	/// \brief Wraps \p aField's words at its width and keeps it from panning.
	///
	/// \details A `QPlainTextEdit` that wraps at its width can still be given
	/// a few pixels of horizontal range: once a block has been laid out, Qt
	/// measures it as its widest line and some room more, and a line that
	/// nearly fills the field then counts as wider than it. The field shows a
	/// horizontal scroll bar under words that all fit, and with the bar
	/// turned off a trackpad still pans them. Wrapped prose has nothing to
	/// pan to, so the range is held at nothing.
	inline void hold_wrapped_text_still(QPlainTextEdit& aField)
	{
		aField.setLineWrapMode(QPlainTextEdit::WidgetWidth);
		aField.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const bar = aField.horizontalScrollBar();
		QObject::connect(bar, &QScrollBar::rangeChanged, bar, [bar](const int aMinimum, const int aMaximum) {
			if (aMaximum > aMinimum) {
				bar->setRange(aMinimum, aMinimum);
			}
		});
		bar->setRange(bar->minimum(), bar->minimum());
	}
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_WRAPPED_TEXT_FIELD_HPP
