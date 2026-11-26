/// \file
/// \brief RFC-BPR-0130 stage 2, AC05. The one composer the Agent tab, the
/// tour's side chat and the side chat about one review item place: Return
/// sends, Shift+Return starts a line, Command+Return (Control+Return off
/// macOS) sends the other way, plain Return in an empty field does nothing,
/// Return is the input method's while it composes; and Send and Stop follow
/// `core::decide_chat_send`, Stop reached from the same control in each.
#include "bpr/qt/chat_composer.hpp"

#include "bpr/core/chat_send.hpp"
#include "bpr/qt/skill_completer.hpp"

#include <QApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using face = core::chat_send_face;

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

	void key(QPlainTextEdit& aText, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aKey == Qt::Key_Return ? QStringLiteral("\r") : QString{ } };
		QApplication::sendEvent(&aText, &press);
		settle();
	}
}

auto main(int aArgumentCount, char** aArguments) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	auto application = QApplication{ aArgumentCount, aArguments };

	auto  host     = QWidget{ };
	auto* column   = new QVBoxLayout{ &host };
	auto* composer = new qt::chat_composer{ { .Field = QStringLiteral("chatField"), .Text = QStringLiteral("chatText"), .Send = QStringLiteral("chatSend"), .Stop = QStringLiteral("chatStop") }, &host };
	column->addWidget(composer);
	auto sends = std::vector<bool>{ };
	auto stops = 0;
	composer->set_dispatch({ .Send = [&sends](const bool aOpposite) { sends.push_back(aOpposite); }, .Stop = [&stops] { ++stops; } });
	host.resize(320, 200);
	host.show();
	settle();
	auto& text = *composer->text();
	expect(text.objectName() == QStringLiteral("chatText") && text.parentWidget() == composer && composer->skills() != nullptr, "the composer makes its words and their skill list");
	expect(composer->send_button()->objectName() == QStringLiteral("chatSend") && composer->stop_button()->objectName() == QStringLiteral("chatStop") && composer->send_button()->isDefault(), "Send is the default button");

	// Keys.
	text.setFocus();
	key(text, Qt::Key_Return);
	expect(sends.empty(), "plain Return in an empty field sends nothing");
	text.setPlainText(QStringLiteral("Why is this clamped?"));
	key(text, Qt::Key_Return, Qt::ShiftModifier);
	expect(sends.empty() && text.toPlainText().size() > QStringLiteral("Why is this clamped?").size(), "Shift+Return starts a line: " + text.toPlainText().toStdString());
	key(text, Qt::Key_Return);
	expect(sends == std::vector{ false }, "Return sends");
	key(text, Qt::Key_Return, Qt::ControlModifier);
	key(text, Qt::Key_Enter, Qt::MetaModifier | Qt::KeypadModifier);
	expect(sends == std::vector{ false, true, true }, "Command+Return and Control+Return send the other way");
	text.clear();
	key(text, Qt::Key_Return, Qt::ControlModifier);
	expect(sends.size() == 4U && sends.back(), "Command+Return in an empty field still asks, so the chat can say why nothing went");
	{
		auto composing = QInputMethodEvent{ QStringLiteral("か"), { } };
		QApplication::sendEvent(&text, &composing);
		text.setPlainText(QStringLiteral("words"));
		key(text, Qt::Key_Return);
		expect(sends.size() == 4U, "Return is the input method's while it composes");
		auto committed = QInputMethodEvent{ };
		committed.setCommitString(QStringLiteral("か"));
		QApplication::sendEvent(&text, &committed);
		key(text, Qt::Key_Return);
		expect(sends.size() == 5U, "and sends again once it has committed");
	}

	// Send and Stop, as core decides.
	const auto shows = [&](const face aFace, const bool aBeside) {
		composer->present(core::decide_chat_send({ .Running = aFace == face::stop || aBeside, .FollowsUp = aFace == face::queue || aFace == face::steer, .Typed = aFace != face::stop, .SteerFirst = aFace == face::steer }));
		settle();
		return composer->controls().Send == aFace && composer->stop_button()->isVisibleTo(&host) == aBeside;
	};
	expect(shows(face::send, false) && composer->send_button()->text().isEmpty() && composer->send_button()->accessibleName() == QStringLiteral("Send"), "idle: the send arrow, named Send");
	expect(shows(face::queue, true) && composer->send_button()->text() == QStringLiteral("Queue") && composer->stop_button()->text() == QStringLiteral("Stop"), "typed while a turn runs: Queue, with Stop beside");
	expect(composer->stop_button()->geometry().right() < composer->send_button()->geometry().left(), "Stop stands left of Send at the row's right end");
	expect(shows(face::steer, true) && composer->send_button()->text() == QStringLiteral("Steer"), "or Steer");
	composer->stop_button()->click();
	expect(stops == 1, "Stop beside Send stops");
	composer->send_button()->click();
	expect(stops == 1 && sends.size() == 6U && !sends.back(), "Send that queues sends");
	expect(shows(face::stop, false) && composer->send_button()->text() == QStringLiteral("Stop") && composer->send_button()->accessibleName() == QStringLiteral("Stop"), "nothing to follow the turn: Send is Stop");
	composer->send_button()->click();
	expect(stops == 2 && sends.size() == 6U, "Send that says Stop stops and sends nothing");

	// A composer around a field another widget made keeps that field.
	auto* const made    = new QPlainTextEdit{ };
	auto* const wrapped = new qt::chat_composer{ { .Field = QStringLiteral("borrowedField"), .Text = { }, .Send = QStringLiteral("borrowedSend"), .Stop = QStringLiteral("borrowedStop") }, &host, made };
	made->setObjectName(QStringLiteral("madeElsewhere"));
	column->addWidget(wrapped);
	settle();
	expect(wrapped->text() == made && made->parentWidget() == wrapped && made->objectName() == QStringLiteral("madeElsewhere"), "the composer wraps a field it is given");

	std::cout << "chat-composer-tests:pass\n";
	return EXIT_SUCCESS;
}
