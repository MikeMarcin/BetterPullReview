#include "bpr/qt/line_bounded_label.hpp"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QStyle>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// The text as one paragraph: a bounded label reads a line break as a
		/// space, so its last line is shortened only where the text goes on.
		[[nodiscard]] auto flattened(const QString& aText) -> QString
		{
			auto text = aText;
			text.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QChar::LineSeparator, QLatin1Char(' ')).replace(QChar::ParagraphSeparator, QLatin1Char(' '));
			return text.trimmed();
		}

		struct laid_out
		{
			QStringList Lines;
			bool        Elided = false;
		};

		/// At most `aMaximum` lines of `aText` wrapped at word boundaries (or
		/// anywhere in a word wider than the line) to `aWidth` pixels; the
		/// last shown ends in "…" when more text follows it.
		[[nodiscard]] auto lay_out(const QString& aText, const QFont& aFont, const int aWidth, const int aMaximum) -> laid_out
		{
			auto       result = laid_out{ };
			const auto text   = flattened(aText);
			if (text.isEmpty()) {
				return result;
			}
			const auto width   = std::max(1, aWidth);
			const auto metrics = QFontMetricsF{ aFont };
			auto       layout  = QTextLayout{ text, aFont };
			auto       option  = QTextOption{ };
			option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			layout.setTextOption(option);
			layout.beginLayout();
			for (auto line = layout.createLine(); line.isValid(); line = layout.createLine()) {
				line.setLineWidth(width);
				const auto end = line.textStart() + line.textLength();
				if (result.Lines.size() + 1 >= aMaximum && end < text.size()) {
					const auto rest  = text.mid(line.textStart()).trimmed();
					auto       shown = metrics.elidedText(rest, Qt::ElideRight, width);
					// "word…", not "word …": a cut after a space drops it.
					if (shown != rest && shown.size() > 1) {
						const auto ellipsis = shown.back();
						shown.chop(1);
						shown = shown.trimmed() + ellipsis;
					}
					result.Lines.push_back(shown);
					result.Elided = shown != rest;
					break;
				}
				result.Lines.push_back(text.mid(line.textStart(), line.textLength()).trimmed());
			}
			layout.endLayout();
			return result;
		}
	} // namespace

	line_bounded_label::line_bounded_label(const int aMaximumLines, QWidget* aParent)
		: QLabel{ aParent }
		, MaximumLines{ std::max(1, aMaximumLines) }
	{
		setTextFormat(Qt::PlainText);
		auto policy = QSizePolicy{ QSizePolicy::Preferred, QSizePolicy::Preferred };
		policy.setHeightForWidth(true);
		setSizePolicy(policy);
	}

	auto line_bounded_label::shown_lines(const int aWidth) const -> QStringList { return lay_out(text(), font(), aWidth, MaximumLines).Lines; }

	auto line_bounded_label::elided(const int aWidth) const -> bool { return lay_out(text(), font(), aWidth, MaximumLines).Elided; }

	auto line_bounded_label::heightForWidth(const int aWidth) const -> int
	{
		const auto margins = contentsMargins();
		const auto inner   = aWidth - margins.left() - margins.right() - (2 * margin());
		const auto lines   = std::max<qsizetype>(1, shown_lines(inner).size());
		return (static_cast<int>(lines) * fontMetrics().lineSpacing()) + margins.top() + margins.bottom() + (2 * margin());
	}

	auto line_bounded_label::sizeHint() const -> QSize
	{
		const auto margins = contentsMargins();
		const auto metrics = fontMetrics();
		const auto natural = std::min(metrics.horizontalAdvance(flattened(text())), 40 * metrics.averageCharWidth());
		const auto width   = std::max(natural, metrics.horizontalAdvance(QStringLiteral("W…"))) + margins.left() + margins.right() + (2 * margin());
		return { width, heightForWidth(width) };
	}

	auto line_bounded_label::minimumSizeHint() const -> QSize
	{
		const auto margins = contentsMargins();
		const auto metrics = fontMetrics();
		return { metrics.horizontalAdvance(QStringLiteral("W…")) + margins.left() + margins.right() + (2 * margin()), metrics.lineSpacing() + margins.top() + margins.bottom() + (2 * margin()) };
	}

	void line_bounded_label::paintEvent(QPaintEvent* /*aEvent*/)
	{
		const auto box     = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
		const auto spacing = fontMetrics().lineSpacing();
		// Whole lines only, even in a box a parent made too short.
		const auto fitting = std::clamp(box.height() / std::max(1, spacing), 1, MaximumLines);
		const auto lines   = lay_out(text(), font(), box.width(), fitting).Lines;
		const auto align   = QStyle::visualAlignment(layoutDirection(), alignment() & Qt::AlignHorizontal_Mask) | Qt::AlignTop | Qt::TextSingleLine;
		auto       painter = QPainter{ this };
		for (qsizetype index = 0; index < lines.size(); ++index) {
			const auto row = QRect{ box.left(), box.top() + (static_cast<int>(index) * spacing), box.width(), spacing };
			style()->drawItemText(&painter, row, static_cast<int>(align), palette(), isEnabled(), lines[index], foregroundRole());
		}
	}
} // namespace bpr::qt
