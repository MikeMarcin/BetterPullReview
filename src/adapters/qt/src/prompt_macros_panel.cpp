#include "bpr/qt/prompt_macros_panel.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QAccessible>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		using core::prompt_place;
		using core::prompt_subject;

		[[nodiscard]] auto place_words(const prompt_place aPlace) -> QString
		{
			switch (aPlace) {
			case prompt_place::agent_tab:    return QObject::tr("Agent tab");
			case prompt_place::conversation: return QObject::tr("Agent tab, about a conversation");
			case prompt_place::tour_stop:    return QObject::tr("Tour chat: a stop");
			case prompt_place::tour_part:    return QObject::tr("Tour chat: Problem and Solution");
			case prompt_place::tour_whole:   return QObject::tr("Tour chat: the whole tour");
			}
			return { };
		}

		[[nodiscard]] auto subject_words(const prompt_subject aSubject) -> QString
		{
			switch (aSubject) {
			case prompt_subject::pull_request:   return QObject::tr("The pull request");
			case prompt_subject::open_file:      return QObject::tr("The open file");
			case prompt_subject::selected_lines: return QObject::tr("The selected lines");
			case prompt_subject::conversation:   return QObject::tr("The conversation");
			case prompt_subject::stop:           return QObject::tr("The stop");
			case prompt_subject::part:           return QObject::tr("This part");
			case prompt_subject::selected_text:  return QObject::tr("The selected text");
			case prompt_subject::tour:           return QObject::tr("The tour");
			}
			return { };
		}

		[[nodiscard]] auto fact_words(const core::prompt_fact aFact) -> QString
		{
			switch (aFact) {
			case core::prompt_fact::file_open:      return QObject::tr("A file is open");
			case core::prompt_fact::selected_lines: return QObject::tr("Lines selected");
			case core::prompt_fact::path:           return QObject::tr("The open file's path");
			case core::prompt_fact::draft:          return QObject::tr("The pull request is a draft");
			case core::prompt_fact::author_is_me:   return QObject::tr("I wrote the pull request");
			case core::prompt_fact::checks_failing: return QObject::tr("A check failed");
			case core::prompt_fact::repository:     return QObject::tr("The repository");
			case core::prompt_fact::takes_replies:  return QObject::tr("The conversation takes replies");
			case core::prompt_fact::empty_chat:     return QObject::tr("The chat is empty");
			}
			return { };
		}

		[[nodiscard]] auto scope_words(const core::prompt_scope aScope) -> QString
		{
			switch (aScope) {
			case core::prompt_scope::thread:       return QObject::tr("Thread");
			case core::prompt_scope::file:         return QObject::tr("File");
			case core::prompt_scope::pull_request: return QObject::tr("Pull request");
			}
			return { };
		}

		/// What a scope toggle asks about, for the subject's own scope or a
		/// wider one (once for each).
		[[nodiscard]] auto scope_tip(const core::prompt_scope aScope, const prompt_subject aSubject) -> QString
		{
			const auto own = core::subject_scope(aSubject);
			if (own && *own == aScope) {
				return aScope == core::prompt_scope::thread ? QObject::tr("Ask about this thread")
					: aScope == core::prompt_scope::file    ? QObject::tr("Ask about the open file")
															: QObject::tr("Ask about the pull request");
			}
			if (aSubject == prompt_subject::conversation) {
				return aScope == core::prompt_scope::file ? QObject::tr("Ask about every thread in the open file") : QObject::tr("Ask about every thread in the pull request");
			}
			return QObject::tr("Ask about every file in the pull request");
		}

		[[nodiscard]] auto problem_words(const core::prompt_macro_problem& aProblem) -> QString
		{
			using enum core::prompt_problem;
			const auto detail = QString::fromStdString(aProblem.Detail);
			switch (aProblem.Problem) {
			case no_name:                     return QObject::tr("Give the prompt a name. It is what the chip says.");
			case name_too_long:               return QObject::tr("The name is too long by %1. A name is one line.").arg(aProblem.Excess);
			case name_not_one_line:           return QObject::tr("A name is one line.");
			case no_template:                 return QObject::tr("Write what the prompt sends.");
			case template_too_long:           return QObject::tr("Too long by %1 bytes. A prompt can be 8 KiB.").arg(aProblem.Excess);
			case unknown_variable:            return QObject::tr("{%1} isn't something this place can fill in. Insert lists what it can.").arg(detail);
			case other_place_variable:        return QObject::tr("{%1} is filled in only in another place.").arg(detail);
			case subject_not_for_place:       return QObject::tr("This place doesn't ask about that.");
			case kind_not_for_subject:        return QObject::tr("A review is asked only in the Agent tab, about the pull request or the open file.");
			case too_many_conditions:         return QObject::tr("A prompt can have four conditions.");
			case fact_not_for_place:          return QObject::tr("Condition %1 isn't known in this place.").arg(aProblem.Condition + 1U);
			case no_patterns:                 return QObject::tr("Condition %1 needs a pattern.").arg(aProblem.Condition + 1U);
			case too_many_patterns:           return QObject::tr("Condition %1 has more than eight patterns.").arg(aProblem.Condition + 1U);
			case pattern_too_long:            return QObject::tr("A pattern of condition %1 is too long.").arg(aProblem.Condition + 1U);
			case bad_pattern:                 return QObject::tr("%1 isn't a pattern. Close the bracket.").arg(detail);
			case conditions_contradict:       return QObject::tr("These can't both be true, so the prompt would never show.");
			case duplicate_name:              return QObject::tr("Another prompt here has this name.");
			case credential_prefix:           return QObject::tr("This looks like it holds a token. A prompt goes to the agent and into exports.");
			case quick_ask_not_for_subject:   return QObject::tr("A quick ask is about the pull request, a file or a thread.");
			case quick_ask_without_scope:     return QObject::tr("Choose where it's asked: Thread, File or Pull request.");
			case scope_narrower_than_subject: return QObject::tr("%1 is narrower than what this prompt is about.").arg(detail);
			}
			return { };
		}

		[[nodiscard]] auto unmet_words(const core::prompt_unmet aUnmet) -> QString
		{
			using enum core::prompt_unmet;
			switch (aUnmet) {
			case off:                     return QObject::tr("No: it's off");
			case invalid:                 return QObject::tr("No: it has a problem to fix");
			case needs_file:              return QObject::tr("No: no file is open");
			case needs_lines:             return QObject::tr("No: no lines are selected");
			case needs_conversation_file: return QObject::tr("No: the conversation isn't on a file");
			case needs_selected_text:     return QObject::tr("No: no text is selected");
			case needs_stop:              return QObject::tr("No: no stop");
			case needs_part:              return QObject::tr("No: no part");
			case needs_pull_request:      return QObject::tr("No: no pull request");
			case needs_repository:        return QObject::tr("No: no repository");
			case condition:               return QObject::tr("No: a condition isn't met");
			case expansion_too_long:      return QObject::tr("No: it expands past 16 KiB");
			}
			return { };
		}

		/// Sample values for a preview with no pull request window.
		[[nodiscard]] auto sample_facts(const prompt_place aPlace) -> core::prompt_facts
		{
			auto facts             = core::prompt_facts{ };
			facts.Place            = aPlace;
			facts.PullRequest      = core::prompt_pull_request{ .Number = 42U, .Title = "Render retained frames before presenting", .Url = "https://github.com/octo/render/pull/42", .Author = "octocat", .Base = "main", .Head = "retained-frames" };
			facts.Repository       = "octo/render";
			facts.OpenFile         = "src/render/renderer.cpp";
			facts.Lines            = core::prompt_line_range{ .First = 5U, .Last = 8U };
			facts.ConversationFile = "src/render/renderer.cpp";
			facts.TakesReplies     = true;
			facts.Draft            = false;
			facts.AuthorIsMe       = false;
			facts.ChecksFailing    = false;
			facts.ChatEmpty        = true;
			if (aPlace == prompt_place::tour_stop || aPlace == prompt_place::tour_part || aPlace == prompt_place::tour_whole) {
				facts.Thread = core::prompt_tour_part{ .Place = aPlace, .Part = aPlace == prompt_place::tour_stop ? "Stop 3" : aPlace == prompt_place::tour_part ? "the Problem" : "", .StopNumber = 3U, .StopTitle = "The frame pool" };
			}
			return facts;
		}

		[[nodiscard]] auto shipped_macro(const std::string& aId) -> std::optional<core::prompt_macro>
		{
			for (const auto& builtin : core::shipped_prompt_macros()) {
				if (builtin.Macro.Id == aId) {
					return builtin.Macro;
				}
			}
			return std::nullopt;
		}
	} // namespace

	// ------------------------------------------------------------------ sheet

	prompt_macro_sheet::prompt_macro_sheet(core::prompt_macro aMacro, std::vector<core::prompt_macro> aSiblings, std::optional<core::prompt_macro> aShipped,
										   std::optional<core::prompt_facts> aPreview, QWidget* const aParent)
		: QDialog{ aParent }
		, Original{ aMacro }
		, Edited{ std::move(aMacro) }
		, Siblings{ std::move(aSiblings) }
		, Shipped{ std::move(aShipped) }
		, Preview{ std::move(aPreview) }
	{
		setObjectName(QStringLiteral("promptMacroSheet"));
		setWindowTitle(tr("Suggested prompt"));
		setAccessibleName(tr("Suggested prompt"));
		auto* const column = new QVBoxLayout{ this };
		auto* const form   = new QFormLayout{ };
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		column->addLayout(form);

		Name = new QLineEdit{ QString::fromStdString(Edited.Body.Name), this };
		Name->setObjectName(QStringLiteral("promptMacroName"));
		Name->setAccessibleName(tr("Shown as"));
		ChipReads = new QLabel{ this };
		ChipReads->setObjectName(QStringLiteral("promptMacroChipReads"));
		ChipReads->setTextFormat(Qt::PlainText);
		auto* const shown = new QVBoxLayout{ };
		shown->addWidget(Name);
		shown->addWidget(ChipReads);
		form->addRow(tr("Shown as"), shown);

		Place = new QComboBox{ this };
		Place->setObjectName(QStringLiteral("promptMacroPlace"));
		Place->setAccessibleName(tr("Where"));
		for (auto index = std::size_t{ 0 }; index < core::prompt_place_count; ++index) {
			Place->addItem(place_words(static_cast<prompt_place>(index)), static_cast<int>(index));
		}
		Place->setCurrentIndex(static_cast<int>(Edited.Place));
		// A built-in keeps its place.
		Place->setEnabled(!Edited.BuiltIn);
		Subject = new QComboBox{ this };
		Subject->setObjectName(QStringLiteral("promptMacroSubject"));
		Subject->setAccessibleName(tr("About"));
		auto* const where = new QHBoxLayout{ };
		where->addWidget(Place, 1);
		where->addWidget(new QLabel{ tr("About"), this });
		where->addWidget(Subject, 1);
		form->addRow(tr("Where"), where);

		AsQuestion = new QRadioButton{ tr("A question"), this };
		AsQuestion->setObjectName(QStringLiteral("promptMacroAsQuestion"));
		AsReview = new QRadioButton{ tr("A review with findings"), this };
		AsReview->setObjectName(QStringLiteral("promptMacroAsReview"));
		(Edited.Body.Kind == core::prompt_ask::review ? AsReview : AsQuestion)->setChecked(true);
		auto* const ask = new QHBoxLayout{ };
		ask->addWidget(AsQuestion);
		ask->addWidget(AsReview);
		ask->addStretch(1);
		form->addRow(tr("Ask as"), ask);

		// RFC-BPR-0128 ([[BPR-2163]]). Quick ask, and where it is asked.
		QuickAsk = new QCheckBox{ tr("for"), this };
		QuickAsk->setObjectName(QStringLiteral("promptMacroQuickAsk"));
		QuickAsk->setAccessibleName(tr("Quick ask"));
		QuickAsk->setAccessibleDescription(tr("List this prompt in the Ask menus"));
		QuickAsk->setChecked(Edited.Body.QuickAsk);
		auto* const quick = new QHBoxLayout{ };
		quick->addWidget(QuickAsk);
		for (const auto scope : { core::prompt_scope::thread, core::prompt_scope::file, core::prompt_scope::pull_request }) {
			auto* const toggle = new QToolButton{ this };
			toggle->setObjectName(QStringLiteral("promptMacroScope_%1").arg(QString::fromLatin1(core::stable_name(scope))));
			toggle->setText(scope_words(scope));
			toggle->setAccessibleName(tr("Quick ask for %1").arg(scope_words(scope)));
			toggle->setCheckable(true);
			toggle->setChecked(Edited.Body.Scopes.has(scope));
			toggle->setFocusPolicy(Qt::StrongFocus);
			toggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
			Scopes.at(static_cast<std::size_t>(scope)) = toggle;
			quick->addWidget(toggle);
			connect(toggle, &QToolButton::toggled, this, [this, scope](const bool aOn) {
				if (!Refreshing) {
					Edited.Body.Scopes.set(scope, aOn);
					refresh();
				}
			});
		}
		quick->addStretch(1);
		form->addRow(tr("Quick ask"), quick);

		Template = new QPlainTextEdit{ QString::fromStdString(Edited.Body.Template), this };
		Template->setObjectName(QStringLiteral("promptMacroTemplate"));
		Template->setAccessibleName(tr("Sends"));
		Template->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		Template->setTabChangesFocus(true);
		Template->setMinimumHeight(fontMetrics().lineSpacing() * 4);
		Bytes = new QLabel{ this };
		Bytes->setObjectName(QStringLiteral("promptMacroBytes"));
		BuiltInNote = new QLabel{ tr("What this sends is in Built-in prompts, below."), this };
		BuiltInNote->setObjectName(QStringLiteral("promptMacroActionNote"));
		BuiltInNote->setWordWrap(true);
		auto* const sends = new QVBoxLayout{ };
		sends->addWidget(Template);
		sends->addWidget(Bytes);
		sends->addWidget(BuiltInNote);
		form->addRow(tr("Sends"), sends);
		// RFC-BPR-0128 (Q13): an action row's sent prompt stays in Built-in
		// prompts.
		const auto action = Edited.Action != core::prompt_action::none;
		Template->setVisible(!action);
		Bytes->setVisible(!action);
		BuiltInNote->setVisible(action);

		PreviewText = new QLabel{ this };
		PreviewText->setObjectName(QStringLiteral("promptMacroPreview"));
		PreviewText->setAccessibleName(tr("What is sent"));
		PreviewText->setWordWrap(true);
		PreviewText->setTextFormat(Qt::PlainText);
		PreviewText->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		PreviewText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		ShowsNow = new QLabel{ this };
		ShowsNow->setObjectName(QStringLiteral("promptMacroShowsNow"));
		auto* const preview = new QVBoxLayout{ };
		preview->addWidget(new QLabel{ Preview ? tr("With this pull request") : tr("With sample values"), this });
		preview->addWidget(PreviewText);
		preview->addWidget(ShowsNow);
		form->addRow(tr("Preview"), preview);
		PreviewText->setVisible(!action);

		auto* const shows = new QVBoxLayout{ };
		shows->addWidget(new QLabel{ tr("All of these are true"), this });
		Conditions = new QVBoxLayout{ };
		shows->addLayout(Conditions);
		AddCondition = new QPushButton{ tr("Add a condition"), this };
		AddCondition->setObjectName(QStringLiteral("promptMacroAddCondition"));
		AddCondition->setAutoDefault(false);
		connect(AddCondition, &QPushButton::clicked, this, [this] {
			auto fact = core::prompt_fact::draft;
			for (auto index = std::size_t{ 0 }; index < core::prompt_fact_count; ++index) {
				if (core::prompt_fact_allowed(Edited.Place, static_cast<core::prompt_fact>(index))) {
					fact = static_cast<core::prompt_fact>(index);
					break;
				}
			}
			Edited.Body.Conditions.push_back({ .Fact = fact });
			rebuild_conditions();
			refresh();
		});
		shows->addWidget(AddCondition, 0, Qt::AlignLeft);
		Needs = new QLabel{ this };
		Needs->setObjectName(QStringLiteral("promptMacroNeeds"));
		Needs->setWordWrap(true);
		shows->addWidget(Needs);
		form->addRow(tr("Shows when"), shows);

		Problems = new QLabel{ this };
		Problems->setObjectName(QStringLiteral("promptMacroProblems"));
		Problems->setWordWrap(true);
		Problems->setTextFormat(Qt::PlainText);
		column->addWidget(Problems);

		auto* const buttons = new QHBoxLayout{ };
		if (Edited.BuiltIn) {
			auto* const reset = new quiet_push_button{ this };
			reset->setText(tr("Reset to built-in"));
			reset->setObjectName(QStringLiteral("promptMacroReset"));
			reset->setEnabled(Shipped.has_value() && (Shipped->Body != Edited.Body));
			connect(reset, &QPushButton::clicked, this, [this, reset] {
				if (Shipped) {
					Edited.Body = Shipped->Body;
					Name->setText(QString::fromStdString(Edited.Body.Name));
					Template->setPlainText(QString::fromStdString(Edited.Body.Template));
					QuickAsk->setChecked(Edited.Body.QuickAsk);
					rebuild_conditions();
					refresh();
					reset->setEnabled(false);
				}
			});
			buttons->addWidget(reset);
		} else {
			auto* const remove = new quiet_push_button{ this };
			remove->setText(tr("Delete…"));
			remove->setObjectName(QStringLiteral("promptMacroDelete"));
			connect(remove, &QPushButton::clicked, this, [this] {
				if (QMessageBox::question(this, tr("Delete this prompt?"), tr("Delete “%1”? You can undo it until Settings closes.").arg(Name->text())) == QMessageBox::Yes) {
					finish(outcome::deleted);
				}
			});
			buttons->addWidget(remove);
		}
		auto* const duplicate = new quiet_push_button{ this };
		duplicate->setText(tr("Duplicate"));
		duplicate->setObjectName(QStringLiteral("promptMacroDuplicate"));
		duplicate->setEnabled(Edited.Action == core::prompt_action::none);
		connect(duplicate, &QPushButton::clicked, this, [this] { finish(outcome::duplicated); });
		buttons->addWidget(duplicate);
		buttons->addStretch(1);
		auto* const cancel = new QPushButton{ tr("Cancel"), this };
		cancel->setObjectName(QStringLiteral("promptMacroCancel"));
		cancel->setAutoDefault(false);
		connect(cancel, &QPushButton::clicked, this, [this] { reject(); });
		buttons->addWidget(cancel);
		Save = new QPushButton{ tr("Save prompt"), this };
		Save->setObjectName(QStringLiteral("promptMacroSave"));
		Save->setDefault(true);
		connect(Save, &QPushButton::clicked, this, [this] { finish(outcome::saved); });
		buttons->addWidget(Save);
		column->addLayout(buttons);

		connect(Name, &QLineEdit::textChanged, this, [this](const QString& aText) {
			Edited.Body.Name = aText.toStdString();
			refresh();
		});
		connect(Template, &QPlainTextEdit::textChanged, this, [this] {
			Edited.Body.Template = Template->toPlainText().toStdString();
			refresh();
		});
		connect(Place, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (Refreshing || aIndex < 0) {
				return;
			}
			Edited.Place = static_cast<prompt_place>(Place->itemData(aIndex).toInt());
			refill_subjects();
			refresh();
		});
		connect(Subject, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (Refreshing || aIndex < 0) {
				return;
			}
			Edited.Body.Subject = static_cast<prompt_subject>(Subject->itemData(aIndex).toInt());
			refresh();
		});
		connect(AsQuestion, &QRadioButton::toggled, this, [this](const bool aOn) {
			if (!Refreshing) {
				Edited.Body.Kind = aOn ? core::prompt_ask::question : core::prompt_ask::review;
				refresh();
			}
		});
		connect(QuickAsk, &QCheckBox::toggled, this, [this](const bool aOn) {
			if (!Refreshing) {
				Edited.Body.QuickAsk = aOn;
				// Ticked with no scope on: the subject's own scope.
				if (aOn && !Edited.Body.Scopes.any()) {
					if (const auto own = core::subject_scope(Edited.Body.Subject)) {
						Edited.Body.Scopes.set(*own, true);
					}
				}
				refresh();
			}
		});
		refill_subjects();
		rebuild_conditions();
		refresh();
		resize(sizeHint().expandedTo(QSize{ 560, 0 }));
	}

	auto prompt_macro_sheet::macro() const -> core::prompt_macro { return Edited; }

	auto prompt_macro_sheet::problems_text() const -> QString { return Problems->text(); }

	auto prompt_macro_sheet::preview_text() const -> QString { return PreviewText->text(); }

	void prompt_macro_sheet::refill_subjects()
	{
		Refreshing = true;
		Subject->clear();
		for (auto index = 0; index < 8; ++index) {
			const auto subject = static_cast<prompt_subject>(index);
			if (core::prompt_subject_allowed(Edited.Place, subject)) {
				Subject->addItem(subject_words(subject), index);
			}
		}
		auto found = Subject->findData(static_cast<int>(Edited.Body.Subject));
		if (found < 0 && Subject->count() > 0) {
			found               = 0;
			Edited.Body.Subject = static_cast<prompt_subject>(Subject->itemData(0).toInt());
		}
		Subject->setCurrentIndex(found);
		Subject->setEnabled(!Edited.BuiltIn || Edited.Action == core::prompt_action::none);
		Refreshing = false;
	}

	void prompt_macro_sheet::rebuild_conditions()
	{
		while (auto* const item = Conditions->takeAt(0)) {
			if (auto* const layout = item->layout(); layout != nullptr) {
				while (auto* const inner = layout->takeAt(0)) {
					delete inner->widget();
					delete inner;
				}
			}
			delete item->widget();
			delete item;
		}
		for (auto index = std::size_t{ 0 }; index < Edited.Body.Conditions.size(); ++index) {
			auto&       condition = Edited.Body.Conditions[index];
			auto* const row       = new QHBoxLayout{ };
			auto* const fact      = new QComboBox{ this };
			fact->setAccessibleName(tr("Condition %1: what it reads").arg(index + 1U));
			for (auto each = std::size_t{ 0 }; each < core::prompt_fact_count; ++each) {
				if (core::prompt_fact_allowed(Edited.Place, static_cast<core::prompt_fact>(each)) || static_cast<core::prompt_fact>(each) == condition.Fact) {
					fact->addItem(fact_words(static_cast<core::prompt_fact>(each)), static_cast<int>(each));
				}
			}
			fact->setCurrentIndex(fact->findData(static_cast<int>(condition.Fact)));
			auto* const polarity = new QComboBox{ this };
			polarity->setAccessibleName(tr("Condition %1: is or is not").arg(index + 1U));
			const auto patterned = condition.Fact == core::prompt_fact::path || condition.Fact == core::prompt_fact::repository;
			polarity->addItem(patterned ? tr("matches") : tr("is"));
			polarity->addItem(patterned ? tr("doesn't match") : tr("is not"));
			polarity->setCurrentIndex(condition.Negated ? 1 : 0);
			auto* const value = new QLineEdit{ this };
			value->setAccessibleName(tr("Condition %1: patterns, separated by commas").arg(index + 1U));
			value->setPlaceholderText(tr("src/**, *.cpp"));
			auto patterns = QStringList{ };
			for (const auto& pattern : condition.Patterns) {
				patterns << QString::fromStdString(pattern);
			}
			value->setText(patterns.join(QStringLiteral(", ")));
			value->setVisible(patterned);
			auto* const lines = new QComboBox{ this };
			lines->setAccessibleName(tr("Condition %1: how many lines").arg(index + 1U));
			lines->addItems({ tr("any"), tr("one line"), tr("several lines") });
			lines->setCurrentIndex(static_cast<int>(condition.Lines));
			lines->setVisible(condition.Fact == core::prompt_fact::selected_lines);
			auto* const remove = new QToolButton{ this };
			remove->setText(tr("Remove"));
			remove->setAccessibleName(tr("Remove condition %1").arg(index + 1U));
			remove->setAutoRaise(true);
			row->addWidget(fact, 2);
			row->addWidget(polarity, 1);
			row->addWidget(value, 2);
			row->addWidget(lines, 1);
			row->addWidget(remove);
			Conditions->addLayout(row);
			connect(fact, &QComboBox::currentIndexChanged, this, [this, index, fact](const int aIndex) {
				Edited.Body.Conditions.at(index).Fact = static_cast<core::prompt_fact>(fact->itemData(aIndex).toInt());
				QMetaObject::invokeMethod(this, [this] { rebuild_conditions(); refresh(); }, Qt::QueuedConnection);
			});
			connect(polarity, &QComboBox::currentIndexChanged, this, [this, index](const int aIndex) {
				Edited.Body.Conditions.at(index).Negated = aIndex == 1;
				refresh();
			});
			connect(value, &QLineEdit::textChanged, this, [this, index](const QString& aText) {
				Edited.Body.Conditions.at(index).Patterns = core::parse_prompt_patterns(aText.toStdString());
				refresh();
			});
			connect(lines, &QComboBox::currentIndexChanged, this, [this, index](const int aIndex) {
				Edited.Body.Conditions.at(index).Lines = static_cast<core::prompt_lines_value>(aIndex);
				refresh();
			});
			connect(remove, &QToolButton::clicked, this, [this, index] {
				Edited.Body.Conditions.erase(Edited.Body.Conditions.begin() + static_cast<std::ptrdiff_t>(index));
				QMetaObject::invokeMethod(this, [this] { rebuild_conditions(); refresh(); }, Qt::QueuedConnection);
			});
		}
		AddCondition->setEnabled(Edited.Body.Conditions.size() < core::maximum_prompt_conditions);
	}

	void prompt_macro_sheet::refresh()
	{
		Refreshing = true;
		// Scopes: one narrower than the subject cannot be set.
		const auto quickable = core::subject_scope(Edited.Body.Subject).has_value() && (Edited.Place == prompt_place::agent_tab || Edited.Place == prompt_place::conversation);
		QuickAsk->setEnabled(quickable || Edited.Body.QuickAsk);
		for (const auto scope : { core::prompt_scope::thread, core::prompt_scope::file, core::prompt_scope::pull_request }) {
			auto* const toggle  = Scopes.at(static_cast<std::size_t>(scope));
			const auto  allowed = core::prompt_scope_allowed(Edited.Place, Edited.Body.Subject, scope);
			if (!allowed && Edited.Body.Scopes.has(scope)) {
				Edited.Body.Scopes.set(scope, false);
			}
			toggle->setChecked(Edited.Body.Scopes.has(scope));
			toggle->setEnabled(allowed && Edited.Body.QuickAsk);
			toggle->setToolTip(allowed ? scope_tip(scope, Edited.Body.Subject) : tr("Narrower than what this prompt is about"));
		}
		AsReview->setEnabled(core::prompt_ask_allowed(Edited.Place, Edited.Body.Subject, core::prompt_ask::review) && Edited.Action == core::prompt_action::none);
		AsQuestion->setChecked(Edited.Body.Kind == core::prompt_ask::question);
		AsReview->setChecked(Edited.Body.Kind == core::prompt_ask::review);
		Bytes->setText(tr("%1 of %2 bytes").arg(QLocale{ }.toString(static_cast<qulonglong>(Edited.Body.Template.size())), QLocale{ }.toString(static_cast<qulonglong>(core::maximum_prompt_template_bytes))));

		const auto facts      = Preview ? *Preview : sample_facts(Edited.Place);
		auto       against    = facts;
		against.Place         = Edited.Place;
		const auto evaluation = core::evaluate_prompt_macro(Edited, against);
		if (evaluation.Offer) {
			ChipReads->setText(tr("The chip reads: %1").arg(QString::fromStdString(evaluation.Offer->Label)));
			PreviewText->setText(QString::fromStdString(evaluation.Offer->Expansion.Text));
			ShowsNow->setText(tr("Shows now: yes"));
		} else {
			ChipReads->setText({ });
			PreviewText->setText({ });
			ShowsNow->setText(tr("Shows now: %1").arg(unmet_words(evaluation.Unmet)));
		}
		auto needs = QStringList{ };
		if (Edited.Body.Template.find("{file") != std::string::npos || Edited.Body.Subject == prompt_subject::open_file) {
			needs << tr("It also needs a file open in Review.");
		}
		if (Edited.Body.Template.find("{lines") != std::string::npos || Edited.Body.Subject == prompt_subject::selected_lines) {
			needs << tr("It also needs selected lines.");
		}
		if (Edited.Action == core::prompt_action::was_addressed) {
			needs << tr("It also needs commits newer than the thread.");
		} else if (Edited.Action == core::prompt_action::what_changed) {
			needs << tr("It also needs commits since your review.");
		}
		Needs->setText(needs.join(QLatin1Char(' ')));
		Needs->setVisible(!needs.isEmpty());

		auto words    = QStringList{ };
		auto blocking = false;
		for (const auto& problem : core::validate_prompt_macro(Edited, Siblings)) {
			words << problem_words(problem);
			blocking = blocking || core::prompt_problem_blocks(problem.Problem);
		}
		Problems->setText(words.join(QLatin1Char('\n')));
		Problems->setVisible(!words.isEmpty());
		Save->setEnabled(!blocking);
		Refreshing = false;
	}

	void prompt_macro_sheet::finish(const outcome aOutcome)
	{
		Outcome = aOutcome;
		QDialog::accept();
	}

	void prompt_macro_sheet::keyPressEvent(QKeyEvent* const aEvent)
	{
		// Ctrl or Cmd with Return saves, as composers that save do.
		if ((aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && aEvent->modifiers().testFlag(Qt::ControlModifier) && Save->isEnabled()) {
			finish(outcome::saved);
			return;
		}
		QDialog::keyPressEvent(aEvent);
	}

	void prompt_macro_sheet::reject()
	{
		if (Edited != Original
			&& QMessageBox::question(this, tr("Discard your changes?"), tr("Your changes to this prompt aren't saved."), QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard) {
			return;
		}
		Outcome = outcome::cancelled;
		QDialog::reject();
	}

	// ----------------------------------------------------------------- roster

	prompt_macros_panel::prompt_macros_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("promptMacrosPanel"));
		setAccessibleName(tr("Suggested prompts"));
		auto* const column  = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		auto* const heading = new QLabel{ tr("Suggested prompts"), this };
		auto        bold    = heading->font();
		bold.setBold(true);
		heading->setFont(bold);
		column->addWidget(heading);
		auto* const intro = new QLabel{ tr("The questions offered as chips in the Agent tab and the tour chats, and in the Ask menus when Quick ask is on. A press sends what the prompt expands to."), this };
		intro->setWordWrap(true);
		column->addWidget(intro);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("promptMacrosStatus"));
		Status->setWordWrap(true);
		Status->hide();
		column->addWidget(Status);
		for (auto index = std::size_t{ 0 }; index < core::prompt_place_count; ++index) {
			const auto place = static_cast<prompt_place>(index);
			auto* const title = new QLabel{ place_words(place), this };
			column->addWidget(title);
			auto* const list = new QListWidget{ this };
			list->setObjectName(QStringLiteral("promptMacros_%1").arg(QString::fromLatin1(core::stable_name(place))));
			list->setAccessibleName(tr("Suggested prompts: %1").arg(place_words(place)));
			list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			list->setWordWrap(true);
			list->setTextElideMode(Qt::ElideRight);
			list->setUniformItemSizes(false);
			list->setSpacing(1);
			list->setDragDropMode(QAbstractItemView::InternalMove);
			list->setDefaultDropAction(Qt::MoveAction);
			list->installEventFilter(this);
			title->setBuddy(list);
			Lists.at(index) = list;
			column->addWidget(list);
			connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
				if (!Filling && aItem != nullptr) {
					toggle(aItem->data(Qt::UserRole).toString().toStdString(), aItem->checkState() == Qt::Checked);
				}
			});
			connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) {
				if (aItem != nullptr) {
					edit(aItem->data(Qt::UserRole).toString().toStdString());
				}
			});
			// A drag within the place is an order.
			connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this, place, list] {
				if (Filling) {
					return;
				}
				auto order = std::vector<core::prompt_macro>{ };
				for (auto row = 0; row < list->count(); ++row) {
					const auto id    = list->item(row)->data(Qt::UserRole).toString().toStdString();
					const auto found = std::ranges::find(Macros, id, &core::prompt_macro::Id);
					if (found != Macros.end()) {
						order.push_back(*found);
					}
				}
				auto edited = std::vector<core::prompt_macro>{ };
				for (const auto& macro : Macros) {
					if (macro.Place != place) {
						edited.push_back(macro);
					} else if (!order.empty()) {
						edited.insert(edited.end(), order.begin(), order.end());
						order.clear();
					}
				}
				save(std::move(edited));
			});
		}
		auto* const buttons = new QHBoxLayout{ };
		Add                 = new QPushButton{ tr("Add prompt…"), this };
		Add->setObjectName(QStringLiteral("promptMacrosAdd"));
		connect(Add, &QPushButton::clicked, this, [this] {
			auto place = prompt_place::agent_tab;
			for (auto index = std::size_t{ 0 }; index < core::prompt_place_count; ++index) {
				if (Lists.at(index)->hasFocus() || Lists.at(index)->currentItem() != nullptr) {
					place = static_cast<prompt_place>(index);
					if (Lists.at(index)->hasFocus()) {
						break;
					}
				}
			}
			add_prompt(place);
		});
		buttons->addWidget(Add);
		buttons->addStretch(1);
		Restore = new quiet_push_button{ this };
		Restore->setText(tr("Restore built-ins…"));
		Restore->setObjectName(QStringLiteral("promptMacrosRestore"));
		connect(Restore, &QPushButton::clicked, this, [this] {
			const auto changed = std::ranges::count_if(Projection.Set.Entries, [](const core::prompt_set_entry& aEntry) { return aEntry.Macro.BuiltIn && (aEntry.Changed || !aEntry.Macro.On); });
			if (QMessageBox::question(this, tr("Restore built-ins?"), tr("Return %1 changed built-in prompts to their shipped text, order and On? Your own prompts stay.").arg(changed)) != QMessageBox::Yes) {
				return;
			}
			auto restored = std::vector<core::prompt_macro>{ };
			for (const auto& builtin : core::shipped_prompt_macros()) {
				restored.push_back(builtin.Macro);
			}
			for (const auto& macro : Macros) {
				if (!macro.BuiltIn) {
					restored.push_back(macro);
				}
			}
			save(std::move(restored));
		});
		buttons->addWidget(Restore);
		column->addLayout(buttons);
		Projection.Set = core::merge_prompt_macros(core::shipped_prompt_macros(), { });
		rebuild();
	}

	void prompt_macros_panel::present(const application::prompt_macro_projection& aProjection)
	{
		Projection = aProjection;
		rebuild();
	}

	void prompt_macros_panel::set_save_dispatch(std::function<void(std::vector<core::prompt_macro>)> aDispatch) { SaveDispatch = std::move(aDispatch); }

	void prompt_macros_panel::set_preview_facts(std::function<std::optional<core::prompt_facts>(core::prompt_place)> aFacts) { PreviewFacts = std::move(aFacts); }

	void prompt_macros_panel::reveal()
	{
		if (auto* const list = Lists.front(); list != nullptr) {
			list->setFocus(Qt::OtherFocusReason);
			if (list->currentRow() < 0 && list->count() > 0) {
				list->setCurrentRow(0);
			}
		}
		// The page scrolls to the roster.
		for (auto* parent = parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const area = qobject_cast<QAbstractScrollArea*>(parent); area != nullptr) {
				area->verticalScrollBar()->setValue(mapTo(area->viewport(), QPoint{ 0, 0 }).y() + area->verticalScrollBar()->value());
				break;
			}
		}
	}

	auto prompt_macros_panel::sentence(const core::prompt_macro& aMacro) -> QString
	{
		auto words = aMacro.Body.Kind == core::prompt_ask::review ? tr("A review of %1").arg(subject_words(aMacro.Body.Subject).toLower())
															   : tr("A question about %1").arg(subject_words(aMacro.Body.Subject).toLower());
		if (aMacro.Body.Conditions.empty()) {
			words += QLatin1Char('.');
		} else {
			auto when = QStringList{ };
			for (const auto& condition : aMacro.Body.Conditions) {
				auto patterns = QStringList{ };
				for (const auto& pattern : condition.Patterns) {
					patterns << QString::fromStdString(pattern);
				}
				when << (patterns.isEmpty() ? (condition.Negated ? tr("not: %1").arg(fact_words(condition.Fact).toLower()) : fact_words(condition.Fact).toLower())
											: tr("%1 %2 %3").arg(fact_words(condition.Fact).toLower(), condition.Negated ? tr("doesn't match") : tr("matches"), patterns.join(QStringLiteral(", "))));
			}
			words += tr(", when %1.").arg(when.join(tr(" and ")));
		}
		if (aMacro.Body.QuickAsk) {
			auto scopes = QStringList{ };
			for (const auto scope : { core::prompt_scope::thread, core::prompt_scope::file, core::prompt_scope::pull_request }) {
				if (aMacro.Body.Scopes.has(scope)) {
					scopes << scope_words(scope).toLower();
				}
			}
			words += tr(" Asked from Ask on a %1.").arg(scopes.join(tr(", a ")));
		}
		return words;
	}

	auto prompt_macros_panel::state_words(const core::prompt_set_entry& aEntry) -> QString
	{
		auto words = QStringList{ };
		if (aEntry.Macro.BuiltIn) {
			words << (aEntry.Changed ? tr("Changed") : tr("Built-in"));
		}
		if (aEntry.Macro.Body.QuickAsk) {
			words << tr("quick ask");
		}
		if (!aEntry.Macro.On) {
			words << tr("off");
		}
		return words.join(QStringLiteral(", "));
	}

	void prompt_macros_panel::rebuild()
	{
		Filling = true;
		Macros  = Projection.Set.macros();
		for (auto index = std::size_t{ 0 }; index < core::prompt_place_count; ++index) {
			auto* const list    = Lists.at(index);
			const auto  current = list->currentItem() != nullptr ? list->currentItem()->data(Qt::UserRole).toString() : QString{ };
			list->clear();
			for (const auto& entry : Projection.Set.Entries) {
				if (entry.Macro.Place != static_cast<prompt_place>(index)) {
					continue;
				}
				const auto  state = state_words(entry);
				auto* const item  = new QListWidgetItem{ QStringLiteral("%1   (%2)\n%3").arg(QString::fromStdString(entry.Macro.Body.Name), state.isEmpty() ? tr("yours") : state, sentence(entry.Macro)), list };
				item->setData(Qt::UserRole, QString::fromStdString(entry.Macro.Id));
				item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
				item->setCheckState(entry.Macro.On ? Qt::Checked : Qt::Unchecked);
				item->setToolTip(sentence(entry.Macro));
				item->setData(Qt::AccessibleTextRole, tr("%1, %2, %3, %4").arg(QString::fromStdString(entry.Macro.Body.Name), place_words(entry.Macro.Place), state.isEmpty() ? tr("yours") : state, entry.Macro.On ? tr("on") : tr("off")));
				if (QString::fromStdString(entry.Macro.Id) == current) {
					list->setCurrentItem(item);
				}
			}
			if (list->count() == 0) {
				auto* const empty = new QListWidgetItem{ tr("No prompts show here. Turn one on, or add your own."), list };
				empty->setFlags(Qt::NoItemFlags);
			}
			auto height = 2 * list->frameWidth();
			for (auto row = 0; row < list->count(); ++row) {
				height += list->sizeHintForRow(row) + 2 * list->spacing();
			}
			list->setFixedHeight(height);
		}
		const auto editable = Projection.Readable || !Projection.Loaded;
		auto       status   = QStringList{ };
		if (Projection.Loaded && !Projection.Readable) {
			status << tr("Your suggested prompts can't be read, so they can't be edited now. The built-in prompts still work.");
		}
		if (Projection.LastWrite == application::prompt_macro_write::stale) {
			status << tr("Suggested prompts changed in another window. Your edit is kept: look at the list, then save again.");
		} else if (Projection.LastWrite == application::prompt_macro_write::failed && Projection.Readable) {
			status << tr("Couldn't save. Nothing changed.");
		}
		if (!Projection.Newer.empty()) {
			status << tr("%n prompt(s) need a newer BetterPullReview.", nullptr, static_cast<int>(Projection.Newer.size()));
		}
		Status->setText(status.join(QLatin1Char('\n')));
		Status->setVisible(!status.isEmpty());
		Add->setEnabled(editable && static_cast<bool>(SaveDispatch));
		Restore->setEnabled(editable && static_cast<bool>(SaveDispatch));
		for (auto* const list : Lists) {
			list->setEnabled(editable);
		}
		Filling = false;
	}

	void prompt_macros_panel::save(std::vector<core::prompt_macro> aMacros)
	{
		Macros = aMacros;
		if (SaveDispatch) {
			SaveDispatch(std::move(aMacros));
		}
	}

	void prompt_macros_panel::toggle(const std::string& aId, const bool aOn)
	{
		auto edited = Macros;
		for (auto& macro : edited) {
			if (macro.Id == aId) {
				macro.On = aOn;
			}
		}
		save(std::move(edited));
		QAccessible::updateAccessibility(nullptr);
	}

	void prompt_macros_panel::move(const prompt_place aPlace, const int aRow, const int aBy)
	{
		auto ids = std::vector<std::string>{ };
		for (const auto& macro : Macros) {
			if (macro.Place == aPlace) {
				ids.push_back(macro.Id);
			}
		}
		const auto to = aRow + aBy;
		if (aRow < 0 || to < 0 || aRow >= static_cast<int>(ids.size()) || to >= static_cast<int>(ids.size())) {
			return;
		}
		std::swap(ids.at(static_cast<std::size_t>(aRow)), ids.at(static_cast<std::size_t>(to)));
		auto edited = std::vector<core::prompt_macro>{ };
		auto placed = false;
		for (const auto& macro : Macros) {
			if (macro.Place != aPlace) {
				edited.push_back(macro);
				continue;
			}
			if (!placed) {
				for (const auto& id : ids) {
					edited.push_back(*std::ranges::find(Macros, id, &core::prompt_macro::Id));
				}
				placed = true;
			}
		}
		save(std::move(edited));
		if (auto* const list = Lists.at(static_cast<std::size_t>(aPlace)); list != nullptr) {
			list->setCurrentRow(to);
			list->setAccessibleDescription(tr("Moved to %1 of %2").arg(to + 1).arg(ids.size()));
		}
	}

	auto prompt_macros_panel::status_text() const -> QString { return Status->isVisible() ? Status->text() : QString{ }; }

	void prompt_macros_panel::edit(const std::string& aId, const std::function<void(prompt_macro_sheet&)>& aEdit)
	{
		const auto found = std::ranges::find(Macros, aId, &core::prompt_macro::Id);
		if (found != Macros.end()) {
			open_sheet(*found, false, aEdit);
		}
	}

	void prompt_macros_panel::add_prompt(const prompt_place aPlace, const std::function<void(prompt_macro_sheet&)>& aEdit)
	{
		auto macro    = core::prompt_macro{ .Id = "own." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(), .Place = aPlace };
		auto subjects = std::vector<prompt_subject>{ };
		for (auto index = 0; index < 8; ++index) {
			if (core::prompt_subject_allowed(aPlace, static_cast<prompt_subject>(index))) {
				macro.Body.Subject = static_cast<prompt_subject>(index);
				break;
			}
		}
		open_sheet(std::move(macro), true, aEdit);
	}

	void prompt_macros_panel::open_sheet(core::prompt_macro aMacro, const bool aNew, const std::function<void(prompt_macro_sheet&)>& aEdit)
	{
		auto siblings = Macros;
		std::erase_if(siblings, [&aMacro](const core::prompt_macro& aOther) { return aOther.Id == aMacro.Id; });
		const auto preview = PreviewFacts ? PreviewFacts(aMacro.Place) : std::nullopt;
		auto       sheet   = prompt_macro_sheet{ aMacro, siblings, aMacro.BuiltIn ? shipped_macro(aMacro.Id) : std::nullopt, preview, this };
		if (aEdit) {
			aEdit(sheet);
		} else if (sheet.exec() != QDialog::Accepted) {
			return;
		}
		auto       edited = Macros;
		const auto made   = sheet.macro();
		switch (sheet.result_outcome()) {
		case prompt_macro_sheet::outcome::cancelled: return;
		case prompt_macro_sheet::outcome::saved:
			if (aNew) {
				edited.push_back(made);
			} else {
				std::ranges::replace_if(edited, [&made](const core::prompt_macro& aOther) { return aOther.Id == made.Id; }, made);
			}
			break;
		case prompt_macro_sheet::outcome::duplicated: {
			auto copy    = made;
			copy.Id      = "own." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
			copy.BuiltIn = false;
			copy.Action  = core::prompt_action::none;
			copy.Body.Name += " (copy)";
			edited.push_back(std::move(copy));
			break;
		}
		case prompt_macro_sheet::outcome::deleted: std::erase_if(edited, [&made](const core::prompt_macro& aOther) { return aOther.Id == made.Id && !aOther.BuiltIn; }); break;
		}
		save(std::move(edited));
	}

	auto prompt_macros_panel::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress) {
			auto* const key  = static_cast<QKeyEvent*>(aEvent);
			auto* const list = qobject_cast<QListWidget*>(aWatched);
			if (list != nullptr && key->modifiers().testFlag(Qt::AltModifier) && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
				const auto place = static_cast<prompt_place>(std::distance(Lists.begin(), std::ranges::find(Lists, list)));
				move(place, list->currentRow(), key->key() == Qt::Key_Up ? -1 : 1);
				return true;
			}
			if (list != nullptr && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && list->currentItem() != nullptr) {
				edit(list->currentItem()->data(Qt::UserRole).toString().toStdString());
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
