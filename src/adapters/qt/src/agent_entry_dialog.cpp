#include "bpr/qt/agent_entry_dialog.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto protocol_words(const core::agent_protocol aProtocol) -> QString
		{
			switch (aProtocol) {
			case core::agent_protocol::claude_stream_json: return QObject::tr("Claude Code (stream JSON)");
			case core::agent_protocol::codex_app_server:   return QObject::tr("Codex (App Server)");
			case core::agent_protocol::codex_jsonl_exec:   return QObject::tr("Codex (JSONL)");
			case core::agent_protocol::external_terminal:  return QObject::tr("Plain text");
			}
			return { };
		}
	} // namespace

	agent_entry_dialog::agent_entry_dialog(const core::agent_entry& aEntry, QWidget* aParent)
		: QDialog{ aParent }
		, Base{ aEntry }
	{
		setObjectName(QStringLiteral("agentEntryDialog"));
		setWindowTitle(aEntry.Id == 0U ? tr("Add agent") : tr("Edit agent"));
		auto* const column = new QVBoxLayout{ this };
		auto* const form   = new QFormLayout{ };
		Name               = new QLineEdit{ text_of(aEntry.Name), this };
		Name->setObjectName(QStringLiteral("agentEntryName"));
		Name->setPlaceholderText(tr("Work Claude"));
		form->addRow(tr("Name"), Name);
		Protocol = new QComboBox{ this };
		Protocol->setObjectName(QStringLiteral("agentEntryProtocol"));
		for (const auto protocol : { core::agent_protocol::claude_stream_json, core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::external_terminal }) {
			Protocol->addItem(protocol_words(protocol), static_cast<int>(protocol));
		}
		Protocol->setCurrentIndex(Protocol->findData(static_cast<int>(aEntry.Protocol)));
		Protocol->setToolTip(tr("What the program speaks: Claude Code's stream JSON, one of Codex's two protocols, or plain text in and out"));
		form->addRow(tr("Speaks"), Protocol);
		auto* const programRow = new QHBoxLayout{ };
		Program                = new QLineEdit{ text_of(aEntry.Program), this };
		Program->setObjectName(QStringLiteral("agentEntryProgram"));
		Program->setPlaceholderText(aEntry.Preset ? tr("Empty: the one found on this computer") : tr("/usr/local/bin/my-agent"));
		programRow->addWidget(Program, 1);
		auto* const browse = new quiet_push_button{ tr("Choose…"), this };
		browse->setObjectName(QStringLiteral("agentEntryChoose"));
		connect(browse, &QPushButton::clicked, this, [this] {
			const auto chosen = QFileDialog::getOpenFileName(this, tr("Choose the program"), Program->text());
			if (!chosen.isEmpty()) {
				Program->setText(QDir::toNativeSeparators(chosen));
			}
		});
		programRow->addWidget(browse);
		form->addRow(tr("Program"), programRow);
		Arguments = new QPlainTextEdit{ this };
		Arguments->setObjectName(QStringLiteral("agentEntryArguments"));
		Arguments->setPlaceholderText(tr("One per line, such as --codex"));
		Arguments->setMaximumHeight(72);
		Arguments->setTabChangesFocus(true);
		auto joined = QStringList{ };
		for (const auto& argument : aEntry.Arguments) {
			joined << text_of(argument);
		}
		Arguments->setPlainText(joined.join(QLatin1Char{ '\n' }));
		form->addRow(tr("Arguments"), Arguments);
		Model = new QLineEdit{ text_of(aEntry.Model), this };
		Model->setObjectName(QStringLiteral("agentEntryModel"));
		Model->setPlaceholderText(tr("Empty: the agent's own default"));
		form->addRow(tr("Model"), Model);
		Effort = new QComboBox{ this };
		Effort->setObjectName(QStringLiteral("agentEntryEffort"));
		form->addRow(tr("Effort"), Effort);
		column->addLayout(form);
		auto* const words = new QLabel{ tr("The arguments are given to the program exactly as written, before the request's own, every time. Nothing is put into them and no shell runs them."), this };
		words->setWordWrap(true);
		words->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(words);
		Problem = new QLabel{ this };
		Problem->setObjectName(QStringLiteral("agentEntryProblem"));
		Problem->setWordWrap(true);
		column->addWidget(Problem);
		Buttons = new QDialogButtonBox{ QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this };
		connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		column->addWidget(Buttons);
		offer_efforts();
		Effort->setCurrentIndex(std::max(0, Effort->findData(text_of(aEntry.Effort))));
		connect(Name, &QLineEdit::textChanged, this, [this] { settle(); });
		connect(Program, &QLineEdit::textChanged, this, [this] { settle(); });
		connect(Arguments, &QPlainTextEdit::textChanged, this, [this] { settle(); });
		connect(Model, &QLineEdit::textChanged, this, [this] { settle(); });
		connect(Effort, &QComboBox::currentIndexChanged, this, [this] { settle(); });
		connect(Protocol, &QComboBox::currentIndexChanged, this, [this] {
			offer_efforts();
			settle();
		});
		settle();
	}

	void agent_entry_dialog::offer_efforts()
	{
		const auto kept = Effort->currentData().toString();
		Effort->clear();
		Effort->addItem(tr("The agent's own default"), QString{ });
		const auto protocol = static_cast<core::agent_protocol>(Protocol->currentData().toInt());
		if (const auto program = core::entry_settings_program(protocol)) {
			const auto levels = *program == core::agent_program::claude ? std::array<const char*, 5>{ "low", "medium", "high", "xhigh", "max" }
																		: std::array<const char*, 5>{ "minimal", "low", "medium", "high", "xhigh" };
			for (const auto* const level : levels) {
				Effort->addItem(QString::fromLatin1(level), QString::fromLatin1(level));
			}
		}
		Effort->setCurrentIndex(std::max(0, Effort->findData(kept)));
		Model->setEnabled(core::entry_settings_program(protocol).has_value());
	}

	auto agent_entry_dialog::entry() const -> core::agent_entry
	{
		auto result     = Base;
		result.Name     = Name->text().trimmed().toStdString();
		result.Protocol = static_cast<core::agent_protocol>(Protocol->currentData().toInt());
		result.Program  = Program->text().trimmed().toStdString();
		result.Arguments.clear();
		for (const auto& line : Arguments->toPlainText().split(QLatin1Char{ '\n' })) {
			if (!line.trimmed().isEmpty()) {
				result.Arguments.push_back(line.trimmed().toStdString());
			}
		}
		result.Model  = Model->isEnabled() ? Model->text().trimmed().toStdString() : std::string{ };
		result.Effort = Effort->currentData().toString().toStdString();
		// A popular agent whose protocol was changed is a custom one now.
		const auto codex = result.Protocol == core::agent_protocol::codex_app_server || result.Protocol == core::agent_protocol::codex_jsonl_exec;
		if ((result.Preset == core::setup_agent::claude_code && result.Protocol != core::agent_protocol::claude_stream_json) || (result.Preset == core::setup_agent::codex && !codex)) {
			result.Preset.reset();
		}
		return result;
	}

	void agent_entry_dialog::settle()
	{
		const auto refusal = core::inspect_agent_entry(entry());
		Problem->setText(refusal ? text_of(core::describe(*refusal)) : QString{ });
		Problem->setVisible(refusal.has_value());
		Buttons->button(QDialogButtonBox::Ok)->setEnabled(!refusal.has_value());
	}

	auto default_agent_entry_editor() -> agent_entry_editor
	{
		return [](const core::agent_entry& aStart, QWidget* aParent) -> std::optional<core::agent_entry> {
			auto dialog = agent_entry_dialog{ aStart, aParent };
			if (dialog.exec() != QDialog::Accepted) {
				return std::nullopt;
			}
			return dialog.entry();
		};
	}
} // namespace bpr::qt
