#include "bpr/qt/home_refusal.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRectF>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

#include <array>
#include <string_view>
#include <utility>

namespace bpr::qt {
	namespace {
		// RFC-BPR-0032's rhythm: a page inset of 24 and a card inset of 16.
		constexpr auto page_inset = 24;
		constexpr auto card_inset = 16;
		constexpr auto row_gap    = 8;

		[[nodiscard]] auto from_core(const std::string_view aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto object_name(const core::pull_request_refusal_action aAction) -> QString
		{
			switch (aAction) {
			case core::pull_request_refusal_action::sign_in:
				return QStringLiteral("homeRefusalSignIn");
			case core::pull_request_refusal_action::use_github_cli:
				return QStringLiteral("homeRefusalUseGithubCli");
			case core::pull_request_refusal_action::use_another_account:
				return QStringLiteral("homeRefusalUseAnotherAccount");
			}
			return QStringLiteral("homeRefusalSignIn");
		}

		constexpr auto every_action = std::array{
			core::pull_request_refusal_action::sign_in,
			core::pull_request_refusal_action::use_github_cli,
			core::pull_request_refusal_action::use_another_account,
		};
	}

	home_refusal::home_refusal(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("homeRefusal"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		Icon = new QLabel{ this };
		Icon->setObjectName(QStringLiteral("homeRefusalIcon"));
		// The icon repeats what the heading says; a reader hears the heading.
		Icon->setAccessibleName(QString{ });
		Icon->setFocusPolicy(Qt::NoFocus);

		Heading = new QLabel{ this };
		Heading->setObjectName(QStringLiteral("homeRefusalHeading"));
		Heading->setTextFormat(Qt::PlainText);
		Heading->setWordWrap(true);
		Heading->setTextInteractionFlags(Qt::TextSelectableByMouse);

		Explanation = new QLabel{ this };
		Explanation->setObjectName(QStringLiteral("homeRefusalExplanation"));
		Explanation->setTextFormat(Qt::PlainText);
		Explanation->setWordWrap(true);
		Explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);

		Buttons = new QHBoxLayout;
		Buttons->setContentsMargins(0, 0, 0, 0);
		Buttons->setSpacing(row_gap);
		for (std::size_t index = 0U; index < action_count; ++index) {
			const auto action = every_action[index];
			auto*      button = new quiet_push_button{ from_core(core::label(action)), this };
			button->setObjectName(object_name(action));
			// Home's one filled control stays the page's own; a refusal's steps
			// are bordered beside it.
			button->setAutoDefault(false);
			button->setDefault(false);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setAccessibleName(from_core(core::label(action)));
			button->setAccessibleDescription(from_core(core::consequence(action)));
			button->setToolTip(from_core(core::consequence(action)));
			button->hide();
			connect(button, &QPushButton::clicked, this, [this, action] {
				if (ActionDispatch) {
					ActionDispatch(action);
				}
			});
			Actions[index] = button;
		}
		CopyDetails = new quiet_push_button{ QStringLiteral("Copy details"), this };
		CopyDetails->setObjectName(QStringLiteral("homeRefusalCopyDetails"));
		CopyDetails->setAccessibleName(QStringLiteral("Copy the details of the refusal"));
		CopyDetails->setFlat(true);
		CopyDetails->setAutoDefault(false);
		CopyDetails->setFocusPolicy(Qt::StrongFocus);
		connect(CopyDetails, &QPushButton::clicked, this, [this] {
			if (CopyDispatch) {
				CopyDispatch();
			}
		});

		auto* const column = new QVBoxLayout;
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(row_gap);
		column->addWidget(Heading);
		column->addWidget(Explanation);
		column->addLayout(Buttons);

		auto* const content = new QHBoxLayout{ this };
		content->setContentsMargins(page_inset + card_inset, row_gap + card_inset, page_inset + card_inset, row_gap + card_inset);
		content->setSpacing(12);
		content->addWidget(Icon, 0, Qt::AlignTop);
		content->addLayout(column, 1);

		refresh_presentation();
		hide();
	}

