// [[BPR-1855]]. The signed-out Home card's buttons fit their words and its
// sentence wraps, with a wide typeface, under a style whose push-button label
// keeps a margin of its own, at the display scale the run is given.
//
// On Windows the run uses Qt's own windows11 style and Segoe UI, the
// combination that clipped "Use GitHub CLI" to "se GitHub CL". Elsewhere it
// uses the platform style behind a probe that takes six pixels from each side
// of a push button's label, as windows11 does, and Segoe UI when installed or
// Verdana, a wider face, when not. The probe records every piece of text the
// style draws, so a clipped or elided label is seen as a rectangle narrower
// than its words or a string other than the button's.
//
// The owner's Windows photos showed the same loss on New profile and Rename in
// Profiles and on Show files in the review page's "No file open" direction:
// all RFC-BPR-0087 quiet push chips. After the card the run sweeps every
// visible button with words in the composed window, its Settings pages, the
// Profiles window and that direction, and names those three outright.

#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOptionButton>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	struct drawn_text
	{
		QRect   Rect;
		QString Text;
		QFont   Font;
	};

	/// \brief Records the text a style draws; optionally keeps Windows 11's
	/// six-pixel push-button label margin over a style that has none.
	class label_probe final : public QProxyStyle
	{
	public:
		label_probe(QStyle* const aBase, const int aLabelInset)
			: QProxyStyle{ aBase }
			, LabelInset{ aLabelInset }
		{
		}

		void drawControl(const ControlElement aElement, const QStyleOption* const aOption, QPainter* const aPainter,
						 const QWidget* const aWidget) const override
		{
			if (aElement == CE_PushButtonLabel && LabelInset > 0) {
				if (const auto* const button = qstyleoption_cast<const QStyleOptionButton*>(aOption)) {
					auto inset = QStyleOptionButton{ *button };
					inset.rect = button->rect.adjusted(LabelInset, 0, -LabelInset, 0);
					QProxyStyle::drawControl(aElement, &inset, aPainter, aWidget);
					return;
				}
			}
			QProxyStyle::drawControl(aElement, aOption, aPainter, aWidget);
		}

		/// \brief Windows 11 widens a push button by its label margin less the
		/// base margin, so a native button that asks for its size still fits.
		[[nodiscard]] auto sizeFromContents(const ContentsType aType, const QStyleOption* const aOption, const QSize& aSize,
											const QWidget* const aWidget) const -> QSize override
		{
			auto size = QProxyStyle::sizeFromContents(aType, aOption, aSize, aWidget);
			if (aType == CT_PushButton && LabelInset > 0) {
				size.rwidth() += std::max(0, (2 * LabelInset) - pixelMetric(PM_ButtonMargin, aOption, aWidget));
			}
			return size;
		}

		void drawItemText(QPainter* const aPainter, const QRect& aRect, const int aFlags, const QPalette& aPalette, const bool aEnabled,
						  const QString& aText, const QPalette::ColorRole aRole) const override
		{
			Drawn.push_back({ aRect, aText, aPainter->font() });
			QProxyStyle::drawItemText(aPainter, aRect, aFlags, aPalette, aEnabled, aText, aRole);
		}

		mutable std::vector<drawn_text> Drawn;

	private:
		int LabelInset = 0;
	};

	[[nodiscard]] auto words_width(const QFont& aFont, const QString& aText) -> int
	{
		return static_cast<int>(std::ceil(QFontMetricsF{ aFont }.size(Qt::TextShowMnemonic, aText).width()));
	}

	[[nodiscard]] auto render(QWidget& aWidget, const qreal aRatio) -> QImage
	{
		auto image = QImage{ (QSizeF{ aWidget.size() } * aRatio).toSize(), QImage::Format_ARGB32_Premultiplied };
		image.setDevicePixelRatio(aRatio);
		image.fill(aWidget.palette().color(QPalette::Window));
		aWidget.render(&image, QPoint{ }, QRegion{ }, QWidget::DrawWindowBackground | QWidget::DrawChildren);
		return image;
	}

	/// \brief Every button's words are drawn whole, in a rectangle at least as
	/// wide as they are, inside the button.
	auto expect_words_drawn(label_probe& aProbe, const QAbstractButton& aButton, const qreal aRatio, const std::string& aWhere) -> bool
	{
		const auto name = aWhere + " " + aButton.text().toStdString();
		aProbe.Drawn.clear();
		auto image = QImage{ (QSizeF{ aButton.size() } * aRatio).toSize(), QImage::Format_ARGB32_Premultiplied };
		image.setDevicePixelRatio(aRatio);
		image.fill(Qt::transparent);
		const_cast<QAbstractButton&>(aButton).render(&image);
		auto found = false;
		for (const auto& drawn : aProbe.Drawn) {
			if (drawn.Text.isEmpty()) {
				continue;
			}
			expect(drawn.Text == aButton.text(), name + ": the words are drawn as given, not elided");
			const auto fit = words_width(drawn.Font, drawn.Text);
			expect(drawn.Rect.width() >= fit, name + ": the words' rectangle is as wide as the words");
			const auto centre = drawn.Rect.center().x();
			expect(centre - (fit / 2) >= 0 && centre + ((fit + 1) / 2) <= aButton.width(), name + ": the words lie inside the button");
			found = true;
		}
		return found;
	}

	void expect_fits(label_probe& aProbe, const QAbstractButton& aButton, const qreal aRatio, const std::string& aWhere)
	{
		const auto name = aWhere + " " + aButton.text().toStdString();
		expect(aButton.width() >= aButton.sizeHint().width(), name + ": the layout gives the button the width it asks for");
		const auto need = words_width(aButton.font(), aButton.text());
		expect(aButton.sizeHint().width() >= need + 12, name + ": sizeHint holds the words and the chip's padding");
		expect(expect_words_drawn(aProbe, aButton, aRatio, aWhere), name + ": the style drew the button's words");
	}

	/// \brief Every visible button with words under aRoot draws them whole.
	/// A button that paints its own words through no style text is not counted.
	auto sweep(label_probe& aProbe, const QWidget& aRoot, const qreal aRatio, const std::string& aWhere) -> int
	{
		auto       checked = 0;
		const auto buttons = aRoot.findChildren<QAbstractButton*>();
		for (auto* const button : buttons) {
			if (!button->isVisibleTo(&aRoot) || button->text().isEmpty() || button->width() <= 0) {
				continue;
			}
			const auto name = aWhere + " [" + button->metaObject()->className() + " " + button->objectName().toStdString() + "]";
			if (expect_words_drawn(aProbe, *button, aRatio, name)) {
				++checked;
			}
		}
		return checked;
	}

	void expect_wraps(const qt::workbench_landing& aLanding, const std::string& aWhere)
	{
		const auto* const body  = aLanding.body_label();
		const auto        right = aLanding.width() - 24;
		expect(body->wordWrap(), aWhere + ": the sentence wraps");
		expect(body->geometry().right() < right, aWhere + ": the sentence ends inside the card");
		expect(body->height() >= body->heightForWidth(body->width()), aWhere + ": the sentence has the height its lines need");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication app{ argc, argv };
	// Windows 11's own style when this Qt has it; otherwise its label margin.
	const auto native = QStyleFactory::keys().contains(QStringLiteral("windows11"), Qt::CaseInsensitive);
	auto*      base   = native ? QStyleFactory::create(QStringLiteral("windows11")) : nullptr;
	if (base == nullptr) {
		base = QStyleFactory::create(QApplication::style()->name());
	}
	auto* const probe = new label_probe{ base, native ? 0 : 6 };
	QApplication::setStyle(probe);

	auto font = QApplication::font();
	if (QFontDatabase::hasFamily(QStringLiteral("Segoe UI"))) {
		font.setFamilies({ QStringLiteral("Segoe UI") });
		if (native) {
			font.setPointSizeF(9.0);
		}
	} else if (QFontDatabase::hasFamily(QStringLiteral("Verdana"))) {
		font.setFamilies({ QStringLiteral("Verdana") });
	} else {
		font.setStretch(QFont::Expanded);
	}
	QApplication::setFont(font);
	const auto family = QFontInfo{ font }.family();
	const auto ratio  = app.devicePixelRatio();
	const auto scale  = static_cast<int>(std::lround(ratio * 100.0));
	const auto where  = std::to_string(scale) + "%";

	// The signed-out Home card, Use GitHub CLI offered, wide and then as narrow
	// as it can be laid out.
	qt::workbench_landing landing;
	landing.set_github_cli_available(true);
	landing.set_state(qt::landing_state::sign_in);
	expect(landing.secondary_button()->text() == QStringLiteral("Use GitHub CLI")
			   && landing.tertiary_button()->text() == QStringLiteral("Add enterprise server"),
		   "the card offers its three ways in");
	const auto directory = qEnvironmentVariable("BPR_HOME_CARD_RENDER_DIR");
	for (const auto& [label, width] : { std::pair{ "wide", 960 }, std::pair{ "narrow", 0 } }) {
		const auto across = width > 0 ? width : landing.minimumSizeHint().width();
		landing.resize(across, landing.sizeHint().height());
		landing.show();
		settle();
		landing.resize(across, landing.layout()->totalHeightForWidth(across));
		settle();
		const auto at = where + " " + label;
		for (const auto* const button : { landing.primary_button(), landing.secondary_button(), landing.tertiary_button() }) {
			expect(button->isVisible(), at + ": every way in is shown");
			expect_fits(*probe, *button, ratio, at);
		}
		expect_wraps(landing, at);
		if (!directory.isEmpty()) {
			expect(QDir{ }.mkpath(directory), "render directory exists");
			const auto file = QDir{ directory }.filePath(QStringLiteral("home-card-%1-%2.png").arg(scale).arg(QString::fromLatin1(label)));
			expect(render(landing, ratio).save(file), "card render saved");
			std::cout << "render " << file.toStdString() << '\n';
		}
	}
	// At its narrowest the sentence takes more than one line rather than
	// running past the card.
	expect(landing.body_label()->height() > QFontMetrics{ landing.body_label()->font() }.height() * 3 / 2,
		   where + " narrow: the sentence wraps onto further lines");

	// The shared chips themselves: a push and a tool button with long words.
	QWidget row;
	auto*   layout = new QHBoxLayout{ &row };
	auto*   push   = new qt::quiet_push_button{ QStringLiteral("Add enterprise server"), &row };
	auto*   tool   = new qt::quiet_tool_button{ &row };
	tool->setText(QStringLiteral("Mark every file viewed"));
	layout->addWidget(push);
	layout->addWidget(tool);
	layout->addStretch(1);
	row.resize(row.sizeHint().width() + 40, row.sizeHint().height());
	row.show();
	settle();
	expect_fits(*probe, *push, ratio, where + " chip");
	expect_fits(*probe, *tool, ratio, where + " chip");

	// The sweep: the composed window on Home, its Settings pages, Profiles,
	// and the review page's "No file open" direction with both its ways on.
	auto swept = 0;
	{
		qt::workbench_window window;
		window.resize(1280, 820);
		window.show();
		settle();
		swept += sweep(*probe, window, ratio, where + " window");
		for (const auto page : { qt::settings_page::home_and_queue, qt::settings_page::notifications, qt::settings_page::agents_and_context }) {
			window.settings()->resize(1100, 900);
			window.settings()->present(page);
			settle();
			swept += sweep(*probe, *window.settings(), ratio, where + " settings " + std::to_string(static_cast<int>(page)));
		}
	}
	{
		qt::profile_management_window profiles;
		profiles.adopt({ .Profiles = { { core::default_bpr_profile_id, "Default", application::profile_management_runtime::closed } }, .Ready = true });
		profiles.present();
		settle();
		swept += sweep(*probe, profiles, ratio, where + " profiles");
		expect_fits(*probe, *profiles.create_button(), ratio, where + " profiles");
		expect_fits(*probe, *profiles.rename_button(), ratio, where + " profiles");
	}
	{
		qt::diff_workbench review;
		auto               direction = qt::diff_workbench::idle_direction{ };
		direction.Title              = QStringLiteral("No file open");
		direction.Detail             = QStringLiteral("Choose one of the 18 files in Files, or open the first one.");
		direction.OpenFirst          = [] { };
		direction.ShowFiles          = [] { };
		review.set_idle_direction(std::move(direction));
		review.resize(900, 600);
		review.show();
		settle();
		swept += sweep(*probe, review, ratio, where + " review idle");
		auto* const show = review.findChild<QPushButton*>(QStringLiteral("diffWorkbenchIdleShowFiles"));
		expect(show != nullptr && show->isVisible(), "the idle direction offers Show files");
		expect_fits(*probe, *show, ratio, where + " review idle");
	}
	expect(swept >= 20, where + ": the sweep reached the app's buttons");

	std::cout << "home-card-fit-tests:pass swept=" << swept << " qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " style=" << base->name().toStdString() << (native ? "" : "+label-inset-6") << " font=" << family.toStdString()
			  << " scale=" << scale << '\n';
	return EXIT_SUCCESS;
}
