// Scratch design harness for docs/design/windows-title-fit-v1. Not product code.
//
// It builds the application's own title row (workbench_strip, strip_style,
// strip_icon, modality_control, windows_caption_buttons, compiled from the
// lane's sources) and lays it out the way workbench_window::apply_strip_width
// and update_strip_identity do, then grabs it without showing a window.
// The fifth mode, the branch chip of the accepted Revise design and the
// options under study are drawn here, on the strip's own style.
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

	// Mock glyphs for the icon option, in the strip glyphs' manner: a 16-pixel
	// box, round 1.3-pixel strokes in one ink.
	void paint_mode_glyph(QPainter& aPainter, const QRectF& aBox, const std::size_t aMode, const QColor& aInk)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.translate(aBox.topLeft());
		aPainter.scale(aBox.width() / 16.0, aBox.height() / 16.0);
		auto pen = QPen{ aInk, 1.3 };
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		aPainter.setPen(pen);
		aPainter.setBrush(Qt::NoBrush);
		switch (aMode) {
		case 0U: // Overview: a card with its heading and two lines.
			aPainter.drawRoundedRect(QRectF{ 2.5, 2.5, 11.0, 11.0 }, 2.0, 2.0);
			aPainter.drawLine(QPointF{ 5.0, 6.0 }, QPointF{ 11.0, 6.0 });
			aPainter.drawLine(QPointF{ 5.0, 8.6 }, QPointF{ 9.5, 8.6 });
			aPainter.drawLine(QPointF{ 5.0, 11.0 }, QPointF{ 8.0, 11.0 });
			break;
		case 1U: // Review: a plus over a minus, the diff.
			aPainter.drawLine(QPointF{ 8.0, 2.5 }, QPointF{ 8.0, 8.5 });
			aPainter.drawLine(QPointF{ 5.0, 5.5 }, QPointF{ 11.0, 5.5 });
			aPainter.drawLine(QPointF{ 5.0, 12.5 }, QPointF{ 11.0, 12.5 });
			break;
		case 2U: { // Discuss: a speech balloon.
			auto balloon = QPainterPath{ };
			balloon.addRoundedRect(QRectF{ 2.5, 3.0, 11.0, 7.5 }, 2.0, 2.0);
			aPainter.drawPath(balloon);
			aPainter.drawLine(QPointF{ 5.5, 10.5 }, QPointF{ 5.5, 13.3 });
			aPainter.drawLine(QPointF{ 5.5, 13.3 }, QPointF{ 8.3, 10.5 });
			break;
		}
		case 3U: // Threads: a list of three, each with its dot.
			for (const auto y : { 4.0, 8.0, 12.0 }) {
				aPainter.drawPoint(QPointF{ 3.4, y });
				aPainter.drawLine(QPointF{ 6.5, y }, QPointF{ 13.0, y });
			}
			break;
		default: // Revise: a commit on its line.
			aPainter.drawLine(QPointF{ 8.0, 2.0 }, QPointF{ 8.0, 5.2 });
			aPainter.drawEllipse(QPointF{ 8.0, 8.0 }, 2.8, 2.8);
			aPainter.drawLine(QPointF{ 8.0, 10.8 }, QPointF{ 8.0, 14.0 });
			break;
		}
		aPainter.restore();
	}

	// One segment. Plain: exactly modality_control's button, a text-only tool
	// button on the strip style. Runs: the same frame from the strip style,
	// with the icon, the name and the key drawn here as separate runs.
	class segment final : public QToolButton
	{
	public:
		segment(workbench_strip& aStrip, const std::size_t aMode, QWidget* const aParent)
			: QToolButton{ aParent }
			, Mode{ aMode }
		{
			setCheckable(true);
			setToolButtonStyle(Qt::ToolButtonTextOnly);
			setProperty(strip_style::filled_property, true);
			aStrip.adopt_button(this);
			setFixedHeight(strip_style::button_height - 4);
		}

		void set_plain(const QString& aText)
		{
			Runs = false;
			setText(aText);
			updateGeometry();
		}

		void set_runs(const bool aIcon, const bool aName, const bool aKey, const QFont& aKeyFont, const bool aQuietKey, const QString& aKeyText)
		{
			Runs     = true;
			Icon     = aIcon;
			Name     = aName;
			Key      = aKey;
			KeyFont  = aKeyFont;
			QuietKey = aQuietKey;
			KeyText  = aKeyText;
			setText(QString{ });
			updateGeometry();
		}

		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			if (!Runs) {
				return QToolButton::sizeHint();
			}
			if (Icon && !Name && !Key) {
				return QSize{ glyph + 12, strip_style::button_height - 4 };
			}
			// A text tool button asks for its text and two spaces; the strip
			// style adds 14 (QToolButton::sizeHint, strip_style::sizeFromContents).
			return QSize{ content_width() + 2 * fontMetrics().horizontalAdvance(QLatin1Char{ ' ' }) + 14, strip_style::button_height - 4 };
		}

		[[nodiscard]] auto minimumSizeHint() const -> QSize override
		{
			return sizeHint();
		}

	protected:
		void paintEvent(QPaintEvent* const aEvent) override
		{
			if (!Runs) {
				QToolButton::paintEvent(aEvent);
				return;
			}
			auto painter = QStylePainter{ this };
			auto option  = QStyleOptionToolButton{ };
			initStyleOption(&option);
			option.text.clear();
			option.icon            = QIcon{ };
			option.toolButtonStyle = Qt::ToolButtonTextOnly;
			painter.drawComplexControl(QStyle::CC_ToolButton, option);
			const auto ink     = palette().color(QPalette::Active, isChecked() ? QPalette::HighlightedText : QPalette::ButtonText);
			auto       quiet   = ink;
			quiet.setAlphaF(isChecked() ? 0.74F : 0.60F);
			const auto metrics = fontMetrics();
			auto       x       = static_cast<double>(rect().left()) + (rect().width() - content_width()) / 2.0;
			const auto line    = rect().top() + (rect().height() - metrics.height()) / 2.0 + metrics.ascent();
			if (Icon) {
				paint_mode_glyph(painter, QRectF{ x, rect().top() + (rect().height() - glyph) / 2.0, glyph, glyph }, Mode, ink);
				x += glyph + (Name || Key ? glyph_gap : 0);
			}
			if (Name) {
				painter.setPen(ink);
				painter.setFont(font());
				painter.drawText(QPointF{ x, line }, mode_title(Mode));
				x += metrics.horizontalAdvance(mode_title(Mode));
			}
			if (Key) {
				x += key_gap;
				painter.setPen(QuietKey ? quiet : ink);
				painter.setFont(KeyFont);
				painter.drawText(QPointF{ x, line }, KeyText);
			}
		}

	private:
		[[nodiscard]] auto content_width() const -> int
		{
			auto width = 0;
			if (Icon) {
				width += glyph + (Name || Key ? glyph_gap : 0);
			}
			if (Name) {
				width += fontMetrics().horizontalAdvance(mode_title(Mode));
			}
			if (Key) {
				width += key_gap + QFontMetrics{ KeyFont }.horizontalAdvance(KeyText);
			}
			return width;
		}

		static constexpr int glyph     = 16;
		static constexpr int glyph_gap = 4;
		static constexpr int key_gap   = 6;

		std::size_t Mode;
		bool        Runs     = false;
		bool        Icon     = false;
		bool        Name     = true;
		bool        Key      = false;
		bool        QuietKey = false;
		QFont       KeyFont;
		QString     KeyText;
	};

	// The switcher: modality_control's frame and row, with any number of modes.
	class switcher final : public QWidget
	{
	public:
		switcher(workbench_strip& aStrip, const std::size_t aCount, const std::size_t aCurrent, QWidget* const aParent)
			: QWidget{ aParent }
		{
			setFocusPolicy(Qt::NoFocus);
			auto* const row = new QHBoxLayout{ this };
			row->setContentsMargins(2, 2, 2, 2);
			row->setSpacing(2);
			for (std::size_t index = 0U; index < aCount; ++index) {
				auto* const button = new segment{ aStrip, index, this };
				button->setChecked(index == aCurrent);
				Segments.push_back(button);
				row->addWidget(button);
			}
			Current = aCurrent;
		}

		std::vector<segment*> Segments;
		std::size_t           Current = 0U;

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

	// ------------------------------------------------------------------- rows
	enum class option { shipped, today, type, keys, icons, number };

	[[nodiscard]] auto option_key(const option aOption) -> QString
	{
		switch (aOption) {
		case option::shipped:
			return QStringLiteral("shipped");
		case option::today:
			return QStringLiteral("today");
		case option::type:
			return QStringLiteral("a-type");
		case option::keys:
			return QStringLiteral("b-keys");
		case option::icons:
			return QStringLiteral("c-icons");
		case option::number:
			return QStringLiteral("d-number");
		}
		return { };
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

	struct type_choice
	{
		face Row; // the name and the chip
		face Name; // a mode's name
		face Key; // a mode's key
		bool QuietKey = true;
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
		modality_control* Shipped       = nullptr;
		QFont            KeyFont;
		bool             QuietKey = false;
		content          Content;
		QString          Form;
		QString          IdentityForm;
		std::vector<std::pair<QString, QWidget*>> Items;
	};

	constexpr int identity_title_minimum  = 80; // workbench_window_home_landing.cpp
	constexpr int identity_fragment_width = 24;
	constexpr int compact_below_windows   = 1'300; // apply_strip_width, _WIN32

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
		// workbench_window_github_presentation.cpp: "%1/%2  #%3 %4".
		return QStringLiteral("%1/%2  #%3 %4").arg(aContent.Owner, aContent.Repo, aContent.Number, aContent.Title);
	}

	// workbench_window::update_strip_identity, the GitHub pull request branch.
	void update_identity(row& aRow, const int aWidth, const int aTitleBudget, const bool aTitleAllowed)
	{
		const auto& metrics    = aRow.Identity->fontMetrics();
		const auto  margins    = aRow.Identity->contentsMargins();
		const auto  room       = std::max(identity_budget(aRow, aWidth) - margins.left() - margins.right(), 0);
		const auto  shown      = whole_identity(aRow.Content);
		const auto  number     = QStringLiteral("  #%1").arg(aRow.Content.Number);
		const auto  owned      = QStringLiteral("%1/%2").arg(aRow.Content.Owner, aRow.Content.Repo) + number;
		const auto  titled     = metrics.horizontalAdvance(owned) + identity_title_minimum;
		const auto  bare       = aRow.Content.Repo + number;
		const auto  wholeRoom  = aTitleBudget - margins.left() - margins.right();
		const auto  ownedWidth = metrics.horizontalAdvance(owned);
		auto        text       = QString{ };
		if (aTitleAllowed && room >= titled && wholeRoom >= titled) {
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

	enum class form { keys, names, prefixes, open, folded };

	void set_form(row& aRow, const form aForm)
	{
		if (aRow.Shipped != nullptr) {
			aRow.Shipped->set_abbreviated(aForm == form::prefixes);
			aRow.Shipped->set_compact(aForm != form::keys);
		} else {
			for (std::size_t index = 0U; index < aRow.Modes->Segments.size(); ++index) {
				auto* const button = aRow.Modes->Segments[index];
				const auto  title  = mode_title(index);
				const auto  plain  = aRow.Option == option::today || aRow.Option == option::number || aRow.Option == option::keys;
				switch (aForm) {
				case form::keys:
					if (plain) {
						// modality_control::relabel: "%1  %2".
						button->set_plain(QStringLiteral("%1  %2").arg(title, mode_key(index)));
					} else {
						button->set_runs(false, true, true, aRow.KeyFont, aRow.QuietKey, mode_key(index));
					}
					break;
				case form::names:
					button->set_plain(title);
					break;
				case form::prefixes:
					button->set_plain(title.left(3));
					break;
				case form::open:
					button->set_runs(true, true, false, aRow.KeyFont, false, QString{ });
					break;
				case form::folded:
					button->set_runs(true, index == aRow.Modes->Current, false, aRow.KeyFont, false, QString{ });
					break;
				}
			}
			aRow.Modes->updateGeometry();
		}
		switch (aForm) {
		case form::keys:
			aRow.Form = QStringLiteral("names and keys");
			break;
		case form::names:
			aRow.Form = QStringLiteral("names");
			break;
		case form::prefixes:
			aRow.Form = QStringLiteral("three-letter prefixes");
			break;
		case form::open:
			aRow.Form = QStringLiteral("icons and names");
			break;
		case form::folded:
			aRow.Form = QStringLiteral("icons, the current mode named");
			break;
		}
		remeasure(aRow);
	}

	// workbench_window::apply_strip_width, with each option's own forms.
	void fit(row& aRow, const int aWidth)
	{
		const auto identityShort = [&aRow, aWidth] {
			return aRow.IdentityAction->isVisible() && identity_budget(aRow, aWidth) < identity_minimum(aRow);
		};
		const auto titleAllowed = aRow.Option != option::number;
		aRow.IdentityAction->setVisible(true);
		aRow.GithubAction->setVisible(true);
		aRow.Identity->setText(whole_identity(aRow.Content));
		auto titleBudget = 0;
		if (aRow.Option == option::icons) {
			set_form(aRow, form::open);
			const auto margins = aRow.Identity->contentsMargins();
			const auto whole   = aRow.Identity->fontMetrics().horizontalAdvance(whole_identity(aRow.Content)) + margins.left() + margins.right();
			if (identity_budget(aRow, aWidth) < whole) {
				// The names of the modes that are not showing yield before the title is cut.
				set_form(aRow, form::folded);
			}
			titleBudget = identity_budget(aRow, aWidth);
		} else if (aRow.Option == option::keys) {
			set_form(aRow, form::names);
			titleBudget = identity_budget(aRow, aWidth);
		} else {
			set_form(aRow, form::keys);
			titleBudget = identity_budget(aRow, aWidth);
			set_form(aRow, aWidth < compact_below_windows ? form::names : form::keys);
			if (identityShort()) {
				set_form(aRow, form::names);
			}
		}
		if (identityShort()) {
			aRow.IdentityAction->setVisible(false);
			aRow.GithubAction->setVisible(false);
			aRow.IdentityForm = QStringLiteral("hidden");
			remeasure(aRow);
		} else {
			update_identity(aRow, aWidth, titleBudget, titleAllowed);
		}
		if (aRow.Option != option::icons && aRow.Strip->sizeHint().width() > aWidth) {
			set_form(aRow, form::prefixes);
			if (aRow.IdentityAction->isVisible()) {
				update_identity(aRow, aWidth, titleBudget, titleAllowed);
			}
		}
	}

	[[nodiscard]] auto build(const option aOption, const bool aDark, const content& aContent, const type_choice& aType, const std::size_t aCurrent) -> row
	{
		const auto palette = profile_palette(aDark);
		auto       made    = row{ };
		made.Option        = aOption;
		made.Content       = aContent;
		made.Window        = new QWidget{ };
		made.Window->setAttribute(Qt::WA_DontShowOnScreen, true);
		made.Window->setPalette(palette);
		made.Window->setFont(aOption == option::type ? make_font(aType.Row) : system_font());
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

		// workbench_window_construction.cpp, the title row's order on Windows.
		auto* const menu = new QToolButton{ strip };
		menu->setToolButtonStyle(Qt::ToolButtonIconOnly);
#if defined(Q_OS_WIN)
		menu->setIcon(windows_menu_icon(palette));
#else
		menu->setIcon(strip_icon(strip_glyph::review_details, palette));
#endif
		strip->addWidget(menu);
		note(QStringLiteral("Menu"), menu);
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

		if (aOption != option::shipped) {
			auto* const chip = new branch_chip{ aContent.Branch, aContent.Ahead, aDark, strip };
			strip->addWidget(chip);
			note(QStringLiteral("Branch"), chip);
		}

		auto* const spacer = new QWidget{ strip };
		spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		spacer->setFocusPolicy(Qt::NoFocus);
		strip->addWidget(spacer);
		note(QStringLiteral("Spare"), spacer);

		if (aOption == option::shipped) {
			made.Shipped = new modality_control{ *strip, strip };
			made.Shipped->adopt(static_cast<review_modality>(std::min<std::size_t>(aCurrent, review_modality_count - 1U)));
			strip->addWidget(made.Shipped);
			note(QStringLiteral("Modes"), made.Shipped);
		} else {
			made.Modes = new switcher{ *strip, mode_count, aCurrent, strip };
			if (aOption == option::type) {
				made.Modes->setFont(make_font(aType.Name));
				for (auto* const button : made.Modes->Segments) {
					button->setFont(make_font(aType.Name));
				}
				made.KeyFont  = make_font(aType.Key);
				made.QuietKey = aType.QuietKey;
			} else {
				made.KeyFont = system_font();
			}
			strip->addWidget(made.Modes);
			note(QStringLiteral("Modes"), made.Modes);
		}

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
#else
		auto* const caption = new QWidget{ strip };
		caption->setFixedWidth(3 * 46 - 4);
		strip->addWidget(caption);
		note(QStringLiteral("Caption buttons"), caption);
#endif
		return made;
	}

	[[nodiscard]] auto render(row& aRow, const int aWidth, const QString& aPath) -> QJsonObject
	{
		fit(aRow, aWidth);
		aRow.Window->resize(aWidth, workbench_strip::height_pixels);
		aRow.Window->show();
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
		const auto image = aRow.Window->grab().toImage();
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
		auto       out     = QJsonObject{ };
		out[QStringLiteral("option")]        = option_key(aRow.Option);
		out[QStringLiteral("width")]         = aWidth;
		out[QStringLiteral("file")]          = QFileInfo{ aPath }.fileName();
		out[QStringLiteral("pixels")]        = QJsonArray{ image.width(), image.height() };
		out[QStringLiteral("form")]          = aRow.Form;
		out[QStringLiteral("identity_form")] = aRow.IdentityForm;
		out[QStringLiteral("identity_text")] = aRow.IdentityAction->isVisible() ? aRow.Identity->text() : QString{ };
		out[QStringLiteral("whole_name")]    = aRow.Identity->fontMetrics().horizontalAdvance(whole_identity(aRow.Content)) + margins.left() + margins.right();
		out[QStringLiteral("asked")]         = aRow.Strip->sizeHint().width();
		out[QStringLiteral("overflow")]      = aRow.Strip->sizeHint().width() > aWidth;
		out[QStringLiteral("items")]         = items;
		return out;
	}

	[[nodiscard]] auto font_record(const QString& aName, const QFont& aFont) -> QJsonObject
	{
		const auto info    = QFontInfo{ aFont };
		const auto metrics = QFontMetrics{ aFont };
		const auto exact   = QFontMetricsF{ aFont };
		auto       names   = QJsonObject{ };
		auto       keys    = QJsonObject{ };
		auto       sumNames = 0;
		auto       sumKeyed = 0;
		for (std::size_t index = 0U; index < mode_count; ++index) {
			names[mode_title(index)] = metrics.horizontalAdvance(mode_title(index));
			keys[mode_key(index)]    = metrics.horizontalAdvance(mode_key(index));
			sumNames += metrics.horizontalAdvance(mode_title(index));
			sumKeyed += metrics.horizontalAdvance(QStringLiteral("%1  %2").arg(mode_title(index), mode_key(index)));
		}
		const auto whole = whole_identity(content{ });
		return QJsonObject{
			{ QStringLiteral("name"), aName },
			{ QStringLiteral("asked_family"), aFont.family() },
			{ QStringLiteral("resolved_family"), info.family() },
			{ QStringLiteral("resolved_style"), info.styleName() },
			{ QStringLiteral("exact_match"), info.exactMatch() },
			{ QStringLiteral("points"), info.pointSizeF() },
			{ QStringLiteral("pixel_size"), info.pixelSize() },
			{ QStringLiteral("weight"), info.weight() },
			{ QStringLiteral("height"), metrics.height() },
			{ QStringLiteral("ascent"), metrics.ascent() },
			{ QStringLiteral("space"), metrics.horizontalAdvance(QLatin1Char{ ' ' }) },
			{ QStringLiteral("names"), names },
			{ QStringLiteral("keys"), keys },
			{ QStringLiteral("five_names"), sumNames },
			{ QStringLiteral("five_names_and_keys"), sumKeyed },
			{ QStringLiteral("five_names_and_keys_exact"), exact.horizontalAdvance(QStringLiteral("Overview  Ctrl+1Review  Ctrl+2Discuss  Ctrl+3Threads  Ctrl+4Revise  Ctrl+5")) },
			{ QStringLiteral("whole_name"), metrics.horizontalAdvance(whole) },
			{ QStringLiteral("owner_repo_number"), metrics.horizontalAdvance(QStringLiteral("bpr-fixtures/engine  #214")) },
		};
	}

	// One specimen line per face, drawn by a label so it takes the widget
	// text path the title row takes.
	void specimen(const QString& aName, const QFont& aFont, const bool aDark, const QString& aPath)
	{
		auto sample = QString{ };
		for (std::size_t index = 0U; index < mode_count; ++index) {
			sample += QStringLiteral("%1  %2").arg(mode_title(index), mode_key(index)) + (index + 1U < mode_count ? QStringLiteral("     ") : QString{ });
		}
		static_cast<void>(aName);
		auto label = QLabel{ sample };
		label.setAttribute(Qt::WA_DontShowOnScreen, true);
		label.setAutoFillBackground(true);
		label.setPalette(profile_palette(aDark));
		label.setFont(aFont);
		label.setContentsMargins(8, 5, 8, 5);
		label.setTextFormat(Qt::PlainText);
		label.resize(label.sizeHint());
		label.show();
		QApplication::processEvents();
		label.grab().toImage().save(aPath);
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
	const auto mode        = argument(arguments, QStringLiteral("--mode"), QStringLiteral("render"));
	QDir{ }.mkpath(out);
	if (const auto ui = argument(arguments, QStringLiteral("--ui")); !ui.isEmpty()) {
		ui_override = parse_face(ui);
	}
	auto console = QTextStream{ stdout };

	const auto* const screen  = QApplication::primaryScreen();
	const auto        ratio   = screen != nullptr ? screen->devicePixelRatio() : 1.0;
	const auto        tag     = QString::number(qRound(ratio * 100.0));
	const auto        general = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	auto              environment = QJsonObject{
		{ QStringLiteral("qt"), QString::fromLatin1(qVersion()) },
		{ QStringLiteral("platform"), QApplication::platformName() },
		{ QStringLiteral("style"), QApplication::style()->objectName() },
		{ QStringLiteral("device_pixel_ratio"), ratio },
		{ QStringLiteral("logical_dpi"), screen != nullptr ? screen->logicalDotsPerInch() : 0.0 },
		{ QStringLiteral("screen"), screen != nullptr ? QStringLiteral("%1x%2").arg(screen->size().width()).arg(screen->size().height()) : QString{ } },
		{ QStringLiteral("general_font_family"), general.family() },
		{ QStringLiteral("general_font_points"), general.pointSizeF() },
		{ QStringLiteral("general_font_weight"), general.weight() },
		{ QStringLiteral("general_font_resolved"), QFontInfo{ general }.family() },
		{ QStringLiteral("general_font_pixel_size"), QFontInfo{ general }.pixelSize() },
		{ QStringLiteral("application_font_family"), QApplication::font().family() },
		{ QStringLiteral("application_font_points"), QApplication::font().pointSizeF() },
		{ QStringLiteral("row_font_family"), QFontInfo{ system_font() }.family() },
		{ QStringLiteral("row_font_points"), QFontInfo{ system_font() }.pointSizeF() },
		{ QStringLiteral("row_font_pixel_size"), QFontInfo{ system_font() }.pixelSize() },
		{ QStringLiteral("os"), QSysInfo::prettyProductName() },
		{ QStringLiteral("kernel"), QSysInfo::kernelVersion() },
	};
	{
		auto keys = QJsonArray{ };
		for (std::size_t index = 0U; index < mode_count; ++index) {
			keys.push_back(mode_key(index));
		}
		environment[QStringLiteral("mode_keys")] = keys;
		auto families = QJsonArray{ };
		for (const auto& family : QFontDatabase::families()) {
			if (family.contains(QStringLiteral("Segoe")) || family.contains(QStringLiteral("Bahnschrift")) || family.contains(QStringLiteral("Tahoma")) || family.contains(QStringLiteral("Arial"))) {
				auto styles = QJsonArray{ };
				for (const auto& style : QFontDatabase::styles(family)) {
					styles.push_back(style);
				}
				families.push_back(QJsonObject{ { QStringLiteral("family"), family }, { QStringLiteral("styles"), styles } });
			}
		}
		environment[QStringLiteral("families")] = families;
	}

	if (mode == QStringLiteral("measure")) {
		// --faces "Name=Family|points|weight|opsz|tracking;Name=..."
		auto records = QJsonArray{ };
		auto index   = 0;
		records.push_back(font_record(QStringLiteral("general"), system_font()));
		for (const auto dark : { true, false }) {
			specimen(QStringLiteral("general"), system_font(), dark, QStringLiteral("%1/face-00-%2@%3.png").arg(out, dark ? QStringLiteral("dark") : QStringLiteral("light"), tag));
		}
		for (const auto& entry : argument(arguments, QStringLiteral("--faces")).split(QLatin1Char{ ';' }, Qt::SkipEmptyParts)) {
			++index;
			const auto name = entry.section(QLatin1Char{ '=' }, 0, 0).trimmed();
			const auto font = make_font(parse_face(entry.section(QLatin1Char{ '=' }, 1)));
			auto       made = font_record(name, font);
			made[QStringLiteral("file")] = QStringLiteral("face-%1").arg(index, 2, 10, QLatin1Char{ '0' });
			records.push_back(made);
			for (const auto dark : { true, false }) {
				specimen(name, font, dark, QStringLiteral("%1/face-%2-%3@%4.png").arg(out).arg(index, 2, 10, QLatin1Char{ '0' }).arg(dark ? QStringLiteral("dark") : QStringLiteral("light"), tag));
			}
		}
		auto file = QFile{ QStringLiteral("%1/faces@%2.json").arg(out, tag) };
		if (file.open(QIODevice::WriteOnly)) {
			file.write(QJsonDocument{ QJsonObject{ { QStringLiteral("environment"), environment }, { QStringLiteral("faces"), records } } }.toJson());
		}
		console << "measured " << records.size() << " faces at " << ratio << "\n";
		return 0;
	}

	auto type = type_choice{ };
	type.Row  = parse_face(argument(arguments, QStringLiteral("--a-row")));
	type.Name = parse_face(argument(arguments, QStringLiteral("--a-name")));
	type.Key  = parse_face(argument(arguments, QStringLiteral("--a-key")));
	auto text = content{ };
	if (const auto title = argument(arguments, QStringLiteral("--title")); !title.isEmpty()) {
		text.Title = title;
	}
	const auto suffix  = argument(arguments, QStringLiteral("--suffix"));
	const auto current = static_cast<std::size_t>(argument(arguments, QStringLiteral("--current"), QStringLiteral("4")).toInt());
	auto       widths  = std::vector<int>{ };
	for (const auto& width : argument(arguments, QStringLiteral("--widths"), QStringLiteral("1280,1366,1600")).split(QLatin1Char{ ',' })) {
		widths.push_back(width.toInt());
	}

	// The harness's switcher against the application's, at four modes: equal
	// widths in every form say the copy measures as the original does.
	auto parity = QJsonArray{ };
	{
		auto shipped = build(option::shipped, true, text, type, 1U);
		auto copied  = build(option::today, true, text, type, 1U);
		for (const auto aForm : { form::keys, form::names, form::prefixes }) {
			set_form(shipped, aForm);
			set_form(copied, aForm);
			auto four = 0;
			for (std::size_t index = 0U; index < review_modality_count; ++index) {
				four += copied.Modes->Segments[index]->sizeHint().width();
			}
			auto real = 0;
			for (std::size_t index = 0U; index < review_modality_count; ++index) {
				real += shipped.Shipped->button(static_cast<review_modality>(index))->sizeHint().width();
			}
			parity.push_back(QJsonObject{ { QStringLiteral("form"), shipped.Form }, { QStringLiteral("application_four_segments"), real }, { QStringLiteral("harness_four_segments"), four }, { QStringLiteral("application_control"), shipped.Shipped->sizeHint().width() }, { QStringLiteral("harness_five_control"), copied.Modes->sizeHint().width() } });
		}
		delete shipped.Window;
		delete copied.Window;
	}

	auto rows = QJsonArray{ };
	for (const auto dark : { true, false }) {
		for (const auto chosen : { option::shipped, option::today, option::type, option::keys, option::icons, option::number }) {
			for (const auto width : widths) {
				auto       made = build(chosen, dark, text, type, chosen == option::shipped ? 1U : current);
				const auto path = QStringLiteral("%1/%2-%3-%4%5@%6.png").arg(out, option_key(chosen)).arg(width).arg(dark ? QStringLiteral("dark") : QStringLiteral("light"), suffix, tag);
				auto       record = render(made, width, path);
				record[QStringLiteral("theme")] = dark ? QStringLiteral("dark") : QStringLiteral("light");
				rows.push_back(record);
				delete made.Window;
			}
		}
	}
	auto file = QFile{ QStringLiteral("%1/rows%2@%3.json").arg(out, suffix, tag) };
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument{ QJsonObject{ { QStringLiteral("environment"), environment }, { QStringLiteral("parity"), parity }, { QStringLiteral("title"), text.Title }, { QStringLiteral("rows"), rows } } }.toJson());
	}
	console << "rendered " << rows.size() << " rows at " << ratio << "\n";
	return 0;
}
