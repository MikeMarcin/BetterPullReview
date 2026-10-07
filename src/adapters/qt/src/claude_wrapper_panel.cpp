#include "bpr/qt/claude_wrapper_panel.hpp"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		[[nodiscard]] auto text(const std::string_view aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		[[nodiscard]] auto note(const QString& aText, QWidget* const aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}
	}

	claude_wrapper_panel::claude_wrapper_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("claudeWrapperPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		auto* const heading = new QLabel{ tr("Claude"), this };
		heading->setObjectName(QStringLiteral("claudeWrapperHeading"));
		auto font = heading->font();
		font.setWeight(QFont::DemiBold);
		heading->setFont(font);
		column->addWidget(heading);

		auto* const form    = new QFormLayout{ };
		auto* const wrapper = new QWidget{ this };
		auto* const stack   = new QVBoxLayout{ wrapper };
		stack->setContentsMargins(0, 0, 0, 0);
		auto* const line = new QHBoxLayout{ };
		Path             = new QLineEdit{ wrapper };
		Path->setObjectName(QStringLiteral("claudeWrapperPath"));
		Path->setAccessibleName(tr("Claude process wrapper"));
		Path->setAccessibleDescription(tr("Full path to the program run in place of claude for this profile"));
#if defined(Q_OS_WIN)
		Path->setPlaceholderText(QStringLiteral("C:\\full\\path\\to\\claude.exe"));
#else
		Path->setPlaceholderText(QStringLiteral("/full/path/to/claude"));
#endif
		Path->setClearButtonEnabled(true);
		line->addWidget(Path, 1);
		Choose = new QPushButton{ tr("Choose…"), wrapper };
		Choose->setObjectName(QStringLiteral("claudeWrapperChoose"));
		Choose->setAccessibleName(tr("Choose Claude process wrapper"));
		line->addWidget(Choose);
		SaveButton = new QPushButton{ tr("Save"), wrapper };
		SaveButton->setObjectName(QStringLiteral("claudeWrapperSave"));
		SaveButton->setAccessibleName(tr("Save Claude process wrapper"));
		line->addWidget(SaveButton);
		stack->addLayout(line);
		// The answer sits under the field it answers; the explanation, quieter,
		// under that.
		Status = note({ }, wrapper);
		Status->setObjectName(QStringLiteral("claudeWrapperStatus"));
		Status->setAccessibleName(tr("Claude process wrapper status"));
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Status->hide();
		stack->addWidget(Status);
		// [[BPR-1301]], owner 2026-10-07. Which claude runs while the field is
		// empty: the path itself, so the person sees exactly what will start.
		Found = note({ }, wrapper);
		Found->setObjectName(QStringLiteral("claudeWrapperFound"));
		Found->setAccessibleName(tr("Claude used without a wrapper"));
		Found->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Found->hide();
		stack->addWidget(Found);
		auto* const help = note(tr("Runs in place of claude for every Claude request in this profile, with Claude's own arguments, "
								   "folder and environment, never through a shell. Use the full path to claude, or to a program "
								   "that sets up your provider and then runs it. Leave it empty to use the claude found on PATH."),
								wrapper);
		help->setObjectName(QStringLiteral("claudeWrapperHelp"));
		help->setForegroundRole(QPalette::PlaceholderText);
		stack->addWidget(help);
		// The label names the path field itself, so search and assistive
		// technology read "Process wrapper" for the field, not its row.
		auto* const label = new QLabel{ tr("Process wrapper"), this };
		label->setBuddy(Path);
		form->addRow(label, wrapper);

		// [[BPR-1301]], owner 2026-10-06 ("Accept (no switch)"). BetterPullReview
		// has no sign-in of its own for an agent, so there is nothing to switch.
		auto* const signIn = note(tr("Sign-in is left to Claude and the wrapper; BetterPullReview never asks you to sign in to Claude."), this);
		signIn->setObjectName(QStringLiteral("claudeSignIn"));
		form->addRow(tr("Sign-in"), signIn);
		column->addLayout(form);

		connect(SaveButton, &QPushButton::clicked, this, [this] { save(Path->text()); });
		connect(Path, &QLineEdit::returnPressed, this, [this] { save(Path->text()); });
		connect(Choose, &QPushButton::clicked, this, [this] {
			const auto chosen = QFileDialog::getOpenFileName(this, tr("Choose Claude process wrapper"), Path->text());
			if (!chosen.isEmpty()) {
				Path->setText(chosen);
				save(chosen);
			}
		});
	}

	void claude_wrapper_panel::set_dispatch(std::function<void(std::string)> aSave, std::function<void()> aLocate)
	{
		Save   = std::move(aSave);
		Locate = std::move(aLocate);
	}

	void claude_wrapper_panel::present(const std::string_view aSaved)
	{
		const auto changed = !Presented || Saved != aSaved;
		Presented          = true;
		Saved              = std::string{ aSaved };
		if (!changed) {
			return;
		}
		Path->setText(text(aSaved));
		show_status(aSaved.empty() ? tr("No wrapper is saved for this profile.") : tr("Saved for this profile."));
		// A saved wrapper always wins, so the found line says nothing then.
		Found->setVisible(aSaved.empty() && !Found->text().isEmpty());
		locate();
	}

	void claude_wrapper_panel::show_found(const std::optional<core::claude_search_candidate>& aFound)
	{
		if (!aFound) {
			Found->setText(tr("claude not found on PATH or in the usual install folders; set a wrapper."));
		} else if (aFound->Origin == core::claude_found_in::path) {
			Found->setText(tr("Using %1 (found on PATH).").arg(text(aFound->Path)));
		} else {
			Found->setText(tr("Using %1 (found in a usual install folder, not on PATH).").arg(text(aFound->Path)));
		}
		Found->setVisible(Presented && Saved.empty());
	}

	void claude_wrapper_panel::showEvent(QShowEvent* const aEvent)
	{
		QWidget::showEvent(aEvent);
		// Opening Settings looks again, so a claude installed or removed since
		// is what the line names.
		if (!aEvent->spontaneous()) {
			locate();
		}
	}

	void claude_wrapper_panel::locate()
	{
		if (Presented && Saved.empty() && Locate && isVisible()) {
			Locate();
		}
	}

	void claude_wrapper_panel::show_answer(const application::claude_wrapper_answer& aAnswer)
	{
		if (aAnswer.Refusal) {
			show_status(tr("Not saved. %1").arg(text(core::describe(*aAnswer.Refusal))));
		} else if (!aAnswer.Submitted) {
			show_status(tr("Not saved. This profile's settings are not available yet."));
		} else if (aAnswer.Path == Saved) {
			show_status(aAnswer.Path.empty() ? tr("No wrapper is saved for this profile.") : tr("Saved for this profile."));
		} else {
			show_status(tr("Saving…"));
		}
	}

	void claude_wrapper_panel::save(const QString& aText)
	{
		if (!Save) {
			show_status(tr("Not saved. This profile's settings are not available yet."));
			return;
		}
		show_status(aText.isEmpty() ? tr("Clearing…") : tr("Checking…"));
		Save(utf8(aText));
	}

	void claude_wrapper_panel::show_status(const QString& aText)
	{
		Status->setText(aText);
		Status->setVisible(!aText.isEmpty());
	}
}
