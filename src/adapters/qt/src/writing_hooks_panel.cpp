#include "bpr/qt/writing_hooks_panel.hpp"
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
namespace bpr::qt {
	writing_hooks_panel::writing_hooks_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("writingHooksPanel"));
		auto* column  = new QVBoxLayout{ this };
		auto* heading = new QLabel{ tr("AI assistance · Writing instructions"), this };
		column->addWidget(heading);
		auto* explanation = new QLabel{ tr("These instructions apply only when you explicitly ask for assistance. Test uses a sample passage with the selected agent in the open review, changes no draft and publishes nothing."), this };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		Hook = new QComboBox{ this };
		Hook->setObjectName(QStringLiteral("writingHookChoice"));
		Hook->setAccessibleName(tr("Writing hook"));
		const auto add = [this](const QString& aName, core::writing_hook aHook) { Hook->addItem(aName, QVariant::fromValue(static_cast<int>(aHook))); };
		add(tr("Problem statement"), core::writing_hook::problem);
		add(tr("Solution summary"), core::writing_hook::solution);
		add(tr("Tour outline"), core::writing_hook::outline);
		add(tr("Tour-stop comment"), core::writing_hook::comment);
		// [[BPR-1090]] The review-feedback guidance is not a tour field's hook; the separator says so.
		Hook->insertSeparator(Hook->count());
		add(tr("Review feedback"), core::writing_hook::feedback);
		column->addWidget(Hook);
		Source = new QLabel{ this };
		Source->setObjectName(QStringLiteral("writingHookSource"));
		column->addWidget(Source);
		Scope = new QLabel{ this };
		Scope->setObjectName(QStringLiteral("writingHookScope"));
		Scope->setTextFormat(Qt::PlainText);
		Scope->setWordWrap(true);
		column->addWidget(Scope);
		Instructions = new QPlainTextEdit{ this };
		Instructions->setObjectName(QStringLiteral("writingHookInstructions"));
		Instructions->setAccessibleName(tr("Writing instructions"));
		column->addWidget(Instructions);
		const auto button = [&](const QString& aText, const QString& aName, auto aCall) { auto* value = new QPushButton{aText,this}; value->setObjectName(aName); column->addWidget(value); connect(value,&QPushButton::clicked,this,aCall); return value; };
		button(tr("Save instructions"), QStringLiteral("writingHookSave"), [this] {
			if (!Save || !Save(selected_hook(), Instructions->toPlainText().toUtf8().toStdString())) {
				show_notice("Instructions could not be saved. Use valid text up to 8192 UTF-8 bytes.");
			} else {
				show_notice("Saving instructions for this profile…");
			}
		});
		button(tr("Restore default"), QStringLiteral("writingHookReset"), [this] { if (Save) { static_cast<void>(Save(selected_hook(),{})); } });
		TestButton = button(tr("Test saved hook (no publishing)"), QStringLiteral("writingHookTest"), [this] { if (Test && selected_hook() != core::writing_hook::feedback) { Test(selected_hook()); } });
		Status     = new QLabel{ this };
		Status->setObjectName(QStringLiteral("writingHookStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse);
		column->addWidget(Status);
		connect(Hook, &QComboBox::currentIndexChanged, this, [this] { show_hook(); });
		show_hook();
	}
	void writing_hooks_panel::set_dispatch(std::function<bool(core::writing_hook, std::string)> aSave, std::function<void(core::writing_hook)> aTest)
	{
		Save = std::move(aSave);
		Test = std::move(aTest);
	}
	void writing_hooks_panel::present(const core::writing_hook_settings& aSettings)
	{
		if (Settings != aSettings) {
			Settings = aSettings;
			show_hook();
			show_notice("Saved instructions are active for this profile.");
		}
	}
	void writing_hooks_panel::present_label_helper(application::review_label_helper aHelper)
	{
		if (LabelHelper != aHelper) {
			LabelHelper = aHelper;
			show_hook();
		}
	}
	void writing_hooks_panel::show_notice(std::string_view aText) { Status->setText(QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()))); }
	auto writing_hooks_panel::selected_hook() const -> core::writing_hook
	{
		const auto current = Hook->currentData();
		return current.isValid() ? static_cast<core::writing_hook>(current.toInt()) : core::writing_hook::problem;
	}
	void writing_hooks_panel::show_hook()
	{
		const auto hook    = selected_hook();
		const auto index   = static_cast<std::size_t>(hook);
		const auto text    = application::effective_writing_hook(Settings, hook);
		const auto version = application::writing_hook_version_of(hook);
		Instructions->setPlainText(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));
		Source->setText(tr("%1 · %2 · profile revision %3").arg(Settings.Overrides[index].empty() ? tr("Built-in") : tr("Profile override"), QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size()))).arg(Settings.Revision));
		const bool feedback = hook == core::writing_hook::feedback;
		Scope->setVisible(feedback);
		if (feedback) {
			Scope->setText(LabelHelper == application::review_label_helper::on
							   ? tr("On. Sent with each comment workshop request while the label helper is on.")
							   : tr("Off. Sent with comment workshop requests only while the label helper is on: Reviews & queue › Comment labels › Label helper."));
		}
		TestButton->setEnabled(!feedback);
		TestButton->setToolTip(feedback ? tr("Test uses a tour field. Ask the comment workshop to try this guidance.") : QString{ });
	}
}
