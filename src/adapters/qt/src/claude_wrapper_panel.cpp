#include "bpr/qt/claude_wrapper_panel.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
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

		/// A wrapped line shown, hidden or rewritten after the page was laid out
		/// changes the height its form row needs; the form, the page and its
		/// scroll area learn it only when their layouts are asked again.
		void relayout(QWidget* const aFrom)
		{
			for (auto* widget = aFrom; widget != nullptr; widget = widget->parentWidget()) {
				if (auto* const layout = widget->layout(); layout != nullptr) {
					layout->invalidate();
				}
				widget->updateGeometry();
				if (widget->isWindow()) {
					break;
				}
			}
		}

		/// Every line in these rows is set in the page's own text role and size,
		/// like the page's other explanations. The quieter placeholder role read
		/// below 4.5:1 against the page in the light and high-contrast light
		/// themes and in macOS light ([[BPR-1301]] sign-in-line-v002, "Cant read
		/// this").
		[[nodiscard]] auto note(const QString& aText, QWidget* const aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setForegroundRole(QPalette::WindowText);
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

		auto* const form = new QFormLayout{ };
		// The field column takes the row's width under every style. macOS keeps
		// fields at their size hint, which drew the wrapped explanation and the
		// Sign-in line narrower than the width their height was measured at, so
		// both were cut off top and bottom ([[BPR-1301]] sign-in-line-v002).
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		auto* const wrapper = new QWidget{ this };
		auto* const stack   = new QVBoxLayout{ wrapper };
		// Space under the explanation, so the Sign-in row reads as its own row.
		stack->setContentsMargins(0, 0, 0, 8);
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
		Choose = new quiet_push_button{ tr("Choose…"), wrapper };
		Choose->setObjectName(QStringLiteral("claudeWrapperChoose"));
		Choose->setAccessibleName(tr("Choose Claude process wrapper"));
		line->addWidget(Choose);
		SaveButton = new QPushButton{ tr("Save"), wrapper };
		SaveButton->setObjectName(QStringLiteral("claudeWrapperSave"));
		SaveButton->setAccessibleName(tr("Save Claude process wrapper"));
		line->addWidget(SaveButton);
		stack->addLayout(line);
		// The answer sits under the field it answers; the explanation under
		// that, set apart by space rather than by a fainter colour.
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
		stack->addSpacing(4);
		stack->addWidget(help);
		// The label names the path field itself, so search and assistive
		// technology read "Process wrapper" for the field, not its row.
		auto* const label = new QLabel{ tr("Process wrapper"), this };
		label->setBuddy(Path);
		form->addRow(label, wrapper);

		// [[BPR-1375]], owner 2026-10-09. The model Claude is asked for, as
		// one `--model` argument; empty leaves the choice to Claude.
		auto* const model      = new QWidget{ this };
		auto* const modelStack = new QVBoxLayout{ model };
		modelStack->setContentsMargins(0, 0, 0, 8);
		auto* const modelLine = new QHBoxLayout{ };
		Model                 = new QLineEdit{ model };
		Model->setObjectName(QStringLiteral("claudeModel"));
		Model->setAccessibleName(tr("Claude model"));
		Model->setAccessibleDescription(tr("Model name passed to claude with --model for this profile"));
		Model->setPlaceholderText(tr("Claude's default"));
		Model->setClearButtonEnabled(true);
		Model->setMaxLength(static_cast<int>(core::maximum_claude_model_bytes));
		modelLine->addWidget(Model, 1);
		ModelSave = new QPushButton{ tr("Save"), model };
		ModelSave->setObjectName(QStringLiteral("claudeModelSave"));
		ModelSave->setAccessibleName(tr("Save Claude model"));
		modelLine->addWidget(ModelSave);
		modelStack->addLayout(modelLine);
		ModelStatus = note({ }, model);
		ModelStatus->setObjectName(QStringLiteral("claudeModelStatus"));
		ModelStatus->setAccessibleName(tr("Claude model status"));
		ModelStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ModelStatus->hide();
		modelStack->addWidget(ModelStatus);
		auto* const modelHelp = note(tr("Passed to claude as --model for tour writing and Ask agent requests in this profile, for example "
										"claude-sonnet-5-5. Leave it empty to use Claude's own default."),
									 model);
		modelHelp->setObjectName(QStringLiteral("claudeModelHelp"));
		modelStack->addSpacing(4);
		modelStack->addWidget(modelHelp);
		auto* const modelLabel = new QLabel{ tr("Model"), this };
		modelLabel->setBuddy(Model);
		form->addRow(modelLabel, model);

		// [[BPR-1301]], owner 2026-10-06 ("Accept (no switch)"). BetterPullReview
		// has no sign-in of its own for an agent, so there is nothing to switch.
		auto* const signIn = note(tr("Claude and your wrapper handle sign-in. BetterPullReview never asks."), this);
		signIn->setObjectName(QStringLiteral("claudeSignIn"));
		form->addRow(tr("Sign-in"), signIn);
		column->addLayout(form);

		connect(ModelSave, &QPushButton::clicked, this, [this] { save_model(); });
		connect(Model, &QLineEdit::returnPressed, this, [this] { save_model(); });
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
		relayout(Found);
		locate();
	}

	void claude_wrapper_panel::set_model_dispatch(std::function<bool(std::string)> aSave)
	{
		SaveModel = std::move(aSave);
	}

	void claude_wrapper_panel::present_model(const std::string_view aSaved)
	{
		const auto changed = !ModelPresented || SavedModel != aSaved;
		ModelPresented     = true;
		SavedModel         = std::string{ aSaved };
		if (!changed) {
			return;
		}
		Model->setText(text(aSaved));
		show_model_status(aSaved.empty() ? tr("Using Claude's default model.") : tr("Saved for this profile."));
	}

	void claude_wrapper_panel::save_model()
	{
		const auto typed = utf8(Model->text().trimmed());
		if (!core::storable_claude_model(typed)) {
			show_model_status(tr("Not saved. Use the model's name as claude --model takes it: letters, digits and . _ - : @ / [ ], starting with a letter or digit."));
			return;
		}
		if (!SaveModel || !SaveModel(typed)) {
			show_model_status(tr("Not saved. This profile's settings are not available yet."));
			return;
		}
		show_model_status(typed == SavedModel ? (typed.empty() ? tr("Using Claude's default model.") : tr("Saved for this profile.")) : tr("Saving…"));
	}

	void claude_wrapper_panel::show_model_status(const QString& aText)
	{
		ModelStatus->setText(aText);
		ModelStatus->setVisible(!aText.isEmpty());
		relayout(ModelStatus);
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
		relayout(Found);
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
		relayout(Status);
	}
}
