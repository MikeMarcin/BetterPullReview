/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). The Explain this file card at the file's
/// header: a running turn's line and activity, then the answer with its
/// references as links under a height of its own, and three actions of one
/// size that stay where they are from the press to the end. Run at 100, 150
/// and 200 per cent (`QT_SCALE_FACTOR`). `--render PATH` saves the answered
/// card for a look.

#include "bpr/core/agent_activity.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/explain_file_card.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto geometry(const qt::explain_file_card& aCard) -> std::array<QRect, 3>
	{
		const auto at = [&aCard](QPushButton* aButton) { return QRect{ aButton->mapTo(&aCard, QPoint{ 0, 0 }), aButton->size() }; };
		return { at(aCard.keep_button()), at(aCard.continue_button()), at(aCard.dismiss_button()) };
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	auto         host   = QWidget{ };
	auto* const  column = new QVBoxLayout{ &host };
	column->setContentsMargins(0, 0, 0, 0);
	auto* const card = new qt::explain_file_card{ &host };
	column->addWidget(card);
	column->addStretch(1);
	host.resize(900, 700);
	host.show();
	QApplication::processEvents();
	expect(card->isHidden(), "no card before a press");

	auto kept      = 0;
	auto continued = std::size_t{ 99U };
	auto dismissed = 0;
	auto followed  = QString{ };
	card->set_keep_dispatch([&](const QString& aPath, const QString& aAnswer) { kept += aPath == QStringLiteral("src/a.cpp") && aAnswer.contains(QStringLiteral("frame pool")) ? 1 : 100; });
	card->set_continue_dispatch([&](const std::size_t aTurn) { continued = aTurn; });
	card->set_dismiss_dispatch([&] { ++dismissed; card->clear(); });
	card->set_link_dispatch([&](const QString& aLink) { followed = aLink; });

	// Running: the line is live and says what the agent is doing.
	auto running    = qt::explain_file_progress{ .Path = QStringLiteral("src/a.cpp"), .Turn = 3U };
	auto status     = qt::agent_run_status{ };
	status.Agent    = QStringLiteral("Claude Code");
	status.Stage    = qt::agent_run_stage::thinking;
	running.Running = status;
	running.Started.start();
	running.Activity = core::agent_turn_activity{ };
	card->present(running);
	QApplication::processEvents();
	expect(card->isVisible() && card->status_line()->live() && card->status_line()->text().contains(QStringLiteral("thinking")), "a running turn says so on the card");
	expect(!card->keep_button()->isEnabled() && card->continue_button()->isEnabled() && card->dismiss_button()->isEnabled(), "Keep local waits for the answer");
	expect(qt::agent_run_control(*card->continue_button()) && !qt::agent_run_control(*card->keep_button()), "Continue in Agent tab wears the Agent accent; the others do not");
	const auto before = geometry(*card);
	expect(before[0].size() == before[1].size() && before[1].size() == before[2].size(), "the three actions are one size");

	// Answered: the answer with its links, the ending, and nothing moved.
	auto answered       = qt::explain_file_progress{ .Path = QStringLiteral("src/a.cpp"), .Turn = 3U, .Seconds = 11 };
	answered.AnswerText = QStringLiteral("It adds a frame pool at src/a.cpp:12.");
	answered.Answer     = QStringLiteral("It adds a frame pool at [src/a.cpp:12](bpr-line:src/a.cpp:12).");
	answered.Ending     = QStringLiteral("Claude answered in 0:11");
	answered.Answered   = true;
	card->present(answered);
	QApplication::processEvents();
	expect(!card->status_line()->live() && card->status_line()->text().contains(QStringLiteral("answered")), "the line says how it ended");
	expect(card->answer_view()->isVisible() && card->answer_view()->original_body().find("](bpr-line:") != std::string::npos, "the answer's references are links");
	expect(card->keep_button()->isEnabled(), "Keep local is offered once answered");
	expect(geometry(*card)[0].size() == before[0].size() && geometry(*card)[1].size() == before[1].size(), "the actions keep their size from the press to the end");

	// A long answer scrolls inside the card rather than pushing the file away.
	auto longer = answered;
	for (auto line = 0; line < 80; ++line) {
		longer.Answer += QStringLiteral("\n\nAnother paragraph of the explanation.");
	}
	card->present(longer);
	QApplication::processEvents();
	auto* const scroll = card->findChild<QScrollArea*>(QStringLiteral("explainFileAnswerScroll"));
	expect(scroll != nullptr && scroll->height() <= 15 * card->fontMetrics().lineSpacing(), "a long answer keeps the card's own height and scrolls");
	card->present(answered);

	// The three actions.
	card->keep_button()->click();
	expect(kept == 1 && !card->keep_button()->isEnabled(), "Keep local sends the file and the answer once");
	card->present_note(true, { }, 42U);
	QApplication::processEvents();
	expect(card->note() == 42U && card->keep_button()->text() == QStringLiteral("Go to comment") && card->keep_button()->isEnabled(), "kept: Go to comment, in the same place");
	expect(geometry(*card)[0] == geometry(*card)[0] && card->keep_button()->size() == before[0].size(), "Go to comment keeps the button's size");
	card->continue_button()->click();
	expect(continued == 3U, "Continue in Agent tab names the turn");
	card->dismiss_button()->click();
	expect(dismissed == 1 && card->isHidden(), "Dismiss removes the card");

	// A refusal is said on the card.
	card->present(qt::explain_file_progress{ .Path = QStringLiteral("src/a.cpp"), .Turn = 99U, .Ending = QStringLiteral("Not sent. The agent is answering another question.") });
	QApplication::processEvents();
	expect(card->isVisible() && card->status_line()->text().contains(QStringLiteral("Not sent")) && !card->keep_button()->isEnabled(), "a refusal is said where the answer would be");

	if (argc > 2 && std::string_view{ argv[1] } == "--render") {
		card->present(answered);
		QApplication::processEvents();
		static_cast<void>(host.grab().save(QString::fromLocal8Bit(argv[2])));
	}
	std::cout << "explain-file-card:pass scale=" << host.devicePixelRatioF() << '\n';
	return EXIT_SUCCESS;
}
