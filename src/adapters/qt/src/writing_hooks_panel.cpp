#include "bpr/qt/writing_hooks_panel.hpp"

#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/skill_completer.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace bpr::qt {
	namespace {
		constexpr int hook_role      = Qt::UserRole + 1;
		constexpr int usage_role     = Qt::UserRole + 2;
		constexpr int marker_role    = Qt::UserRole + 3;
		constexpr int header_role    = Qt::UserRole + 4;
		constexpr int review_skill_v = 100;

		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		/// What each built-in prompt is for and where it is used, in the
		/// person's words.
		struct prompt_facts
		{
			core::writing_hook Hook;
			const char*        Name;
			const char*        Where;
			const char*        Purpose;
			const char*        UsedBy;
		};
		constexpr auto facts = std::array<prompt_facts, core::writing_hook_count>{ {
			{ core::writing_hook::problem, "Problem statement", "Tour tab, Problem", "How the agent writes the tour's Problem: why this change is needed.",
			  "Used by Draft from PR context and Enhance on the Tour tab's Problem field." },
			{ core::writing_hook::solution, "Solution summary", "Tour tab, Solution", "How the agent writes the tour's Solution: what the change does and how it works.",
			  "Used by Draft from PR context and Enhance on the Tour tab's Solution field." },
			{ core::writing_hook::outline, "Tour outline", "Tour tab, Suggest an outline", "How the agent proposes the tour's stops.", "Used by Suggest an outline on the Tour tab." },
			{ core::writing_hook::comment, "Tour-stop comment", "Tour tab, each stop", "How the agent writes one stop's comment.", "Used by Generate comment and Enhance on a tour stop." },
			{ core::writing_hook::feedback, "Review feedback", "Comment workshop", "Guidance the comment workshop follows when it helps you word a review comment.",
			  "Sent with comment workshop requests while Comments › Comment workshop turns it on." },
		} };

		[[nodiscard]] auto facts_of(const core::writing_hook aHook) -> const prompt_facts& { return facts[static_cast<std::size_t>(aHook)]; }

		/// A prompt's row: its name, where it is used on a quieter second
		/// line, and on the right whether it is changed or has unsaved edits.
		/// A group's heading row is drawn small and quiet and is not chosen.
		/// Colors and fonts come from the row's own style option.
		class prompt_row_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto name = option.text;
				option.text.clear();
				const auto* const widget = option.widget;
				auto* const       style  = widget != nullptr ? widget->style() : QApplication::style();
				aPainter->save();
				if (aIndex.data(header_role).toBool()) {
					auto small = option.font;
					small.setPointSizeF(small.pointSizeF() * 0.9);
					small.setWeight(QFont::DemiBold);
					auto quiet = option.palette.color(QPalette::Text);
					quiet.setAlphaF(0.55F);
					aPainter->setFont(small);
					aPainter->setPen(quiet);
					aPainter->drawText(option.rect.adjusted(10, 0, -10, -3), Qt::AlignLeft | Qt::AlignBottom, name);
					aPainter->restore();
					return;
				}
				style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, widget);
				const bool selected = (option.state & QStyle::State_Selected) != 0;
				const auto text     = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
				auto       quiet    = text;
				quiet.setAlphaF(selected ? 0.85F : 0.6F);
				auto bold = option.font;
				bold.setWeight(QFont::DemiBold);
				const auto boldMetrics = QFontMetrics{ bold };
				const auto metrics     = QFontMetrics{ option.font };
				const auto area        = option.rect.adjusted(10, 6, -10, -6);
				const auto marker      = aIndex.data(marker_role).toString();
				auto       small       = bold;
				small.setPointSizeF(small.pointSizeF() * 0.85);
				const auto smallMetrics = QFontMetrics{ small };
				const auto markerWidth  = marker.isEmpty() ? 0 : smallMetrics.horizontalAdvance(marker) + 6;
				// The name has the whole first line; where it is used and whether
				// it is changed share the second.
				aPainter->setFont(bold);
				aPainter->setPen(text);
				aPainter->drawText(QRect{ area.left(), area.top(), area.width(), boldMetrics.height() }, Qt::AlignLeft | Qt::AlignVCenter, boldMetrics.elidedText(name, Qt::ElideRight, area.width()));
				const auto second = QRect{ area.left(), area.top() + boldMetrics.height() + 1, area.width(), metrics.height() };
				aPainter->setFont(option.font);
				aPainter->setPen(quiet);
				aPainter->drawText(second.adjusted(0, 0, -markerWidth, 0), Qt::AlignLeft | Qt::AlignVCenter,
								   metrics.elidedText(aIndex.data(usage_role).toString(), Qt::ElideRight, second.width() - markerWidth));
				if (!marker.isEmpty()) {
					aPainter->setFont(small);
					aPainter->setPen(text);
					aPainter->drawText(second, Qt::AlignRight | Qt::AlignVCenter, marker);
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto metrics = QFontMetrics{ aOption.font };
				if (aIndex.data(header_role).toBool()) {
					return { 200, metrics.height() + 12 };
				}
				return { 200, 2 * metrics.height() + 15 };
			}
		};

		[[nodiscard]] auto make_label(QWidget* aParent, const QString& aName, const bool aWrap = true) -> QLabel*
		{
			auto* const label = new QLabel{ aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(aWrap);
			return label;
		}

		[[nodiscard]] auto make_header(QListWidget* aList, const QString& aText) -> QListWidgetItem*
		{
			auto* const item = new QListWidgetItem{ aText, aList };
			item->setData(header_role, true);
			item->setFlags(Qt::NoItemFlags);
			return item;
		}
	}

	writing_hooks_panel::writing_hooks_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("writingHooksPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		auto* const heading = make_label(this, QStringLiteral("writingHooksHeading"), false);
		heading->setText(tr("Built-in prompts"));
		column->addWidget(heading);
		auto* const explanation = make_label(this, QStringLiteral("writingHooksExplanation"));
		explanation->setText(tr("What the agent is told when you ask it for help. Nothing is sent until you ask. Type / to bring in one of your skills."));
		explanation->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(explanation);

		auto* const body = new QHBoxLayout{ };
		body->setSpacing(12);
		Hook = new QListWidget{ this };
		Hook->setObjectName(QStringLiteral("writingHookChoice"));
		Hook->setAccessibleName(tr("Built-in prompts"));
		Hook->setItemDelegate(new prompt_row_delegate{ Hook });
		Hook->setFixedWidth(168);
		Hook->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Hook->setSelectionMode(QAbstractItemView::SingleSelection);
		static_cast<void>(make_header(Hook, tr("Tour tab")));
		for (const auto hook : { core::writing_hook::problem, core::writing_hook::solution, core::writing_hook::outline, core::writing_hook::comment }) {
			auto* const item = new QListWidgetItem{ tr(facts_of(hook).Name), Hook };
			item->setData(hook_role, static_cast<int>(hook));
			item->setData(usage_role, tr(facts_of(hook).Where));
		}
		// [[BPR-1090]] The review-feedback guidance is not a tour field's hook;
		// its own group says where it is used.
		static_cast<void>(make_header(Hook, tr("Comments")));
		{
			auto* const item = new QListWidgetItem{ tr(facts_of(core::writing_hook::feedback).Name), Hook };
			item->setData(hook_role, static_cast<int>(core::writing_hook::feedback));
			item->setData(usage_role, tr(facts_of(core::writing_hook::feedback).Where));
		}
		body->addWidget(Hook, 0);

		// The prompt shown and the review skill's own editor share the pane;
		// only the one shown counts toward its width.
		Pages                  = new QWidget{ this };
		auto* const pageColumn = new QVBoxLayout{ Pages };
		pageColumn->setContentsMargins(0, 0, 0, 0);
		HookPage         = new QWidget{ Pages };
		auto* const page = new QVBoxLayout{ HookPage };
		page->setContentsMargins(0, 0, 0, 0);
		page->setSpacing(6);
		Title = make_label(HookPage, QStringLiteral("writingHookTitle"));
		page->addWidget(Title);
		Purpose = make_label(HookPage, QStringLiteral("writingHookPurpose"));
		page->addWidget(Purpose);
		UsedBy = make_label(HookPage, QStringLiteral("writingHookUsedBy"));
		UsedBy->setForegroundRole(QPalette::PlaceholderText);
		page->addWidget(UsedBy);
		Scope = make_label(HookPage, QStringLiteral("writingHookScope"));
		page->addWidget(Scope);
		// Edit or Preview, and whether this prompt is the built-in one.
		auto* const modeRow = new QHBoxLayout{ };
		EditMode            = new quiet_push_button{ tr("Edit"), HookPage };
		EditMode->setObjectName(QStringLiteral("writingHookEditMode"));
		EditMode->setCheckable(true);
		EditMode->setChecked(true);
		PreviewMode = new quiet_push_button{ tr("Preview"), HookPage };
		PreviewMode->setObjectName(QStringLiteral("writingHookPreviewMode"));
		PreviewMode->setCheckable(true);
		PreviewMode->setToolTip(tr("What a run receives, with the skills it names brought in"));
		auto* const modes = new QButtonGroup{ HookPage };
		modes->setExclusive(true);
		modes->addButton(EditMode);
		modes->addButton(PreviewMode);
		modeRow->addWidget(EditMode);
		modeRow->addWidget(PreviewMode);
		modeRow->addStretch(1);
		Badge = make_label(HookPage, QStringLiteral("writingHookBadge"), false);
		modeRow->addWidget(Badge);
		page->addLayout(modeRow);

		Views        = new QStackedWidget{ HookPage };
		Instructions = new QPlainTextEdit{ Views };
		Instructions->setObjectName(QStringLiteral("writingHookInstructions"));
		Instructions->setAccessibleName(tr("Prompt text"));
		Instructions->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		Instructions->setMinimumHeight(300);
		Instructions->setTabChangesFocus(true);
		Views->addWidget(Instructions);
		Preview = new QPlainTextEdit{ Views };
		Preview->setObjectName(QStringLiteral("writingHookPreview"));
		Preview->setAccessibleName(tr("What the run receives"));
		Preview->setReadOnly(true);
		Preview->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		Preview->setMinimumHeight(300);
		Views->addWidget(Preview);
		page->addWidget(Views, 1);
		Completer = new skill_completer{ Instructions };

		SkillLine = make_label(HookPage, QStringLiteral("writingHookSkills"));
		SkillLine->setForegroundRole(QPalette::PlaceholderText);
		page->addWidget(SkillLine);

		// Two rows: what is unsaved and what to do with it, then what to do
		// with the prompt as saved.
		auto* const editRow = new QHBoxLayout{ };
		SaveState           = make_label(HookPage, QStringLiteral("writingHookSaveState"));
		editRow->addWidget(SaveState, 1);
		Count = make_label(HookPage, QStringLiteral("writingHookCount"), false);
		Count->setForegroundRole(QPalette::PlaceholderText);
		editRow->addWidget(Count, 0, Qt::AlignTop);
		page->addLayout(editRow);
		auto* const buttonRow = new QHBoxLayout{ };
		const auto  quiet     = [&](const QString& aText, const QString& aName) {
			auto* const value = new quiet_push_button{ aText, HookPage };
			value->setObjectName(aName);
			buttonRow->addWidget(value);
			return value;
		};
		ResetButton = quiet(tr("Reset to built-in"), QStringLiteral("writingHookReset"));
		UndoButton  = quiet(tr("Undo reset"), QStringLiteral("writingHookUndo"));
		TestButton  = quiet(tr("Test saved prompt"), QStringLiteral("writingHookTest"));
		buttonRow->addStretch(1);
		page->addLayout(buttonRow);
		DiscardButton = new quiet_push_button{ tr("Discard changes"), HookPage };
		DiscardButton->setObjectName(QStringLiteral("writingHookDiscard"));
		editRow->addWidget(DiscardButton, 0, Qt::AlignTop);
		SaveButton = new QPushButton{ tr("Save"), HookPage };
		SaveButton->setObjectName(QStringLiteral("writingHookSave"));
		editRow->addWidget(SaveButton, 0, Qt::AlignTop);
		Source = make_label(HookPage, QStringLiteral("writingHookSource"));
		Source->setForegroundRole(QPalette::PlaceholderText);
		page->addWidget(Source);
		Status = make_label(HookPage, QStringLiteral("writingHookStatus"));
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse);
		page->addWidget(Status);
		pageColumn->addWidget(HookPage);
		body->addWidget(Pages, 1);
		column->addLayout(body);

		connect(SaveButton, &QPushButton::clicked, this, [this] {
			const auto hook = selected_hook();
			Undo.reset();
			show_notice("Saving the prompt for this profile…");
			if (!Save || !Save(hook, Instructions->toPlainText().toUtf8().toStdString())) {
				show_notice("The prompt could not be saved. Use valid text up to 8192 UTF-8 bytes; your edits are still here.");
			}
			refresh_state();
		});
		connect(ResetButton, &QPushButton::clicked, this, [this] {
			const auto hook  = selected_hook();
			const auto index = static_cast<std::size_t>(hook);
			// Reset puts the built-in text back at once; what it replaced can
			// be brought back as unsaved edits, so nothing is lost by a click.
			auto replaced = Unsaved[index] ? *Unsaved[index] : Settings.Overrides[index];
			Unsaved[index].reset();
			Undo = replaced.empty() ? std::nullopt : std::optional{ std::pair{ hook, std::move(replaced) } };
			if (Save) {
				static_cast<void>(Save(hook, { }));
			}
			show_hook();
			show_notice(Undo ? "Reset to the built-in prompt. Undo reset brings your text back as unsaved edits." : "This prompt is the built-in one.");
		});
		connect(UndoButton, &QPushButton::clicked, this, [this] {
			if (!Undo) {
				return;
			}
			const auto hook                         = Undo->first;
			Unsaved[static_cast<std::size_t>(hook)] = std::move(Undo->second);
			Undo.reset();
			select_hook(hook);
			show_hook();
			show_notice("Your text is back, unsaved. Save keeps it for this profile.");
		});
		connect(DiscardButton, &QPushButton::clicked, this, [this] {
			Unsaved[static_cast<std::size_t>(selected_hook())].reset();
			show_hook();
			show_notice("Unsaved edits discarded.");
		});
		connect(TestButton, &QPushButton::clicked, this, [this] {
			if (Test && selected_hook() != core::writing_hook::feedback) {
				Test(selected_hook());
			}
		});
		connect(Instructions, &QPlainTextEdit::textChanged, this, [this] {
			if (Loading) {
				return;
			}
			const auto hook = selected_hook();
			auto       text = Instructions->toPlainText().toUtf8().toStdString();
			auto&      slot = Unsaved[static_cast<std::size_t>(hook)];
			if (text == saved_text(hook)) {
				slot.reset();
			} else {
				slot = std::move(text);
			}
			refresh_state();
			refresh_rows();
		});
		connect(PreviewMode, &QPushButton::toggled, this, [this](const bool aOn) { show_preview(aOn); });
		connect(Hook, &QListWidget::currentRowChanged, this, [this] { show_hook(); });
		Hook->setCurrentRow(1);
		apply_fonts();
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
			// An edit that is now the saved text is saved.
			for (std::size_t index = 0U; index < Unsaved.size(); ++index) {
				if (Unsaved[index] && *Unsaved[index] == saved_text(static_cast<core::writing_hook>(index))) {
					Unsaved[index].reset();
				}
			}
			show_hook();
			show_notice("Saved. This profile's requests use this prompt now.");
		}
	}

	void writing_hooks_panel::present_label_helper(application::review_label_helper aHelper)
	{
		if (LabelHelper != aHelper) {
			LabelHelper = aHelper;
			show_hook();
		}
	}

	void writing_hooks_panel::present_skills(const application::skill_library& aLibrary)
	{
		if (Skills == aLibrary) {
			return;
		}
		Skills = aLibrary;
		Completer->present(Skills.Catalog);
		refresh_preview();
	}

	void writing_hooks_panel::show_notice(std::string_view aText) { Status->setText(text_of(aText)); }

	auto writing_hooks_panel::selected_hook() const -> core::writing_hook
	{
		const auto* const item = Hook->currentItem();
		const auto        role = item != nullptr ? item->data(hook_role) : QVariant{ };
		return role.isValid() && role.toInt() < static_cast<int>(core::writing_hook_count) ? static_cast<core::writing_hook>(role.toInt()) : core::writing_hook::problem;
	}

	void writing_hooks_panel::select_hook(const core::writing_hook aHook)
	{
		for (int row = 0; row < Hook->count(); ++row) {
			const auto role = Hook->item(row)->data(hook_role);
			if (role.isValid() && role.toInt() == static_cast<int>(aHook)) {
				Hook->setCurrentRow(row);
				return;
			}
		}
	}

	void writing_hooks_panel::adopt_review_skill(QWidget* const aPanel)
	{
		if (aPanel == nullptr || ReviewSkillShown) {
			return;
		}
		ReviewSkillShown = true;
		static_cast<void>(make_header(Hook, tr("Agent tab")));
		auto* const item = new QListWidgetItem{ tr("Review skill"), Hook };
		item->setData(hook_role, review_skill_v);
		item->setData(usage_role, tr("Agent tab, every question"));
		auto* const page = new QWidget{ Pages };
		ReviewPage       = page;
		page->hide();
		auto* const pageColumn = new QVBoxLayout{ page };
		pageColumn->setContentsMargins(0, 0, 0, 0);
		pageColumn->setSpacing(6);
		// [[BPR-1917]], owner look v001 ("why does the review skill render
		// differently than the other ones?"): the same title, purpose, use and
		// state as every other prompt; the review skill's own panel keeps only
		// what is its own (its version offer, its copy to Downloads).
		ReviewTitle = make_label(page, QStringLiteral("reviewSkillTitle"));
		ReviewTitle->setText(tr("Review skill"));
		ReviewTitle->setFont(Title->font());
		pageColumn->addWidget(ReviewTitle);
		auto* const purpose = make_label(page, QStringLiteral("reviewSkillPurpose"));
		purpose->setText(tr("How the agent reviews: the skill bpr-pr-review, sent with every review request, the same for every agent. It grants nothing; the agent's own tools and permissions still govern."));
		pageColumn->addWidget(purpose);
		auto* const usedBy = make_label(page, QStringLiteral("reviewSkillUsedBy"));
		usedBy->setText(tr("Used by every question about the pull request in the Agent tab."));
		usedBy->setForegroundRole(QPalette::PlaceholderText);
		pageColumn->addWidget(usedBy);
		auto* const stateRow = new QHBoxLayout{ };
		stateRow->addStretch(1);
		ReviewBadge = make_label(page, QStringLiteral("reviewSkillBadge"), false);
		stateRow->addWidget(ReviewBadge);
		pageColumn->addLayout(stateRow);
		if (auto* const skill = dynamic_cast<review_skill_panel*>(aPanel); skill != nullptr) {
			skill->show_as_built_in_prompt(Instructions->font());
		}
		aPanel->setParent(page);
		pageColumn->addWidget(aPanel, 1);
		present_review_skill_changed(ReviewChanged);
		Pages->layout()->addWidget(page);
	}

	void writing_hooks_panel::present_review_skill_changed(const bool aChanged)
	{
		ReviewChanged = aChanged;
		if (ReviewBadge == nullptr) {
			return;
		}
		ReviewBadge->setText(aChanged ? tr("Changed from built-in") : tr("Built-in"));
		ReviewBadge->setForegroundRole(aChanged ? QPalette::WindowText : QPalette::PlaceholderText);
		auto badgeFont = font();
		badgeFont.setWeight(aChanged ? QFont::DemiBold : QFont::Normal);
		ReviewBadge->setFont(badgeFont);
		for (int row = 0; row < Hook->count(); ++row) {
			if (Hook->item(row)->data(hook_role).toInt() == review_skill_v) {
				Hook->item(row)->setData(marker_role, aChanged ? tr("Changed") : QString{ });
			}
		}
	}

	void writing_hooks_panel::select_review_skill()
	{
		for (int row = 0; row < Hook->count(); ++row) {
			if (Hook->item(row)->data(hook_role).toInt() == review_skill_v) {
				Hook->setCurrentRow(row);
				return;
			}
		}
	}

	void writing_hooks_panel::show_preview(const bool aPreview)
	{
		const auto blocker = QSignalBlocker{ PreviewMode };
		PreviewMode->setChecked(aPreview);
		EditMode->setChecked(!aPreview);
		Views->setCurrentWidget(aPreview ? static_cast<QWidget*>(Preview) : Instructions);
		refresh_preview();
	}

	auto writing_hooks_panel::unsaved_count() const -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Unsaved, [](const std::optional<std::string>& aText) { return aText.has_value(); }));
	}

	auto writing_hooks_panel::preview_text() const -> QString { return Preview->toPlainText(); }

	void writing_hooks_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::ApplicationFontChange) {
			apply_fonts();
		}
	}

	void writing_hooks_panel::apply_fonts()
	{
		auto heading = QApplication::font(this);
		heading.setWeight(QFont::DemiBold);
		heading.setPointSizeF(heading.pointSizeF() * 1.15);
		if (auto* const label = findChild<QLabel*>(QStringLiteral("writingHooksHeading")); label != nullptr) {
			label->setFont(heading);
		}
		auto title = QApplication::font(this);
		title.setWeight(QFont::DemiBold);
		title.setPointSizeF(title.pointSizeF() * 1.3);
		Title->setFont(title);
		// Prompts are text an agent reads: a fixed-pitch face keeps lists,
		// indentation and Markdown readable, at the body size.
		auto fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		fixed.setPointSizeF(QApplication::font(this).pointSizeF());
		Instructions->setFont(fixed);
		Preview->setFont(fixed);
		if (ReviewTitle != nullptr) {
			ReviewTitle->setFont(title);
		}
	}

	auto writing_hooks_panel::saved_text(const core::writing_hook aHook) const -> std::string { return std::string{ application::effective_writing_hook(Settings, aHook) }; }

	auto writing_hooks_panel::shown_text(const core::writing_hook aHook) const -> std::string
	{
		const auto& unsaved = Unsaved[static_cast<std::size_t>(aHook)];
		return unsaved ? *unsaved : saved_text(aHook);
	}

	void writing_hooks_panel::show_hook()
	{
		const auto* const item = Hook->currentItem();
		if (item != nullptr && item->data(hook_role).toInt() == review_skill_v) {
			HookPage->hide();
			ReviewPage->show();
			return;
		}
		if (ReviewPage != nullptr) {
			ReviewPage->hide();
		}
		HookPage->show();
		const auto  hook  = selected_hook();
		const auto& about = facts_of(hook);
		Title->setText(tr(about.Name));
		Purpose->setText(tr(about.Purpose));
		UsedBy->setText(tr(about.UsedBy));
		{
			Loading            = true;
			const auto text    = shown_text(hook);
			const auto current = Instructions->toPlainText().toUtf8().toStdString();
			if (current != text) {
				Instructions->setPlainText(text_of(text));
			}
			Loading = false;
		}
		const bool feedback = hook == core::writing_hook::feedback;
		Scope->setVisible(feedback);
		if (feedback) {
			// [[BPR-1568]]. Whether it is sent is Comments › Comment
			// workshop's; this line only says so.
			Scope->setText(LabelHelper == application::review_label_helper::on ? tr("On. Sent with each comment workshop request: Comments › Comment workshop.")
																			   : tr("Off. Not sent with comment workshop requests; turn it on in Comments › Comment workshop."));
		}
		TestButton->setEnabled(!feedback);
		TestButton->setToolTip(feedback ? tr("Test uses a tour field. Ask the comment workshop to try this guidance.") : tr("Runs the saved prompt on a sample passage. Nothing is inserted or published."));
		refresh_state();
		refresh_rows();
	}

	void writing_hooks_panel::refresh_state()
	{
		const auto hook    = selected_hook();
		const auto index   = static_cast<std::size_t>(hook);
		const bool changed = !Settings.Overrides[index].empty();
		const bool unsaved = Unsaved[index].has_value();
		const auto version = application::writing_hook_version_of(hook);
		Badge->setText(changed ? tr("Changed from built-in") : tr("Built-in"));
		Badge->setForegroundRole(changed ? QPalette::WindowText : QPalette::PlaceholderText);
		auto badgeFont = font();
		badgeFont.setWeight(changed ? QFont::DemiBold : QFont::Normal);
		Badge->setFont(badgeFont);
		Source->setText(changed ? tr("Changed from built-in %1, profile revision %2").arg(text_of(version)).arg(Settings.Revision)
								: tr("Built-in %1, profile revision %2").arg(text_of(version)).arg(Settings.Revision));
		SaveState->setText(unsaved ? tr("Unsaved changes. They stay here until you save or discard them.") : changed ? tr("Saved for this profile.")
																													 : tr("Using the built-in text."));
		auto stateFont = font();
		stateFont.setWeight(unsaved ? QFont::DemiBold : QFont::Normal);
		SaveState->setFont(stateFont);
		SaveButton->setEnabled(unsaved);
		SaveButton->setDefault(unsaved);
		DiscardButton->setEnabled(unsaved);
		ResetButton->setEnabled(changed || unsaved);
		UndoButton->setVisible(Undo.has_value());
		const auto bytes = Instructions->toPlainText().toUtf8().size();
		Count->setText(tr("%1 of %2 bytes").arg(QLocale{ }.toString(bytes)).arg(QLocale{ }.toString(static_cast<qlonglong>(core::maximum_writing_instruction_bytes))));
		Count->setForegroundRole(bytes > static_cast<qsizetype>(core::maximum_writing_instruction_bytes) ? QPalette::Link : QPalette::PlaceholderText);
		refresh_preview();
	}

	void writing_hooks_panel::refresh_rows()
	{
		for (int row = 0; row < Hook->count(); ++row) {
			auto* const item = Hook->item(row);
			const auto  role = item->data(hook_role);
			if (!role.isValid() || role.toInt() >= static_cast<int>(core::writing_hook_count)) {
				continue;
			}
			const auto index = static_cast<std::size_t>(role.toInt());
			item->setData(marker_role, Unsaved[index] ? tr("Unsaved") : !Settings.Overrides[index].empty() ? tr("Changed")
																										   : QString{ });
		}
	}

	void writing_hooks_panel::refresh_preview()
	{
		const auto hook    = selected_hook();
		const auto text    = Instructions->toPlainText().toUtf8().toStdString();
		const auto preview = application::preview_writing_hook(hook, text, Skills);
		if (Views->currentWidget() == Preview) {
			Preview->setPlainText(text_of(preview.Text));
			// Open on the person's own part: the contract above it is the
			// application's, the same for every prompt.
			if (const auto found = Preview->document()->find(QStringLiteral("Selected writing instructions:")); !found.isNull()) {
				Preview->setTextCursor(found);
				Preview->ensureCursorVisible();
			}
		}
		auto line = QString{ };
		if (!preview.Guidance.Skills.empty()) {
			auto names = QStringList{ };
			for (const auto index : preview.Guidance.Skills) {
				const auto& entry = Skills.Catalog[index];
				names.push_back(QStringLiteral("/%1 (%2, %3)").arg(text_of(entry.Name), entry.Source == core::skill_source::personal ? tr("personal") : tr("repository"), text_of(entry.Origin)));
			}
			line = hook == core::writing_hook::feedback
				? tr("Names %1. Skills aren't brought into comment workshop requests yet.").arg(names.join(QStringLiteral(", ")))
				: tr("Brings in %1. Its SKILL.md is sent with the request; only skills a prompt names are sent.").arg(names.join(QStringLiteral(", ")));
		} else if (Skills.Catalog.empty()) {
			line = tr("No skills found in ~/.claude/skills, ~/.agents/skills or ~/.codex/skills.");
		} else {
			line = tr("Type / to bring in one of %n skills.", nullptr, static_cast<int>(Skills.Catalog.size()));
		}
		if (preview.Guidance.Note) {
			line += QStringLiteral(" ") + text_of(*preview.Guidance.Note);
		}
		if (Skills.RepositoryNote) {
			line += QStringLiteral(" ") + text_of(*Skills.RepositoryNote);
		}
		SkillLine->setText(line);
	}
}
