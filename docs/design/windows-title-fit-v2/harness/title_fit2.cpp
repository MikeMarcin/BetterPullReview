// Scratch design harness for docs/design/windows-title-fit-v2. Not product code.
//
// It builds the application's own title row (workbench_strip, strip_style,
// strip_icon, modality_control, windows_caption_buttons, compiled from the
// lane's sources) and lays it out the way workbench_window::apply_strip_width
// and update_strip_identity do, then grabs it without showing a window.
// The fifth mode, the branch chip of the accepted Revise design and the
// folding switcher under study are built here from the strip's own buttons,
// style and glyphs; nothing in the switcher is painted by this file.
#include "conversation_glyphs.hpp"

#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/workbench_strip.hpp"
#if defined(Q_OS_WIN)
#	include "bpr/qt/windows_caption_buttons.hpp"
#endif

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

using namespace bpr::qt;

namespace {
	// ---------------------------------------------------------------- palette
	[[nodiscard]] auto mix(const QColor& aFirst, const QColor& aSecond, const double aSecondShare) -> QColor
	{
		return QColor::fromRgbF(
			static_cast<float>(aFirst.redF() * (1.0 - aSecondShare) + aSecond.redF() * aSecondShare),
			static_cast<float>(aFirst.greenF() * (1.0 - aSecondShare) + aSecond.greenF() * aSecondShare),
			static_cast<float>(aFirst.blueF() * (1.0 - aSecondShare) + aSecond.blueF() * aSecondShare));
	}

