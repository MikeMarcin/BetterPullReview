#include "bpr/qt/about_window.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QClipboard>
#include <QColor>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The logo's side and corner, in logical pixels. The master is 640
		/// pixels square, enough for this side at 3x.
		constexpr auto logo_side   = 176;
		constexpr auto logo_corner = 16.0;
		/// How long Copy version info says Copied.
		constexpr auto copied_milliseconds = 2000;

		[[nodiscard]] auto logo_master() -> const QImage&
		{
			static const auto master = QImage{ QStringLiteral(":/bpr/about/game-core-tech-logo.png") };
			return master;
		}

		/// `aBase` at `aFactor` of its size and `aWeight`, in points or pixels
		/// as the base is given.
		[[nodiscard]] auto scaled_font(QFont aBase, const qreal aFactor, const QFont::Weight aWeight) -> QFont
		{
			if (aBase.pointSizeF() > 0.0) {
				aBase.setPointSizeF(aBase.pointSizeF() * aFactor);
			} else if (aBase.pixelSize() > 0) {
				aBase.setPixelSize(std::max(1, static_cast<int>(std::lround(aBase.pixelSize() * aFactor))));
			}
			aBase.setWeight(aWeight);
			return aBase;
		}

		[[nodiscard]] auto centred_label(const QString& aText, QWidget* const aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
			return label;
		}

		[[nodiscard]] auto link_button(const QString& aText, const QString& aDescription, QWidget* const aParent, const QString& aName) -> QToolButton*
		{
			auto* const button = new QToolButton{ aParent };
			button->setObjectName(aName);
			button->setText(aText);
			button->setAutoRaise(true);
			button->setCursor(Qt::PointingHandCursor);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolTip(aDescription);
			button->setAccessibleName(aText);
			button->setAccessibleDescription(aDescription);
			anchor_tooltips(*button);
			return button;
		}

		[[nodiscard]] auto page_site(const core::reference_page aPage) -> QString
		{
			switch (aPage) {
			case core::reference_page::conventional_comments: return QStringLiteral("conventionalcomments.org");
			case core::reference_page::game_core_tech:        return QStringLiteral("gamecoretech.com");
			case core::reference_page::source_repository:     return QStringLiteral("github.com/MikeMarcin/BetterPullReview");
			case core::reference_page::product_site:          return QStringLiteral("betterpullreview.com");
			}
			return { };
		}

		[[nodiscard]] auto document_title(const about_document aDocument) -> QString
		{
			switch (aDocument) {
			case about_document::third_party_notices: return QStringLiteral("third-party notices");
			case about_document::source_offer:        return QStringLiteral("corresponding-source offer");
			}
			return { };
		}

		[[nodiscard]] auto short_commit(const QString& aCommit) -> QString { return aCommit.left(10); }
	}

	auto about_build_line(const about_facts& aFacts) -> QString
	{
		if (aFacts.Commit.isEmpty()) {
			return QStringLiteral("No commit is recorded for this build");
		}
		auto line = aFacts.CommitDate.isEmpty() ? QStringLiteral("Commit %1").arg(short_commit(aFacts.Commit))
												: QStringLiteral("Commit %1 from %2").arg(short_commit(aFacts.Commit), aFacts.CommitDate);
		if (aFacts.LocalChanges) {
			line += QStringLiteral(", with local changes");
		}
		return line;
	}

	auto about_version_report(const about_facts& aFacts) -> QString
	{
		auto commit = aFacts.Commit.isEmpty() ? QStringLiteral("unavailable") : aFacts.Commit;
		if (!aFacts.Commit.isEmpty() && !aFacts.CommitDate.isEmpty()) {
			commit += QStringLiteral(" (%1)").arg(aFacts.CommitDate);
		}
		if (aFacts.LocalChanges) {
			commit += QStringLiteral(", with local changes");
		}
		const auto lines = QStringList{
			QStringLiteral("BetterPullReview %1").arg(aFacts.Version),
			QStringLiteral("Commit: %1").arg(commit),
			QStringLiteral("Configuration: %1").arg(aFacts.Configuration),
			QStringLiteral("Qt: %1").arg(aFacts.QtVersion),
			QStringLiteral("System: %1").arg(aFacts.System),
		};
		return lines.join(QLatin1Char{ '\n' }) + QLatin1Char{ '\n' };
	}

	about_window::about_window(about_facts aFacts, about_actions aActions, QWidget* const aParent)
		: QDialog{ aParent }
		, Facts{ std::move(aFacts) }
		, Actions{ std::move(aActions) }
	{
		setObjectName(QStringLiteral("aboutWindow"));
		// A window does not inherit its owner's palette and font unless asked:
		// with this it takes the workbench's theme, and follows a change to it.
		setAttribute(Qt::WA_WindowPropagation);
		setWindowTitle(QStringLiteral("About BetterPullReview"));
		setAccessibleName(QStringLiteral("About BetterPullReview"));
		setAccessibleDescription(QStringLiteral("The version and build of BetterPullReview, who makes it, and its licences"));

		// One column, centred, as the platform's About panels are. Its width and
		// type come from the font it inherits (apply_metrics), so it reads alike
		// at every scale; the spacing from the font it opens with.
		const auto em = QFontMetrics{ aParent != nullptr ? aParent->font() : font() }.horizontalAdvance(QLatin1Char{ 'M' });

		auto* const layout = new QVBoxLayout{ this };
		Column             = layout;
		layout->setSizeConstraint(QLayout::SetFixedSize);
		layout->setSpacing(0);

		// The one bold element: the maker's mark on its own dark plate.
		Logo = new QLabel{ this };
		Logo->setObjectName(QStringLiteral("aboutLogo"));
		Logo->setFixedSize(logo_side, logo_side);
		Logo->setAccessibleName(QStringLiteral("Game Core Tech logo"));
		layout->addWidget(Logo, 0, Qt::AlignHCenter);
		layout->addSpacing(em + em / 2);

		Name = centred_label(QStringLiteral("BetterPullReview"), this, QStringLiteral("aboutName"));
		layout->addWidget(Name);
		layout->addSpacing(em / 3);

		Version = centred_label(QStringLiteral("Version %1").arg(Facts.Version), this, QStringLiteral("aboutVersion"));
		Version->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(Version);
		layout->addSpacing(em / 6);

		Build = centred_label(about_build_line(Facts), this, QStringLiteral("aboutBuild"));
		Build->setForegroundRole(QPalette::PlaceholderText);
		Build->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Build->setWordWrap(true);
		if (!Facts.Commit.isEmpty()) {
			Build->setToolTip(Facts.Commit);
			anchor_tooltips(*Build);
		}
		layout->addWidget(Build, 0, Qt::AlignHCenter);
		layout->addSpacing(em);

		Copy = new quiet_push_button{ QStringLiteral("Copy version info"), this };
		Copy->setObjectName(QStringLiteral("aboutCopyVersionInfo"));
		Copy->setAutoDefault(false);
		Copy->setAccessibleDescription(QStringLiteral("Copies the version, commit, configuration, Qt and system to the clipboard, for a bug report"));
		{
			// Wide enough for either wording, so "Copied" does not move it.
			const auto shown = Copy->text();
			Copy->setText(QStringLiteral("Copied"));
			const auto copied = Copy->sizeHint().width();
			Copy->setText(shown);
			Copy->setMinimumWidth(std::max(copied, Copy->sizeHint().width()));
		}
		layout->addWidget(Copy, 0, Qt::AlignHCenter);
		CopiedReset = new QTimer{ this };
		CopiedReset->setSingleShot(true);
		CopiedReset->setInterval(copied_milliseconds);
		connect(CopiedReset, &QTimer::timeout, this, [this] { Copy->setText(QStringLiteral("Copy version info")); });
		connect(Copy, &QPushButton::clicked, this, [this] { copy_version_report(); });
		layout->addSpacing(em + em / 2);

		// The application's own two links side by side, its site first; then
		// who makes it, with the maker's site under the credit (owner
		// amendment, 2026-10-21).
		auto* const links = new QHBoxLayout{ };
		links->setSpacing(em / 2);
		links->addStretch(1);
		Product = link_button(QStringLiteral("betterpullreview.com"), QStringLiteral("Opens betterpullreview.com in your browser."), this, QStringLiteral("aboutProductLink"));
		Source  = link_button(QStringLiteral("Source code"), QStringLiteral("Opens github.com/MikeMarcin/BetterPullReview in your browser."), this, QStringLiteral("aboutSourceLink"));
		links->addWidget(Product);
		links->addWidget(Source);
		links->addStretch(1);
		layout->addLayout(links);
		connect(Product, &QAbstractButton::clicked, this, [this] { open_page(core::reference_page::product_site); });
		connect(Source, &QAbstractButton::clicked, this, [this] { open_page(core::reference_page::source_repository); });
		layout->addSpacing(em);

		MakerLine = centred_label(QStringLiteral("Made by Game Core Tech"), this, QStringLiteral("aboutMaker"));
		layout->addWidget(MakerLine);
		Maker = link_button(QStringLiteral("gamecoretech.com"), QStringLiteral("Opens gamecoretech.com in your browser."), this, QStringLiteral("aboutMakerLink"));
		layout->addWidget(Maker, 0, Qt::AlignHCenter);
		connect(Maker, &QAbstractButton::clicked, this, [this] { open_page(core::reference_page::game_core_tech); });
		layout->addSpacing(em);

		// The rule divides who made it from the terms it comes under.
		auto* const rule = new QFrame{ this };
		rule->setObjectName(QStringLiteral("aboutRule"));
		rule->setFrameShape(QFrame::HLine);
		rule->setFrameShadow(QFrame::Plain);
		rule->setForegroundRole(QPalette::Mid);
		Rule = rule;
		layout->addWidget(rule, 0, Qt::AlignHCenter);
		layout->addSpacing(em);

		const auto legal = [this](const QString& aText, const QString& aName) {
			auto* const label = centred_label(aText, this, aName);
			label->setForegroundRole(QPalette::PlaceholderText);
			label->setWordWrap(true);
			Small.push_back(label);
			return label;
		};
		layout->addWidget(legal(Facts.Copyright + QStringLiteral(" Released under the MIT License."), QStringLiteral("aboutCopyright")), 0, Qt::AlignHCenter);
		layout->addSpacing(em / 3);
		layout->addWidget(legal(QStringLiteral("Qt %1 is used under the GNU LGPL version\u00a03; you may replace it.").arg(Facts.QtVersion),
								QStringLiteral("aboutQtLicence")),
						  0, Qt::AlignHCenter);
		layout->addSpacing(em / 3);

		auto* const documents = new QHBoxLayout{ };
		documents->setSpacing(em / 2);
		documents->addStretch(1);
		Notices = link_button(QStringLiteral("Third-party notices"), QStringLiteral("Opens THIRD-PARTY-NOTICES.txt, every component's licence, in your text viewer."), this, QStringLiteral("aboutNoticesLink"));
		Offer   = link_button(QStringLiteral("Corresponding source"), QStringLiteral("Opens SOURCE-OFFERS.txt: where this release's source is, and how to rebuild and replace its LGPL libraries."), this, QStringLiteral("aboutSourceOfferLink"));
		Notices->setVisible(Facts.NoticesInstalled);
		Offer->setVisible(Facts.SourceOfferInstalled);
		documents->addWidget(Notices);
		documents->addWidget(Offer);
		documents->addStretch(1);
		layout->addLayout(documents);
		connect(Notices, &QAbstractButton::clicked, this, [this] { open_document(about_document::third_party_notices); });
		connect(Offer, &QAbstractButton::clicked, this, [this] { open_document(about_document::source_offer); });
		DocumentsAbsent = legal(QStringLiteral("Built from source, so no notices or source offer are installed."),
								QStringLiteral("aboutDocumentsAbsent"));
		DocumentsAbsent->setVisible(!Facts.NoticesInstalled && !Facts.SourceOfferInstalled);
		layout->addWidget(DocumentsAbsent, 0, Qt::AlignHCenter);

		Status = centred_label(QString{ }, this, QStringLiteral("aboutStatus"));
		Status->setWordWrap(true);
		Status->hide();
		layout->addSpacing(em / 2);
		layout->addWidget(Status, 0, Qt::AlignHCenter);

		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Close, this };
		buttons->setObjectName(QStringLiteral("aboutButtons"));
		Close = buttons->button(QDialogButtonBox::Close);
		Close->setDefault(true);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		layout->addSpacing(em / 2);
		layout->addWidget(buttons);

		setTabOrder(Product, Source);
		setTabOrder(Source, Maker);
		setTabOrder(Maker, Copy);
		setTabOrder(Copy, Notices);
		setTabOrder(Notices, Offer);
		setTabOrder(Offer, Close);
		apply_metrics();
		apply_link_ink();
		ring_keyboard_focus(*this, QStringLiteral("aboutFocusRing"));
		render_logo();
	}

	void about_window::apply_metrics()
	{
		const auto base   = font();
		const auto em     = QFontMetrics{ base }.horizontalAdvance(QLatin1Char{ 'M' });
		const auto column = std::max(em * 26, logo_side + em * 4);
		const auto small  = scaled_font(base, 0.92, QFont::Normal);
		Column->setContentsMargins(em * 2, em * 2, em * 2, em);
		Name->setFont(scaled_font(base, 1.75, QFont::DemiBold));
		MakerLine->setFont(scaled_font(base, 1.0, QFont::Medium));
		for (auto* const label : Small) {
			label->setFont(small);
			label->setFixedWidth(column);
		}
		Notices->setFont(small);
		Offer->setFont(small);
		for (auto* const wide : { static_cast<QWidget*>(Build), Rule, static_cast<QWidget*>(Status) }) {
			wide->setFixedWidth(column);
		}
	}

	about_window::~about_window() = default;

	auto about_window::facts() const noexcept -> const about_facts& { return Facts; }
	auto about_window::logo() const noexcept -> QLabel* { return Logo; }
	auto about_window::version_label() const noexcept -> QLabel* { return Version; }
	auto about_window::build_label() const noexcept -> QLabel* { return Build; }
	auto about_window::copy_button() const noexcept -> QPushButton* { return Copy; }
	auto about_window::documents_absent_label() const noexcept -> QLabel* { return DocumentsAbsent; }
	auto about_window::status_label() const noexcept -> QLabel* { return Status; }
	auto about_window::close_button() const noexcept -> QPushButton* { return Close; }

	auto about_window::page_button(const core::reference_page aPage) const noexcept -> QAbstractButton*
	{
		switch (aPage) {
		case core::reference_page::game_core_tech:        return Maker;
		case core::reference_page::source_repository:     return Source;
		case core::reference_page::product_site:          return Product;
		case core::reference_page::conventional_comments: break;
		}
		return nullptr;
	}

	auto about_window::document_button(const about_document aDocument) const noexcept -> QAbstractButton*
	{
		switch (aDocument) {
		case about_document::third_party_notices: return Notices;
		case about_document::source_offer:        return Offer;
		}
		return nullptr;
	}

	void about_window::open_page(const core::reference_page aPage)
	{
		const auto result = Actions.OpenPage ? Actions.OpenPage(aPage) : core::markdown_link_result::unavailable;
		const auto site   = page_site(aPage);
		switch (result) {
		case core::markdown_link_result::opened:      show_status(QString{ }); break;
		case core::markdown_link_result::stale:
		case core::markdown_link_result::unavailable: show_status(QStringLiteral("There is no browser to open %1 with.").arg(site)); break;
		case core::markdown_link_result::refused:     show_status(QStringLiteral("%1 cannot be opened safely from here.").arg(site)); break;
		case core::markdown_link_result::failed:      show_status(QStringLiteral("The browser could not open %1. Open that address yourself.").arg(site)); break;
		}
	}

	void about_window::open_document(const about_document aDocument)
	{
		if (Actions.OpenDocument && Actions.OpenDocument(aDocument)) {
			show_status(QString{ });
			return;
		}
		const auto title = document_title(aDocument);
		show_status(Facts.LegalFolder.isEmpty() ? QStringLiteral("The %1 did not open.").arg(title)
												: QStringLiteral("The %1 did not open. They are in %2.").arg(title, Facts.LegalFolder));
	}

	void about_window::copy_version_report()
	{
		if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
			clipboard->setText(about_version_report(Facts));
		}
		Copy->setText(QStringLiteral("Copied"));
		CopiedReset->start();
		auto announcement = QAccessibleAnnouncementEvent{ Copy, QStringLiteral("Version info copied") };
		announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&announcement);
	}

	void about_window::show_status(const QString& aMessage)
	{
		Status->setText(aMessage);
		Status->setAccessibleName(aMessage);
		Status->setVisible(!aMessage.isEmpty());
	}

	void about_window::render_logo()
	{
		const auto& master = logo_master();
		const auto  scale  = devicePixelRatioF();
		if (master.isNull() || Logo == nullptr) {
			return;
		}
		LogoScale         = scale;
		const auto pixels = static_cast<int>(std::lround(logo_side * scale));
		auto       plate  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
		plate.fill(Qt::transparent);
		{
			auto painter = QPainter{ &plate };
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
			const auto bounds = QRectF{ 0.0, 0.0, static_cast<qreal>(pixels), static_cast<qreal>(pixels) };
			auto       shape  = QPainterPath{ };
			shape.addRoundedRect(bounds, logo_corner * scale, logo_corner * scale);
			painter.setClipPath(shape);
			// Area-averaged once to the exact device size, so every scale is
			// drawn from the master rather than from a smaller copy.
			painter.drawImage(QPointF{ 0.0, 0.0 }, master.scaled(pixels, pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			painter.setClipping(false);
			// A hairline in the text colour: lost against the plate in a light
			// theme, a lifted edge in a dark one, where the plate would
			// otherwise sink into the window.
			auto edge = palette().color(QPalette::Active, QPalette::WindowText);
			edge.setAlphaF(0.16F);
			painter.setPen(QPen{ edge, scale });
			painter.setBrush(Qt::NoBrush);
			const auto inset = scale / 2.0;
			painter.drawRoundedRect(bounds.adjusted(inset, inset, -inset, -inset), logo_corner * scale - inset, logo_corner * scale - inset);
		}
		auto pixmap = QPixmap::fromImage(plate);
		pixmap.setDevicePixelRatio(scale);
		Logo->setPixmap(pixmap);
	}

	void about_window::apply_link_ink()
	{
		for (auto* const button : { Product, Source, Maker, Notices, Offer }) {
			auto ink = button->palette();
			ink.setColor(QPalette::ButtonText, readable_link_colour(palette()));
			button->setPalette(ink);
		}
	}

	auto about_window::event(QEvent* const aEvent) -> bool
	{
		const auto handled = QDialog::event(aEvent);
		if (aEvent->type() == QEvent::DevicePixelRatioChange && !qFuzzyCompare(LogoScale, devicePixelRatioF())) {
			render_logo();
		}
		return handled;
	}

	void about_window::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Logo != nullptr) {
			apply_link_ink();
			render_logo();
		}
		if (aEvent->type() == QEvent::FontChange && Column != nullptr) {
			apply_metrics();
		}
	}

	void about_window::showEvent(QShowEvent* const aEvent)
	{
		QDialog::showEvent(aEvent);
		apply_metrics();
		if (!qFuzzyCompare(LogoScale, devicePixelRatioF())) {
			render_logo();
		}
		Close->setFocus(Qt::OtherFocusReason);
	}

	about_menu::about_menu(about_facts aFacts, about_actions aActions)
		: Facts{ std::move(aFacts) }
		, Actions{ std::move(aActions) }
	{
	}

	about_menu::~about_menu() = default;

	void about_menu::attach(QMenuBar& aMenu)
	{
		if (std::ranges::any_of(Menus, [&aMenu](const auto& aKnown) { return aKnown == &aMenu; })) {
			return;
		}
		Menus.emplace_back(&aMenu);
		auto* help = aMenu.findChild<QMenu*>(QStringLiteral("releaseUpdateMenu"), Qt::FindDirectChildrenOnly);
		if (help == nullptr) {
			help = aMenu.addMenu(QStringLiteral("&Help"));
			help->setObjectName(QStringLiteral("releaseUpdateMenu"));
		}
		// macOS moves the item to the application menu; a separator left
		// behind would end Help with a rule.
		if (QGuiApplication::platformName() != QStringLiteral("cocoa") && !help->actions().isEmpty()) {
			help->addSeparator();
		}
		auto* const action = help->addAction(QStringLiteral("About BetterPullReview"));
		action->setObjectName(QStringLiteral("aboutAction"));
		action->setMenuRole(QAction::AboutRole);
		action->setStatusTip(QStringLiteral("The version, build and licences of BetterPullReview"));
		connect(action, &QAction::triggered, this, [this, menu = QPointer<QMenuBar>{ &aMenu }] {
			if (menu) {
				static_cast<void>(present(*menu));
			}
		});
	}

	auto about_menu::present(QMenuBar& aMenu) -> about_window*
	{
		auto* page = static_cast<about_window*>(nullptr);
		if (aMenu.parentWidget() != nullptr) {
			auto* const owner = aMenu.window();
			page              = dynamic_cast<about_window*>(owner->findChild<QDialog*>(QStringLiteral("aboutWindow"), Qt::FindDirectChildrenOnly));
			if (page == nullptr) {
				page = new about_window{ Facts, Actions, owner };
			}
		} else {
			if (!Unowned) {
				Unowned = std::make_unique<about_window>(Facts, Actions);
			}
			page = Unowned.get();
		}
		page->show();
		page->raise();
		page->activateWindow();
		return page;
	}
}
