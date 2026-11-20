#include "settings_card.hpp"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>
#include <QVBoxLayout>

namespace bpr::qt::detail {
	namespace {
		/// \brief A section title is a heading to assistive technology, read
		/// by its words, so a screen reader can move from section to section.
		class settings_card_title_accessible final : public QAccessibleWidget
		{
		public:
			explicit settings_card_title_accessible(settings_card_title* const aTitle)
				: QAccessibleWidget{ aTitle, QAccessible::Heading }
			{
			}

			[[nodiscard]] auto text(const QAccessible::Text aText) const -> QString override
			{
				if (aText == QAccessible::Name && widget()->accessibleName().isEmpty()) {
					return static_cast<const QLabel*>(widget())->text();
				}
				return QAccessibleWidget::text(aText);
			}
		};

		[[nodiscard]] auto settings_card_accessible_factory(const QString& aKey, QObject* const aObject) -> QAccessibleInterface*
		{
			static_cast<void>(aKey);
			if (auto* const title = dynamic_cast<settings_card_title*>(aObject); title != nullptr) {
				return new settings_card_title_accessible{ title };
			}
			return nullptr;
		}

		void install_settings_card_accessibility()
		{
			static auto installed = false;
			if (!installed) {
				QAccessible::installFactory(&settings_card_accessible_factory);
				installed = true;
			}
		}
	} // namespace

	settings_card_title::settings_card_title(const QString& aText, QWidget* const aParent)
		: QLabel{ aText, aParent }
	{
		install_settings_card_accessibility();
		setTextFormat(Qt::PlainText);
		auto title = font();
		title.setWeight(QFont::DemiBold);
		setFont(title);
	}

	auto make_settings_hairline(QWidget* const aParent) -> QFrame*
	{
		auto* const line = new QFrame{ aParent };
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Plain);
		line->setForegroundRole(QPalette::Mid);
		line->setFixedHeight(1);
		return line;
	}

	auto make_settings_card(QWidget* const aParent, const QString& aTitle, const QString& aObjectName) -> settings_card
	{
		auto card  = settings_card{ };
		card.Frame = new QFrame{ aParent };
		card.Frame->setObjectName(aObjectName);
		// The platform's own panel frame rather than a style sheet, which
		// RFC-BPR-0034 prohibits along with a second palette.
		card.Frame->setFrameShape(QFrame::StyledPanel);
		card.Frame->setAccessibleName(aTitle);
		auto* const column = new QVBoxLayout{ card.Frame };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		auto* const header = new QWidget{ card.Frame };
		header->setObjectName(aObjectName + QStringLiteral("Header"));
		header->setAutoFillBackground(true);
		// A native palette role, so the header tint follows every theme.
		// `Light` is the profile's raised surface: a near-white strip in
		// Light and a lifted, never bright, strip in Dark. AlternateBase is
		// the code current-line fill and is not settings chrome (BPR-1513).
		header->setBackgroundRole(QPalette::Light);
		card.Header = new QHBoxLayout{ header };
		card.Header->setContentsMargins(settings_card_inset, settings_row_inset, settings_card_inset, settings_row_inset);
		card.Header->setSpacing(settings_card_gap / 2);
		card.Title = new settings_card_title{ aTitle, header };
		card.Title->setObjectName(aObjectName + QStringLiteral("Title"));
		card.Header->addWidget(card.Title);
		card.Header->addStretch(1);
		column->addWidget(header);
		column->addWidget(make_settings_hairline(card.Frame));

		auto* const body = new QWidget{ card.Frame };
		body->setObjectName(aObjectName + QStringLiteral("Body"));
		card.Body = new QVBoxLayout{ body };
		card.Body->setContentsMargins(settings_card_inset, settings_row_inset / 2, settings_card_inset, settings_row_inset / 2);
		card.Body->setSpacing(0);
		column->addWidget(body);
		return card;
	}
} // namespace bpr::qt::detail