	[[nodiscard]] auto luminance(const QColor& aColour) -> double
	{
		const auto channel = [](const double aValue) {
			return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4);
		};
		return 0.2126 * channel(aColour.redF()) + 0.7152 * channel(aColour.greenF()) + 0.0722 * channel(aColour.blueF());
	}

	[[nodiscard]] auto contrast(const QColor& aFirst, const QColor& aSecond) -> double
	{
		const auto first  = luminance(aFirst);
		const auto second = luminance(aSecond);
		return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
	}

	// appearance_profile::derive, for the built-in Dark and Light profiles
	// (appearance_profile.cpp: dark_profile, light_profile).
	[[nodiscard]] auto profile_palette(const bool aDark) -> QPalette
	{
		const auto surface     = aDark ? QColor{ 0x1C, 0x20, 0x26 } : QColor{ 0xF6, 0xF7, 0xF9 };
		const auto surfaceText = aDark ? QColor{ 0xE6, 0xED, 0xF3 } : QColor{ 0x1F, 0x23, 0x28 };
		const auto base        = aDark ? QColor{ 0x0D, 0x11, 0x17 } : QColor{ 0xFF, 0xFF, 0xFF };
		const auto baseText    = aDark ? QColor{ 0xDC, 0xDC, 0xDC } : QColor{ 0x1F, 0x23, 0x28 };
		const auto alternate   = aDark ? QColor{ 0x16, 0x1C, 0x24 } : QColor{ 0xF2, 0xF6, 0xFC };
		const auto highlight   = aDark ? QColor{ 0x28, 0x51, 0x8C } : QColor{ 0x7F, 0xB0, 0xE8 };
		const auto highlighted = contrast(highlight, baseText) >= contrast(highlight, base) ? baseText : base;
		auto       muted       = mix(baseText, base, 0.5);
		for (auto share = 0.5; share > 0.0 && (contrast(muted, base) < 4.6 || contrast(muted, surface) < 4.5); share -= 0.02) {
			muted = mix(baseText, base, share);
		}
		auto palette = QPalette{ };
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			const auto set = [&palette, group](const QPalette::ColorRole aRole, const QColor& aColour) {
				palette.setColor(group, aRole, aColour);
			};
			set(QPalette::Window, surface);
			set(QPalette::WindowText, surfaceText);
			set(QPalette::Base, base);
			set(QPalette::AlternateBase, alternate);
			set(QPalette::Text, baseText);
			set(QPalette::Highlight, highlight);
			set(QPalette::HighlightedText, highlighted);
			set(QPalette::Button, surface);
			set(QPalette::ButtonText, surfaceText);
			set(QPalette::Light, surface.lighter(150));
			set(QPalette::Midlight, surface.lighter(115));
			set(QPalette::Mid, surface.darker(150));
			set(QPalette::Dark, surface.darker(200));
			set(QPalette::Shadow, surface.darker(300));
			set(QPalette::ToolTipBase, surface);
			set(QPalette::ToolTipText, surfaceText);
			set(QPalette::PlaceholderText, muted);
		}
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, mix(surfaceText, surface, 0.5));
		palette.setColor(QPalette::Disabled, QPalette::WindowText, mix(surfaceText, surface, 0.5));
		return palette;
	}

	// ------------------------------------------------------------------ faces
	struct face
	{
		QString              Family; // empty: the platform's general font
		double               Points = 0.0; // 0: the platform's size
		int                  Weight = 0; // 0: the platform's weight
		std::optional<float> OpticalSize;
		double               Tracking = 0.0; // letter spacing, pixels
	};

	[[nodiscard]] auto parse_face(const QString& aText) -> face
	{
		// Family|points|weight|opsz|tracking, every field optional.
		const auto parts = aText.split(QLatin1Char{ '|' });
		auto       made  = face{ };
		if (!parts.isEmpty()) {
			made.Family = parts[0].trimmed();
		}
		if (parts.size() > 1 && !parts[1].trimmed().isEmpty()) {
			made.Points = parts[1].toDouble();
		}
		if (parts.size() > 2 && !parts[2].trimmed().isEmpty()) {
			made.Weight = parts[2].toInt();
		}
		if (parts.size() > 3 && !parts[3].trimmed().isEmpty()) {
			made.OpticalSize = parts[3].toFloat();
		}
		if (parts.size() > 4 && !parts[4].trimmed().isEmpty()) {
			made.Tracking = parts[4].toDouble();
		}
		return made;
	}

	// --ui: the general font's size when the session misreports it. Over SSH
	// the station's session has no DPI awareness and reads the message font's
	// 150% pixel height as if at 96 DPI.
	std::optional<face> ui_override;

	[[nodiscard]] auto system_font() -> QFont
	{
		// appearance_profile.cpp default_typography_settings: the UI font is
		// QFontDatabase::GeneralFont, family, point size and weight.
		const auto system = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
		auto       font   = QFont{ };
		font.setFamily(system.family());
		font.setPointSizeF(system.pointSizeF());
		font.setWeight(system.weight());
		if (ui_override.has_value()) {
			if (!ui_override->Family.isEmpty()) {
				font.setFamily(ui_override->Family);
			}
			if (ui_override->Points > 0.0) {
				font.setPointSizeF(ui_override->Points);
			}
		}
		return font;
	}

	[[nodiscard]] auto make_font(const face& aFace) -> QFont
	{
		auto font = system_font();
		if (!aFace.Family.isEmpty()) {
			font.setFamily(aFace.Family);
		}
		if (aFace.Points > 0.0) {
			font.setPointSizeF(aFace.Points);
		}
		if (aFace.Weight > 0) {
			font.setWeight(static_cast<QFont::Weight>(aFace.Weight));
		}
		if (aFace.OpticalSize.has_value()) {
			font.setVariableAxis(QFont::Tag{ "opsz" }, *aFace.OpticalSize);
		}
		if (aFace.Tracking != 0.0) {
			font.setLetterSpacing(QFont::AbsoluteSpacing, aFace.Tracking);
		}
		return font;
	}

	// ------------------------------------------------------------------ modes
	constexpr std::size_t mode_count = 5U;

	[[nodiscard]] auto mode_title(const std::size_t aIndex) -> QString
	{
		return aIndex < review_modality_count ? review_modality_title(static_cast<review_modality>(aIndex)) : QStringLiteral("Revise");
	}

	[[nodiscard]] auto mode_key(const std::size_t aIndex) -> QString
	{
		// The fifth key is the accepted Revise design's: Ctrl+5 (Command-5).
		const auto key = aIndex < review_modality_count ? review_modality_shortcut(static_cast<review_modality>(aIndex)) : QKeySequence{ QStringLiteral("Ctrl+5") };
		return key.toString(QKeySequence::NativeText);
	}

	// A mode's glyph, from the sets the drawers' tabs and tools already draw
	// (conversation_glyphs.cpp, workbench_strip.cpp). Nothing is new art.
	[[nodiscard]] auto mode_icon(const std::size_t aIndex, const QPalette& aPalette, const bool aOnAccent) -> QIcon
	{
		const auto ink = aPalette.color(QPalette::Active, aOnAccent ? QPalette::HighlightedText : QPalette::ButtonText);
		switch (aIndex) {
		case 0U: // Overview: the rendered post, three lines of text with a heading.
			return conversation_icon(conversation_glyph::rendered, ink);
		case 1U: // Review: source, angle brackets around a slash.
			return conversation_icon(conversation_glyph::source, ink);
		case 2U: // Discuss: the speech balloon the Comments tab wears.
			return conversation_icon(conversation_glyph::comment, ink);
		case 3U: // Threads: the finder, a thread of posts with a lens.
			return conversation_icon(conversation_glyph::finder, ink);
		default: { // Revise: the Edit view's frame with a caret.
			auto palette = aPalette;
			palette.setColor(QPalette::ButtonText, ink);
			return strip_icon(strip_glyph::edit_file, palette);
		}
		}
	}

	// The switcher: modality_control's frame, row and buttons, with any number
	// of modes. Each segment is the strip's own tool button; a face only sets
	// its text, its icon and its tool-button style.
	class switcher final : public QWidget
	{
	public:
		switcher(workbench_strip& aStrip, const std::size_t aCount, const std::size_t aCurrent, QWidget* const aParent)
			: QWidget{ aParent }
			, Current{ aCurrent }
		{
			setFocusPolicy(Qt::NoFocus);
			auto* const row = new QHBoxLayout{ this };
			row->setContentsMargins(2, 2, 2, 2);
			row->setSpacing(2);
			for (std::size_t index = 0U; index < aCount; ++index) {
				auto* const button = new QToolButton{ this };
				button->setCheckable(true);
				button->setToolButtonStyle(Qt::ToolButtonTextOnly);
				button->setProperty(strip_style::filled_property, true);
				aStrip.adopt_button(button);
				button->setFixedHeight(strip_style::button_height - 4);
				button->setChecked(index == aCurrent);
				Segments.push_back(button);
				row->addWidget(button);
			}
		}

		void show_segment(const std::size_t aIndex, const QString& aText, const bool aGlyph)
		{
			auto* const button = Segments[aIndex];
			button->setText(aText);
			button->setIcon(aGlyph ? mode_icon(aIndex, palette(), aIndex == Current) : QIcon{ });
			button->setToolButtonStyle(!aGlyph ? Qt::ToolButtonTextOnly : aText.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
			button->updateGeometry();
		}

		std::vector<QToolButton*> Segments;
		std::size_t               Current = 0U;

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			// modality_control::paintEvent.
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
			painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
		}
	};

	// The branch chip of the accepted Revise design (local-draft-workflow-v2):
	// the head branch and how far it is ahead, after the name. Not in the
	// application today.
	class branch_chip final : public QWidget
	{
	public:
		branch_chip(QString aBranch, QString aAhead, const bool aDark, QWidget* const aParent)
			: QWidget{ aParent }
			, Branch{ std::move(aBranch) }
			, Ahead{ std::move(aAhead) }
			, Dark{ aDark }
		{
			setFocusPolicy(Qt::NoFocus);
			setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		}

		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			const auto metrics = fontMetrics();
			return QSize{ pad + glyph + gap + metrics.horizontalAdvance(Branch) + (Ahead.isEmpty() ? 0 : gap + metrics.horizontalAdvance(Ahead)) + pad, std::max(20, metrics.height() + 4) };
		}

		[[nodiscard]] auto minimumSizeHint() const -> QSize override
		{
			return sizeHint();
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			painter.setPen(QPen{ mix(palette().color(QPalette::Window), palette().color(QPalette::WindowText), 0.28), 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
			const auto ink = mix(palette().color(QPalette::WindowText), palette().color(QPalette::Window), 0.22);
			auto       pen = QPen{ ink, 1.2 };
			pen.setCapStyle(Qt::RoundCap);
			painter.setPen(pen);
			const auto left = static_cast<double>(pad);
			const auto top  = (height() - glyph) / 2.0;
			painter.drawEllipse(QPointF{ left + 3.0, top + 2.6 }, 1.6, 1.6);
			painter.drawEllipse(QPointF{ left + 3.0, top + 9.4 }, 1.6, 1.6);
			painter.drawEllipse(QPointF{ left + 9.0, top + 4.0 }, 1.6, 1.6);
			painter.drawLine(QPointF{ left + 3.0, top + 4.2 }, QPointF{ left + 3.0, top + 7.8 });
			auto curve = QPainterPath{ QPointF{ left + 9.0, top + 5.6 } };
			curve.quadTo(QPointF{ left + 9.0, top + 7.8 }, QPointF{ left + 4.6, top + 8.0 });
			painter.drawPath(curve);
			const auto metrics = fontMetrics();
			const auto line    = (height() - metrics.height()) / 2.0 + metrics.ascent();
			auto       x       = left + glyph + gap;
			painter.setPen(ink);
			painter.drawText(QPointF{ x, line }, Branch);
			x += metrics.horizontalAdvance(Branch) + gap;
			painter.setPen(Dark ? QColor{ 0xD2, 0x99, 0x22 } : QColor{ 0x8A, 0x5A, 0x00 });
			painter.drawText(QPointF{ x, line }, Ahead);
		}

	private:
		static constexpr int pad   = 8;
		static constexpr int glyph = 12;
		static constexpr int gap   = 5;

		QString Branch;
		QString Ahead;
		bool    Dark;
	};

	// The three window buttons macOS draws in the row's leading inset. They
	// are the system's, not the application's; drawn here only so the inset
	// does not read as spare room.
	class window_buttons final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(Qt::NoPen);
			auto x = 19.0;
			for (const auto colour : { QColor{ 0xFF, 0x5F, 0x57 }, QColor{ 0xFE, 0xBC, 0x2E }, QColor{ 0x28, 0xC8, 0x40 } }) {
				painter.setBrush(colour);
				painter.drawEllipse(QPointF{ x, height() / 2.0 }, 6.0, 6.0);
				x += 20.0;
			}
		}
	};

	// ------------------------------------------------------------------- rows
	enum class option { today, fold };

	[[nodiscard]] auto platform_key() -> QString
	{
#if defined(Q_OS_WIN)
		return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
		return QStringLiteral("macos");
#else
		return QStringLiteral("linux");
#endif
	}

	struct content
	{
		QString Owner  = QStringLiteral("bpr-fixtures");
		QString Repo   = QStringLiteral("engine");
		QString Number = QStringLiteral("214");
		QString Title  = QStringLiteral("Rank search results by match quality and cap the list at result_limit");
		QString Branch = QStringLiteral("feature/search-results");
		QString Ahead  = QStringLiteral("↑2");
	};

	struct row
	{
		option           Option = option::today;
		QWidget*         Window = nullptr;
		workbench_strip* Strip  = nullptr;
		QLabel*          Identity = nullptr;
		QAction*         IdentityAction = nullptr;
		QAction*         GithubAction   = nullptr;
		switcher*        Modes          = nullptr;
		content          Content;
		QString          Face;
		QString          IdentityForm;
		int              Forced = -1; // a shape to show whatever the room, for the specimens
		std::vector<std::pair<QString, QWidget*>> Items;
	};

	constexpr int identity_title_minimum  = 80; // workbench_window_home_landing.cpp
	constexpr int identity_fragment_width = 24;
