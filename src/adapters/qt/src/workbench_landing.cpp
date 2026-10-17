#include "bpr/qt/workbench_landing.hpp"

#include "this_machine_text.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
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
		// RFC-BPR-0032's rhythm: a page inset of 24 and a card inset of 16.
		constexpr auto page_inset = 24;
		constexpr auto card_inset = 16;
		constexpr auto row_inset  = 8;
	}

	workbench_landing::workbench_landing(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchHomeLanding"));
		setAccessibleName(QStringLiteral("Home"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		Title = new QLabel{ this };
		Title->setObjectName(QStringLiteral("workbenchHomeTitle"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);

		Body = new QLabel{ this };
		Body->setObjectName(QStringLiteral("workbenchHomeBody"));
		Body->setTextFormat(Qt::PlainText);
		Body->setWordWrap(true);
		Body->setTextInteractionFlags(
			Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

		Primary = new QPushButton{ this };
		Primary->setObjectName(QStringLiteral("workbenchHomePrimaryButton"));
		// The page's one filled control: Sign in on the card, the way back on the row.
		Primary->setDefault(true);
		Primary->setFocusPolicy(Qt::StrongFocus);
		Secondary = new QPushButton{ this };
		Secondary->setObjectName(QStringLiteral("workbenchHomeSecondaryButton"));
		Secondary->setAutoDefault(false);
		Secondary->setFocusPolicy(Qt::StrongFocus);
		Tertiary = new QPushButton{ this };
		Tertiary->setObjectName(QStringLiteral("workbenchHomeTertiaryButton"));
		Tertiary->setAutoDefault(false);
		Tertiary->setFocusPolicy(Qt::StrongFocus);
		// [[BPR-1573]]. Bordered like the card's second ways in: the way back
		// stays the page's one filled control and Return's default.
		Close = new QPushButton{ this };
		Close->setObjectName(QStringLiteral("workbenchHomeCloseReviewButton"));
		Close->setAutoDefault(false);
		Close->setFocusPolicy(Qt::StrongFocus);
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
		connect(Tertiary, &QPushButton::clicked, this, [this] {
			if (TertiaryDispatch) {
				TertiaryDispatch();
			}
		});
		connect(Close, &QPushButton::clicked, this, [this] {
			if (CloseDispatch) {
				CloseDispatch();
			}
		});

		Header = new QHBoxLayout;
		Header->setContentsMargins(0, 0, 0, 0);
		Header->setSpacing(8);
		Header->addWidget(Title, 1);

		Buttons = new QHBoxLayout;
		Buttons->setContentsMargins(0, 0, 0, 0);
		Buttons->setSpacing(8);
		Buttons->addWidget(Primary);
		Buttons->addWidget(Secondary);
		Buttons->addWidget(Tertiary);
		Buttons->addStretch(1);

		auto* const column = new QVBoxLayout;
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(8);
		column->addLayout(Header);
		column->addWidget(Body);
		column->addLayout(Buttons);

		Content = new QHBoxLayout{ this };
		Content->setSpacing(12);
		Content->addLayout(column, 1);

		set_state(landing_state::sign_in);
	}

	void workbench_landing::set_state(const landing_state aState)
	{
		State = aState;
		switch (aState) {
		case landing_state::sign_in:
			Title->setText(QStringLiteral("Sign in to GitHub"));
			// [[BPR-0775]]. The accepted first-launch card: one sentence, then
			// the two ways in. The bar above it reads a public pull request
			// without an account, and it says so itself. [[BPR-1137]]: the
			// machine is named by the product's rule, not the macOS mockup.
			Body->setText(
				QStringLiteral("A device code in the browser signs %1 in once; the session is "
							   "remembered here. Paste a pull request link to start a review.")
					.arg(detail::this_machine_text()));
			Primary->setText(QStringLiteral("Sign in"));
			Primary->setAccessibleName(QStringLiteral("Sign in to GitHub"));
			Primary->setAccessibleDescription(QStringLiteral(
				"Opens the sign-in dialog and begins a device sign-in."));
			Secondary->setText(QStringLiteral("Use GitHub CLI"));
			Secondary->setAccessibleName(QStringLiteral("Use GitHub CLI"));
			Secondary->setAccessibleDescription(QStringLiteral(
				"Checks the active authenticated hosts in your installed gh after you choose it."));
			Tertiary->setText(QStringLiteral("Add enterprise server"));
			Tertiary->setAccessibleName(QStringLiteral("Add enterprise server"));
			Tertiary->setAccessibleDescription(QStringLiteral(
				"Opens the sign-in dialog to add an enterprise server."));
			set_close_idle();
			setAccessibleDescription(
				GithubCliAvailable
					? QStringLiteral("Sign in to GitHub, use GitHub CLI, or add an enterprise server")
					: QStringLiteral("Sign in to GitHub or add an enterprise server"));
			break;
		case landing_state::back_to_review: {
			// Home over an open review closes nothing; the first thing on the
			// page is the way back, so an accidental Home costs one press.
			auto reviewing = ReviewIdentity.isEmpty()
				? QStringLiteral("Reviewing a pull request")
				: QStringLiteral("Reviewing %1").arg(ReviewIdentity);
			if (!ReviewPosition.isEmpty()) {
				reviewing = QStringLiteral("%1, %2").arg(reviewing, ReviewPosition);
			}
			Title->setText(reviewing);
			Body->clear();
			Primary->setText(QStringLiteral("Back to the review"));
			Primary->setAccessibleName(QStringLiteral("Back to the review"));
			// The way back stands beside the bar, so what it returns to is said
			// on the button rather than on a row of its own. [[BPR-1573]]: its
			// key is said there too, in the tooltip, not beside the button
			// (the owner, 2026-10-17: "Move this hotkey into the tooltip").
			Primary->setAccessibleDescription(
				QStringLiteral("%1. Returns to it at the same file and change (%2).")
					.arg(reviewing, QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_H }.toString(QKeySequence::NativeText)));
			// The row has no second button; its name must not linger on the
			// hidden one (review of BPR-0775, finding 5).
			Secondary->setText(QString{ });
			Secondary->setAccessibleName(QString{ });
			Secondary->setAccessibleDescription(QString{ });
			Tertiary->setText(QString{ });
			Tertiary->setAccessibleName(QString{ });
			Tertiary->setAccessibleDescription(QString{ });
			// [[BPR-1573]]. What closing does, said where it is done: the
			// review leaves this window, and its drafts wait for it.
			Close->setText(QStringLiteral("Close review"));
			Close->setAccessibleName(QStringLiteral("Close review"));
			Close->setAccessibleDescription(
				QStringLiteral("%1. Closes it in this window; its drafts are kept for when you open it again.").arg(reviewing));
			setAccessibleDescription(reviewing);
			break;
		}
		case landing_state::connecting:
			Title->setText(QStringLiteral("Connecting to GitHub…"));
			Body->setText(QStringLiteral("Checking your connection before loading your account."));
			Primary->setText({ });
			Primary->setAccessibleName({ });
			Primary->setAccessibleDescription({ });
			Secondary->setText({ });
			Secondary->setAccessibleName({ });
			Secondary->setAccessibleDescription({ });
			Tertiary->setText({ });
			Tertiary->setAccessibleName({ });
			Tertiary->setAccessibleDescription({ });
			set_close_idle();
			setAccessibleDescription(Title->text() + QStringLiteral(" ") + Body->text());
			break;
		case landing_state::choose_pull_request:
			// Nothing is offered; the hidden buttons carry no stale names a
			// reader or a selector could find.
			Title->clear();
			Body->clear();
			Primary->setText(QString{ });
			Primary->setAccessibleName(QString{ });
			Primary->setAccessibleDescription(QString{ });
			Secondary->setText(QString{ });
			Secondary->setAccessibleName(QString{ });
			Secondary->setAccessibleDescription(QString{ });
			Tertiary->setText(QString{ });
			Tertiary->setAccessibleName(QString{ });
			Tertiary->setAccessibleDescription(QString{ });
			set_close_idle();
			setAccessibleDescription(QString{ });
			break;
		}
		Primary->setToolTip(Primary->accessibleDescription());
		Secondary->setToolTip(Secondary->accessibleDescription());
		Tertiary->setToolTip(Tertiary->accessibleDescription());
		Close->setToolTip(Close->accessibleDescription());
		apply_arrangement();
		apply_illustration_layout();
		updateGeometry();
		update();
	}

	void workbench_landing::set_close_idle()
	{
		// [[BPR-1573]]. Hidden with no review open, but still a declared focus
		// stop of the window, so it keeps its one name and says what it does.
		Close->setText(QStringLiteral("Close review"));
		Close->setAccessibleName(QStringLiteral("Close review"));
		Close->setAccessibleDescription(QStringLiteral(
			"Closes the review open in this window; its drafts are kept for when you open it again."));
	}

	void workbench_landing::apply_arrangement()
	{
		const auto signingIn = State == landing_state::sign_in;
		const auto card      = signingIn || State == landing_state::connecting;
		const auto row       = State == landing_state::back_to_review;
		// The card and the row hold the same button in different places: under
		// the sentence on the card, at the right of the row.
		Header->removeWidget(Primary);
		Header->removeWidget(Close);
		Buttons->removeWidget(Primary);
		if (row) {
			// [[BPR-1573]]. The way back, then Close review: two buttons, the
			// way back's key in its tooltip.
			Header->addWidget(Primary);
			Header->addWidget(Close);
		} else {
			Buttons->insertWidget(0, Primary);
		}
		auto titleFont = font();
		if (card) {
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
			titleFont.setWeight(QFont::DemiBold);
		}
		Title->setFont(titleFont);
		// The way back is the bar's neighbour, its buttons only: a
		// row of its own above the bar came and went with the review, and moved
		// the bar and the list under the pointer when it did.
		Title->setVisible(!row);
		setSizePolicy(row ? QSizePolicy::Maximum : QSizePolicy::Expanding, QSizePolicy::Fixed);
		Body->setVisible(card);
		Primary->setVisible(signingIn || row);
		Secondary->setVisible(signingIn && GithubCliAvailable);
		Tertiary->setVisible(signingIn);
		Close->setVisible(row);
		if (card) {
			Content->setContentsMargins(
				page_inset + card_inset, row_inset + card_inset, page_inset + card_inset, row_inset + card_inset);
		} else if (row) {
			Content->setContentsMargins(0, 0, 0, 0);
		} else {
			Content->setContentsMargins(page_inset, row_inset, page_inset, row_inset);
		}
	}

	void workbench_landing::set_github_cli_available(const bool aAvailable)
	{
		if (GithubCliAvailable == aAvailable) {
			return;
		}
		GithubCliAvailable = aAvailable;
		set_state(State);
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

	void workbench_landing::set_review_position(const QString& aPosition)
	{
		if (ReviewPosition == aPosition) {
			return;
		}
		ReviewPosition = aPosition;
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

	auto workbench_landing::tertiary_button() const noexcept -> QPushButton*
	{
		return Tertiary;
	}

	auto workbench_landing::close_button() const noexcept -> QPushButton*
	{
		return Close;
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

	auto workbench_landing::take_illustration() -> QWidget*
	{
		if (Illustration == nullptr) {
			return nullptr;
		}
		auto* const result = Illustration;
		Content->removeWidget(result);
		Illustration = nullptr;
		result->hide();
		updateGeometry();
		return result;
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

	void workbench_landing::set_tertiary_dispatch(std::function<void()> aDispatch)
	{
		TertiaryDispatch = std::move(aDispatch);
	}

	void workbench_landing::set_close_dispatch(std::function<void()> aDispatch)
	{
		CloseDispatch = std::move(aDispatch);
	}

	void workbench_landing::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto       painter = QPainter{ this };
		const auto mid     = palette().color(QPalette::Active, QPalette::Mid);
		// Only the card is drawn; beside the bar the way back is its button.
		if (State != landing_state::sign_in && State != landing_state::connecting) {
			return;
		}
		// A card on the window's ground: Base inside a Mid rule, one 8-point radius.
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ mid, 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(
			QRectF{ rect() }.adjusted(page_inset + 0.5, row_inset + 0.5, -page_inset - 0.5, -row_inset - 0.5), 8.0, 8.0);
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
		if (!IllustrationEnabled || State != landing_state::sign_in || !illustration_space_available()) {
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
