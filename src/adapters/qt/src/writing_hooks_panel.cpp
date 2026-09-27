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
		auto* heading = new QLabel{ tr("AI assistance · Review preparation"), this };
		column->addWidget(heading);
		auto* explanation = new QLabel{ tr("These instructions apply only when you explicitly ask for assistance. Test uses a sample passage with the selected agent in the open review, changes no draft and publishes nothing."), this };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		Hook = new QComboBox{ this };
		Hook->setObjectName(QStringLiteral("writingHookChoice"));
		Hook->setAccessibleName(tr("Review preparation writing hook"));
		Hook->addItems({ tr("Problem statement"), tr("Solution summary"), tr("Tour outline"), tr("Tour-stop comment") });
		column->addWidget(Hook);
		Source = new QLabel{ this };
		Source->setObjectName(QStringLiteral("writingHookSource"));
		column->addWidget(Source);
		Instructions = new QPlainTextEdit{ this };
		Instructions->setObjectName(QStringLiteral("writingHookInstructions"));
		Instructions->setAccessibleName(tr("Writing instructions"));
		column->addWidget(Instructions);
		const auto button = [&](const QString& aText, const QString& aName, auto aCall) { auto* value = new QPushButton{aText,this}; value->setObjectName(aName); column->addWidget(value); connect(value,&QPushButton::clicked,this,aCall); };
		button(tr("Save instructions"), QStringLiteral("writingHookSave"), [this] {
			if (!Save || !Save(static_cast<core::writing_hook>(Hook->currentIndex()), Instructions->toPlainText().toUtf8().toStdString())) {
				show_notice("Instructions could not be saved. Use valid text up to 8192 UTF-8 bytes.");
			} else {
				show_notice("Saving instructions for this profile…");
			}
		});
		button(tr("Restore default"), QStringLiteral("writingHookReset"), [this] { if (Save) { static_cast<void>(Save(static_cast<core::writing_hook>(Hook->currentIndex()),{})); } });
		button(tr("Test saved hook (no publishing)"), QStringLiteral("writingHookTest"), [this] { if (Test) { Test(static_cast<core::writing_hook>(Hook->currentIndex())); } });
		Status = new QLabel{ this };
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
	void writing_hooks_panel::show_notice(std::string_view aText) { Status->setText(QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()))); }
	void writing_hooks_panel::show_hook()
	{
		const auto index = static_cast<std::size_t>(Hook->currentIndex());
		const auto text  = application::effective_writing_hook(Settings, static_cast<core::writing_hook>(index));
		Instructions->setPlainText(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));
		Source->setText(tr("%1 · %2 · profile revision %3").arg(Settings.Overrides[index].empty() ? tr("Built-in") : tr("Profile override"), QString::fromUtf8(application::writing_hook_version.data(), static_cast<qsizetype>(application::writing_hook_version.size()))).arg(Settings.Revision));
	}
}