#if defined(Q_OS_WIN)
	constexpr int compact_below = 1'300; // apply_strip_width
#else
	constexpr int compact_below = 1'100;
#endif

	void remeasure(const row& aRow)
	{
		if (auto* const layout = aRow.Strip->layout(); layout != nullptr) {
			layout->invalidate();
		}
	}

	// workbench_window::strip_identity_budget.
	[[nodiscard]] auto identity_budget(const row& aRow, const int aWidth) -> int
	{
		constexpr int slack    = 12;
		const auto    identity = aRow.IdentityAction->isVisible() ? aRow.Identity->sizeHint().width() : 0;
		return aWidth - (aRow.Strip->sizeHint().width() - identity) - slack;
	}

	// workbench_window::strip_identity_minimum.
	[[nodiscard]] auto identity_minimum(const row& aRow) -> int
	{
		const auto margins = aRow.Identity->contentsMargins();
		return aRow.Identity->fontMetrics().horizontalAdvance(QStringLiteral("  #%1").arg(aRow.Content.Number)) + identity_fragment_width + margins.left() + margins.right();
	}

	[[nodiscard]] auto whole_identity(const content& aContent) -> QString
	{
		return QStringLiteral("%1/%2  #%3 %4").arg(aContent.Owner, aContent.Repo, aContent.Number, aContent.Title);
	}

	[[nodiscard]] auto owned_identity(const content& aContent) -> QString
	{
		return QStringLiteral("%1/%2  #%3").arg(aContent.Owner, aContent.Repo, aContent.Number);
	}

	// workbench_window::update_strip_identity, the GitHub pull request branch.
	void update_identity(row& aRow, const int aWidth, const int aTitleBudget)
	{
		const auto& metrics    = aRow.Identity->fontMetrics();
		const auto  margins    = aRow.Identity->contentsMargins();
		const auto  room       = std::max(identity_budget(aRow, aWidth) - margins.left() - margins.right(), 0);
		const auto  shown      = whole_identity(aRow.Content);
		const auto  number     = QStringLiteral("  #%1").arg(aRow.Content.Number);
		const auto  owned      = owned_identity(aRow.Content);
		const auto  titled     = metrics.horizontalAdvance(owned) + identity_title_minimum;
		const auto  bare       = aRow.Content.Repo + number;
		const auto  wholeRoom  = aTitleBudget - margins.left() - margins.right();
		const auto  ownedWidth = metrics.horizontalAdvance(owned);
		auto        text       = QString{ };
		if (room >= titled && wholeRoom >= titled) {
			text              = metrics.elidedText(shown, Qt::ElideRight, room);
			aRow.IdentityForm = text == shown ? QStringLiteral("whole title") : QStringLiteral("title cut");
		} else if (room >= ownedWidth && wholeRoom >= ownedWidth) {
			text              = owned;
			aRow.IdentityForm = QStringLiteral("owner/repository and number");
		} else if (room >= metrics.horizontalAdvance(bare)) {
			text              = bare;
			aRow.IdentityForm = QStringLiteral("repository and number");
		} else {
			const auto budget = room - metrics.horizontalAdvance(number);
			text              = metrics.elidedText(aRow.Content.Repo, Qt::ElideMiddle, std::max(budget, identity_fragment_width)) + number;
			aRow.IdentityForm = QStringLiteral("repository cut, number");
		}
		aRow.Identity->setText(text);
		remeasure(aRow);
	}

	// Widest first. keys, names and prefixes are modality_control's own
	// three forms; open_named and glyphs are the drawer tabs' two narrow
	// faces (fit_tray_tabs: shape::open_named, shape::glyphs).
	enum class shape { keys, names, prefixes, open_named, glyphs };

	[[nodiscard]] auto face_name(const shape aFace) -> QString
	{
		switch (aFace) {
		case shape::keys:
			return QStringLiteral("names and keys");
		case shape::names:
			return QStringLiteral("names");
		case shape::prefixes:
			return QStringLiteral("three-letter prefixes");
		case shape::open_named:
			return QStringLiteral("icons, the open mode named");
		case shape::glyphs:
			return QStringLiteral("icons");
		}
		return { };
	}

	void set_face(row& aRow, const shape aFace)
	{
		for (std::size_t index = 0U; index < aRow.Modes->Segments.size(); ++index) {
			const auto title = mode_title(index);
			switch (aFace) {
			case shape::keys: // modality_control::relabel: "%1  %2".
				aRow.Modes->show_segment(index, QStringLiteral("%1  %2").arg(title, mode_key(index)), false);
				break;
			case shape::names:
				aRow.Modes->show_segment(index, title, false);
				break;
			case shape::prefixes:
				aRow.Modes->show_segment(index, title.left(3), false);
				break;
			case shape::open_named:
				aRow.Modes->show_segment(index, index == aRow.Modes->Current ? title : QString{ }, true);
				break;
			case shape::glyphs:
				aRow.Modes->show_segment(index, QString{ }, true);
				break;
			}
		}
		aRow.Modes->updateGeometry();
		aRow.Face = face_name(aFace);
		remeasure(aRow);
	}

	void fit(row& aRow, const int aWidth)
	{
		const auto identityShort = [&aRow, aWidth] {
			return aRow.IdentityAction->isVisible() && identity_budget(aRow, aWidth) < identity_minimum(aRow);
		};
		const auto margins = aRow.Identity->contentsMargins();
		const auto needs   = [&aRow, &margins](const QString& aText) {
			return aRow.Identity->fontMetrics().horizontalAdvance(aText) + margins.left() + margins.right();
		};
		aRow.IdentityAction->setVisible(true);
		aRow.GithubAction->setVisible(true);
		aRow.Identity->setText(whole_identity(aRow.Content));
		if (aRow.Option == option::today) {
			// workbench_window::apply_strip_width as it stands.
			set_face(aRow, shape::keys);
			const auto titleBudget = identity_budget(aRow, aWidth);
			set_face(aRow, aWidth < compact_below ? shape::names : shape::keys);
			if (identityShort()) {
				set_face(aRow, shape::names);
			}
			if (identityShort()) {
				aRow.IdentityAction->setVisible(false);
				aRow.GithubAction->setVisible(false);
				aRow.IdentityForm = QStringLiteral("hidden");
				remeasure(aRow);
			} else {
				update_identity(aRow, aWidth, titleBudget);
			}
			if (aRow.Strip->sizeHint().width() > aWidth) {
				set_face(aRow, shape::prefixes);
				if (aRow.IdentityAction->isVisible()) {
					update_identity(aRow, aWidth, titleBudget);
				}
			}
			return;
		}
		if (aRow.Forced >= 0) {
			set_face(aRow, static_cast<shape>(aRow.Forced));
			update_identity(aRow, aWidth, identity_budget(aRow, aWidth));
			return;
		}
		// The fold, as the drawer tabs take their faces (fit_tray_tabs): the
		// widest face beside which the whole name fits. When none does, the
		// open mode keeps its name while a title of identity_title_minimum
		// still fits beside it; the last face is icons alone.
		auto chosen = shape::glyphs;
		for (const auto candidate : { shape::keys, shape::names, shape::open_named }) {
			set_face(aRow, candidate);
			const auto room = identity_budget(aRow, aWidth);
			if (room >= needs(whole_identity(aRow.Content)) || (candidate == shape::open_named && room >= needs(owned_identity(aRow.Content)) + identity_title_minimum)) {
				chosen = candidate;
				break;
			}
		}
		set_face(aRow, chosen);
		if (identityShort()) {
			aRow.IdentityAction->setVisible(false);
			aRow.GithubAction->setVisible(false);
			aRow.IdentityForm = QStringLiteral("hidden");
			remeasure(aRow);
		} else {
			update_identity(aRow, aWidth, identity_budget(aRow, aWidth));
		}
	}

	[[nodiscard]] auto build(const option aOption, const bool aDark, const content& aContent, const std::size_t aCurrent) -> row
	{
		const auto palette = profile_palette(aDark);
		auto       made    = row{ };
		made.Option        = aOption;
		made.Content       = aContent;
		made.Window        = new QWidget{ };
		made.Window->setAttribute(Qt::WA_DontShowOnScreen, true);
		made.Window->setPalette(palette);
		made.Window->setFont(system_font());
		auto* const column = new QVBoxLayout{ made.Window };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		auto* const strip = new workbench_strip{ QStringLiteral("Title row"), made.Window, strip_role::title_row };
		made.Strip        = strip;
		column->addWidget(strip);

		const auto note = [&made](const QString& aName, QWidget* const aWidget) {
			made.Items.emplace_back(aName, aWidget);
		};
		const auto action = [&](const QString& aName, const strip_glyph aGlyph, const bool aCheckable, const bool aChecked) {
			auto* const made_action = new QAction{ strip_icon(aGlyph, palette), aName, strip };
			made_action->setCheckable(aCheckable);
			made_action->setChecked(aChecked);
			strip->addAction(made_action);
			note(aName, strip->widgetForAction(made_action));
			return made_action;
		};

		// workbench_window_construction.cpp, the title row's order per platform.
#if defined(Q_OS_MACOS)
		auto* const inset = new window_buttons{ strip };
		inset->setFixedWidth(title_row_leading_inset(title_row_platform::macos));
		strip->addWidget(inset);
		note(QStringLiteral("Window buttons"), inset);
#elif defined(Q_OS_WIN)
		auto* const menu = new QToolButton{ strip };
		menu->setToolButtonStyle(Qt::ToolButtonIconOnly);
		menu->setIcon(windows_menu_icon(palette));
		strip->addWidget(menu);
		note(QStringLiteral("Menu"), menu);
#endif
		action(QStringLiteral("Home"), strip_glyph::home, true, false);
		action(QStringLiteral("Repositories"), strip_glyph::repositories_drawer, true, false);
		action(QStringLiteral("Files"), strip_glyph::files_drawer, true, true);

		made.Identity = new QLabel{ strip };
		made.Identity->setTextFormat(Qt::PlainText);
		made.Identity->setFocusPolicy(Qt::NoFocus);
		made.Identity->setContentsMargins(4, 0, 4, 0);
		made.IdentityAction = strip->addWidget(made.Identity);
		note(QStringLiteral("Name"), made.Identity);

		auto* const github = new QToolButton{ strip };
		github->setToolButtonStyle(Qt::ToolButtonIconOnly);
		github->setIcon(strip_icon(strip_glyph::open_on_github, palette));
		github->setProperty(strip_style::filled_property, true);
		strip->adopt_button(github);
		made.GithubAction = strip->addWidget(github);
		note(QStringLiteral("Open on GitHub"), github);

		auto* const chip = new branch_chip{ aContent.Branch, aContent.Ahead, aDark, strip };
		strip->addWidget(chip);
		note(QStringLiteral("Branch"), chip);

		auto* const spacer = new QWidget{ strip };
		spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		spacer->setFocusPolicy(Qt::NoFocus);
		strip->addWidget(spacer);
		note(QStringLiteral("Spare"), spacer);

		made.Modes = new switcher{ *strip, mode_count, aCurrent, strip };
		strip->addWidget(made.Modes);
		note(QStringLiteral("Modes"), made.Modes);

		action(QStringLiteral("Refresh"), strip_glyph::refresh, false, false);
		action(QStringLiteral("Comments"), strip_glyph::comments_drawer, true, true);
		action(QStringLiteral("Hide drawers"), strip_glyph::drawers_shown, false, false);
		auto* const divider = new strip_divider{ strip };
		strip->addWidget(divider);
		note(QStringLiteral("Rule"), divider);
		action(QStringLiteral("Account"), strip_glyph::account, false, false);
		action(QStringLiteral("Settings"), strip_glyph::settings, false, false);
#if defined(Q_OS_WIN)
		auto* const caption = new windows_caption_buttons{ strip };
		strip->addWidget(caption->reserve());
		note(QStringLiteral("Caption buttons"), caption->reserve());
#endif
		return made;
	}

	void settle(QWidget& aWindow)
	{
		aWindow.show();
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	[[nodiscard]] auto render(row& aRow, const int aWidth, const QString& aPath) -> QJsonObject
	{
		fit(aRow, aWidth);
		// The row sits in a holder that is never shown and is wider than any
		// row: a top-level widget wider than the screen does not paint on
		// macOS, and a child is not held to the screen's size.
		auto holder = QWidget{ };
		holder.setAttribute(Qt::WA_DontShowOnScreen, true);
		holder.resize(aWidth, workbench_strip::height_pixels);
		aRow.Window->setParent(&holder);
		aRow.Window->setGeometry(0, 0, aWidth, workbench_strip::height_pixels);
		aRow.Window->show();
		settle(holder);
		const auto image = aRow.Window->grab().toImage();
		aRow.Window->setParent(nullptr);
		image.save(aPath);
		auto items = QJsonArray{ };
		for (const auto& [name, widget] : aRow.Items) {
			if (widget == nullptr || widget->isHidden()) {
				continue;
			}
			const auto origin = widget->mapTo(aRow.Window, QPoint{ 0, 0 });
			items.push_back(QJsonObject{ { QStringLiteral("name"), name }, { QStringLiteral("x"), origin.x() }, { QStringLiteral("w"), widget->width() } });
		}
		const auto margins = aRow.Identity->contentsMargins();
		return QJsonObject{
			{ QStringLiteral("option"), aRow.Option == option::today ? QStringLiteral("today") : QStringLiteral("fold") },
			{ QStringLiteral("width"), aWidth },
			{ QStringLiteral("file"), QFileInfo{ aPath }.fileName() },
			{ QStringLiteral("pixels"), QJsonArray{ image.width(), image.height() } },
			{ QStringLiteral("face"), aRow.Face },
			{ QStringLiteral("identity_form"), aRow.IdentityForm },
			{ QStringLiteral("identity_text"), aRow.IdentityAction->isVisible() ? aRow.Identity->text() : QString{ } },
			{ QStringLiteral("whole_name"), aRow.Identity->fontMetrics().horizontalAdvance(whole_identity(aRow.Content)) + margins.left() + margins.right() },
			{ QStringLiteral("asked"), aRow.Strip->sizeHint().width() },
			{ QStringLiteral("overflow"), aRow.Strip->sizeHint().width() > aWidth },
			{ QStringLiteral("items"), items },
		};
	}

	[[nodiscard]] auto argument(const QStringList& aArguments, const QString& aName, const QString& aDefault = { }) -> QString
	{
		const auto at = aArguments.indexOf(aName);
		return at >= 0 && at + 1 < aArguments.size() ? aArguments[at + 1] : aDefault;
	}
} // namespace

