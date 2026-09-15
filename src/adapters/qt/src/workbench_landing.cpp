#include "bpr/qt/workbench_landing.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRectF>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto compact_illustration_width = 1'100;
		constexpr auto hidden_illustration_width  = 960;
		constexpr auto normal_illustration_size   = 96;
		constexpr auto compact_illustration_size  = 72;
	}

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

		auto* const column = new QVBoxLayout;
		column->setContentsMargins(24, 20, 24, 20);
		column->setSpacing(8);
		column->addWidget(Title);
		column->addWidget(Body);
		column->addSpacing(4);
		column->addLayout(buttons);

		Content = new QHBoxLayout{ this };
		Content->setContentsMargins(0, 0, 0, 0);
		Content->setSpacing(12);
		Content->addLayout(column, 1);

		set_state(landing_state::sign_in);
	}

	void workbench_landing::set_state(const landing_state aState)
	{
		State = aState;
		switch (aState) {
		case landing_state::sign_in:
			Title->setText(QStringLiteral("Connect a GitHub account"));
			// [[BPR-0732]]. The bar directly below this card is the one way
			// in without an account: a pasted public link opens signed out,
			// read-only, in one gesture. The card says so; there is no
			// separate button and no second screen (which replaced the
			// [[BPR-0670]] "Read without signing in" button and its detour).
			Body->setText(QStringLiteral(
				"Paste a public pull request link, or owner/name#123, in the bar "
				"below to read it without signing in; comments and review actions "
				"stay read-only. Sign in once with a device code for your review "
				"queue; the session is remembered on this Mac."));
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
		case landing_state::back_to_review:
			// Home over an open review closes nothing; the first thing on the
			// page is the way back, so an accidental Home costs one press.
			Title->setText(
				ReviewIdentity.isEmpty()
					? QStringLiteral("Reviewing a pull request")
					: QStringLiteral("Reviewing %1").arg(ReviewIdentity));
			Body->setText(QStringLiteral(
				"The review is still open behind Home, exactly where you left it."));
			Primary->setText(QStringLiteral("Back to the review"));
			Primary->setAccessibleName(QStringLiteral("Back to the review"));
			Primary->setAccessibleDescription(QStringLiteral(
				"Returns to the open pull request at the same file and change."));
			Secondary->setText(QStringLiteral("Choose another pull request"));
			Secondary->setAccessibleName(QStringLiteral("Choose another pull request"));
			Secondary->setAccessibleDescription(
				QueueOffered ? QStringLiteral("Moves to the review queue below.")
							 : QStringLiteral("Moves to the bar below, which takes a pull request link or owner/name#number."));
			break;
		case landing_state::choose_pull_request:
			Title->setText(QStringLiteral("Open a pull request"));
			// [[BPR-0669]]. The bar below takes a pasted link; the locator in
			// Settings is no longer the way a pull request is named.
			Body->setText(QStringLiteral(
				"Paste a pull request link in the bar below, or pick one from the "
				"review queue."));
			Primary->setText(QStringLiteral("Choose from the queue"));
			Primary->setAccessibleName(QStringLiteral("Choose from the queue"));
			Primary->setAccessibleDescription(QStringLiteral(
				"Moves to the review queue in the files drawer."));
			Secondary->setText(QStringLiteral("Paste a link"));
			Secondary->setAccessibleName(QStringLiteral("Paste a pull request link"));
			Secondary->setAccessibleDescription(QStringLiteral(
				"Moves to the bar below, which takes a pull request link or owner/name#number."));
			break;
		}
		Primary->setToolTip(Primary->accessibleDescription());
		Secondary->setToolTip(Secondary->accessibleDescription());
		updateGeometry();
		update();
	}

	void workbench_landing::set_review_identity(const QString& aIdentity)
	{
		if (ReviewIdentity == aIdentity) {
			return;
		}
		ReviewIdentity = aIdentity;
		if (State == landing_state::back_to_review) {
			set_state(State);
		}
	}

	void workbench_landing::set_queue_offered(const bool aOffered)
	{
		if (QueueOffered == aOffered) {
			return;
		}
		QueueOffered = aOffered;
		if (State == landing_state::back_to_review) {
			set_state(State);
		}
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

	void workbench_landing::set_illustration(QWidget* const aIllustration)
	{
		if (Illustration == aIllustration) {
			return;
		}
		if (Illustration != nullptr) {
			Content->removeWidget(Illustration);
			Illustration->hide();
			Illustration->deleteLater();
		}
		Illustration = aIllustration;
		if (Illustration != nullptr) {
			Illustration->setParent(this);
			Illustration->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			Content->addWidget(Illustration, 0, Qt::AlignCenter);
		}
		apply_illustration_layout();
		updateGeometry();
	}

	auto workbench_landing::illustration() const noexcept -> QWidget*
	{
		return Illustration;
	}

	auto workbench_landing::illustration_space_available() const noexcept -> bool
	{
		return width() >= hidden_illustration_width;
	}

	void workbench_landing::set_illustration_enabled(const bool aEnabled)
	{
		if (IllustrationEnabled == aEnabled) {
			return;
		}
		IllustrationEnabled = aEnabled;
		apply_illustration_layout();
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

	void workbench_landing::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		apply_illustration_layout();
	}

	void workbench_landing::apply_illustration_layout()
	{
		if (Illustration == nullptr) {
			return;
		}
		if (!IllustrationEnabled || !illustration_space_available()) {
			Illustration->hide();
			return;
		}
		const auto side = width() < compact_illustration_width
			? compact_illustration_size
			: normal_illustration_size;
		Illustration->setFixedSize(side, side);
		Illustration->show();
	}
} // namespace bpr::qt
