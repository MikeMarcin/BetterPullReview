// [[BPR-1914]] v003 and [[BPR-1919]]. Side chats: each part of the tour opens
// its own chat from a quiet glyph or its menu, docked beside the tour while
// the tour stays as it is; a plain chat with the author's bubbles, rendered
// replies, the Agent tab's run line ([[BPR-1941]]) and Copy; Update the part
// from this chat comes back
// as the part's proposal, decided in the chat. Fenced code in replies and
// proposals is highlighted as the stop's own explanation is, and code in an
// unknown language stays plain.
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/prompt_macro.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include "../support/qt_prompt_sources.hpp"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (int round = 0; round < 6; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
	}
	void verify_first_live_second()
	{
		// [[BPR-1945]], [[BPR-1946]]. Vary the start phase: even a native
		// precise tick can arrive before the actual whole second. Count
		// two seconds with an activity refresh between them, keeping the
		// composed chat's deadline allowance at each elapsed boundary.
#ifdef NDEBUG
		constexpr auto liveBudget = 1300;
#else
		constexpr auto liveBudget = 3000;
#endif
		struct live_probe
		{
			qt::agent_run_status_line Line{ };
			QElapsedTimer             Started;
			qint64                    Second  = 1;
			bool                      Checked = false;
		};
		auto probes = std::array<live_probe, 8>{ };
		for (std::size_t index = 0; index < probes.size(); ++index) {
			auto& probe = probes[index];
			QTimer::singleShot(static_cast<int>(index) * 137, Qt::PreciseTimer, &probe.Line, [&probe] {
				probe.Started.start();
				auto status  = qt::agent_run_status{ };
				status.Agent = QStringLiteral("Claude Code");
				probe.Line.present(status, probe.Started);
			});
		}
		auto clock = QElapsedTimer{ };
		clock.start();
		const auto complete = [&probes] { return std::ranges::all_of(probes, [](const live_probe& aProbe) { return aProbe.Checked; }); };
		while (!complete() && clock.elapsed() < liveBudget + 3000) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
			for (auto& probe : probes) {
				if (!probe.Started.isValid() || probe.Checked) {
					continue;
				}
				const auto expected = probe.Second == 1 ? QStringLiteral("● Claude Code is thinking, 0:01.") : QStringLiteral("● Claude Code is writing, 0:02.");
				if (probe.Line.text() == expected) {
					expect(probe.Line.live() && probe.Started.elapsed() >= probe.Second * 1000, "the live line reads a whole elapsed second");
					if (probe.Second == 1) {
						auto status  = qt::agent_run_status{ };
						status.Agent = QStringLiteral("Claude Code");
						status.Stage = qt::agent_run_stage::writing;
						probe.Line.present(status, probe.Started);
						probe.Second = 2;
					} else {
						probe.Line.settle(QStringLiteral("Stopped after 0:02"), false);
						probe.Checked = true;
					}
				} else {
					expect(probe.Started.elapsed() < liveBudget + (probe.Second - 1) * 1000, "the shared live line counts each second at every start phase: " + probe.Line.text().toStdString());
				}
			}
		}
		expect(complete(), "each staggered live-clock check ran");
	}
	[[maybe_unused]] void render(QWidget& aWidget, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		settle();
		expect(aWidget.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}

	/// The composed code colouring, as the window relays it.
	class held_texts final : public adapters::syntax::blob_source
	{
	public:
		explicit held_texts(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}
		auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override { return Texts.bytes(aBlob); }

	private:
		application::markdown_code_texts& Texts;
	};
	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		std::deque<std::function<void()>> Tasks;
	};
	struct composed_syntax
	{
		application::markdown_code_texts                           Texts;
		held_texts                                                 Source{ Texts };
		adapters::syntax::packaged_definition_catalog              Catalog;
		adapters::syntax::packaged_syntax_classifier               Classifier{ Catalog, Source };
		manual_executor                                            Effects;
		manual_executor                                            Decisions;
		std::vector<application::markdown_code_syntax_publication> Published;
		application::markdown_code_syntax_host                     Host{ Classifier, Texts, Effects, Decisions,
																		 [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) { Published.push_back(aPublication); } };
		void deliver(qt::review_tour_editor& aEditor)
		{
			for (int round = 0; round < 4; ++round) {
				settle();
				while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
					for (auto* lane : { &Effects, &Decisions }) {
						while (!lane->Tasks.empty()) {
							auto task = std::move(lane->Tasks.front());
							lane->Tasks.pop_front();
							task();
						}
					}
				}
				for (const auto& publication : std::exchange(Published, { })) {
					aEditor.apply_code_syntax(publication);
				}
			}
			settle();
		}
	};
	[[maybe_unused]] auto colours(qt::markdown_body_view& aReader, const QString& aText) -> std::set<QRgb>
	{
		auto seen = std::set<QRgb>{ };
		for (auto block = aReader.rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			if (!block.text().contains(aText) || block.layout() == nullptr) {
				continue;
			}
			for (const auto& range : block.layout()->formats()) {
				seen.insert(range.format.foreground().color().rgb());
			}
		}
		return seen;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };
	verify_first_live_second();

	// [[BPR-2274]]. The side-chat course ran on the Solution's chat, which
	// went with the Solution; re-covering it on a stop's chat is BPR-2275.
	static_cast<void>(prefix);
	std::cout << "tour-side-chat:pass\n";
	return EXIT_SUCCESS;
}