auto main(int aCount, char** aValues) -> int
{
	auto       application = QApplication{ aCount, aValues };
	const auto arguments   = QApplication::arguments();
	const auto out         = argument(arguments, QStringLiteral("--out"), QStringLiteral("out"));
	QDir{ }.mkpath(out);
	if (const auto ui = argument(arguments, QStringLiteral("--ui")); !ui.isEmpty()) {
		ui_override = parse_face(ui);
	}
	auto console = QTextStream{ stdout };

	const auto* const screen  = QApplication::primaryScreen();
	const auto        ratio   = screen != nullptr ? screen->devicePixelRatio() : 1.0;
	const auto        tag     = QString::number(qRound(ratio * 100.0));
	const auto        system  = platform_key();
	const auto        general = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	auto              keys    = QJsonArray{ };
	for (std::size_t index = 0U; index < mode_count; ++index) {
		keys.push_back(mode_key(index));
	}
	// The application's own tooltips, as a segment showing only its glyph
	// would say them: its name first (fit_tray_tabs).
	auto tips = QJsonArray{ };
	{
		auto strip = workbench_strip{ QStringLiteral("Title row"), nullptr, strip_role::title_row };
		auto real  = modality_control{ strip };
		for (std::size_t index = 0U; index < review_modality_count; ++index) {
			tips.push_back(QStringLiteral("%1\n%2").arg(mode_title(index), real.button(static_cast<review_modality>(index))->toolTip()));
		}
	}
	const auto environment = QJsonObject{
		{ QStringLiteral("system"), system },
		{ QStringLiteral("qt"), QString::fromLatin1(qVersion()) },
		{ QStringLiteral("platform"), QApplication::platformName() },
		{ QStringLiteral("style"), QApplication::style()->objectName() },
		{ QStringLiteral("device_pixel_ratio"), ratio },
		{ QStringLiteral("general_font_family"), general.family() },
		{ QStringLiteral("general_font_points"), general.pointSizeF() },
		{ QStringLiteral("row_font_family"), QFontInfo{ system_font() }.family() },
		{ QStringLiteral("row_font_points"), QFontInfo{ system_font() }.pointSizeF() },
		{ QStringLiteral("row_font_pixel_size"), QFontInfo{ system_font() }.pixelSize() },
		{ QStringLiteral("os"), QSysInfo::prettyProductName() },
		{ QStringLiteral("mode_keys"), keys },
		{ QStringLiteral("tooltips"), tips },
	};

	auto widths = std::vector<int>{ };
	for (const auto& width : argument(arguments, QStringLiteral("--widths"), QStringLiteral("1600,1366,1280,1100,960")).split(QLatin1Char{ ',' })) {
		widths.push_back(width.toInt());
	}
	const auto current = static_cast<std::size_t>(argument(arguments, QStringLiteral("--current"), QStringLiteral("4")).toInt());

	// The switcher alone in each face, and what each face asks for.
	auto faces = QJsonArray{ };
	for (const auto dark : { true, false }) {
		for (const auto shown : { shape::keys, shape::names, shape::prefixes, shape::open_named, shape::glyphs }) {
			auto text  = content{ };
			text.Title = QStringLiteral("Search results");
			auto made   = build(option::fold, dark, text, current);
			made.Forced = static_cast<int>(shown);
			const auto index  = static_cast<int>(shown);
			const auto file   = QStringLiteral("%1-face-%2-%3-%4.png").arg(system).arg(index).arg(dark ? QStringLiteral("dark") : QStringLiteral("light"), tag);
			const auto record = render(made, 1'500, out + QLatin1Char{ '/' } + file);
			static_cast<void>(record);
			if (dark) {
				auto segments = QJsonArray{ };
				for (const auto* const button : made.Modes->Segments) {
					segments.push_back(button->width());
				}
				faces.push_back(QJsonObject{ { QStringLiteral("face"), face_name(shown) }, { QStringLiteral("index"), index }, { QStringLiteral("width"), made.Modes->width() }, { QStringLiteral("x"), made.Modes->mapTo(made.Window, QPoint{ 0, 0 }).x() }, { QStringLiteral("segments"), segments } });
			}
			delete made.Window;
		}
	}

	auto rows = QJsonArray{ };
	for (const auto& [name, title] : { std::pair{ QStringLiteral("long"), content{ }.Title }, std::pair{ QStringLiteral("short"), QStringLiteral("Search results") } }) {
		auto text  = content{ };
		text.Title = title;
		for (const auto dark : { true, false }) {
			for (const auto chosen : { option::today, option::fold }) {
				for (const auto width : widths) {
					auto       made   = build(chosen, dark, text, current);
					const auto file   = QStringLiteral("%1-%2-%3-%4-%5-%6.png").arg(system, chosen == option::today ? QStringLiteral("today") : QStringLiteral("fold"), name).arg(width).arg(dark ? QStringLiteral("dark") : QStringLiteral("light"), tag);
					auto       record = render(made, width, out + QLatin1Char{ '/' } + file);
					record[QStringLiteral("theme")] = dark ? QStringLiteral("dark") : QStringLiteral("light");
					record[QStringLiteral("title")] = name;
					rows.push_back(record);
					delete made.Window;
				}
			}
		}
	}
	auto file = QFile{ QStringLiteral("%1/%2-%3.json").arg(out, system, tag) };
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument{ QJsonObject{ { QStringLiteral("environment"), environment }, { QStringLiteral("faces"), faces }, { QStringLiteral("rows"), rows } } }.toJson());
	}
	console << "rendered " << rows.size() << " rows on " << system << " at " << ratio << "\n";
	return 0;
}
