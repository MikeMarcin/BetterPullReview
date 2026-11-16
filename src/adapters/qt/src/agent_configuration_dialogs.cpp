#include "bpr/qt/agent_configuration_dialogs.hpp"

#include "bpr/application/writing_hooks.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "profile_settings_parts.hpp"
#include "pull_request_overview_support.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <ranges>

namespace bpr::qt {
	namespace {
		constexpr int ItemRole   = Qt::UserRole;
		constexpr int IndexRole  = Qt::UserRole + 1;
		constexpr int AgentKind  = 1;
		constexpr int PromptKind = 2;
		/// [[BPR-2032]].
		constexpr int WorkflowKind = 3;
		/// [[BPR-2198]]. Identity's own row, and one repository.
		constexpr int IdentityKind   = 4;
		constexpr int RepositoryKind = 5;
		/// A section's row, by `core::profile_settings_section`.
		constexpr int SectionKind = 6;

		/// \brief A section's row: ticking it ticks every item under it, and
		/// unticking it unticks them; its tick says whether all, some or none
		/// are chosen.
		[[nodiscard]] auto section_row(QTreeWidget* const aItems, const QString& aName, const QString& aSays, const core::profile_settings_section aSection) -> QTreeWidgetItem*
		{
			auto* const row = new QTreeWidgetItem{ aItems, { aName, aSays } };
			row->setData(0, ItemRole, SectionKind);
			row->setData(0, IndexRole, static_cast<int>(aSection));
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
			// Its name is set semibold by the row delegate; its line stays quiet.
			row->setData(0, detail::row_strong_role, true);
			return row;
		}

		/// A dialog is a window, and Qt does not pass a window's palette to
		/// the windows it opens unless asked (owner, look workflow-import-v002:
		/// "Something went way wrong with the colors"). Asked, and attached
		/// to its opener again so the asking takes effect, it inherits the
		/// opener's palette and font and follows every later change; set
		/// explicitly, it would keep the first one.
		void adopt_opener_look(QWidget& aDialog, QWidget* const aOpener)
		{
			aDialog.setAttribute(Qt::WA_WindowPropagation);
			if (aOpener != nullptr) {
				aDialog.setParent(aOpener, aDialog.windowFlags());
			}
		}

		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		/// [[BPR-2204]]. Secondary text: the placeholder colour at Settings'
		/// secondary size, as the design's second lines are.
		[[nodiscard]] auto note(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			label->setFont(detail::secondary_font(label->font()));
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		}

		/// Body text that wraps: what a choice does.
		[[nodiscard]] auto body(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		}

		/// [[BPR-2204]]. A sentence under a title wraps short of the full
		/// width, about 78 figures, as the design's do.
		void wrap_short(QLabel& aLabel, const int aFigures)
		{
			aLabel.setMaximumWidth(QFontMetrics{ aLabel.font() }.horizontalAdvance(QLatin1Char('0')) * aFigures);
		}

		/// [[BPR-2204]]. The dialog's title: semibold, larger, close above its sentence.
		[[nodiscard]] auto sheet_title(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setWeight(QFont::Bold);
			font.setPointSizeF(font.pointSizeF() * 1.15);
			label->setFont(font);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}

		/// Lays a view's rows out again when its viewport's width changes.
		class relayout_on_resize final : public QObject
		{
		public:
			explicit relayout_on_resize(QAbstractItemView* const aView)
				: QObject{ aView }
				, View{ aView }
			{
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::Resize && static_cast<QResizeEvent*>(aEvent)->oldSize().width() != static_cast<QResizeEvent*>(aEvent)->size().width()) {
					QTimer::singleShot(0, View, [view = View] { view->doItemsLayout(); });
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QAbstractItemView* View;
		};

		/// [[BPR-2204]]. A list in the Prompt library's shape: ticked rows on
		/// the panel colour inside one rounded border, at a fixed width.
		template <class TView>
		[[nodiscard]] auto well_list(TView* aView, QWidget* aParent, const int aWidth) -> detail::rounded_well*
		{
			auto* const well = new detail::rounded_well{ aParent };
			auto* const fill = new QVBoxLayout{ well };
			fill->setContentsMargins(1, 3, 1, 3);
			aView->setParent(well);
			aView->setFrameShape(QFrame::NoFrame);
			aView->setAutoFillBackground(false);
			aView->viewport()->setAutoFillBackground(false);
			aView->setItemDelegate(new detail::settings_row_delegate{ aView });
			aView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			aView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
			aView->setWordWrap(true);
			aView->setTextElideMode(Qt::ElideNone);
			aView->setEditTriggers(QAbstractItemView::NoEditTriggers);
			fill->addWidget(aView);
			well->setFixedWidth(aWidth);
			// A row's height follows the width it wraps at: lay the rows out
			// again once the view has its real width.
			aView->viewport()->installEventFilter(new relayout_on_resize{ aView });
			return well;
		}

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        bold  = label->font();
			bold.setWeight(QFont::DemiBold);
			label->setFont(bold);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}

		[[nodiscard]] auto mono(QWidget* aParent, const QString& aName) -> QPlainTextEdit*
		{
			auto* const text = new QPlainTextEdit{ aParent };
			text->setReadOnly(true);
			text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
			text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
			text->setAccessibleName(aName);
			text->setTabChangesFocus(true);
			return text;
		}

		[[nodiscard]] auto agent_kind(const core::agent_protocol aProtocol, const std::optional<core::setup_agent> aPreset) -> QString
		{
			if (aPreset == core::setup_agent::claude_code) {
				return QObject::tr("Claude Code");
			}
			if (aPreset == core::setup_agent::codex) {
				return QObject::tr("Codex");
			}
			switch (aProtocol) {
			case core::agent_protocol::claude_stream_json: return QObject::tr("Custom agent that speaks Claude Code");
			case core::agent_protocol::codex_app_server:
			case core::agent_protocol::codex_jsonl_exec:   return QObject::tr("Custom agent that speaks Codex");
			case core::agent_protocol::external_terminal:  return QObject::tr("Plain-text harness");
			}
			return QObject::tr("Custom agent");
		}

		[[nodiscard]] auto field_words(const core::agent_field aField) -> QString
		{
			switch (aField) {
			case core::agent_field::name:              return QObject::tr("Name");
			case core::agent_field::protocol:          return QObject::tr("Kind of agent");
			case core::agent_field::program:           return QObject::tr("Program");
			case core::agent_field::arguments:         return QObject::tr("Arguments");
			case core::agent_field::check_arguments:   return QObject::tr("Check arguments");
			case core::agent_field::model:             return QObject::tr("Model");
			case core::agent_field::effort:            return QObject::tr("Effort");
			case core::agent_field::strict_run_checks: return QObject::tr("Strict run checks");
			case core::agent_field::scratch_starts:    return QObject::tr("Scratch folder starts");
			case core::agent_field::builds_and_tests:  return QObject::tr("Build and run tests");
			case core::agent_field::mcp_servers:       return QObject::tr("MCP servers");
			case core::agent_field::instructions:      return QObject::tr("Instructions");
			}
			return { };
		}

		[[nodiscard]] auto field_value(const core::agent_entry& aEntry, const core::agent_field aField) -> QString
		{
			const auto onOff = [](const bool aOn) { return aOn ? QObject::tr("on") : QObject::tr("off"); };
			const auto list  = [](const std::vector<std::string>& aValues) {
				auto words = QStringList{ };
				for (const auto& value : aValues) {
					words.push_back(text_of(value));
				}
				return words.isEmpty() ? QObject::tr("none") : words.join(QLatin1Char(' '));
			};
			const auto orDefault = [](const std::string& aValue) { return aValue.empty() ? QObject::tr("agent's default") : text_of(aValue); };
			switch (aField) {
			case core::agent_field::name:              return text_of(aEntry.Name);
			case core::agent_field::protocol:          return agent_kind(aEntry.Protocol, aEntry.Preset);
			case core::agent_field::program:           return aEntry.Program.empty() ? QObject::tr("looked for when it runs") : text_of(aEntry.Program);
			case core::agent_field::arguments:         return list(aEntry.Arguments);
			case core::agent_field::check_arguments:   return list(aEntry.CheckArguments);
			case core::agent_field::model:             return orDefault(aEntry.Model);
			case core::agent_field::effort:            return orDefault(aEntry.Effort);
			case core::agent_field::strict_run_checks: return onOff(aEntry.StrictRunChecks);
			case core::agent_field::scratch_starts:    return onOff(aEntry.ScratchStarts);
			case core::agent_field::builds_and_tests:  return onOff(aEntry.BuildsAndTests);
			case core::agent_field::mcp_servers:       return aEntry.McpServers == core::agent_mcp_servers::own ? QObject::tr("the agent's own configuration") : QObject::tr("off");
			case core::agent_field::instructions:      return aEntry.Instructions == core::agent_instruction_delivery::message ? QObject::tr("in the message") : QObject::tr("system prompt");
			}
			return { };
		}

		[[nodiscard]] auto counted(const std::size_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return aCount == 1U ? aOne : aMany.arg(aCount);
		}

		[[nodiscard]] auto program_required(const core::portable_agent& aAgent) -> bool
		{
			return !aAgent.Preset || *aAgent.Preset == core::setup_agent::plain_text_harness || aAgent.Protocol == core::agent_protocol::external_terminal;
		}
	} // namespace

	auto prompt_title(const std::string_view aPurpose) -> QString
	{
		if (aPurpose == application::review_skill_purpose) {
			return QObject::tr("Review skill");
		}
		const auto hook = application::prompt_hook(aPurpose);
		if (!hook) {
			return text_of(aPurpose);
		}
		switch (*hook) {
		case core::writing_hook::problem:               return QObject::tr("Problem statement");
		case core::writing_hook::solution:              return QObject::tr("Solution summary");
		case core::writing_hook::outline:               return QObject::tr("Tour outline");
		case core::writing_hook::comment:               return QObject::tr("Tour-stop comment");
		case core::writing_hook::feedback:              return QObject::tr("Review feedback");
		case core::writing_hook::enhance_stop:          return QObject::tr("Enhance a stop");
		case core::writing_hook::enhance_writing:       return QObject::tr("Enhance writing");
		case core::writing_hook::suggested_comment:     return QObject::tr("Suggested comments");
		case core::writing_hook::local_description:     return QObject::tr("Description writing");
		case core::writing_hook::local_discussion:      return QObject::tr("Change discussion");
		case core::writing_hook::local_commit_message:  return QObject::tr("Commit message");
		case core::writing_hook::review_latest_upload:  return QObject::tr("What changed");
		case core::writing_hook::review_proposed_fix:   return QObject::tr("Was this addressed");
		case core::writing_hook::review_explain_file:   return QObject::tr("Explain this file");
		case core::writing_hook::review_suggest_change: return QObject::tr("Suggest the change");
		}
		return text_of(aPurpose);
	}

	// ----- Export -----

