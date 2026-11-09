#ifndef BPR_SRC_ADAPTERS_QT_SRC_SOURCE_COLUMN_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SOURCE_COLUMN_HPP

#include <QStringView>

namespace bpr::qt {
	/// \brief A zero-based native caret offset, including the end of a line.
	/// \details Qt positions count UTF-16 units. A surrogate pair is indivisible.
	[[nodiscard]] inline auto valid_source_column(const QStringView aText, const qsizetype aColumn) -> bool
	{
		return aColumn >= 0 && aColumn <= aText.size()
			&& !(aColumn > 0 && aColumn < aText.size() && aText[aColumn - 1].isHighSurrogate() && aText[aColumn].isLowSurrogate());
	}
}

#endif
