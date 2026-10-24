#include "bpr/qt/resident_filter.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QHBoxLayout>
#include <QList>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// One term as an ordered, case-folded subsequence of the text.
		[[nodiscard]] auto term_matches(const QStringView aText, const QStringView aTerm) -> bool
		{
			auto position = qsizetype{ 0 };
			for (const auto needle : aTerm) {
				const auto folded = needle.toCaseFolded();
				while (position < aText.size() && aText[position].toCaseFolded() != folded) {
					++position;
				}
				if (position == aText.size()) {
					return false;
				}
				++position;
			}
			return true;
		}
	} // namespace

	auto resident_filter_matches(const QStringView aText, const QStringView aNeedle) -> bool
	{
		const auto terms = aNeedle.split(u' ', Qt::SkipEmptyParts);
		return std::ranges::all_of(terms, [aText](const QStringView aTerm) {
			return term_matches(aText, aTerm);
		});
	}

	filter_empty_notice::filter_empty_notice(
		const QString& aSubject,
		const QString& aObjectPrefix,
		QWidget* const aParent)
		: QWidget{ aParent }
		, Subject{ aSubject }
	{
		setObjectName(aObjectPrefix + QStringLiteral("FilterNotice"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(6, 4, 6, 4);
		Message = new QLabel{ this };
		Message->setObjectName(aObjectPrefix + QStringLiteral("FilterMessage"));
		Message->setWordWrap(true);
		row->addWidget(Message, 1);
		Clear = new quiet_push_button{ QStringLiteral("Clear filter"), this };
		Clear->setObjectName(aObjectPrefix + QStringLiteral("ClearFilterButton"));
		Clear->setAccessibleName(QStringLiteral("Clear %1 filter").arg(aSubject));
		Clear->setAccessibleDescription(QStringLiteral("Empty the filter and list every one of the %1 again").arg(aSubject));
		Clear->setFlat(true);
		row->addWidget(Clear);
		set_filter(QString{ });
	}

	void filter_empty_notice::set_filter(const QString& aFilter)
	{
		Message->setText(QStringLiteral("No %1 match “%2”.").arg(Subject, aFilter));
	}

	auto filter_empty_notice::message() const -> QString
	{
		return Message->text();
	}

	auto filter_empty_notice::clear_button() const noexcept -> QPushButton*
	{
		return Clear;
	}
} // namespace bpr::qt
