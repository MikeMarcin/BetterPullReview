#include "bpr/qt/workbench_landing.hpp"

#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include "this_machine_text.hpp"

#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
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
		// [[BPR-1843]]. The Active review card: the accent's rail at its left,
		// its ground the accent washed into Base, its edge a little stronger.
		constexpr auto review_rail_width     = 4;
		constexpr auto review_card_radius    = 8.0;
		constexpr auto review_ground_tint    = 0.07;
		constexpr auto review_edge_tint      = 0.45;
		constexpr auto review_caption_tint   = 0.62;
		constexpr auto review_muted_strength = 0.68;
		constexpr auto review_card_padding   = 12;
		constexpr auto review_ink_contrast   = 4.5;
		constexpr auto review_ink_step       = 0.04;

		[[nodiscard]] auto relative_luminance(const QColor& aColour) -> qreal
		{
			const auto channel = [](const qreal aValue) {
				return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4);
			};
			const auto rgb = aColour.toRgb();
			return (0.2126 * channel(rgb.redF())) + (0.7152 * channel(rgb.greenF())) + (0.0722 * channel(rgb.blueF()));
		}

		[[nodiscard]] auto contrast_between(const QColor& aOne, const QColor& aOther) -> qreal
		{
			const auto one   = relative_luminance(aOne);
			const auto other = relative_luminance(aOther);
			return (std::max(one, other) + 0.05) / (std::min(one, other) + 0.05);
		}

		/// \brief [[BPR-1843]]. Close review's ×: a push button, so it keeps
		/// QPushButton's keyboard and accessibility behaviour, painted as a
		/// quiet × that lifts under the pointer and rings in the accent on focus.
		///
		/// \details Its text is empty, so nothing but the mark is drawn; its
		/// accessible name is "Close review". Drawn as vectors at the device's
		/// own scale, and sized from the font, so it follows display scaling.
		class close_mark_button final : public QPushButton
		{
		public:
			explicit close_mark_button(QWidget* aParent)
				: QPushButton{ aParent }
			{
				setAttribute(Qt::WA_Hover);
				setCursor(Qt::PointingHandCursor);
				setFlat(true);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto side = std::max(28, fontMetrics().height() + 12);
				return { side, side };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return sizeHint();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				const auto  base    = colours.color(QPalette::Active, QPalette::Base);
				const auto  text    = colours.color(QPalette::Active, QPalette::WindowText);
				const auto  lifted  = underMouse() || isDown();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
				if (lifted) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(mix_colour(text, base, isDown() ? 0.18 : 0.10));
					painter.drawEllipse(box);
				}
				const auto side   = std::max(8.0, box.width() * 0.34);
				const auto centre = box.center();
				const auto ink    = lifted || hasFocus() ? text : mix_colour(text, base, review_muted_strength);
				painter.setPen(QPen{ ink, 1.6, Qt::SolidLine, Qt::RoundCap });
				painter.drawLine(QPointF{ centre.x() - (side / 2.0), centre.y() - (side / 2.0) }, QPointF{ centre.x() + (side / 2.0), centre.y() + (side / 2.0) });
				painter.drawLine(QPointF{ centre.x() + (side / 2.0), centre.y() - (side / 2.0) }, QPointF{ centre.x() - (side / 2.0), centre.y() + (side / 2.0) });
				if (hasFocus()) {
					painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Highlight), 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawEllipse(box);
				}
			}

			void enterEvent(QEnterEvent* aEvent) override
			{
				QPushButton::enterEvent(aEvent);
				update();
			}

			void leaveEvent(QEvent* aEvent) override
			{
				QPushButton::leaveEvent(aEvent);
				update();
			}
		};
	}

	workbench_landing::workbench_landing(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchHomeLanding"));
		setAccessibleName(QStringLiteral("Home"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		Caption = new QLabel{ this };
		Caption->setObjectName(QStringLiteral("workbenchHomeActiveReviewCaption"));
		Caption->setTextFormat(Qt::PlainText);
		Caption->setText(QStringLiteral("Active review"));
		Caption->hide();

		Title = new QLabel{ this };
		Title->setObjectName(QStringLiteral("workbenchHomeTitle"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);

		// [[BPR-1843]]. The open review's literal title: remote text, plain,
		// on one line so the card keeps one height whichever review is open
		// and the lists under it never move ([[BPR-1381]]). Where the line is
		// too narrow it ends in an ellipsis, and the whole title is its
		// tooltip and part of the card's accessible description.
		ReviewTitleLabel = new QLabel{ this };
		ReviewTitleLabel->setObjectName(QStringLiteral("workbenchHomeActiveReviewTitle"));
		ReviewTitleLabel->setTextFormat(Qt::PlainText);
		ReviewTitleLabel->setWordWrap(false);
		ReviewTitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		ReviewTitleLabel->setMinimumWidth(0);
		ReviewTitleLabel->installEventFilter(this);
		ReviewTitleLabel->hide();

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
		Secondary = new quiet_push_button{ this };
		Secondary->setObjectName(QStringLiteral("workbenchHomeSecondaryButton"));
		Secondary->setAutoDefault(false);
		Secondary->setFocusPolicy(Qt::StrongFocus);
		Tertiary = new quiet_push_button{ this };
		Tertiary->setObjectName(QStringLiteral("workbenchHomeTertiaryButton"));
		Tertiary->setAutoDefault(false);
		Tertiary->setFocusPolicy(Qt::StrongFocus);
		// [[BPR-1573]] [[BPR-1843]]. The card's ×: the way back stays the
		// page's one filled control and Return's default.
		Close = new close_mark_button{ this };
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
		Header->addWidget(Title, 0, Qt::AlignTop);
		Header->addWidget(ReviewTitleLabel, 1, Qt::AlignTop);

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
		column->addWidget(Caption);
		column->addLayout(Header);
		column->addWidget(Body);
		column->addLayout(Buttons);

		Column = column;
		// [[BPR-1843]]. Over an open review its two controls stand at the
		// card's right, centred on its height.
		Aside = new QHBoxLayout;
		Aside->setContentsMargins(0, 0, 0, 0);
		Aside->setSpacing(8);

		Content = new QHBoxLayout{ this };
		Content->setSpacing(12);
		Content->addLayout(column, 1);
		Content->addLayout(Aside, 0);

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
			// Home over an open review closes nothing; the card above every
			// list is the way back, so an accidental Home costs one press.
			const auto reviewing = ReviewIdentity.isEmpty()
				? QStringLiteral("Reviewing a pull request")
				: QStringLiteral("Reviewing %1").arg(ReviewIdentity);
			// [[BPR-1843]]. The card says what is open: its identity, its
			// literal title beside it, and where the reading was left.
			Title->setText(ReviewIdentity.isEmpty() ? QStringLiteral("A pull request") : ReviewIdentity);
			fit_review_title();
			Body->setText(ReviewPosition.isEmpty() ? QStringLiteral("Open in this window")
												   : QStringLiteral("Left off at %1").arg(ReviewPosition));
			auto said = reviewing;
			if (!ReviewTitle.isEmpty()) {
				said = QStringLiteral("%1, %2").arg(said, ReviewTitle);
			}
			if (!ReviewPosition.isEmpty()) {
				said = QStringLiteral("%1, left off at %2").arg(said, ReviewPosition);
			}
			Primary->setText(QStringLiteral("Back to the review"));
			Primary->setAccessibleName(QStringLiteral("Back to the review"));
			// What it returns to is said on the button as well as on the card.
			// [[BPR-1573]]: its key is said there too, in the tooltip, not
			// beside the button (the owner, 2026-10-17: "Move this hotkey into
			// the tooltip").
			Primary->setAccessibleDescription(
				QStringLiteral("%1. Returns to it at the same file and change (%2).")
					.arg(reviewing, QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_H }.toString(QKeySequence::NativeText)));
			// The card has no second way in; its name must not linger on the
			// hidden one (review of BPR-0775, finding 5).
			Secondary->setText(QString{ });
			Secondary->setAccessibleName(QString{ });
			Secondary->setAccessibleDescription(QString{ });
			Tertiary->setText(QString{ });
			Tertiary->setAccessibleName(QString{ });
			Tertiary->setAccessibleDescription(QString{ });
			// [[BPR-1573]]. What closing does, said where it is done: the
			// review leaves this window, and its drafts wait for it.
			// [[BPR-1843]]. The × has no visible words; its name says it.
			Close->setText(QString{ });
			Close->setAccessibleName(QStringLiteral("Close review"));
			Close->setAccessibleDescription(
				QStringLiteral("%1. Closes it in this window; its drafts are kept for when you open it again.").arg(reviewing));
			setAccessibleName(QStringLiteral("Active review"));
			setAccessibleDescription(said);
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
		if (aState != landing_state::back_to_review) {
			setAccessibleName(QStringLiteral("Home"));
			ReviewTitleLabel->clear();
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
		Close->setText(QString{ });
		Close->setAccessibleName(QStringLiteral("Close review"));
		Close->setAccessibleDescription(QStringLiteral(
			"Closes the review open in this window; its drafts are kept for when you open it again."));
	}

	void workbench_landing::apply_arrangement()
	{
		const auto signingIn = State == landing_state::sign_in;
		const auto card      = signingIn || State == landing_state::connecting;
		const auto review    = State == landing_state::back_to_review;
		// The first-launch card holds the primary under its sentence; the
		// Active review card holds it, then its ×, at its right ([[BPR-1843]]).
		Aside->removeWidget(Primary);
		Aside->removeWidget(Close);
		Buttons->removeWidget(Primary);
		if (review) {
			Aside->addWidget(Primary, 0, Qt::AlignVCenter);
			Aside->addWidget(Close, 0, Qt::AlignVCenter);
		} else {
			Buttons->insertWidget(0, Primary);
		}
		auto titleFont = font();
		if (card) {
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
			titleFont.setWeight(QFont::DemiBold);
		} else if (review) {
			// [[BPR-1843]]. The identity leads the card at body size, set
			// heavier than its literal title beside it.
			titleFont.setWeight(QFont::DemiBold);
		}
		Title->setFont(titleFont);
		auto captionFont = font();
		captionFont.setPointSizeF(captionFont.pointSizeF() * 0.92);
		captionFont.setWeight(QFont::DemiBold);
		Caption->setFont(captionFont);
		Title->setVisible(State != landing_state::choose_pull_request);
		Caption->setVisible(review);
		ReviewTitleLabel->setVisible(review && !ReviewTitle.isEmpty());
		Body->setVisible(card || review);
		Title->setWordWrap(!review);
		Primary->setVisible(signingIn || review);
		Secondary->setVisible(signingIn && GithubCliAvailable);
		Tertiary->setVisible(signingIn);
		Close->setVisible(review);
		Column->setSpacing(review ? 2 : 8);
		// [[BPR-1843]]. Nothing offered, but the card's place is held.
		setMinimumHeight(State == landing_state::choose_pull_request ? HeldHeight : 0);
		if (card) {
			Content->setContentsMargins(
				page_inset + card_inset, row_inset + card_inset, page_inset + card_inset, row_inset + card_inset);
		} else if (review) {
			Content->setContentsMargins(page_inset + review_rail_width + card_inset, row_inset + review_card_padding,
										page_inset + review_card_padding, row_inset + review_card_padding);
		} else {
			Content->setContentsMargins(page_inset, row_inset, page_inset, row_inset);
		}
		apply_card_ink();
	}

	auto workbench_landing::review_card_ink_for(const QPalette& aPalette) -> review_card_ink
	{
		const auto base   = aPalette.color(QPalette::Active, QPalette::Base);
		const auto accent = aPalette.color(QPalette::Active, QPalette::Highlight);
		const auto text   = aPalette.color(QPalette::Active, QPalette::WindowText);
		const auto ground = mix_colour(accent, base, review_ground_tint);
		// A quieter ink, moved toward the text in fixed steps until it reads
		// on the card's ground at the WCAG AA ratio for body text.
		const auto readable = [&ground, &text](const QColor& aTone, qreal aStrength) {
			auto ink = mix_colour(aTone, text, aStrength);
			while (aStrength > 0.0 && contrast_between(ink, ground) < review_ink_contrast) {
				aStrength = std::max(0.0, aStrength - review_ink_step);
				ink       = mix_colour(aTone, text, aStrength);
			}
			return ink;
		};
		return review_card_ink{
			.Ground = ground,
			.Edge   = mix_colour(accent, base, review_edge_tint),
			.Rail   = accent,
			// The accent pulled toward the text, so the caption keeps the
			// text's contrast on the washed ground in every theme.
			.Caption = readable(accent, review_caption_tint),
			// The position: the text washed toward Base, as far as it reads.
			.Muted = readable(base, 1.0 - review_muted_strength),
		};
	}

	void workbench_landing::apply_card_ink()
	{
		if (ApplyingInk) {
			return;
		}
		ApplyingInk     = true;
		const auto ink  = review_card_ink_for(palette());
		const auto tint = [](QLabel& aLabel, const QColor& aInk) {
			auto colours = aLabel.palette();
			colours.setColor(QPalette::WindowText, aInk);
			aLabel.setPalette(colours);
		};
		tint(*Caption, ink.Caption);
		if (State == landing_state::back_to_review) {
			tint(*Body, ink.Muted);
		} else {
			// The first-launch card's sentence keeps the window's own ink.
			tint(*Body, palette().color(QPalette::WindowText));
		}
		ApplyingInk = false;
	}

	void workbench_landing::fit_review_title()
	{
		const auto width  = ReviewTitleLabel->width();
		const auto fitted = width > 0 ? ReviewTitleLabel->fontMetrics().elidedText(ReviewTitle, Qt::ElideRight, width) : ReviewTitle;
		ReviewTitleLabel->setText(fitted);
		ReviewTitleLabel->setToolTip(fitted == ReviewTitle ? QString{ } : ReviewTitle);
	}

	auto workbench_landing::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == ReviewTitleLabel && aEvent != nullptr
			&& (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::FontChange)) {
			fit_review_title();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void workbench_landing::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr && aEvent->type() == QEvent::PaletteChange) {
			apply_card_ink();
			update();
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

	void workbench_landing::set_held_height(const int aHeight)
	{
		if (HeldHeight == aHeight) {
			return;
		}
		HeldHeight = std::max(0, aHeight);
		apply_arrangement();
		updateGeometry();
	}

	void workbench_landing::set_review_title(const QString& aTitle)
	{
		if (ReviewTitle == aTitle) {
			return;
		}
		ReviewTitle = aTitle;
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

	auto workbench_landing::caption_label() const noexcept -> QLabel*
	{
		return Caption;
	}

	auto workbench_landing::review_title_label() const noexcept -> QLabel*
	{
		return ReviewTitleLabel;
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
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(page_inset + 0.5, row_inset + 0.5, -page_inset - 0.5, -row_inset - 0.5);
		if (State == landing_state::back_to_review) {
			// [[BPR-1843]]. The Active review card: the accent washed into
			// Base inside a stronger accent edge, and the accent's rail down
			// its left, clipped to the card's corners.
			const auto ink  = review_card_ink_for(palette());
			auto       card = QPainterPath{ };
			card.addRoundedRect(box, review_card_radius, review_card_radius);
			painter.fillPath(card, ink.Ground);
			painter.save();
			painter.setClipPath(card);
			painter.fillRect(QRectF{ box.left(), box.top(), static_cast<qreal>(review_rail_width), box.height() }, ink.Rail);
			painter.restore();
			painter.setPen(QPen{ ink.Edge, 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawPath(card);
			return;
		}
		// Only the first-launch card is drawn otherwise.
		if (State != landing_state::sign_in && State != landing_state::connecting) {
			return;
		}
		// A card on the window's ground: Base inside a Mid rule, one 8-point radius.
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(box, 8.0, 8.0);
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
