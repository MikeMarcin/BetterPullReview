#include "bpr/qt/sensitive_text_input.hpp"

#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Widget>
	[[nodiscard]] auto top_level_widget(const char* const aObjectName) -> Widget*
	{
		for (auto* const widget : QApplication::topLevelWidgets()) {
			if (widget->objectName() == QString::fromLatin1(aObjectName)) {
				return qobject_cast<Widget*>(widget);
			}
		}
		return nullptr;
	}

	auto close_active_dialog() -> void
	{
		if (auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
			dialog != nullptr) {
			dialog->reject();
		}
	}

	void start_dialog_watchdog(QTimer& aWatchdog)
	{
		aWatchdog.setSingleShot(true);
		QObject::connect(
			&aWatchdog,
			&QTimer::timeout,
			[] { close_active_dialog(); });
		aWatchdog.start(2'000);
	}

	struct prompt_probe
	{
		bool DialogFound           = false;
		bool EditorFound           = false;
		bool PasswordEcho          = false;
		bool MaximumApplied        = false;
		bool AccessibleNamePresent = false;
	};

	auto test_accepts_bounded_text() -> void
	{
		constexpr std::size_t maximumBytes = 64U;
		const auto            expectedText = std::string{ "bounded-sensitive-value" };
		auto                  probe        = prompt_probe{ };

		QTimer watchdog;
		start_dialog_watchdog(watchdog);

		QTimer::singleShot(0, [&probe, &expectedText] {
			auto* const dialog = top_level_widget<QDialog>("bpr-sensitive-text-prompt");
			probe.DialogFound  = dialog != nullptr;
			if (dialog == nullptr) {
				close_active_dialog();
				return;
			}

			auto* const editor = dialog->findChild<QLineEdit*>("bpr-sensitive-text-editor");
			probe.EditorFound  = editor != nullptr;
			if (editor == nullptr) {
				dialog->reject();
				return;
			}

			probe.PasswordEcho   = editor->echoMode() == QLineEdit::Password;
			probe.MaximumApplied = editor->maxLength()
				== static_cast<int>(maximumBytes);
			probe.AccessibleNamePresent = !editor->accessibleName().isEmpty();
			editor->setText(QString::fromUtf8(
				expectedText.data(),
				static_cast<qsizetype>(expectedText.size())));
			dialog->accept();
		});

		auto value = qt::prompt_for_sensitive_text(maximumBytes);
		watchdog.stop();

		expect(probe.DialogFound, "prompt dialog becomes the active Qt surface");
		expect(probe.EditorFound, "prompt owns a line editor");
		expect(probe.PasswordEcho, "sensitive text uses password echo");
		expect(probe.MaximumApplied, "caller byte bound limits editor characters");
		expect(
			probe.AccessibleNamePresent,
			"sensitive editor has an accessible name");
		expect(value.has_value(), "accepted bounded input returns a value");

		const auto expected = std::as_bytes(std::span{
			expectedText.data(),
			expectedText.size() });
		expect(
			std::ranges::equal(value->bytes(), expected),
			"returned bytes exactly match accepted UTF-8 input");

		auto moved = std::move(*value);
		expect(value->empty(), "move construction empties the source value");
		expect(
			std::ranges::equal(moved.bytes(), expected),
			"move construction preserves the owned bytes");
		moved.reset();
		expect(moved.empty() && moved.empty(), "explicit reset empties value");
	}

	auto test_cancel_returns_no_value() -> void
	{
		auto   dialogFound = false;
		QTimer watchdog;
		start_dialog_watchdog(watchdog);

		QTimer::singleShot(0, [&dialogFound] {
			auto* const dialog = top_level_widget<QDialog>("bpr-sensitive-text-prompt");
			dialogFound        = dialog != nullptr;
			if (dialog == nullptr) {
				close_active_dialog();
				return;
			}
			auto* const editor = dialog->findChild<QLineEdit*>("bpr-sensitive-text-editor");
			if (editor != nullptr) {
				editor->setText(QStringLiteral("cancelled-sensitive-value"));
			}
			dialog->reject();
		});

		auto value = qt::prompt_for_sensitive_text(64U);
		watchdog.stop();
		expect(dialogFound, "cancel test observes the prompt");
		expect(!value.has_value(), "cancel returns no sensitive value");
	}

	struct warning_probe
	{
		bool PromptFound        = false;
		bool WarningFound       = false;
		bool WarningIsPlainText = false;
		bool WarningOmitsInput  = false;
	};

	auto test_rejects_utf8_over_byte_limit_with_safe_warning() -> void
	{
		constexpr std::size_t maximumBytes = 8U;
		const auto            overLimit    = QStringLiteral("秘密秘密秘密");
		auto                  probe        = warning_probe{ };

		QTimer watchdog;
		start_dialog_watchdog(watchdog);

		QTimer::singleShot(0, [&probe, &overLimit] {
			auto* const dialog = top_level_widget<QDialog>("bpr-sensitive-text-prompt");
			probe.PromptFound  = dialog != nullptr;
			if (dialog == nullptr) {
				close_active_dialog();
				return;
			}

			auto* const editor = dialog->findChild<QLineEdit*>("bpr-sensitive-text-editor");
			if (editor == nullptr) {
				dialog->reject();
				return;
			}
			editor->setText(overLimit);

			QTimer::singleShot(0, [&probe, &overLimit] {
				auto* const warning = top_level_widget<QMessageBox>(
					"bpr-sensitive-text-unavailable-warning");
				probe.WarningFound = warning != nullptr;
				if (warning == nullptr) {
					close_active_dialog();
					return;
				}
				probe.WarningIsPlainText = warning->textFormat() == Qt::PlainText;
				probe.WarningOmitsInput  = !warning->text().contains(overLimit);
				warning->accept();
			});
			dialog->accept();
		});

		auto value = qt::prompt_for_sensitive_text(maximumBytes);
		watchdog.stop();

		expect(probe.PromptFound, "over-limit test observes the prompt");
		expect(probe.WarningFound, "over-limit input shows a safe warning");
		expect(probe.WarningIsPlainText, "warning cannot interpret rich text");
		expect(probe.WarningOmitsInput, "warning does not echo sensitive input");
		expect(!value.has_value(), "over-limit UTF-8 input is rejected");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_accepts_bounded_text();
	test_cancel_returns_no_value();
	test_rejects_utf8_over_byte_limit_with_safe_warning();

	std::cout << "sensitive-text-input-tests:pass\n";
	return EXIT_SUCCESS;
}
