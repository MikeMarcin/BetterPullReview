#ifndef BPR_SRC_ADAPTERS_QT_SRC_SOURCE_DISPLAY_TEXT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SOURCE_DISPLAY_TEXT_HPP

/// \file
/// \brief Source bytes as text for a Qt document, with Git's line count.
///
/// \details [[BPR-1432]]. Qt starts a new block at a lone CR, but Git, GitHub
/// and the decoded diff end a line only at LF (a CR before it joins a CRLF
/// ending). A lone CR is therefore shown as U+240D SYMBOL FOR CARRIAGE RETURN:
/// one UTF-16 unit, like the CR it stands for, so byte-to-position mapping is
/// unchanged and block numbers stay the decoded line numbers. CRLF and LF still
/// start blocks. Internal to the Qt adapter.

#include <QChar>
#include <QString>

namespace bpr::qt {
	inline constexpr char16_t lone_carriage_return_symbol = u'␍';

	/// \brief Decodes UTF-8 source and shows each lone CR as U+240D.
	[[nodiscard]] inline auto source_display_text(const char* aBytes, const qsizetype aSize) -> QString
	{
		auto text = QString::fromUtf8(aBytes, aSize);
		if (!text.contains(QLatin1Char('\r'))) {
			return text;
		}
		auto* const units = text.data();
		for (qsizetype index = 0; index < text.size(); ++index) {
			if (units[index] == QLatin1Char('\r') && (index + 1 == text.size() || units[index + 1] != QLatin1Char('\n'))) {
				units[index] = QChar{ lone_carriage_return_symbol };
			}
		}
		return text;
	}
}

#endif
