#include "bpr/qt/workbench_landing.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRectF>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	workbench_landing::workbench_landing(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchHomeLanding"));
		setAccessibleName(QStringLiteral("Home"));
		setAccessibleDescription(QStringLiteral(
			"What to do next while nothing is open: sign in, or choose a pull "
			"request"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		Title = new QLabel{ this };
		Title->setObjectName(QStringLiteral("workbenchHomeTitle"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.35);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);

		Body = new QLabel{ this };
		Body->setObjectName(QStringLiteral("workbenchHomeBody"));
		Body->setTextFormat(Qt::PlainText);
		Body->setWordWrap(true);
		Body->setTextInteractionFlags(
			Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

		Primary = new QPushButton{ this };
		Primary->setObjectName(QStringLiteral("workbenchHomePrimaryButton"));
		Primary->setDefault(true);
		Primary->setFocusPolicy(Qt::StrongFocus);
		Secondary = new QPushButton{ this };
		Secondary->setObjectName(QStringLiteral("workbenchHomeSecondaryButton"));
		Secondary->setFlat(true);
		Secondary->setFocusPolicy(Qt::StrongFocus);
		connect(Primary, &QPushButton::clicked, this, [this] {
			if (PrimaryDispatch) {
				PrimaryDispatch();
			}
		});
		connect(Secondary, &QPushButton::clicked, this, [this] {
			if (SecondaryDispatch) {
				SecondaryDispatch();
			}
		});

		auto* const buttons = new QHBoxLayout;
		buttons->setContentsMargins(0, 0, 0, 0);
		buttons->setSpacing(8);
		buttons->addWidget(Primary);
		buttons->addWidget(Secondary);
		buttons->addStretch(1);

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(24, 20, 24, 20);
		column->setSpacing(8);
		column->addWidget(Title);
		column->addWidget(Body);
		column->addSpacing(4);
		column->addLayout(buttons);

		set_state(landing_state::sign_in);
	}

	void workbench_landing::set_state(const landing_state aState)
	{
		State = aState;
		switch (aState) {
		case landing_state::sign_in:
			Title->setText(QStringLiteral("Connect a GitHub account"));
			Body->setText(QStringLiteral(
				"BetterPullReview reviews pull requests from your GitHub accounts. "
				"Sign in once with a device code; the session is remembered on "
				"this Mac."));
			Primary->setText(QStringLiteral("Sign in to GitHub…"));
			Primary->setAccessibleName(QStringLiteral("Sign in to GitHub"));
			Primary->setAccessibleDescription(QStringLiteral(
				"Opens Accounts and hosts in Settings with the device sign-in ready."));
			// A doubled ampersand: a single one is a mnemonic marker on a button.
			Secondary->setText(QStringLiteral("Accounts && hosts settings"));
			Secondary->setAccessibleName(QStringLiteral("Accounts and hosts settings"));
			Secondary->setAccessibleDescription(QStringLiteral(
				"Opens the Accounts and hosts page of Settings."));
			break;
		case landing_state::choose_pull_request:
			Title->setText(QStringLiteral("Open a pull request"));
			Body->setText(QStringLiteral(
				"Pick one from the review queue in the files drawer, or enter its "
				"owner, repository and number under Accounts and hosts."));
			Primary->setText(QStringLiteral("Choose from the queue"));
			Primary->setAccessibleName(QStringLiteral("Choose from the queue"));
			Primary->setAccessibleDescription(QStringLiteral(
				"Moves to the review queue in the files drawer."));
			Secondary->setText(QStringLiteral("Enter a pull request…"));
			Secondary->setAccessibleName(QStringLiteral("Enter a pull request"));
			Secondary->setAccessibleDescription(QStringLiteral(
				"Opens Accounts and hosts in Settings with the locator ready."));
			break;
		}
		Primary->setToolTip(Primary->accessibleDescription());
		Secondary->setToolTip(Secondary->accessibleDescription());
		updateGeometry();
		update();
	}

	auto workbench_landing::state() const noexcept -> landing_state
	{
		return State;
	}

	auto workbench_landing::primary_button() const noexcept -> QPushButton*
	{
		return Primary;
	}

	auto workbench_landing::secondary_button() const noexcept -> QPushButton*
	{
		return Secondary;
	}

	auto workbench_landing::title_label() const noexcept -> QLabel*
	{
		return Title;
	}

	auto workbench_landing::body_label() const noexcept -> QLabel*
	{
		return Body;
	}

	void workbench_landing::set_primary_dispatch(std::function<void()> aDispatch)
	{
		PrimaryDispatch = std::move(aDispatch);
	}

	void workbench_landing::set_secondary_dispatch(std::function<void()> aDispatch)
	{
		SecondaryDispatch = std::move(aDispatch);
	}

	void workbench_landing::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		// A card on the window's ground, with the same rule the strip uses.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Button));
		painter.drawRoundedRect(
			QRectF{ rect() }.adjusted(12.5, 8.5, -12.5, -8.5), 8.0, 8.0);
	}
} // namespace bpr::qt