	void home_refusal::present(const core::pull_request_refusal_wording& aWording)
	{
		Wording = aWording;
		Heading->setText(from_core(Wording.Heading));
		Explanation->setText(from_core(Wording.Explanation));
		setAccessibleName(Heading->text());
		setAccessibleDescription(Explanation->text());
		// The core's order is the row's order: the step the heading names first,
		// then the others, then Copy details.
		for (auto* const button : Actions) {
			Buttons->removeWidget(button);
			button->hide();
		}
		Buttons->removeWidget(CopyDetails);
		while (Buttons->count() > 0) {
			delete Buttons->takeAt(0);
		}
		for (const auto action : Wording.Actions) {
			auto* const button = action_button(action);
			if (button != nullptr) {
				Buttons->addWidget(button);
				button->show();
			}
		}
		Buttons->addWidget(CopyDetails);
		Buttons->addStretch(1);
		updateGeometry();
		update();
	}

	void home_refusal::set_action_dispatch(action_dispatch aDispatch)
	{
		ActionDispatch = std::move(aDispatch);
	}

	void home_refusal::set_copy_dispatch(std::function<void()> aDispatch)
	{
		CopyDispatch = std::move(aDispatch);
	}

	auto home_refusal::text() const -> QString
	{
		return Heading->text() + QLatin1Char{ '\n' } + Explanation->text();
	}

	auto home_refusal::heading_label() const noexcept -> QLabel*
	{
		return Heading;
	}

	auto home_refusal::explanation_label() const noexcept -> QLabel*
	{
		return Explanation;
	}

	auto home_refusal::icon_label() const noexcept -> QLabel*
	{
		return Icon;
	}

	auto home_refusal::action_button(const core::pull_request_refusal_action aAction) const noexcept -> QPushButton*
	{
		for (std::size_t index = 0U; index < action_count; ++index) {
			if (every_action[index] == aAction) {
				return Actions[index];
			}
		}
		return nullptr;
	}

	auto home_refusal::offered_buttons() const -> std::vector<QPushButton*>
	{
		auto result = std::vector<QPushButton*>{ };
		for (const auto action : Wording.Actions) {
			if (auto* const button = action_button(action); button != nullptr) {
				result.push_back(button);
			}
		}
		return result;
	}

	auto home_refusal::copy_details_button() const noexcept -> QPushButton*
	{
		return CopyDetails;
	}

	auto home_refusal::sizeHint() const -> QSize
	{
		auto hint = QWidget::sizeHint();
		// Wrapped words are as tall as the width they are given makes them. The
		// default hint guesses a narrower width, and Home would then give the
		// card height its words do not use.
		if (layout() != nullptr && layout()->hasHeightForWidth() && width() > 0) {
			hint.setHeight(layout()->totalHeightForWidth(width()));
		}
		return hint;
	}

	void home_refusal::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		if (aEvent != nullptr && aEvent->oldSize().width() != aEvent->size().width()) {
			updateGeometry();
		}
	}

	void home_refusal::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		// A card on the window's ground, as the first-launch card is: Base inside
		// a Mid rule with one 8-point radius. The icon and the heading are what
		// set it apart from the helper text around the bar.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(
			QRectF{ rect() }.adjusted(page_inset + 0.5, row_gap + 0.5, -page_inset - 0.5, -row_gap - 0.5), 8.0, 8.0);
	}

	void home_refusal::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr
			&& (aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::FontChange
				|| aEvent->type() == QEvent::PaletteChange)) {
			refresh_presentation();
		}
	}

	void home_refusal::refresh_presentation()
	{
		// Derived from the current style and font, so a theme or font change
		// redraws the icon and the heading rather than keeping stale values.
		auto headingFont = font();
		headingFont.setPointSizeF(headingFont.pointSizeF() * 1.15);
		headingFont.setWeight(QFont::DemiBold);
		Heading->setFont(headingFont);
		const auto extent = style()->pixelMetric(QStyle::PM_LargeIconSize, nullptr, this);
		Icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this).pixmap(extent, extent));
		Icon->setFixedSize(extent, extent);
	}
}
