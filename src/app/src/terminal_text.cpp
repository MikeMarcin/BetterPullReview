#include "terminal_text.hpp"
#include <QChar>
#include <QList>
#include <algorithm>
#if defined(_WIN32)
	#include <io.h>
	#include <stdio.h>
#else
	#include <unistd.h>
#endif
namespace bpr::app {
	auto neutral_terminal_text(const QString& aText) -> QString
	{
		QString result;
		for (const auto value : aText.toUcs4()) {
			if ((value < 32 && value != 10 && value != 9) || (value >= 127 && value <= 159) || (value >= 0x202a && value <= 0x202e) || value == 0x061c || value == 0x200e || value == 0x200f || (value >= 0x2066 && value <= 0x2069)) {
				result += QStringLiteral("\\u%1").arg(value, 4, 16, QChar{ '0' });
			} else {
				const char32_t scalar = value;
				result += QString::fromUcs4(&scalar, 1);
			}
		}
		return result;
	}
	auto wrapped_terminal_text(const QString& aText) -> QString
	{
		bool       parsed    = false;
		const auto requested = qEnvironmentVariable("COLUMNS").toInt(&parsed);
		const int  width     = parsed ? std::clamp(requested, 20, 240) : 80;
		QString    result;
		int        column = 0;
		for (const auto scalar : neutral_terminal_text(aText).toUcs4()) {
			if (scalar == '\n') {
				result += '\n';
				column = 0;
				continue;
			}
			if (scalar == '\t') {
				for (int space = 0; space < 4; ++space) {
					if (column == width) {
						result += '\n';
						column = 0;
					}
					result += ' ';
					++column;
				}
				continue;
			}
			const auto category  = QChar::category(scalar);
			const bool combining = category == QChar::Mark_NonSpacing || category == QChar::Mark_Enclosing || scalar == 0x200d;
			const bool wide      = (scalar >= 0x1100 && scalar <= 0x115f) || (scalar >= 0x2e80 && scalar <= 0xa4cf) || (scalar >= 0xac00 && scalar <= 0xd7a3) || (scalar >= 0xf900 && scalar <= 0xfaff) || (scalar >= 0xfe10 && scalar <= 0xfe6f) || (scalar >= 0xff01 && scalar <= 0xff60) || scalar >= 0x1f300;
			const int  cells     = combining ? 0 : wide ? 2
														: 1;
			if (column && column + cells > width) {
				result += '\n';
				column = 0;
			}
			const char32_t value = scalar;
			result += QString::fromUcs4(&value, 1);
			column += cells;
		}
		return result;
	}

	auto interactive_terminal() -> bool
	{
#if defined(_WIN32)
		return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
		return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
	}
}
