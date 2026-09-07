#include "bpr/qt/account_card.hpp"

#include "github_presentation_text.hpp"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRectF>

namespace bpr::qt {
	account_card::account_card(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("accountCard"));
		setAccessibleName(QStringLiteral("Account"));
		setAccessibleDescription(QStringLiteral(
			"The signed-in account, its host, its connection state and its rate limit"));
		auto* const grid = new QGridLayout{ this };
		grid->setContentsMargins(16, 12, 16, 12);
		grid->setHorizontalSpacing(10);
		grid->setVerticalSpacing(2);

		Avatar = new QLabel{ this };
		Avatar->setObjectName(QStringLiteral("accountAvatar"));
		Avatar->setFixedSize(28, 28);
		Avatar->setAlignment(Qt::AlignCenter);
		Avatar->setAutoFillBackground(true);
		Avatar->setBackgroundRole(QPalette::Mid);
		auto avatarFont = Avatar->font();
		avatarFont.setWeight(QFont::DemiBold);
		Avatar->setFont(avatarFont);

		Login = new QLabel{ this };
		Login->setObjectName(QStringLiteral("accountLogin"));
		Login->setTextFormat(Qt::PlainText);
		auto loginFont = Login->font();
		loginFont.setWeight(QFont::DemiBold);
		Login->setFont(loginFont);
		Detail = new QLabel{ this };
		Detail->setObjectName(QStringLiteral("accountDetail"));
		Detail->setTextFormat(Qt::PlainText);
		Detail->setWordWrap(true);
		Detail->setForegroundRole(QPalette::PlaceholderText);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("accountStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setAlignment(Qt::AlignRight | Qt::AlignTop);
		RateLimit = new QLabel{ this };
		RateLimit->setObjectName(QStringLiteral("accountRateLimit"));
		RateLimit->setTextFormat(Qt::PlainText);
		RateLimit->setWordWrap(true);
		RateLimit->setForegroundRole(QPalette::PlaceholderText);

		grid->addWidget(Avatar, 0, 0, 2, 1, Qt::AlignTop);
		grid->addWidget(Login, 0, 1);
		grid->addWidget(Status, 0, 2, Qt::AlignRight);
		grid->addWidget(Detail, 1, 1, 1, 2);
		grid->addWidget(RateLimit, 2, 1, 1, 2);
		grid->setColumnStretch(1, 1);
		clear();
	}

	void account_card::apply(const application::github_session_projection& aSession, const QString& aHost)
	{
		SignedIn         = !aSession.Login.empty();
		const auto login = QString::fromStdString(aSession.Login);
		Avatar->setText(login.isEmpty() ? QStringLiteral("?") : login.left(1).toUpper());
		Login->setText(login.isEmpty() ? QStringLiteral("No account") : login);
		auto detail = QString{ };
		if (!aSession.DisplayName.empty() && aSession.DisplayName != aSession.Login) {
			detail = QString::fromStdString(aSession.DisplayName);
		}
		if (!aHost.isEmpty()) {
			detail += detail.isEmpty() ? aHost : QStringLiteral(" on %1").arg(aHost);
		}
		Detail->setText(detail);
		if (aSession.RateLimits.Core.has_value()) {
			RateLimit->setText(detail::rate_limit_line(QStringLiteral("GitHub API"), *aSession.RateLimits.Core));
			RateLimit->show();
		} else {
			RateLimit->clear();
			RateLimit->hide();
		}
		update();
	}

	void account_card::set_status(const QString& aStatus, const bool aConnected)
	{
		Connected = aConnected;
		Status->setText(aStatus);
		auto colours = palette();
		colours.setColor(QPalette::WindowText,
						 aConnected ? QColor{ 0x1A, 0x7F, 0x37 } : palette().color(QPalette::Active, QPalette::PlaceholderText));
		Status->setPalette(colours);
		Status->setForegroundRole(QPalette::WindowText);
		update();
	}

	void account_card::clear()
	{
		SignedIn = false;
		Avatar->setText(QStringLiteral("?"));
		Login->setText(QStringLiteral("No account"));
		Detail->setText(QStringLiteral("Sign in with a device code to review from an account."));
		RateLimit->clear();
		RateLimit->hide();
		set_status(QStringLiteral("Signed out"), false);
	}

	auto account_card::login_label() const noexcept -> QLabel*
	{
		return Login;
	}

	auto account_card::detail_label() const noexcept -> QLabel*
	{
		return Detail;
	}

	auto account_card::status_label() const noexcept -> QLabel*
	{
		return Status;
	}

	auto account_card::rate_limit_label() const noexcept -> QLabel*
	{
		return RateLimit;
	}

	auto account_card::signed_in() const noexcept -> bool
	{
		return SignedIn;
	}

	void account_card::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
	}
} // namespace bpr::qt