	agent_configuration_export_dialog::agent_configuration_export_dialog(application::agent_configuration_snapshot aSnapshot, const std::array<bool, core::writing_hook_count + 1U> aUnsaved,
																		 std::string aExportedBy, core::profile_facts aProfile, QWidget* const aParent)
		: QDialog{ aParent }
		, Snapshot{ std::move(aSnapshot) }
		, Local{ application::local_prompts(Snapshot) }
		, ExportedBy{ std::move(aExportedBy) }
		, Identity{ core::portable_identity_of(aProfile) }
		, Repositories{ core::portable_repositories_of(aProfile) }
	{
		using core::profile_settings_section;
		adopt_opener_look(*this, aParent);
		setWindowTitle(tr("Export Profile Settings"));
		setObjectName(QStringLiteral("agentConfigurationExport"));
		// [[BPR-2204]]. Drawn in the import preview's language (design
		// workflow-import-v1, B): a title close over its sentence, the ticked
		// list at the left in one rounded well, the file at the right in
		// another, and a footer that says what goes in the file.
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 18, 20, 16);
		column->setSpacing(0);
		auto* const title = sheet_title(tr("Export profile settings"), this);
		title->setObjectName(QStringLiteral("agentConfigurationExportTitle"));
		column->addWidget(title);
		column->addSpacing(2);
		auto* const intro = note(tr("Tick the parts of this profile to put in the file. It never holds a sign-in, a token, where a program is on this computer, a local folder or your history."), this);
		intro->setObjectName(QStringLiteral("agentConfigurationExportIntro"));
		wrap_short(*intro, 86);
		column->addWidget(intro);
		column->addSpacing(14);
		Items = new QTreeWidget{ this };
		Items->setObjectName(QStringLiteral("agentConfigurationExportItems"));
		Items->setAccessibleName(tr("Parts of the profile to export"));
		Items->setColumnCount(2);
		Items->setHeaderLabels({ tr("Item"), tr("What is shared") });
		// One column, drawn by the row delegate: what is shared is each
		// row's second line, and a child's tick sits under its section's name.
		Items->setHeaderHidden(true);
		Items->setColumnHidden(1, true);
		Items->setRootIsDecorated(false);
		Items->setItemsExpandable(false);
		Items->setExpandsOnDoubleClick(false);
		Items->setUniformRowHeights(false);
		Items->setSelectionMode(QAbstractItemView::SingleSelection);
		Items->setIndentation(style()->pixelMetric(QStyle::PM_IndicatorWidth) + 7);
		// [[BPR-2198]]. Each section starts as the core says
		// (`core::exported_by_default`): ticked, but for Identity.
		const auto  ticked = [](const profile_settings_section aSection) { return core::exported_by_default(aSection) ? Qt::Checked : Qt::Unchecked; };
		auto* const agents = section_row(Items, tr("Agents"), tr("What each agent is set to; no program locations"), profile_settings_section::agents);
		for (const auto& entry : Snapshot.Agents.Agents) {
			auto* const row = new QTreeWidgetItem{ agents, { text_of(entry.Name), agent_kind(entry.Protocol, entry.Preset) } };
			row->setData(0, detail::row_mark_role, static_cast<int>(detail::row_mark::agent));
			row->setData(0, ItemRole, AgentKind);
			row->setData(0, IndexRole, entry.Id);
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			row->setCheckState(0, ticked(profile_settings_section::agents));
		}
		if (Snapshot.Agents.Agents.empty()) {
			agents->setFlags(Qt::ItemIsEnabled);
			agents->setText(1, tr("No agents added"));
		}
		auto* const prompts = section_row(Items, tr("Prompts"), tr("Your edited prompts; tick a built-in one to send its text"), profile_settings_section::prompts);
		for (std::size_t index = 0U; index < Local.size(); ++index) {
			const auto& prompt = Local[index];
			const auto  edited = !prompt.Override.empty();
			const auto  says   = edited ? tr("Your edit") + (prompt.WrittenAgainst ? tr(", written against %1").arg(text_of(*prompt.WrittenAgainst)) : QString{ })
										: tr("Built-in text, %1").arg(text_of(prompt.BuiltInVersion));
			auto* const row    = new QTreeWidgetItem{ prompts, { prompt_title(prompt.Purpose), says } };
			row->setData(0, ItemRole, PromptKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
			if (index < aUnsaved.size() && aUnsaved[index]) {
				// No tick at all, so ticking Prompts cannot tick it either.
				row->setFlags(Qt::ItemIsSelectable);
				row->setText(1, tr("Has edits not saved: save or discard them first"));
				row->setToolTip(0, row->text(1));
				row->setData(0, detail::row_tag_role, tr("Not saved"));
				row->setData(0, detail::row_tone_role, static_cast<int>(detail::tag_tone::caution));
			} else {
				if (edited) {
					row->setData(0, detail::row_tag_role, tr("Edited"));
					row->setData(0, detail::row_tone_role, static_cast<int>(detail::tag_tone::updated));
				}
				row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
				row->setCheckState(0, edited && ticked(profile_settings_section::prompts) == Qt::Checked ? Qt::Checked : Qt::Unchecked);
			}
		}
		// [[BPR-2032]]. A chosen workflow brings its agents.
		auto* const workflows = section_row(Items, tr("Workflows"), Snapshot.Workflows.empty() ? tr("No workflows added") : tr("Actions and guidance, with the agents they ask; no program paths"),
											profile_settings_section::workflows);
		for (std::size_t index = 0U; index < Snapshot.Workflows.size(); ++index) {
			const auto& workflow = Snapshot.Workflows[index].Workflow;
			auto* const row      = new QTreeWidgetItem{ workflows, { text_of(workflow.Name), tr("Its actions and guidance") } };
			row->setData(0, ItemRole, WorkflowKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			row->setCheckState(0, ticked(profile_settings_section::workflows));
		}
		if (Snapshot.Workflows.empty()) {
			workflows->setFlags(Qt::ItemIsEnabled);
		}
		// [[BPR-2198]] (owner, 2026-11-15). Identity: the profile's name and
		// the accounts it uses, each by host and login. Ticked as one.
		auto* const identity = section_row(Items, tr("Identity"), tr("This profile's name and the accounts it uses; never a sign-in"), profile_settings_section::identity);
		identity->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		identity->setData(0, ItemRole, IdentityKind);
		auto* const name = new QTreeWidgetItem{ identity, { text_of(Identity.ProfileName), tr("Profile name") } };
		name->setFlags(Qt::ItemIsEnabled);
		for (const auto& account : Identity.Accounts) {
			auto* const row = new QTreeWidgetItem{ identity, { text_of(account.Login), text_of(account.Host) } };
			row->setFlags(Qt::ItemIsEnabled);
		}
		identity->setCheckState(0, ticked(profile_settings_section::identity));
		// Repositories: each saved or followed one, by host and owner/name.
		auto* const repositories = section_row(Items, tr("Repositories"), Repositories.empty() ? tr("No saved or followed repositories") : tr("Saved and followed, by host and name; no local folders"),
											   profile_settings_section::repositories);
		for (std::size_t index = 0U; index < Repositories.size(); ++index) {
			const auto& repository = Repositories[index];
			const auto  kind       = repository.Saved && repository.Followed ? tr("Saved on %1 and followed").arg(text_of(repository.Host))
				: repository.Saved                                           ? tr("Saved on %1").arg(text_of(repository.Host))
																			 : tr("Followed");
			auto* const row        = new QTreeWidgetItem{ repositories, { text_of(repository.Repository), kind } };
			row->setData(0, ItemRole, RepositoryKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			row->setCheckState(0, ticked(profile_settings_section::repositories));
		}
		if (Repositories.empty()) {
			repositories->setFlags(Qt::ItemIsEnabled);
		}
		Items->expandAll();
		identity->setExpanded(true);

		auto* const body = new QHBoxLayout{ };
		body->setSpacing(16);
		auto* const left = new QVBoxLayout{ };
		left->setSpacing(8);
		auto* const list = well_list(Items, this, 300);
		list->setObjectName(QStringLiteral("agentConfigurationExportList"));
		list->setMinimumHeight(380);
		left->addWidget(list, 1);
		AllPrompts = new quiet_push_button{ tr("Choose every prompt"), this };
		AllPrompts->setObjectName(QStringLiteral("agentConfigurationAllPrompts"));
		AllPrompts->setToolTip(tr("Every prompt you can export, your edits and the built-in text of the rest, so you can improve them elsewhere and import them back."));
		left->addWidget(AllPrompts, 0, Qt::AlignLeft);
		body->addLayout(left, 0);

		// The file: its label, then its exact text, which is what is written.
		auto* const right = new QVBoxLayout{ };
		right->setSpacing(0);
		auto* const form = new QHBoxLayout{ };
		form->setSpacing(10);
		auto* const named = new QLabel{ tr("&Label"), this };
		Label             = new QLineEdit{ this };
		named->setBuddy(Label);
		Label->setObjectName(QStringLiteral("agentConfigurationLabel"));
		Label->setMaxLength(static_cast<int>(core::maximum_agent_configuration_label_bytes / 4U));
		Label->setPlaceholderText(tr("Optional, for example: Team agents"));
		form->addWidget(named);
		form->addWidget(Label, 1);
		right->addLayout(form);
		right->addSpacing(14);
		auto* const fileTitle = heading(tr("The file"), this);
		fileTitle->setObjectName(QStringLiteral("agentConfigurationExportFileTitle"));
		right->addWidget(fileTitle);
		right->addSpacing(3);
		auto* const readFirst = note(tr("Read it before you share it: arguments and prompts can hold anything you typed."), this);
		right->addWidget(readFirst);
		right->addSpacing(10);
		auto* const fileWell = new detail::rounded_well{ this };
		fileWell->setObjectName(QStringLiteral("agentConfigurationExportFile"));
		auto* const fileFill = new QVBoxLayout{ fileWell };
		fileFill->setContentsMargins(4, 4, 2, 4);
		Exact = mono(fileWell, tr("Exact file contents"));
		Exact->setObjectName(QStringLiteral("agentConfigurationExact"));
		Exact->setFrameShape(QFrame::NoFrame);
		Exact->setAutoFillBackground(false);
		Exact->viewport()->setAutoFillBackground(false);
		Exact->setFont(detail::secondary_font(Exact->font()));
		fileFill->addWidget(Exact);
		right->addWidget(fileWell, 1);
		body->addLayout(right, 1);
		column->addLayout(body, 1);

		column->addSpacing(12);
		auto* const footer = new QHBoxLayout{ };
		footer->setSpacing(12);
		Size = note({ }, this);
		Size->setObjectName(QStringLiteral("agentConfigurationExportSummary"));
		wrap_short(*Size, 58);
		footer->addWidget(Size, 1, Qt::AlignVCenter);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, this };
		ExportButton        = buttons->addButton(tr("Export…"), QDialogButtonBox::AcceptRole);
		ExportButton->setDefault(true);
		ExportButton->setObjectName(QStringLiteral("agentConfigurationExportButton"));
		footer->addWidget(buttons, 0, Qt::AlignBottom);
		column->addLayout(footer);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(ExportButton, &QPushButton::clicked, this, [this] {
			if (Bytes.empty() || !Chooser) {
				return;
			}
			Path = Chooser();
			if (!Path.isEmpty()) {
				accept();
			}
		});
		connect(Items, &QTreeWidget::itemChanged, this, [this] { refresh(); });
		connect(Label, &QLineEdit::textChanged, this, [this] { refresh(); });
		connect(AllPrompts, &QPushButton::clicked, this, [prompts] {
			for (int index = 0; index < prompts->childCount(); ++index) {
				if (prompts->child(index)->flags().testFlag(Qt::ItemIsUserCheckable)) {
					prompts->child(index)->setCheckState(0, Qt::Checked);
				}
			}
		});
		resize(980, 720);
		refresh();
	}

	void agent_configuration_export_dialog::set_file_chooser(std::function<QString()> aChooser) { Chooser = std::move(aChooser); }

	auto agent_configuration_export_dialog::agents() const -> std::vector<std::uint32_t>
	{
		auto chosen = std::vector<std::uint32_t>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == AgentKind && (*it)->checkState(0) == Qt::Checked) {
				chosen.push_back((*it)->data(0, IndexRole).toUInt());
			}
		}
		return chosen;
	}

	auto agent_configuration_export_dialog::prompts() const -> std::vector<application::exported_prompt>
	{
		auto chosen = std::vector<application::exported_prompt>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == PromptKind && (*it)->checkState(0) == Qt::Checked && (*it)->flags().testFlag(Qt::ItemIsEnabled)) {
				const auto& prompt = Local[(*it)->data(0, IndexRole).toULongLong()];
				chosen.push_back({ .Purpose = prompt.Purpose, .BuiltIn = prompt.Override.empty() });
			}
		}
		return chosen;
	}

	auto agent_configuration_export_dialog::workflows() const -> std::vector<std::string>
	{
		auto chosen = std::vector<std::string>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == WorkflowKind && (*it)->checkState(0) == Qt::Checked) {
				chosen.push_back(Snapshot.Workflows[(*it)->data(0, IndexRole).toULongLong()].Workflow.PortableId);
			}
		}
		return chosen;
	}

	auto agent_configuration_export_dialog::identity_chosen() const -> bool
	{
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == IdentityKind) {
				return (*it)->checkState(0) == Qt::Checked;
			}
		}
		return false;
	}

	auto agent_configuration_export_dialog::repositories() const -> std::vector<core::portable_repository>
	{
		auto chosen = std::vector<core::portable_repository>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == RepositoryKind && (*it)->checkState(0) == Qt::Checked) {
				chosen.push_back(Repositories[(*it)->data(0, IndexRole).toULongLong()]);
			}
		}
		return chosen;
	}

	auto agent_configuration_export_dialog::section(const core::profile_settings_section aSection) const -> QTreeWidgetItem*
	{
		for (auto index = 0; index < Items->topLevelItemCount(); ++index) {
			auto* const row = Items->topLevelItem(index);
			if (row->data(0, IndexRole).toInt() == static_cast<int>(aSection) && (row->data(0, ItemRole).toInt() == SectionKind || row->data(0, ItemRole).toInt() == IdentityKind)) {
				return row;
			}
		}
		return nullptr;
	}

	void agent_configuration_export_dialog::refresh()
	{
		const auto chosenAgents    = agents();
		const auto chosenPrompts   = prompts();
		const auto chosenWorkflows = workflows();
		const auto chosenIdentity  = identity_chosen();
		const auto chosenRepos     = repositories();
		const auto encoded         = application::export_agent_configuration(Snapshot, chosenAgents, chosenPrompts, Label->text().trimmed().toStdString(), ExportedBy, chosenWorkflows,
																			 chosenIdentity ? &Identity : nullptr, chosenRepos);
		Bytes                      = encoded.value_or(std::string{ });
		Exact->setPlainText(text_of(Bytes));
		const auto nothing = chosenAgents.empty() && chosenPrompts.empty() && chosenWorkflows.empty() && !chosenIdentity && chosenRepos.empty();
		auto       parts   = QStringList{ };
		if (!chosenAgents.empty()) {
			parts.push_back(counted(chosenAgents.size(), tr("1 agent"), tr("%1 agents")));
		}
		if (!chosenPrompts.empty()) {
			parts.push_back(counted(chosenPrompts.size(), tr("1 prompt"), tr("%1 prompts")));
		}
		if (!chosenWorkflows.empty()) {
			parts.push_back(counted(chosenWorkflows.size(), tr("1 workflow"), tr("%1 workflows")));
		}
		if (chosenIdentity) {
			parts.push_back(tr("Identity"));
		}
		if (!chosenRepos.empty()) {
			parts.push_back(counted(chosenRepos.size(), tr("1 repository"), tr("%1 repositories")));
		}
		Size->setText(!encoded      ? tr("Too large, or a chosen workflow's agent is no longer here: a Profile Settings file holds at most 1 MiB.")
						  : nothing ? tr("Tick at least one part of the profile.")
									: tr("Puts %1 in the file, %2.").arg(QLocale{ }.createSeparatedList(parts), QLocale{ }.formattedDataSize(static_cast<qint64>(Bytes.size()))));
		ExportButton->setEnabled(encoded.has_value() && !nothing);
	}

	// ----- Import -----

	namespace {
		constexpr int KindRole   = Qt::UserRole + 2;
		constexpr int HeaderRole = detail::row_heading_role;
		/// A choice's parts: the file's (Add or Update), one of yours, none.
		constexpr int TakePart  = 0;
		constexpr int UsePart   = 1;
		constexpr int LeavePart = 2;

		[[nodiscard]] auto standing_tag(const core::import_standing aStanding) -> QString
		{
			switch (aStanding) {
			case core::import_standing::added:     return QObject::tr("New");
			case core::import_standing::updated:   return QObject::tr("Updated");
			case core::import_standing::unchanged: return QObject::tr("Unchanged");
			case core::import_standing::refused:   return QObject::tr("Cannot be imported");
			}
			return { };
		}

		/// [[BPR-2204]]. New in blue, Updated in teal, the rest quiet.
		[[nodiscard]] auto standing_tone(const core::import_standing aStanding) -> detail::tag_tone
		{
			switch (aStanding) {
			case core::import_standing::added:     return detail::tag_tone::fresh;
			case core::import_standing::updated:   return detail::tag_tone::updated;
			case core::import_standing::unchanged:
			case core::import_standing::refused:   break;
			}
			return detail::tag_tone::quiet;
		}

		/// A path or file name wraps at its separators rather than widen the pane.
		[[nodiscard]] auto breakable(const QString& aText) -> QString
		{
			auto out = aText;
			out.replace(QLatin1Char('/'), QStringLiteral("/​"));
			return out;
		}

		[[nodiscard]] auto escaped(const std::string_view aText) -> QString { return text_of(aText).toHtmlEscaped(); }

		/// A section of a detail: grouped rows, as Settings draws them.
		/// [[BPR-2204]]. One filled, rounded card with tight padding, not a
		/// second border inside the detail.
		[[nodiscard]] auto group_box(QWidget* aParent) -> std::pair<QFrame*, QVBoxLayout*>
		{
			auto* const frame  = new detail::filled_group{ aParent };
			auto* const layout = new QVBoxLayout{ frame };
			layout->setContentsMargins(12, 8, 10, 9);
			layout->setSpacing(3);
			return { frame, layout };
		}

		/// [[BPR-2204]]. A count said in words up to twelve, as the design says
		/// "Four actions ask for it".
		[[nodiscard]] auto counted_words(const std::size_t aCount) -> QString
		{
			static const auto words = std::array{ QT_TR_NOOP("One"), QT_TR_NOOP("Two"), QT_TR_NOOP("Three"), QT_TR_NOOP("Four"), QT_TR_NOOP("Five"),
												  QT_TR_NOOP("Six"), QT_TR_NOOP("Seven"), QT_TR_NOOP("Eight"), QT_TR_NOOP("Nine"), QT_TR_NOOP("Ten"),
												  QT_TR_NOOP("Eleven"), QT_TR_NOOP("Twelve") };
			return aCount >= 1U && aCount <= words.size() ? QObject::tr(words[aCount - 1U]) : QString::number(aCount);
		}

		/// A part of one choice, drawn as the Prompts page draws its own (the
		/// Overview's Open and All): the chosen part a quiet chip.
		[[nodiscard]] auto segmented(const QString& aText, QWidget* const aParent) -> QToolButton*
		{
			auto* const button = new counts_mode_button{ aParent };
			button->setText(aText);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			return button;
		}

		[[nodiscard]] auto rich(const QString& aHtml, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aHtml, aParent };
			label->setTextFormat(Qt::RichText);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			// [[BPR-2204]]. Preferred, top-aligned: an ignored width left its
			// height asked at no width, and the line floated in a tall box.
			label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
			label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			detail::follow_wrapped_height(*label);
			return label;
		}
	} // namespace

	agent_configuration_import_dialog::agent_configuration_import_dialog(application::agent_configuration_preview aPreview, facts aFacts, QWidget* const aParent)
		: QDialog{ aParent }
		, Preview{ std::move(aPreview) }
		, Facts{ std::move(aFacts) }
		, Plan{ core::plan_configuration_import(Preview.File, Preview.Snapshot.Agents.Agents, Preview.Prompts, Preview.Snapshot.Workflows, Preview.Here) }
		, Agents(Preview.File.Agents.size())
		, Prompts(Preview.File.Prompts.size(), false)
		, Workflows(Preview.File.Workflows.size())
		, Repositories(Preview.File.Repositories.size(), false)
	{
		adopt_opener_look(*this, aParent);
		setWindowTitle(tr("Import Profile Settings"));
		setObjectName(QStringLiteral("agentConfigurationImport"));
		// The core's starting choices: everything the file can bring.
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			const auto& plan     = Plan.Agents[index];
			Agents[index].Action = plan.Default.Action;
			Agents[index].Target = plan.Default.Target;
			Agents[index].Ticked = plan.Default.Action != core::agent_import_action::leave ? plan.Default.Action
				: plan.StandIns.empty()                                                    ? core::agent_import_action::leave
																						   : core::agent_import_action::use_existing;
		}
		for (std::size_t index = 0U; index < Prompts.size(); ++index) {
			Prompts[index] = Plan.Prompts[index].Default;
		}
		for (std::size_t index = 0U; index < Workflows.size(); ++index) {
			Workflows[index].Action = Plan.Workflows[index].Default;
		}
		// [[BPR-2198]]. Identity starts unchosen; repositories as the core says.
		IdentityChosen = Plan.Identity && Plan.Identity->Default;
		for (std::size_t index = 0U; index < Repositories.size(); ++index) {
			Repositories[index] = Plan.Repositories[index].Default;
		}

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 18, 20, 16);
		column->setSpacing(0);
		const auto  label = Preview.File.Label.empty() ? tr("Import profile settings") : tr("Import “%1”").arg(text_of(Preview.File.Label));
		auto* const title = sheet_title(label, this);
		title->setObjectName(QStringLiteral("agentConfigurationSheetTitle"));
		column->addWidget(title);
		column->addSpacing(2);
		const auto  by    = Preview.File.ExportedBy.empty() ? tr("Exported by an unnamed application.") : tr("Exported by %1.").arg(text_of(Preview.File.ExportedBy));
		const auto  where = Facts.Profile.empty() ? tr("Importing saves what is ticked here into this profile and runs nothing.")
												  : tr("Importing saves what is ticked here into the profile “%1” and runs nothing.").arg(text_of(Facts.Profile));
		auto* const intro = note(by + QLatin1Char(' ') + where + QLatin1Char(' ') + tr("No sign-in, token, program path, local folder or history came with the file."), this);
		intro->setObjectName(QStringLiteral("agentConfigurationIntro"));
		wrap_short(*intro, 86);
		column->addWidget(intro);
		column->addSpacing(14);

		// The Prompt library's shape: the list at a fixed width, the detail beside it.
		// [[BPR-2204]]. The list is the one bordered thing beside the detail,
		// on the panel colour; the detail sits on the sheet's own ground.
		auto* const body = new QHBoxLayout{ };
		body->setSpacing(16);
		Items = new QListWidget{ this };
		Items->setObjectName(QStringLiteral("agentConfigurationImportItems"));
		Items->setAccessibleName(tr("Items in the file"));
		Items->setSelectionMode(QAbstractItemView::SingleSelection);
		auto* const list = well_list(Items, this, 290);
		list->setObjectName(QStringLiteral("agentConfigurationImportList"));
		list->setMinimumHeight(360);
		body->addWidget(list, 0);
		auto* const pane = new QFrame{ this };
		pane->setObjectName(QStringLiteral("agentConfigurationDetail"));
		pane->setFrameShape(QFrame::NoFrame);
		// The narrowest the detail goes: its widest unbreakable part, a
		// choice of three or a permission's tick, fits inside it.
		pane->setMinimumWidth(460);
		auto* const paneColumn = new QVBoxLayout{ pane };
		paneColumn->setContentsMargins(0, 0, 0, 0);
		Scroll = new QScrollArea{ pane };
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setWidgetResizable(true);
		// Every line wraps, so there is never anything to pan to.
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->viewport()->setAutoFillBackground(false);
		paneColumn->addWidget(Scroll);
		body->addWidget(pane, 1);
		column->addLayout(body, 1);

		Problem = new QLabel{ this };
		Problem->setObjectName(QStringLiteral("agentConfigurationProblem"));
		Problem->setWordWrap(true);
		Problem->setForegroundRole(QPalette::Highlight);
		Problem->hide();
		column->addWidget(Problem);
		column->addSpacing(12);
		auto* const footer = new QHBoxLayout{ };
		footer->setSpacing(12);
		Summary = note({ }, this);
		Summary->setObjectName(QStringLiteral("agentConfigurationSummary"));
		wrap_short(*Summary, 58);
		footer->addWidget(Summary, 1, Qt::AlignVCenter);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, this };
		ApplyButton         = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
		ApplyButton->setObjectName(QStringLiteral("agentConfigurationApply"));
		ApplyButton->setDefault(true);
		footer->addWidget(buttons, 0, Qt::AlignBottom);
		column->addLayout(footer);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(ApplyButton, &QPushButton::clicked, this, &QDialog::accept);

		fill_list();
		connect(Items, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || aItem->data(HeaderRole).toBool()) {
				return;
			}
			Shown = std::pair{ static_cast<item_kind>(aItem->data(KindRole).toInt()), static_cast<std::size_t>(aItem->data(IndexRole).toULongLong()) };
			// Rebuilt once the list's own signal returns.
			QTimer::singleShot(0, this, [this] { show_selected(); });
		});
		connect(Items, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || !aItem->flags().testFlag(Qt::ItemIsUserCheckable)) {
				return;
			}
			const auto kind  = static_cast<item_kind>(aItem->data(KindRole).toInt());
			const auto index = static_cast<std::size_t>(aItem->data(IndexRole).toULongLong());
			const auto on    = aItem->checkState() == Qt::Checked;
			auto       part  = on ? TakePart : LeavePart;
			auto       whom  = std::uint32_t{ 0U };
			if (on && kind == item_kind::agent && Agents[index].Ticked == core::agent_import_action::use_existing) {
				part = UsePart;
				whom = Agents[index].Target != 0U ? Agents[index].Target : Plan.Agents[index].StandIns.front();
			}
			QTimer::singleShot(0, this, [this, kind, index, part, whom] { choose(kind, index, part, whom); });
		});
		resize(980, 680);
		refresh_rows();
		for (int row = 0; row < Items->count(); ++row) {
			if (!Items->item(row)->data(HeaderRole).toBool()) {
				Items->setCurrentRow(row);
				break;
			}
		}
		show_selected();
	}

	void agent_configuration_import_dialog::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		// The detail's rich text carries colours from the palette: a theme
		// change draws it again.
		if ((aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) && Scroll != nullptr) {
			QTimer::singleShot(0, this, [this] { show_selected(); });
		}
	}

	void agent_configuration_import_dialog::set_program_chooser(std::function<QString(const QString&)> aChooser) { Chooser = std::move(aChooser); }

	void agent_configuration_import_dialog::show_problem(const QString& aText)
	{
		Problem->setText(aText);
		Problem->setVisible(!aText.isEmpty());
	}

	auto agent_configuration_import_dialog::bound(const std::size_t aItem) const -> bool
	{
		return Agents[aItem].Action != core::agent_import_action::add || !Plan.Agents[aItem].NeedsProgram || !Agents[aItem].Program.empty();
	}

	auto agent_configuration_import_dialog::agent_choices() const -> std::vector<core::agent_import_choice>
	{
		auto choices = std::vector<core::agent_import_choice>{ };
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			const auto& state = Agents[index];
			if (Preview.File.Agents[index].Problem || state.Action == core::agent_import_action::leave || !bound(index)) {
				continue;
			}
			choices.push_back({ .Item = index, .Action = state.Action, .Target = state.Target, .Program = state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds });
		}
		return choices;
	}

	auto agent_configuration_import_dialog::prompt_choices() const -> std::vector<std::size_t>
	{
		auto chosen = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Prompts.size(); ++index) {
			if (Prompts[index] && Plan.Prompts[index].Standing == core::import_standing::updated) {
				chosen.push_back(index);
			}
		}
		return chosen;
	}

	auto agent_configuration_import_dialog::workflow_choices() const -> std::vector<core::workflow_import_choice>
	{
		auto choices = std::vector<core::workflow_import_choice>{ };
		for (std::size_t index = 0U; index < Workflows.size(); ++index) {
			if (!Preview.File.Workflows[index].Problem && Workflows[index].Action != core::workflow_import_action::leave) {
				choices.push_back({ .Item = index, .Action = Workflows[index].Action, .Programs = Workflows[index].Programs, .Agents = { } });
			}
		}
		return choices;
	}

	auto agent_configuration_import_dialog::standing(const item_kind aKind, const std::size_t aItem) const -> core::import_standing
	{
		switch (aKind) {
		case item_kind::agent:      return Plan.Agents[aItem].Standing;
		case item_kind::prompt:     return Plan.Prompts[aItem].Standing;
		case item_kind::workflow:   return Plan.Workflows[aItem].Standing;
		case item_kind::identity:   return Plan.Identity ? Plan.Identity->Standing : core::import_standing::refused;
		case item_kind::repository: return Plan.Repositories[aItem].Standing;
		}
		return core::import_standing::refused;
	}

	auto agent_configuration_import_dialog::chosen(const item_kind aKind, const std::size_t aItem) const -> bool
	{
		switch (aKind) {
		case item_kind::agent:      return Agents[aItem].Action != core::agent_import_action::leave;
		case item_kind::prompt:     return Prompts[aItem];
		case item_kind::workflow:   return Workflows[aItem].Action != core::workflow_import_action::leave;
		case item_kind::identity:   return IdentityChosen;
		case item_kind::repository: return Repositories[aItem];
		}
		return false;
	}

	auto agent_configuration_import_dialog::local_name(const std::uint32_t aId) const -> QString
	{
		const auto& local = Preview.Snapshot.Agents.Agents;
		const auto  entry = std::ranges::find(local, aId, &core::agent_entry::Id);
		return entry == local.end() ? QString{ } : text_of(entry->Name);
	}

	auto agent_configuration_import_dialog::asking_actions(const std::size_t aAgent) const -> QStringList
	{
		auto tasks = QStringList{ };
		for (const auto& workflow : Preview.File.Workflows) {
			for (const auto& action : workflow.Workflow.Actions) {
				if (action.Binding == core::workflow_binding::agent && action.Agent == Preview.File.Agents[aAgent].Agent.PortableId) {
					tasks << tr("“%1”").arg(text_of(action.Task));
				}
			}
		}
		return tasks;
	}

	auto agent_configuration_import_dialog::row_line(const item_kind aKind, const std::size_t aItem) const -> QString
	{
		const auto stood = standing(aKind, aItem);
		if (stood == core::import_standing::unchanged) {
			return tr("Same as yours");
		}
		if (aKind == item_kind::identity) {
			if (!IdentityChosen) {
				return tr("Left out");
			}
			return Plan.Identity->Renames ? tr("Renames this profile “%1”").arg(text_of(Preview.File.Identity->ProfileName)) : tr("Remembers its servers");
		}
		if (aKind == item_kind::repository) {
			const auto& plan = Plan.Repositories[aItem];
			const auto& item = Preview.File.Repositories[aItem];
			if (plan.SignInToUse) {
				return Repositories[aItem] && item.Followed ? tr("Followed; sign in to save it") : tr("Sign in to use");
			}
			return !Repositories[aItem] ? tr("Left out") : item.Saved && item.Followed ? tr("Saved and followed")
				: item.Saved                                                           ? tr("Saved")
																					   : tr("Followed");
		}
		if (aKind == item_kind::agent) {
			const auto& item = Preview.File.Agents[aItem];
			if (item.Problem) {
				return item.Unsupported.empty() ? text_of(core::describe(*item.Problem)) : tr("Names “%1”").arg(text_of(item.Unsupported));
			}
			const auto& state = Agents[aItem];
			switch (state.Action) {
			case core::agent_import_action::add:          return bound(aItem) ? tr("Added as a new agent") : tr("Added once you choose its program");
			case core::agent_import_action::replace:      return tr("Updates yours");
			case core::agent_import_action::use_existing: return tr("%1 stands for it").arg(local_name(state.Target));
			case core::agent_import_action::leave:        return tr("Left out");
			}
		}
		if (aKind == item_kind::prompt) {
			if (stood == core::import_standing::refused) {
				return tr("Not known to this version");
			}
			return Prompts[aItem] ? tr("Replaces yours") : tr("Yours kept");
		}
		const auto& item = Preview.File.Workflows[aItem];
		if (item.Problem) {
			return item.Unsupported.empty() ? text_of(core::describe(*item.Problem)) : tr("Names “%1”").arg(text_of(item.Unsupported));
		}
		if (Workflows[aItem].Action == core::workflow_import_action::leave) {
			return tr("Left out");
		}
		if (const auto local = Plan.Workflows[aItem].Local) {
			return tr("%1 replaces yours, %2")
				.arg(text_of(core::workflow_revision(item.Workflow).substr(0U, 6U)), text_of(core::workflow_revision(Preview.Snapshot.Workflows[*local].Workflow).substr(0U, 6U)));
		}
		return tr("Added as a new workflow");
	}

	void agent_configuration_import_dialog::fill_list()
	{
		Filling = true;
		Items->clear();
		const auto header = [this](const QString& aText) {
			// Body text, semibold, with the same room above every section
			// (the row delegate draws it).
			auto* const item = new QListWidgetItem{ aText, Items };
			item->setData(HeaderRole, true);
			item->setFlags(Qt::NoItemFlags);
		};
		const auto add = [this](const item_kind aKind, const std::size_t aIndex, const QString& aName) {
			auto* const item = new QListWidgetItem{ aName, Items };
			item->setData(KindRole, static_cast<int>(aKind));
			item->setData(IndexRole, static_cast<qulonglong>(aIndex));
			item->setData(ItemRole, aName);
		};
		// Each section as Export lists it; what cannot be imported last.
		const auto section = [&](const QString& aTitle, const item_kind aKind, const std::size_t aCount, const auto& aName) {
			auto titled = false;
			for (std::size_t index = 0U; index < aCount; ++index) {
				if (standing(aKind, index) == core::import_standing::refused) {
					continue;
				}
				if (!titled) {
					header(aTitle);
					titled = true;
				}
				add(aKind, index, aName(index));
			}
		};
		const auto agentName  = [this](const std::size_t aIndex) { return text_of(Preview.File.Agents[aIndex].Agent.Name); };
		const auto promptName = [this](const std::size_t aIndex) {
			const auto& item = Preview.File.Prompts[aIndex];
			return item.Local && *item.Local < Preview.Prompts.size() ? prompt_title(Preview.Prompts[*item.Local].Purpose) : text_of(item.Prompt.Purpose);
		};
		const auto workflowName = [this](const std::size_t aIndex) { return text_of(Preview.File.Workflows[aIndex].Workflow.Name); };
		section(tr("Agents"), item_kind::agent, Agents.size(), agentName);
		section(tr("Workflows"), item_kind::workflow, Workflows.size(), workflowName);
		section(tr("Prompts"), item_kind::prompt, Prompts.size(), promptName);
		// [[BPR-2198]]. Identity and Repositories, as Export lists them.
		section(tr("Identity"), item_kind::identity, Plan.Identity ? 1U : 0U, [this](std::size_t) { return text_of(Preview.File.Identity->ProfileName); });
		section(tr("Repositories"), item_kind::repository, Repositories.size(), [this](const std::size_t aIndex) { return text_of(Preview.File.Repositories[aIndex].Repository); });
		auto       titled  = false;
		const auto refused = [&](const item_kind aKind, const std::size_t aCount, const auto& aName) {
			for (std::size_t index = 0U; index < aCount; ++index) {
				if (standing(aKind, index) != core::import_standing::refused) {
					continue;
				}
				if (!titled) {
					header(tr("Cannot be imported"));
					titled = true;
				}
				add(aKind, index, aName(index));
			}
		};
		refused(item_kind::agent, Agents.size(), agentName);
		refused(item_kind::workflow, Workflows.size(), workflowName);
		refused(item_kind::prompt, Prompts.size(), promptName);
		Filling = false;
	}

	void agent_configuration_import_dialog::refresh_rows()
	{
		Filling = true;
		for (int row = 0; row < Items->count(); ++row) {
			auto* const item = Items->item(row);
			if (item->data(HeaderRole).toBool()) {
				continue;
			}
			const auto kind  = static_cast<item_kind>(item->data(KindRole).toInt());
			const auto index = static_cast<std::size_t>(item->data(IndexRole).toULongLong());
			const auto stood = standing(kind, index);
			// The library's row: the name, its tag, and one line of what happens.
			// The library's row: the name, its tag, and one line of what
			// happens; under Cannot be imported the section says the tag.
			const auto name = item->data(ItemRole).toString();
			item->setText((stood == core::import_standing::refused ? name : name + QStringLiteral("   ") + standing_tag(stood)) + QLatin1Char('\n') + row_line(kind, index));
			// [[BPR-2204]]. Drawn as the design draws it: the name, its tag as
			// a tinted pill, the line in the secondary colour, the agent's
			// sparkle and a refused row's mark where its tick would be.
			item->setData(detail::row_name_role, name);
			item->setData(detail::row_second_role, row_line(kind, index));
			item->setData(detail::row_tag_role, stood == core::import_standing::refused ? QString{ } : standing_tag(stood));
			item->setData(detail::row_tone_role, static_cast<int>(standing_tone(stood)));
			item->setData(detail::row_mark_role, static_cast<int>(stood == core::import_standing::refused ? detail::row_mark::refused : kind == item_kind::agent ? detail::row_mark::agent
																																								 : detail::row_mark::none));
			auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			// Something to choose is ticked; an agent of another kind under
			// its name, with none of yours to stand for it, has nothing.
			const auto choosable = (stood == core::import_standing::added || stood == core::import_standing::updated)
				&& (kind != item_kind::agent || Plan.Agents[index].Default.Action != core::agent_import_action::leave || !Plan.Agents[index].StandIns.empty())
				&& (kind != item_kind::repository || !Plan.Repositories[index].SignInToUse || Preview.File.Repositories[index].Followed);
			if (choosable) {
				flags |= Qt::ItemIsUserCheckable;
				item->setFlags(flags);
				item->setCheckState(chosen(kind, index) ? Qt::Checked : Qt::Unchecked);
			} else {
				item->setFlags(flags);
				item->setData(Qt::CheckStateRole, QVariant{ });
			}
			item->setData(Qt::AccessibleDescriptionRole, standing_tag(stood) + QStringLiteral(", ") + row_line(kind, index));
			item->setToolTip(item->text());
		}
		Filling = false;

		// The footer: what Import does, in a sentence.
		auto agentsAdded = std::size_t{ 0U }, agentsUpdated = std::size_t{ 0U }, agentsUsed = std::size_t{ 0U }, waiting = std::size_t{ 0U }, refused = std::size_t{ 0U };
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			refused += Plan.Agents[index].Standing == core::import_standing::refused ? 1U : 0U;
			waiting += Plan.Agents[index].Standing != core::import_standing::refused && !bound(index) ? 1U : 0U;
		}
		for (const auto& choice : agent_choices()) {
			agentsAdded += choice.Action == core::agent_import_action::add ? 1U : 0U;
			agentsUpdated += choice.Action == core::agent_import_action::replace ? 1U : 0U;
			agentsUsed += choice.Action == core::agent_import_action::use_existing ? 1U : 0U;
		}
		auto workflowsAdded = std::size_t{ 0U }, workflowsUpdated = std::size_t{ 0U };
		for (const auto& choice : workflow_choices()) {
			(choice.Action == core::workflow_import_action::add ? workflowsAdded : workflowsUpdated) += 1U;
		}
		for (const auto& plan : Plan.Prompts) {
			refused += plan.Standing == core::import_standing::refused ? 1U : 0U;
		}
		for (const auto& plan : Plan.Workflows) {
			refused += plan.Standing == core::import_standing::refused ? 1U : 0U;
		}
		const auto prompts = prompt_choices().size();
		auto       adds    = QStringList{ };
		auto       updates = QStringList{ };
		if (agentsAdded > 0U) {
			adds << counted(agentsAdded, tr("1 agent"), tr("%1 agents"));
		}
		if (workflowsAdded > 0U) {
			adds << counted(workflowsAdded, tr("1 workflow"), tr("%1 workflows"));
		}
		if (agentsUpdated > 0U) {
			updates << counted(agentsUpdated, tr("1 agent"), tr("%1 agents"));
		}
		if (workflowsUpdated > 0U) {
			updates << counted(workflowsUpdated, tr("1 workflow"), tr("%1 workflows"));
		}
		if (prompts > 0U) {
			updates << counted(prompts, tr("1 prompt"), tr("%1 prompts"));
		}
		const auto repositories = repository_choices().size();
		if (repositories > 0U) {
			adds << counted(repositories, tr("1 repository"), tr("%1 repositories"));
		}
		if (IdentityChosen) {
			updates << tr("this profile's identity");
		}
		auto parts = QStringList{ };
		if (!adds.isEmpty()) {
			parts << tr("adds %1").arg(adds.join(tr(" and ")));
		}
		if (!updates.isEmpty()) {
			parts << tr("updates %1").arg(updates.join(tr(" and ")));
		}
		if (agentsUsed > 0U) {
			parts << counted(agentsUsed, tr("lets 1 of your agents stand in"), tr("lets %1 of your agents stand in"));
		}
		// [[BPR-2204]]. As the design says it: "Adds 1 agent and updates 1 workflow."
		auto words = QString{ };
		if (parts.isEmpty()) {
			words = tr("Nothing is ticked to import.");
		} else {
			auto       first = parts;
			const auto last  = first.takeLast();
			words            = first.isEmpty() ? last : tr("%1 and %2").arg(first.join(tr(", ")), last);
			words[0]         = words[0].toUpper();
			words += QLatin1Char('.');
		}
		if (waiting > 0U) {
			words += QLatin1Char(' ') + counted(waiting, tr("1 agent waits for its program."), tr("%1 agents wait for their programs."));
		}
		if (refused > 0U) {
			words += QLatin1Char(' ') + counted(refused, tr("1 item cannot be imported."), tr("%1 items cannot be imported."));
		}
		if (!parts.isEmpty()) {
			words += QLatin1Char(' ') + tr("Nothing runs until you start an action in a draft.");
		}
		Summary->setText(words);
		ApplyButton->setEnabled(!agent_choices().empty() || prompts > 0U || !workflow_choices().empty() || IdentityChosen || repositories > 0U);
	}

	void agent_configuration_import_dialog::select(const item_kind aKind, const std::size_t aItem)
	{
		for (int row = 0; row < Items->count(); ++row) {
			auto* const item = Items->item(row);
			if (!item->data(HeaderRole).toBool() && item->data(KindRole).toInt() == static_cast<int>(aKind) && item->data(IndexRole).toULongLong() == aItem) {
				Items->setCurrentItem(item);
			}
		}
		Shown = std::pair{ aKind, aItem };
		show_selected();
	}

	void agent_configuration_import_dialog::select_agent(const std::size_t aItem) { select(item_kind::agent, aItem); }
	void agent_configuration_import_dialog::select_prompt(const std::size_t aItem) { select(item_kind::prompt, aItem); }
	void agent_configuration_import_dialog::select_workflow(const std::size_t aItem) { select(item_kind::workflow, aItem); }
	void agent_configuration_import_dialog::select_identity() { select(item_kind::identity, 0U); }
	void agent_configuration_import_dialog::select_repository(const std::size_t aItem) { select(item_kind::repository, aItem); }

	auto agent_configuration_import_dialog::repository_choices() const -> std::vector<std::size_t>
	{
		auto chosen = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Repositories.size(); ++index) {
			if (Repositories[index] && Plan.Repositories[index].Standing == core::import_standing::added) {
				chosen.push_back(index);
			}
		}
		return chosen;
	}

	auto agent_configuration_import_dialog::identity_choice() const -> bool { return IdentityChosen && Plan.Identity && Plan.Identity->Standing == core::import_standing::updated; }

	auto agent_configuration_import_dialog::choice_buttons() const -> QList<QToolButton*>
	{
		auto buttons = QList<QToolButton*>{ };
		if (Choice != nullptr) {
			for (auto* const button : Choice->buttons()) {
				buttons << static_cast<QToolButton*>(button);
			}
		}
		return buttons;
	}

	auto agent_configuration_import_dialog::workflow_program_buttons() const -> QList<QPushButton*>
	{
		return Detail == nullptr ? QList<QPushButton*>{ } : Detail->findChildren<QPushButton*>(QStringLiteral("agentConfigurationWorkflowProgram"));
	}

	void agent_configuration_import_dialog::choose(const item_kind aKind, const std::size_t aItem, const int aPart, const std::uint32_t aTarget)
	{
		switch (aKind) {
		case item_kind::agent: {
			auto&       state = Agents[aItem];
			const auto& plan  = Plan.Agents[aItem];
			if (aPart == TakePart) {
				state.Action = plan.Meets && plan.Standing == core::import_standing::updated ? core::agent_import_action::replace : core::agent_import_action::add;
				state.Target = state.Action == core::agent_import_action::replace ? *plan.Meets : 0U;
			} else if (aPart == UsePart && !plan.StandIns.empty()) {
				state.Action = core::agent_import_action::use_existing;
				state.Target = std::ranges::contains(plan.StandIns, aTarget) ? aTarget : plan.StandIns.front();
			} else {
				state.Action = core::agent_import_action::leave;
			}
			if (state.Action != core::agent_import_action::leave) {
				state.Ticked = state.Action;
			}
			break;
		}
		case item_kind::prompt:     Prompts[aItem] = aPart == TakePart; break;
		case item_kind::identity:   IdentityChosen = aPart == TakePart; break;
		case item_kind::repository: Repositories[aItem] = aPart == TakePart; break;
		case item_kind::workflow:
			Workflows[aItem].Action = aPart != TakePart ? core::workflow_import_action::leave
				: Plan.Workflows[aItem].Local           ? core::workflow_import_action::replace
														: core::workflow_import_action::add;
			break;
		}
		refresh_rows();
		if (Shown && Shown->first == aKind && Shown->second == aItem) {
			show_selected();
		}
	}

	void agent_configuration_import_dialog::show_selected()
	{
		Choice        = nullptr;
		ProgramButton = nullptr;
		ProgramLine   = nullptr;
		Relaxed       = nullptr;
		Builds        = nullptr;
		Outcome       = nullptr;
		Settings      = nullptr;
		Comparison    = nullptr;
		Yours         = nullptr;
		Theirs        = nullptr;
		// [[BPR-2204]]. Built again for the same item (a choice, a program),
		// it keeps its place; another item opens at its top.
		const auto same     = Built == Shown;
		const auto scrolled = same ? Scroll->verticalScrollBar()->value() : 0;
		Built               = Shown;
		if (auto* const old = Scroll->takeWidget(); old != nullptr) {
			old->hide();
			old->deleteLater();
		}
		// Parented from the start, so what it builds reads the dialog's palette.
		Detail = new QWidget{ Scroll->viewport() };
		Detail->setObjectName(QStringLiteral("agentConfigurationDetailPage"));
		Detail->setAutoFillBackground(false);
		auto* const column = new QVBoxLayout{ Detail };
		// [[BPR-2204]]. On the sheet's own ground, no frame: its edge is the list's gap.
		column->setContentsMargins(0, 0, 6, 4);
		column->setSpacing(10);
		if (!Shown) {
			column->addWidget(note(tr("Choose an item to see what importing it changes."), Detail));
		} else if (Shown->first == item_kind::agent) {
			show_agent(Shown->second, *Detail);
		} else if (Shown->first == item_kind::prompt) {
			show_prompt(Shown->second, *Detail);
		} else if (Shown->first == item_kind::identity) {
			show_identity(*Detail);
		} else if (Shown->first == item_kind::repository) {
			show_repository(Shown->second, *Detail);
		} else {
			show_workflow(Shown->second, *Detail);
		}
		column->addStretch(1);
		Scroll->setWidget(Detail);
		Scroll->verticalScrollBar()->setValue(scrolled);
		if (same && scrolled > 0) {
			// The new page's height is known once it is laid out.
			QTimer::singleShot(0, this, [this, scrolled, shown = Shown] {
				if (Built == shown) {
					Scroll->verticalScrollBar()->setValue(scrolled);
				}
			});
		}
	}

	namespace {
		/// The detail's title: the agent's sparkle, the name, larger and
		/// semibold, and its tag as a tinted pill ([[BPR-2204]]); its sentence
		/// goes close under it in the returned column.
		auto title_row(QVBoxLayout& aColumn, const QString& aName, const QString& aTag, const detail::tag_tone aTone, QWidget& aPage, const bool aAgent = false) -> QVBoxLayout*
		{
			auto* const block = new QVBoxLayout{ };
			block->setSpacing(3);
			auto* const row = new QHBoxLayout{ };
			row->setSpacing(7);
			if (aAgent) {
				row->addWidget(new detail::sparkle_mark{ &aPage }, 0, Qt::AlignVCenter);
			}
			auto* const title = new detail::line_label{ aName, &aPage };
			title->setObjectName(QStringLiteral("agentConfigurationTitle"));
			title->setTextInteractionFlags(Qt::TextSelectableByMouse);
			auto font = title->font();
			font.setWeight(QFont::DemiBold);
			font.setPointSizeF(font.pointSizeF() * 1.08);
			title->setFont(font);
			row->addWidget(title, 0, Qt::AlignVCenter);
			if (!aTag.isEmpty()) {
				auto* const tag = new detail::tag_label{ aTag, aTone, &aPage };
				tag->setObjectName(QStringLiteral("agentConfigurationTitleTag"));
				row->addWidget(tag, 0, Qt::AlignVCenter);
			}
			row->addStretch(1);
			block->addLayout(row);
			aColumn.addLayout(block);
			return block;
		}
	} // namespace

	void agent_configuration_import_dialog::show_refused(const QString& aName, const QString& aWhy, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		auto* const block  = title_row(column, aName, standing_tag(core::import_standing::refused), detail::tag_tone::quiet, aPage);
		Outcome            = note(aWhy + QLatin1Char(' ') + tr("It is not saved. The rest of the file can still be imported."), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationItemProblem"));
		block->addWidget(Outcome);
	}

	void agent_configuration_import_dialog::show_agent(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item   = Preview.File.Agents[aItem];
		const auto& agent  = item.Agent;
		const auto& plan   = Plan.Agents[aItem];
		if (item.Problem) {
			auto why = text_of(core::describe(*item.Problem));
			if (item.Refusal) {
				why += QLatin1Char(' ') + text_of(core::describe(*item.Refusal));
			}
			if (!item.Unsupported.empty()) {
				why += QLatin1Char(' ') + tr("It names “%1”.").arg(text_of(item.Unsupported));
			}
			show_refused(text_of(agent.Name), why, aPage);
			return;
		}
		auto&       state = Agents[aItem];
		const auto& local = Preview.Snapshot.Agents.Agents;
		auto* const block = title_row(column, text_of(agent.Name), standing_tag(plan.Standing), standing_tone(plan.Standing), aPage, true);
		const auto  asks  = asking_actions(aItem);
		auto        says  = agent_kind(agent.Protocol, agent.Preset) + QStringLiteral(".");
		if (!asks.isEmpty()) {
			says += QLatin1Char(' ') + counted(static_cast<std::size_t>(asks.size()), tr("The workflow's action %1 asks for this agent, read-only.").arg(asks.join(tr(", "))), tr("The workflow's actions %1 ask for this agent, read-only.").arg(asks.join(tr(", "))));
		}
		block->addWidget(note(says, &aPage));
		const auto* const meets = plan.Meets ? &*std::ranges::find(local, *plan.Meets, &core::agent_entry::Id) : nullptr;
		if (plan.Standing == core::import_standing::unchanged) {
			Outcome = note(tr("“%1” already has these settings. Nothing to import.").arg(local_name(*plan.Meets)), &aPage);
			block->addWidget(Outcome);
			return;
		}

		// One control, every part showing: the file's, one of yours, none.
		auto* const parts = new QHBoxLayout{ };
		parts->setSpacing(0);
		Choice = new QButtonGroup{ &aPage };
		Choice->setExclusive(true);
		const auto updating = plan.Standing == core::import_standing::updated;
		if (!updating || (meets != nullptr && core::compatible_agent(*meets, agent))) {
			auto* const take = segmented(updating ? tr("Update") : tr("Add"), &aPage);
			take->setToolTip(updating ? tr("“%1” takes the file's settings.").arg(local_name(*plan.Meets)) : tr("Added as a new agent."));
			Choice->addButton(take, TakePart);
			parts->addWidget(take);
		}
		if (!plan.StandIns.empty()) {
			const auto shownTarget = state.Action == core::agent_import_action::use_existing ? state.Target : plan.StandIns.front();
			auto*      use         = segmented(tr("Use %1").arg(local_name(shownTarget)), &aPage);
			use->setToolTip(tr("One of your agents stands for it as it is, and is not changed."));
			if (plan.StandIns.size() > 1U) {
				// More than one could stand for it: the part names one and opens a menu of them.
				auto* const menu = new QMenu{ use };
				for (const auto id : plan.StandIns) {
					menu->addAction(local_name(id), this, [this, aItem, id] { QTimer::singleShot(0, this, [this, aItem, id] { choose(item_kind::agent, aItem, UsePart, id); }); });
				}
				use->setMenu(menu);
				use->setPopupMode(QToolButton::InstantPopup);
			}
			Choice->addButton(use, UsePart);
			parts->addWidget(use);
		}
		auto* const leave = segmented(tr("Leave out"), &aPage);
		Choice->addButton(leave, LeavePart);
		parts->addWidget(leave);
		parts->addStretch(1);
		const auto part = state.Action == core::agent_import_action::leave ? LeavePart
			: state.Action == core::agent_import_action::use_existing      ? UsePart
																		   : TakePart;
		if (auto* const on = Choice->button(part); on != nullptr) {
			on->setChecked(true);
		}
		connect(Choice, &QButtonGroup::idClicked, this, [this, aItem](const int aPart) {
			if (aPart == UsePart && Plan.Agents[aItem].StandIns.size() > 1U) {
				return;
			}
			QTimer::singleShot(0, this, [this, aItem, aPart] { choose(item_kind::agent, aItem, aPart, Agents[aItem].Target); });
		});
		column.addLayout(parts);

		const auto adding    = state.Action == core::agent_import_action::add;
		const auto replacing = state.Action == core::agent_import_action::replace && meets != nullptr;
		const auto choice    = core::agent_import_choice{ .Item = aItem, .Action = state.Action, .Target = state.Target, .Program = state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds };
		auto       outcome   = QString{ };
		if (adding) {
			outcome = tr("Added at the end of your agents, not yet checked on this computer.");
		} else if (replacing) {
			const auto after = core::imported_entry(agent, choice, meets);
			outcome          = core::keeps_agent_identity(*meets, after) ? tr("“%1” takes the file's settings. Its conversations stay and can be continued.").arg(local_name(meets->Id))
																		 : tr("“%1” takes the file's settings. Its conversations stay readable but can no longer be continued, because what it runs changes.").arg(local_name(meets->Id));
		} else if (state.Action == core::agent_import_action::use_existing) {
			outcome = tr("“%1” stands for it in the workflow and is not changed.").arg(local_name(state.Target));
		} else {
			outcome = asks.isEmpty() ? tr("Not imported.") : tr("Not imported. The actions that ask for it wait until an agent stands for it.");
		}
		Outcome = body(outcome, &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationChanges"));
		column.addWidget(Outcome);
		if (!adding && !replacing) {
			return;
		}

		// What it is set to; an update shows yours beside the file's.
		const auto after = core::imported_entry(agent, choice, replacing ? meets : nullptr);
		column.addSpacing(4);
		auto* const fieldsHeading = heading(replacing ? tr("What changes") : tr("What it is set to"), &aPage);
		fieldsHeading->setForegroundRole(QPalette::PlaceholderText);
		fieldsHeading->setFont(detail::secondary_font(fieldsHeading->font()));
		column.addWidget(fieldsHeading);
		auto [box, rows] = group_box(&aPage);
		box->setObjectName(QStringLiteral("agentConfigurationSettings"));
		Settings         = box;
		auto* const grid = new QGridLayout{ };
		grid->setHorizontalSpacing(14);
		grid->setVerticalSpacing(6);
		grid->setColumnStretch(1, 1);
		auto line = 0;
		if (replacing) {
			grid->setColumnStretch(2, 1);
			grid->addWidget(note(tr("Yours"), box), line, 1);
			grid->addWidget(note(tr("In the file"), box), line, 2);
			++line;
			const auto fields = core::agent_differences(*meets, after);
			if (fields.empty()) {
				grid->addWidget(note(tr("Nothing changes."), box), line++, 0, 1, 3);
			}
			for (const auto field : fields) {
				grid->addWidget(new QLabel{ field_words(field), box }, line, 0, Qt::AlignTop);
				auto* const before = rich(breakable(field_value(*meets, field)).toHtmlEscaped(), box);
				auto* const now    = rich(QStringLiteral("<b>%1</b>").arg(breakable(field_value(after, field)).toHtmlEscaped()), box);
				grid->addWidget(before, line, 1);
				grid->addWidget(now, line, 2);
				++line;
			}
		} else {
			for (const auto field : { core::agent_field::protocol, core::agent_field::model, core::agent_field::effort, core::agent_field::strict_run_checks, core::agent_field::scratch_starts,
									  core::agent_field::mcp_servers, core::agent_field::instructions }) {
				grid->addWidget(new QLabel{ field_words(field), box }, line, 0, Qt::AlignTop);
				auto* const value = rich(field_value(after, field).toHtmlEscaped(), box);
				value->setForegroundRole(QPalette::PlaceholderText);
				grid->addWidget(value, line, 1);
				++line;
			}
		}
		rows->addLayout(grid);
		column.addWidget(box);

		// The program this computer runs it with: never a path from the file.
		auto [programBox, programRows] = group_box(&aPage);
		auto* const programRow         = new QHBoxLayout{ };
		ProgramLine                    = body({ }, programBox);
		ProgramLine->setObjectName(QStringLiteral("agentConfigurationProgram"));
		programRow->addWidget(ProgramLine, 1);
		if (plan.NeedsProgram || (replacing && program_required(agent))) {
			ProgramButton = new quiet_push_button{ tr("Choose program…"), programBox };
			ProgramButton->setObjectName(QStringLiteral("agentConfigurationChooseProgram"));
			programRow->addWidget(ProgramButton, 0, Qt::AlignTop);
			connect(ProgramButton, &QPushButton::clicked, this, [this, aItem] {
				if (!Chooser) {
					return;
				}
				const auto picked = Chooser(text_of(Preview.File.Agents[aItem].Agent.ProgramName));
				if (!picked.isEmpty()) {
					Agents[aItem].Program = picked.toStdString();
					QTimer::singleShot(0, this, [this] {
						refresh_rows();
						show_selected();
					});
				}
			});
		}
		if (!program_required(agent)) {
			const auto found = std::ranges::contains(Facts.Found, *agent.Preset);
			ProgramLine->setText(found ? tr("Program: %1, found on this computer; looked for each time it runs.").arg(agent_kind(agent.Protocol, agent.Preset))
									   : tr("Program: not found on this computer yet; looked for each time it runs."));
		} else if (!state.Program.empty()) {
			ProgramLine->setText(tr("Program: %1").arg(breakable(text_of(state.Program))));
		} else if (replacing) {
			ProgramLine->setText(tr("Program: keeps %1").arg(meets->Program.empty() ? tr("its own") : breakable(text_of(meets->Program))));
		} else {
			ProgramLine->setText(agent.ProgramName.empty() ? tr("Program: not chosen yet. The file carries no path, so you say which program runs it on this computer. It is left out until you do.")
														   : tr("Program: %1 is not chosen yet. The file names it and carries no path, so you say where it is on this computer. It is left out until you do.")
																 .arg(breakable(text_of(agent.ProgramName))));
		}
		programRows->addLayout(programRow);
		column.addWidget(programBox);

		// What the file asks for that only you can allow: unticked until you do.
		const auto permission = [&column, &aPage](const QString& aName, const QString& aWhy, const bool aOn, const QString& aObject) {
			auto [frame, lines] = group_box(&aPage);
			auto* const check   = new QCheckBox{ aName, frame };
			check->setObjectName(aObject);
			check->setChecked(aOn);
			lines->addWidget(check);
			lines->addWidget(note(aWhy, frame));
			column.addWidget(frame);
			return check;
		};
		if (!agent.StrictRunChecks && !agent.Preset) {
			Relaxed = permission(tr("Let %1 relax its run checks").arg(text_of(agent.Name)), tr("The file asks for this. It stays strict unless you tick; you can change it later in the agent's settings."),
								 state.AcceptRelaxed, QStringLiteral("agentConfigurationRelaxed"));
			connect(Relaxed, &QCheckBox::toggled, this, [this, aItem](const bool aOn) {
				Agents[aItem].AcceptRelaxed = aOn;
				QTimer::singleShot(0, this, [this] { show_selected(); });
			});
		}
		if (agent.BuildsAndTests) {
			Builds = permission(tr("Let %1 build and run tests").arg(text_of(agent.Name)), tr("The file asks for this. It stays off unless you tick; you can turn it on later in the agent's settings."),
								state.AcceptBuilds, QStringLiteral("agentConfigurationBuilds"));
			connect(Builds, &QCheckBox::toggled, this, [this, aItem](const bool aOn) {
				Agents[aItem].AcceptBuilds = aOn;
				QTimer::singleShot(0, this, [this] { show_selected(); });
			});
		}
	}

	void agent_configuration_import_dialog::show_workflow(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column   = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item     = Preview.File.Workflows[aItem];
		const auto& workflow = item.Workflow;
		const auto& plan     = Plan.Workflows[aItem];
		if (item.Problem) {
			auto why = text_of(core::describe(*item.Problem));
			if (!item.Unsupported.empty()) {
				why += QLatin1Char(' ') + tr("It names “%1”, which this version does not know.").arg(text_of(item.Unsupported));
			}
			if (item.Action && *item.Action < workflow.Actions.size()) {
				why += QLatin1Char(' ') + tr("In the action “%1”.").arg(text_of(workflow.Actions[*item.Action].Task));
			}
			show_refused(text_of(workflow.Name), why, aPage);
			return;
		}
		auto&       state   = Workflows[aItem];
		const auto* yours   = plan.Local ? &Preview.Snapshot.Workflows[*plan.Local] : nullptr;
		const auto  theirs  = text_of(core::workflow_revision(workflow).substr(0U, 6U));
		const auto  mineRev = yours != nullptr ? text_of(core::workflow_revision(yours->Workflow).substr(0U, 6U)) : QString{ };
		// [[BPR-2204]]. The tick in the list is the workflow's choice, as the
		// design draws it: the detail has no Update / Leave out of its own.
		auto* const block = title_row(column, text_of(workflow.Name), standing_tag(plan.Standing), standing_tone(plan.Standing), aPage);
		const auto  bold  = [this](const QString& aText) { return QStringLiteral("<span style=\"color:%1;font-weight:600\">%2</span>").arg(palette().color(QPalette::WindowText).name(), aText.toHtmlEscaped()); };
		auto        says  = tr("Says it comes from %1.").arg(workflow.Origin.empty() ? tr("no one in particular") : text_of(workflow.Origin)).toHtmlEscaped();
		if (plan.Standing == core::import_standing::updated) {
			says += QLatin1Char(' ') + (state.Action == core::workflow_import_action::leave ? tr("Left out: yours stays revision %1.").arg(bold(mineRev)) : tr("Ticked, revision %1 replaces yours, %2.").arg(bold(theirs), bold(mineRev)));
		} else if (plan.Standing == core::import_standing::unchanged) {
			says += QLatin1Char(' ') + tr("Yours is the same revision, %1. Nothing to import.").arg(bold(theirs));
		} else if (state.Action == core::workflow_import_action::leave) {
			says += QLatin1Char(' ') + tr("Left out: tick it in the list to add it.");
		}
		says += QLatin1Char(' ') + tr("No action runs on import; each is yours to start, in a draft.").toHtmlEscaped();
		auto* const sentence = rich(says, &aPage);
		sentence->setObjectName(QStringLiteral("agentConfigurationWorkflowSays"));
		sentence->setForegroundRole(QPalette::PlaceholderText);
		sentence->setFont(detail::secondary_font(sentence->font()));
		block->addWidget(sentence);
		column.addSpacing(4);

		// Who runs each action, and with what; what it says it does.
		const auto agentName = [this](const std::string& aPortable) {
			const auto found = std::ranges::find(Preview.File.Agents, aPortable, [](const core::decoded_agent& aAgent) { return aAgent.Agent.PortableId; });
			return found == Preview.File.Agents.end() ? text_of(aPortable) : text_of(found->Agent.Name);
		};
		auto* const table = new detail::comparison_table{ &aPage };
		table->setObjectName(QStringLiteral("agentConfigurationWorkflowActions"));
		Comparison       = table;
		auto* const grid = table->grid();
		// One action's cell: its task and tag on the first line, who runs it,
		// and what it says it does, each line the same on both sides so the
		// two sides of a row stay in line.
		const auto cell = [&](const core::workflow_action& aAction, const core::workflow_action* const aBefore, const QString& aTag) {
			auto* const box = new QWidget{ table };
			box->setObjectName(QStringLiteral("agentConfigurationAction-%1").arg(text_of(aAction.Id)));
			auto* const lines = new QVBoxLayout{ box };
			lines->setContentsMargins(10, 6, 10, 7);
			lines->setSpacing(1);
			auto* const head = new QHBoxLayout{ };
			head->setSpacing(6);
			// Required follows the task; New action keeps the cell's top right.
			auto* const task = new detail::line_label{ text_of(aAction.Task), box };
			task->setObjectName(QStringLiteral("agentConfigurationActionTask"));
			head->addWidget(task, 0, Qt::AlignTop);
			if (aAction.Required) {
				head->addWidget(new detail::tag_label{ tr("Required"), detail::tag_tone::quiet, box }, 0, Qt::AlignTop);
			}
			head->addStretch(1);
			if (!aTag.isEmpty()) {
				head->addWidget(new detail::tag_label{ aTag, detail::tag_tone::updated, box }, 0, Qt::AlignTop);
			}
			lines->addLayout(head);
			auto* const who = new QHBoxLayout{ };
			who->setSpacing(4);
			auto* const runner = rich(QString{ }, box);
			runner->setObjectName(QStringLiteral("agentConfigurationActionWho"));
			runner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
			runner->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			runner->setForegroundRole(QPalette::PlaceholderText);
			runner->setFont(detail::secondary_font(runner->font()));
			if (aAction.Binding == core::workflow_binding::agent) {
				who->addWidget(new detail::sparkle_mark{ box }, 0, Qt::AlignTop);
				runner->setText(QStringLiteral("<span style=\"color:%1\">%2</span>, %3").arg(agent_accent(palette()).name(), agentName(aAction.Agent).toHtmlEscaped(), tr("read-only")));
			} else {
				runner->setText(tr("You run %1").arg(QStringLiteral("<code style=\"color:%1\">%2</code>").arg(palette().color(QPalette::WindowText).name(), breakable(text_of(aAction.ProgramName)).toHtmlEscaped())));
			}
			who->addWidget(runner, 1);
			lines->addLayout(who);
			// An effect the earlier revision did not have is drawn in the
			// caution colour; its words say it to a screen reader too.
			const auto ground  = palette().color(QPalette::AlternateBase);
			const auto caution = detail::tag_ink(palette(), detail::tag_tone::caution, ground);
			const auto wash    = detail::tag_wash(palette(), detail::tag_tone::caution);
			auto       effects = QStringList{ };
			auto       added   = QStringList{ };
			for (const auto effect : aAction.Effects) {
				const auto words = escaped(core::describe(effect));
				if (aBefore != nullptr && !std::ranges::contains(aBefore->Effects, effect)) {
					added << text_of(core::describe(effect));
					effects << QStringLiteral("<span style=\"color:%1;background-color:%2\">&nbsp;%3&nbsp;</span>").arg(caution.name(), wash.name(QColor::HexArgb), words);
				} else {
					effects << words;
				}
			}
			auto* const said = rich(effects.join(QStringLiteral(", ")), box);
			said->setObjectName(QStringLiteral("agentConfigurationActionEffects"));
			said->setForegroundRole(QPalette::PlaceholderText);
			said->setFont(detail::secondary_font(said->font()));
			if (!added.isEmpty()) {
				said->setAccessibleDescription(tr("Does more than yours: %1").arg(added.join(tr(", "))));
			}
			lines->addWidget(said);
			lines->addStretch(1);
			return box;
		};
		const auto absent = [table](const QString& aText) {
			auto* const label = note(aText, table);
			label->setContentsMargins(10, 6, 10, 7);
			label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			return label;
		};
		const auto header = [table](const QString& aText) {
			auto* const label = note(aText, table);
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			label->setContentsMargins(10, 6, 10, 6);
			return label;
		};
		auto line = 0;
		if (yours != nullptr && plan.Standing == core::import_standing::updated) {
			grid->addWidget(header(tr("Yours, revision %1").arg(mineRev)), line, 0);
			grid->addWidget(header(tr("In the file, revision %1").arg(theirs)), line, 1);
			++line;
			for (const auto& action : workflow.Actions) {
				const auto before = std::ranges::find(yours->Workflow.Actions, action.Id, &core::workflow_action::Id);
				const auto had    = before != yours->Workflow.Actions.end();
				grid->addWidget(had ? cell(*before, nullptr, { }) : absent(tr("No such action")), line, 0);
				auto* const now = cell(action, had ? &*before : nullptr, had ? QString{ } : tr("New action"));
				if (!had) {
					detail::comparison_table::set_tinted(*now);
				}
				grid->addWidget(now, line, 1);
				++line;
			}
			for (const auto& action : yours->Workflow.Actions) {
				if (std::ranges::find(workflow.Actions, action.Id, &core::workflow_action::Id) == workflow.Actions.end()) {
					grid->addWidget(cell(action, nullptr, { }), line, 0);
					grid->addWidget(absent(tr("Removed")), line, 1);
					++line;
				}
			}
		} else {
			grid->addWidget(header(tr("Its actions")), line++, 0, 1, 2);
			for (const auto& action : workflow.Actions) {
				grid->addWidget(cell(action, nullptr, { }), line++, 0, 1, 2);
			}
		}
		column.addWidget(table);

		// One program for every action that names it: chosen here, never from the file.
		if (plan.Standing != core::import_standing::unchanged) {
			auto names = QStringList{ };
			for (const auto& action : workflow.Actions) {
				if (action.Binding == core::workflow_binding::command && !names.contains(text_of(action.ProgramName))) {
					names << text_of(action.ProgramName);
				}
			}
			for (const auto& name : names) {
				auto asking = std::size_t{ 0U };
				auto bound  = QString{ };
				for (const auto& action : workflow.Actions) {
					if (action.Binding == core::workflow_binding::command && text_of(action.ProgramName) == name) {
						++asking;
						if (const auto program = std::ranges::find(state.Programs, action.Id, &core::workflow_program::Action); program != state.Programs.end()) {
							bound = text_of(program->Program);
						}
					}
				}
				// [[BPR-2204]]. One filled card: the program on one line,
				// Choose program… at its top right, the count in words.
				auto [programBox, programRows] = group_box(&aPage);
				programBox->setObjectName(QStringLiteral("agentConfigurationWorkflowProgramCard"));
				auto* const row = new QHBoxLayout{ };
				row->setSpacing(12);
				const auto  code  = QStringLiteral("<code>%1</code>").arg(breakable(name).toHtmlEscaped());
				auto* const line1 = rich(bound.isEmpty() ? tr("%1 is not chosen yet").arg(code) : tr("%1 is %2").arg(code, breakable(bound).toHtmlEscaped()), programBox);
				line1->setObjectName(QStringLiteral("agentConfigurationWorkflowProgramName"));
				row->addWidget(line1, 1, Qt::AlignVCenter);
				auto* const choose = new quiet_push_button{ tr("Choose program…"), programBox };
				choose->setObjectName(QStringLiteral("agentConfigurationWorkflowProgram"));
				choose->setAccessibleName(tr("Choose program for %1").arg(name));
				row->addWidget(choose, 0, Qt::AlignTop);
				programRows->addLayout(row);
				const auto count = asking == 1U ? tr("One action asks for it.") : tr("%1 actions ask for it.").arg(counted_words(asking));
				programRows->addWidget(note(count + QLatin1Char(' ')
												+ (bound.isEmpty() ? tr("The file names the program and carries no path, so you say where it is on this computer. Until then those actions cannot be handed off.")
																   : tr("Chosen on this computer for every action that names it.")),
											programBox));
				column.addWidget(programBox);
				connect(choose, &QPushButton::clicked, this, [this, aItem, name] {
					if (!Chooser) {
						return;
					}
					const auto picked = Chooser(name);
					if (picked.isEmpty()) {
						return;
					}
					auto& programs = Workflows[aItem].Programs;
					for (const auto& action : Preview.File.Workflows[aItem].Workflow.Actions) {
						if (action.Binding == core::workflow_binding::command && text_of(action.ProgramName) == name) {
							std::erase_if(programs, [&action](const core::workflow_program& aProgram) { return aProgram.Action == action.Id; });
							programs.push_back({ .Action = action.Id, .Program = picked.toStdString() });
						}
					}
					QTimer::singleShot(0, this, [this] { show_selected(); });
				});
			}
		}
		if (plan.Standing == core::import_standing::updated) {
			Outcome = note(tr("Drafts that selected revision %1 keep it until you accept the update there.").arg(mineRev), &aPage);
			Outcome->setObjectName(QStringLiteral("agentConfigurationWorkflowChanges"));
			column.addWidget(Outcome);
		}
	}

	void agent_configuration_import_dialog::show_prompt(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item   = Preview.File.Prompts[aItem];
		const auto& plan   = Plan.Prompts[aItem];
		if (plan.Standing == core::import_standing::refused) {
			show_refused(text_of(item.Prompt.Purpose), text_of(core::describe(item.Problem.value_or(core::configuration_item_problem::unsupported_purpose))), aPage);
			return;
		}
		const auto& local = Preview.Prompts[*item.Local];
		const auto  mine  = local.Override.empty() ? local.BuiltIn : local.Override;
		title_row(column, prompt_title(local.Purpose), standing_tag(plan.Standing), standing_tone(plan.Standing), aPage);
		if (plan.Standing == core::import_standing::updated) {
			auto* const parts = new QHBoxLayout{ };
			parts->setSpacing(0);
			Choice            = new QButtonGroup{ &aPage };
			auto* const take  = segmented(tr("Update"), &aPage);
			auto* const leave = segmented(tr("Leave out"), &aPage);
			Choice->addButton(take, TakePart);
			Choice->addButton(leave, LeavePart);
			parts->addWidget(take);
			parts->addWidget(leave);
			parts->addStretch(1);
			(Prompts[aItem] ? take : leave)->setChecked(true);
			connect(Choice, &QButtonGroup::idClicked, this, [this, aItem](const int aPart) { QTimer::singleShot(0, this, [this, aItem, aPart] { choose(item_kind::prompt, aItem, aPart); }); });
			column.addLayout(parts);
		}
		auto words = QStringList{ };
		words.push_back(local.Override.empty() ? tr("Yours is the built-in text, %1.").arg(text_of(local.BuiltInVersion)) : tr("Yours is edited."));
		if (item.Prompt.Origin == core::portable_prompt_origin::built_in) {
			words.push_back(tr("The file's is the built-in text of %1.").arg(text_of(item.Prompt.WrittenAgainst.value_or(std::string{ "an unnamed version" }))));
		} else if (item.Prompt.WrittenAgainst) {
			words.push_back(*item.Prompt.WrittenAgainst == local.BuiltInVersion ? tr("The file's was written against %1, the built-in this version has.").arg(text_of(local.BuiltInVersion))
																				: tr("The file's was written against %1; this version's built-in is %2.")
																					  .arg(text_of(*item.Prompt.WrittenAgainst), text_of(local.BuiltInVersion)));
		} else {
			words.push_back(tr("The file does not say which built-in version its text was written against."));
		}
		if (plan.Standing == core::import_standing::unchanged) {
			words.push_back(tr("The text is the same as yours."));
		} else if (item.Prompt.Text == local.BuiltIn) {
			words.push_back(tr("Updating restores the built-in text."));
		} else {
			words.push_back(tr("Updating makes it your edited prompt; Reset to built-in still brings the built-in text back."));
		}
		const auto texts    = std::array{ std::string_view{ item.Prompt.Text } };
		const auto resolved = core::resolve_skill_references(texts, Facts.Skills);
		if (!resolved.Unknown.empty()) {
			auto names = QStringList{ };
			for (const auto& name : resolved.Unknown) {
				names.push_back(QLatin1Char('/') + text_of(name));
			}
			words.push_back(tr("Names skills not found on this computer: %1. They stay as written; nothing is installed.").arg(names.join(QStringLiteral(", "))));
		}
		Outcome = note(words.join(QLatin1Char(' ')), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationPromptNote"));
		column.addWidget(Outcome);
		auto* const compare = new QHBoxLayout{ };
		compare->setSpacing(12);
		for (const auto mineSide : { true, false }) {
			auto* const side = new QVBoxLayout{ };
			side->addWidget(heading(mineSide ? tr("Yours") : tr("In the file"), &aPage));
			auto* const text = mono(&aPage, mineSide ? tr("Your prompt") : tr("Imported prompt"));
			text->setPlainText(text_of(mineSide ? std::string_view{ mine } : std::string_view{ item.Prompt.Text }));
			text->setMinimumHeight(220);
			text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			side->addWidget(text);
			compare->addLayout(side, 1);
			(mineSide ? Yours : Theirs) = text;
		}
		column.addLayout(compare);
	}
	void agent_configuration_import_dialog::choice_row(const item_kind aKind, const std::size_t aItem, const QString& aTake, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		auto* const parts  = new QHBoxLayout{ };
		parts->setSpacing(0);
		Choice            = new QButtonGroup{ &aPage };
		auto* const take  = segmented(aTake, &aPage);
		auto* const leave = segmented(tr("Leave out"), &aPage);
		Choice->addButton(take, TakePart);
		Choice->addButton(leave, LeavePart);
		parts->addWidget(take);
		parts->addWidget(leave);
		parts->addStretch(1);
		(chosen(aKind, aItem) ? take : leave)->setChecked(true);
		connect(Choice, &QButtonGroup::idClicked, this, [this, aKind, aItem](const int aPart) { QTimer::singleShot(0, this, [this, aKind, aItem, aPart] { choose(aKind, aItem, aPart); }); });
		column.addLayout(parts);
	}

	/// [[BPR-2198]] (owner, 2026-11-15). Identity: the profile's name and the
	/// accounts it uses. Importing it renames this profile and remembers its
	/// enterprise servers; it signs in to nothing.
	void agent_configuration_import_dialog::show_identity(QWidget& aPage)
	{
		auto&       column   = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& identity = *Preview.File.Identity;
		const auto& plan     = *Plan.Identity;
		title_row(column, tr("Identity"), standing_tag(plan.Standing), standing_tone(plan.Standing), aPage);
		if (plan.Standing == core::import_standing::updated) {
			choice_row(item_kind::identity, 0U, tr("Import"), aPage);
		}
		auto words = QStringList{ };
		if (plan.Standing == core::import_standing::unchanged) {
			words << tr("This profile already has this name and knows these accounts' servers.");
		} else if (plan.Renames) {
			words << tr("Importing renames this profile from “%1” to “%2” and remembers each enterprise server below.").arg(text_of(Preview.Here.ProfileName), text_of(identity.ProfileName));
		} else {
			words << tr("Importing remembers each enterprise server below.");
		}
		words << tr("It signs in to nothing: an account says Sign in to use until you sign in on Settings › Accounts & hosts.");
		Outcome = note(words.join(QLatin1Char(' ')), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationIdentityNote"));
		column.addWidget(Outcome);
		auto [frame, rows] = group_box(&aPage);
		frame->setObjectName(QStringLiteral("agentConfigurationAccounts"));
		auto* const name = new QHBoxLayout{ };
		name->addWidget(new QLabel{ tr("Profile name"), frame });
		name->addStretch(1);
		name->addWidget(new QLabel{ text_of(identity.ProfileName), frame });
		rows->addLayout(name);
		for (std::size_t index = 0U; index < identity.Accounts.size(); ++index) {
			const auto& account = identity.Accounts[index];
			auto* const row     = new QHBoxLayout{ };
			auto* const who     = new QLabel{ tr("%1 on %2").arg(text_of(account.Login), text_of(account.Host)), frame };
			who->setWordWrap(true);
			row->addWidget(who, 1);
			const auto  signedIn = index < plan.Accounts.size() && plan.Accounts[index] == core::account_standing::signed_in;
			auto* const state    = new QLabel{ signedIn ? tr("Signed in") : tr("Sign in to use"), frame };
			state->setObjectName(QStringLiteral("agentConfigurationAccountState"));
			state->setForegroundRole(signedIn ? QPalette::PlaceholderText : QPalette::WindowText);
			row->addWidget(state);
			rows->addLayout(row);
		}
		column.addWidget(frame);
	}

	/// [[BPR-2198]]. One repository: saved under the account signed in on its
	/// host, followed on any host. No local folder comes with it.
	void agent_configuration_import_dialog::show_repository(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item   = Preview.File.Repositories[aItem];
		const auto& plan   = Plan.Repositories[aItem];
		const auto  waits  = plan.SignInToUse && !item.Followed;
		title_row(column, text_of(item.Repository), waits ? tr("Sign in to use") : standing_tag(plan.Standing), waits ? detail::tag_tone::caution : standing_tone(plan.Standing), aPage);
		if (plan.Standing == core::import_standing::added && (!plan.SignInToUse || item.Followed)) {
			choice_row(item_kind::repository, aItem, tr("Add"), aPage);
		}
		auto words = QStringList{ };
		if (plan.Standing == core::import_standing::unchanged) {
			words << tr("This profile already has it.");
		}
		if (item.Saved) {
			words << (plan.SignInToUse ? tr("Saved on %1 in the file. Sign in to use: no account is signed in on %1 here, so it cannot be saved yet. Sign in on Settings › Accounts & hosts, then import the file again.")
											 .arg(text_of(item.Host))
									   : tr("Saved on %1: Home finds your pull requests in it.").arg(text_of(item.Host)));
		}
		if (item.Followed) {
			words << tr("Followed: its pull requests join your queue on any host.");
		}
		words << tr("No local folder came with it; Settings › Repositories finds its clone.");
		Outcome = note(words.join(QLatin1Char(' ')), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationRepositoryNote"));
		column.addWidget(Outcome);
	}
} // namespace bpr::qt
