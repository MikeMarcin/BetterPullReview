#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/enhance_control.hpp"
#include "bpr/qt/private_notes_panel.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const char* aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void drain() { QApplication::processEvents(); }

	struct fixture
	{
		QWidget                          Host;
		QPlainTextEdit*                  Editor  = nullptr;
		qt::enhance_button*              Button  = nullptr;
		qt::enhance_suggestion*          Panel   = nullptr;
		qt::enhance_binding*             Binding = nullptr;
		std::vector<qt::enhance_request> Asked;
		std::vector<std::uint64_t>       Cancelled;

		explicit fixture(const qt::enhance_surface aSurface)
		{
			auto* const column = new QVBoxLayout{ &Host };
			Editor             = new QPlainTextEdit{ &Host };
			Button             = new qt::enhance_button{ aSurface, &Host };
			Panel              = new qt::enhance_suggestion{ aSurface, &Host };
			auto* const use    = new QPushButton{ QStringLiteral("Use suggestion") };
			auto* const drop   = new QPushButton{ QStringLiteral("Discard") };
			Panel->set_buttons(use, drop);
			column->addWidget(Editor);
			column->addWidget(Button);
			column->addWidget(Panel);
			Binding = new qt::enhance_binding{ *Editor, aSurface, *Button, *Panel, &Host };
			Host.show();
			drain();
		}
		void wire()
		{
			Binding->set_dispatch([this](qt::enhance_request aRequest) { Asked.push_back(std::move(aRequest)); }, [this](const std::uint64_t aId) { Cancelled.push_back(aId); });
			drain();
		}
	};

	/// Nothing is offered until a composition answers it, nothing is asked
	/// of an empty field, and a press sends the whole text once.
	void test_press_asks_once_and_changes_nothing()
	{
		auto f = fixture{ qt::enhance_surface::comment };
		expect(f.Button->isHidden(), "unwired, Enhance is not offered");
		f.wire();
		expect(f.Button->isVisible() && !f.Button->isEnabled(), "wired, an empty comment cannot be enhanced");
		f.Editor->setPlainText(QStringLiteral("nit: maybe café is simpler"));
		drain();
		expect(f.Button->isEnabled() && f.Asked.empty(), "with text it is offered, and nothing runs until pressed");
		expect(qt::agent_run_control(*f.Button) && f.Button->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(f.Host.palette()),
			   "Enhance starts a run, so it wears the Agent accent");
		f.Button->click();
		drain();
		expect(f.Asked.size() == 1U && f.Asked.front().Surface == qt::enhance_surface::comment && !f.Asked.front().Selection && f.Asked.front().Begin == 0U
				   && f.Asked.front().End == f.Asked.front().Raw.size() && f.Asked.front().Raw == "nit: maybe café is simpler",
			   "a press asks once about the whole comment, in UTF-8 bytes");
		expect(f.Binding->pending() == f.Asked.front().Id && f.Button->running() && !qt::agent_run_control(*f.Button), "while it runs the control stops it and wears the ordinary roles");
		expect(f.Panel->isVisible() && f.Editor->toPlainText() == QStringLiteral("nit: maybe café is simpler"), "the run shows under the field; the text is unchanged");
		// Another press's answer is not this one's.
		expect(!f.Binding->present({ .Id = f.Asked.front().Id + 100U, .Replacement = std::string{ "wrong" } }), "an answer to another press is ignored");
		expect(f.Binding->present({ .Id = f.Asked.front().Id, .Replacement = std::string{ "Nit: café would be simpler here." } }), "its own answer is taken");
		drain();
		expect(f.Panel->suggestion_text() == QStringLiteral("Nit: café would be simpler here.") && f.Editor->toPlainText() == QStringLiteral("nit: maybe café is simpler"),
			   "the suggestion is shown beside the text, never applied");
		expect(qt::agent_run_control(*f.Button) && !f.Button->running(), "the run over, Enhance wears the accent again");
		// Discard: nothing changes.
		f.Panel->discard_button()->click();
		drain();
		expect(f.Panel->isHidden() && f.Editor->toPlainText() == QStringLiteral("nit: maybe café is simpler") && !f.Binding->suggested(), "Discard drops the suggestion and keeps the text");
	}

	/// Use writes the suggestion in place of what was asked about, as one
	/// edit Undo takes back; a selection is rewritten alone.
	void test_use_replaces_once_and_undo_restores()
	{
		auto f = fixture{ qt::enhance_surface::note };
		f.wire();
		f.Editor->setPlainText(QStringLiteral("check 👩‍💻 this\nand that later"));
		auto cursor = f.Editor->textCursor();
		cursor.setPosition(6);
		cursor.setPosition(16, QTextCursor::KeepAnchor);
		f.Editor->setTextCursor(cursor);
		f.Button->click();
		drain();
		const auto& asked = f.Asked.back();
		expect(asked.Selection && asked.Surface == qt::enhance_surface::note && asked.Raw.substr(asked.Begin, asked.End - asked.Begin) == "👩‍💻 this", "a selection asks about its exact bytes");
		expect(f.Binding->present({ .Id = asked.Id, .Replacement = std::string{ "the 👩‍💻 setup" } }), "the answer arrives");
		drain();
		auto* const use = f.Panel->use_button();
		expect(use->isEnabled(), "Use is offered while the text is what was asked about");
		use->click();
		drain();
		expect(f.Editor->toPlainText() == QStringLiteral("check the 👩‍💻 setup\nand that later") && f.Panel->isHidden(), "Use rewrites the selection alone");
		f.Editor->undo();
		expect(f.Editor->toPlainText() == QStringLiteral("check 👩‍💻 this\nand that later"), "one Undo restores the text");

		// A text changed after the press is never overwritten.
		f.Button->click();
		drain();
		const auto id = f.Asked.back().Id;
		f.Editor->appendPlainText(QStringLiteral("typed meanwhile"));
		expect(f.Binding->present({ .Id = id, .Replacement = std::string{ "Rewritten." } }), "an answer after typing is still shown");
		drain();
		expect(!f.Panel->use_button()->isEnabled() && f.Panel->use_button()->toolTip().contains(QStringLiteral("changed")), "Use is refused once the text changed, and says why");
		f.Panel->use_button()->click();
		expect(f.Editor->toPlainText().endsWith(QStringLiteral("typed meanwhile")), "the typed text stays");

		// Stop: the run is cancelled by its own ID and the text is kept.
		f.Panel->discard_button()->click();
		f.Button->click();
		drain();
		const auto stopped = f.Asked.back().Id;
		f.Button->click();
		drain();
		expect(f.Cancelled.size() == 1U && f.Cancelled.front() == stopped && !f.Binding->pending() && f.Panel->notice_text().contains(QStringLiteral("Stopped")), "pressing again stops the run it started");
		expect(!f.Binding->present({ .Id = stopped, .Replacement = std::string{ "late" } }), "a stopped run's late answer is ignored");
		// No suggestion: the reason, and the text kept.
		f.Button->click();
		drain();
		expect(f.Binding->present({ .Id = f.Asked.back().Id, .Notice = QStringLiteral("No agent is set up.") }) && f.Panel->notice_text() == QStringLiteral("No agent is set up.") && !f.Panel->use_button()->isVisible(),
			   "a refusal says why and offers nothing to use");
	}

	/// The accent follows the theme's Agent accent, as every run control does.
	void test_accent_follows_the_theme()
	{
		auto f = fixture{ qt::enhance_surface::comment };
		f.wire();
		const auto custom = QColor{ 0x0B, 0x6E, 0x4F };
		expect(qt::set_agent_accent(custom), "the theme's accent is set");
		auto changed = QEvent{ QEvent::PaletteChange };
		QCoreApplication::sendEvent(f.Button, &changed);
		expect(f.Button->palette().color(QPalette::Active, QPalette::ButtonText) == custom, "a changed accent reaches Enhance");
		static_cast<void>(qt::set_agent_accent(std::nullopt));
	}

	auto themed(const bool aDark) -> QPalette
	{
		auto       palette = QPalette{ };
		const auto set     = [&palette](const QPalette::ColorRole aRole, const QColor& aColour) {
			for (const auto group : { QPalette::Active, QPalette::Inactive }) {
				palette.setColor(group, aRole, aColour);
			}
		};
		set(QPalette::Window, aDark ? QColor{ 0x1E, 0x1E, 0x20 } : QColor{ 0xF5, 0xF5, 0xF7 });
		set(QPalette::Base, aDark ? QColor{ 0x26, 0x26, 0x29 } : QColor{ 0xFF, 0xFF, 0xFF });
		set(QPalette::WindowText, aDark ? QColor{ 0xEC, 0xEC, 0xEE } : QColor{ 0x1D, 0x1D, 0x1F });
		set(QPalette::Text, aDark ? QColor{ 0xEC, 0xEC, 0xEE } : QColor{ 0x1D, 0x1D, 0x1F });
		set(QPalette::ButtonText, aDark ? QColor{ 0xEC, 0xEC, 0xEE } : QColor{ 0x1D, 0x1D, 0x1F });
		set(QPalette::Button, aDark ? QColor{ 0x3A, 0x3A, 0x3C } : QColor{ 0xFF, 0xFF, 0xFF });
		set(QPalette::Mid, aDark ? QColor{ 0x48, 0x48, 0x4A } : QColor{ 0xC7, 0xC7, 0xCC });
		set(QPalette::PlaceholderText, aDark ? QColor{ 0x98, 0x98, 0x9D } : QColor{ 0x6E, 0x6E, 0x73 });
		set(QPalette::Highlight, QColor{ 0x0A, 0x84, 0xFF });
		return palette;
	}

	/// [[BPR-2051]] owner look. With an output folder, draws the three icon
	/// options in Light and Dark: the icon alone at 16 and 32 points, and in a
	/// real note's foot beside Ask agent with a suggestion shown.
	void render_options(const QString& aFolder)
	{
		QDir{ }.mkpath(aFolder);
		for (const auto dark : { false, true }) {
			const auto palette = themed(dark);
			for (std::size_t index = 0; index < qt::enhance_glyph_style_count; ++index) {
				const auto style = static_cast<qt::enhance_glyph_style>(index);
				qt::set_enhance_glyph_style(style);
				QApplication::setPalette(palette);
				auto panel = qt::private_notes_panel{ };
				panel.setPalette(palette);
				panel.resize(360, 420);
				auto asked = std::uint64_t{ 0U };
				panel.set_enhance_dispatch([&asked](qt::enhance_request aRequest) { asked = aRequest.Id; }, [](std::uint64_t) { });
				panel.present({ .Scope = 1U, .Available = true, .Notes = { { .Id = 7U, .Revision = 1U, .Label = "Pull request", .Text = "check the cache invalidation here, it looks like it might be missing the delete path??" } } });
				panel.show_elsewhere(true);
				panel.show();
				drain();
				panel.open_note(7U);
				drain();
				if (auto* const button = panel.enhance_control(); button != nullptr) {
					button->click();
					drain();
					expect(panel.present_enhancement({ .Id = asked, .Replacement = std::string{ "Check cache invalidation here: the delete path may be missing." } }), "the render's suggestion is shown");
				}
				drain();
				const auto name = QStringLiteral("%1/note-%2-%3.png").arg(aFolder, QString::number(index + 1), dark ? QStringLiteral("dark") : QStringLiteral("light"));
				expect(panel.grab().save(name), "a note render saves");
				panel.hide();
			}
			// The icons alone, side by side, at two sizes.
			auto sheet = QImage{ 3 * 200, 120, QImage::Format_ARGB32_Premultiplied };
			sheet.fill(palette.color(QPalette::Window));
			auto       painter       = QPainter{ &sheet };
			auto       accentPalette = palette;
			const auto accent        = qt::agent_accent(palette);
			accentPalette.setColor(QPalette::Active, QPalette::ButtonText, accent);
			for (std::size_t index = 0; index < qt::enhance_glyph_style_count; ++index) {
				const auto icon = qt::enhance_icon(accentPalette, static_cast<qt::enhance_glyph_style>(index));
				painter.drawPixmap(static_cast<int>(index) * 200 + 30, 48, icon.pixmap(16, 16));
				painter.drawPixmap(static_cast<int>(index) * 200 + 60, 40, icon.pixmap(32, 32));
				painter.drawPixmap(static_cast<int>(index) * 200 + 106, 16, icon.pixmap(64, 64));
				painter.setPen(palette.color(QPalette::WindowText));
				painter.drawText(QRect{ static_cast<int>(index) * 200, 92, 200, 20 }, Qt::AlignCenter, QStringLiteral("%1. %2").arg(index + 1).arg(qt::enhance_glyph_style_name(static_cast<qt::enhance_glyph_style>(index))));
			}
			painter.end();
			expect(sheet.save(QStringLiteral("%1/icons-%2.png").arg(aFolder, dark ? QStringLiteral("dark") : QStringLiteral("light"))), "the icon sheet saves");
		}
		qt::set_enhance_glyph_style(qt::enhance_glyph_style::nib_cut);
	}

	/// Owner look, 2026-11-10: "The tooltip should be much shorter and adapt
	/// to the context", e.g. "Enhance writing for this comment with agent."
	void test_tooltip_is_short_and_follows_the_selection()
	{
		for (const auto& [surface, noun] : { std::pair{ qt::enhance_surface::comment, "comment" }, std::pair{ qt::enhance_surface::reply, "reply" }, std::pair{ qt::enhance_surface::note, "note" } }) {
			auto f = fixture{ surface };
			f.wire();
			f.Editor->setPlainText(QStringLiteral("check the cache"));
			drain();
			expect(f.Button->toolTip() == QStringLiteral("Enhance writing for this %1 with agent.").arg(QString::fromLatin1(noun)), "the tooltip names the surface, in one short sentence");
			auto cursor = f.Editor->textCursor();
			cursor.setPosition(0);
			cursor.setPosition(5, QTextCursor::KeepAnchor);
			f.Editor->setTextCursor(cursor);
			drain();
			expect(f.Button->toolTip() == QStringLiteral("Enhance writing for the selected text with agent."), "with a selection it says the selected text");
			cursor.clearSelection();
			f.Editor->setTextCursor(cursor);
			drain();
			expect(f.Button->toolTip().endsWith(QStringLiteral("with agent.")) && !f.Button->toolTip().contains(QStringLiteral("selected")), "and goes back when the selection goes");
		}
		expect(qt::current_enhance_glyph_style() == qt::enhance_glyph_style::nib_cut, "the product draws the nib with the spark cut out until the owner chooses");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	if (argc > 1) {
		render_options(QString::fromLocal8Bit(argv[1]));
		std::cout << "enhance-control:rendered\n";
		return EXIT_SUCCESS;
	}
	test_press_asks_once_and_changes_nothing();
	test_use_replaces_once_and_undo_restores();
	test_accent_follows_the_theme();
	test_tooltip_is_short_and_follows_the_selection();
	std::cout << "enhance-control:pass\n";
	return EXIT_SUCCESS;
}
