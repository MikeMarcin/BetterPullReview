#include "bpr/qt/local_draft_workflow_area.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "wrapping_row.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// The indent that puts a row's second line under its name, past the mark.
		constexpr int row_indent = 24;
		constexpr int mark_size  = 16;
		/// The longest publisher and agent names a button shows whole: a
		/// button does not elide, and no drawer pans sideways (owner, look
		/// workflow-v002). The whole name is in the button's tooltip.
		constexpr qsizetype publisher_name_length = 28;
		constexpr qsizetype agent_name_length     = 20;

		[[nodiscard]] auto shortened(const QString& aName, const qsizetype aLength) -> QString
		{
			return aName.size() > aLength ? aName.left(aLength - 1).trimmed() + QStringLiteral("…") : aName;
		}

		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto label(const QString& aText, QWidget* aParent, const QString& aName = { }) -> QLabel*
		{
			auto* const line = new QLabel{ aText, aParent };
			line->setWordWrap(true);
			line->setTextInteractionFlags(Qt::TextSelectableByMouse);
			if (!aName.isEmpty()) {
				line->setObjectName(aName);
			}
			return line;
		}

		[[nodiscard]] auto secondary(QLabel* aLabel) -> QLabel*
		{
			auto palette = aLabel->palette();
			palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
			aLabel->setPalette(palette);
			return aLabel;
		}

		[[nodiscard]] auto weighted(QLabel* aLabel, const QFont::Weight aWeight) -> QLabel*
		{
			auto font = aLabel->font();
			font.setWeight(aWeight);
			aLabel->setFont(font);
			return aLabel;
		}

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }
		[[nodiscard]] auto success_ink(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 63, 185, 80 } : QColor{ 26, 127, 55 }; }
		[[nodiscard]] auto danger_ink(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 248, 81, 73 } : QColor{ 207, 34, 46 }; }

		/// \brief A checklist row's mark: filled is seen by BetterPullReview,
		/// outlined is reported, dotted waits, a ring has not run.
		class checklist_mark_view final : public QWidget
		{
		public:
			checklist_mark_view(const core::checklist_mark aMark, QWidget* aParent)
				: QWidget{ aParent }
				, Mark{ aMark }
			{
				setFixedSize(mark_size, mark_size);
				setObjectName(QStringLiteral("localDraftChecklistMark"));
				setProperty("bprMark", text_of(core::stable_name(aMark)));
				setAccessibleName(accessible());
			}

			void set_mark(const core::checklist_mark aMark)
			{
				Mark = aMark;
				setProperty("bprMark", text_of(core::stable_name(aMark)));
				setAccessibleName(accessible());
				update();
			}

		protected:
			void paintEvent(QPaintEvent* aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto  box     = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
				const auto  quiet   = palette().color(QPalette::PlaceholderText);
				const auto  good    = success_ink(palette());
				const auto  bad     = danger_ink(palette());
				const auto& surface = palette().color(QPalette::Base);
				switch (Mark) {
				case core::checklist_mark::not_yet:
					painter.setPen(QPen{ quiet, 1.4 });
					painter.drawEllipse(box);
					return;
				case core::checklist_mark::waiting: {
					auto pen = QPen{ quiet, 1.4 };
					pen.setStyle(Qt::DotLine);
					painter.setPen(pen);
					painter.drawEllipse(box);
					return;
				}
				case core::checklist_mark::in_hand:
					painter.setPen(QPen{ quiet, 1.4 });
					painter.drawEllipse(box);
					painter.setBrush(quiet);
					painter.setPen(Qt::NoPen);
					painter.drawEllipse(box.center(), 2.5, 2.5);
					return;
				case core::checklist_mark::observed:
					painter.setPen(Qt::NoPen);
					painter.setBrush(good);
					painter.drawEllipse(box);
					tick(painter, box, surface);
					return;
				case core::checklist_mark::reported:
					painter.setPen(QPen{ good, 1.4 });
					painter.drawEllipse(box);
					tick(painter, box, good);
					return;
				case core::checklist_mark::reported_unmet:
					painter.setPen(QPen{ bad, 1.4 });
					painter.drawEllipse(box);
					cross(painter, box, bad);
					return;
				}
			}

		private:
			[[nodiscard]] auto accessible() const -> QString
			{
				switch (Mark) {
				case core::checklist_mark::not_yet:        return QObject::tr("Not run");
				case core::checklist_mark::waiting:        return QObject::tr("Waiting");
				case core::checklist_mark::in_hand:        return QObject::tr("Under way");
				case core::checklist_mark::observed:       return QObject::tr("Done, seen by BetterPullReview");
				case core::checklist_mark::reported:       return QObject::tr("Reported done");
				case core::checklist_mark::reported_unmet: return QObject::tr("Reported not met");
				}
				return { };
			}

			static void tick(QPainter& aPainter, const QRectF& aBox, const QColor& aInk)
			{
				aPainter.setPen(QPen{ aInk, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				const auto unit = aBox.width() / 12.0;
				aPainter.drawPolyline(QList<QPointF>{ aBox.topLeft() + QPointF{ 3.2 * unit, 6.3 * unit }, aBox.topLeft() + QPointF{ 5.2 * unit, 8.3 * unit },
													  aBox.topLeft() + QPointF{ 8.8 * unit, 4.2 * unit } });
			}

			static void cross(QPainter& aPainter, const QRectF& aBox, const QColor& aInk)
			{
				aPainter.setPen(QPen{ aInk, 1.6, Qt::SolidLine, Qt::RoundCap });
				const auto inset = aBox.width() * 0.32;
				const auto inner = aBox.adjusted(inset, inset, -inset, -inset);
				aPainter.drawLine(inner.topLeft(), inner.bottomRight());
				aPainter.drawLine(inner.topRight(), inner.bottomLeft());
			}

			core::checklist_mark Mark;
		};

		/// Who runs a row and what it says it does, said once in the glyph's tooltip.
		[[nodiscard]] auto runner_text(const core::checklist_runner aRunner, const QString& aAgent) -> QString
		{
			switch (aRunner) {
			case core::checklist_runner::agent:
				static_cast<void>(aAgent);
				return QObject::tr("Asked in BetterPullReview's read-only writing lane.");
			case core::checklist_runner::companion:
				return QObject::tr("You run it. BetterPullReview does not start or restrict it.");
			case core::checklist_runner::betterpullreview:
				return QObject::tr("BetterPullReview does this and sees it itself.");
			case core::checklist_runner::author:
				return QObject::tr("You do this.");
			}
			return { };
		}

		[[nodiscard]] auto runner_glyph(const core::checklist_runner aRunner) -> QString
		{
			switch (aRunner) {
			case core::checklist_runner::agent:            return QStringLiteral("✦");
			case core::checklist_runner::companion:        return QStringLiteral("⌨");
			case core::checklist_runner::betterpullreview: return QString{ };
			case core::checklist_runner::author:           return QString{ };
			}
			return { };
		}

		/// What an action declares, never a restriction BPR enforces on a program.
		[[nodiscard]] auto declared_text(const core::workflow_action& aAction) -> QString
		{
			auto effects = QStringList{ };
			for (const auto effect : aAction.Effects) {
				effects << text_of(core::describe(effect));
			}
			return effects.isEmpty() ? QString{ } : QObject::tr("It says it %1.").arg(effects.join(QStringLiteral(", ")));
		}

		[[nodiscard]] auto finding_lines(const core::workflow_action_record& aRecord) -> QStringList
		{
			auto lines = QStringList{ };
			for (const auto& finding : aRecord.Findings) {
				if (finding.State == core::workflow_finding_state::met && finding.Detail.empty()) {
					continue;
				}
				lines << (finding.Detail.empty() ? text_of(finding.Check) : text_of(finding.Detail));
			}
			for (const auto& limitation : aRecord.Limitations) {
				lines << text_of(core::describe(limitation.Kind)) + (limitation.Detail.empty() ? QString{ } : QStringLiteral(" (") + text_of(limitation.Detail) + QLatin1Char(')'));
			}
			for (const auto& reference : aRecord.References) {
				lines << QObject::tr("%1 %2: %3")
							 .arg(text_of(core::stable_name(reference.Kind)).replace(QLatin1Char('_'), QLatin1Char(' ')), text_of(reference.Id), text_of(core::stable_name(reference.State)));
			}
			if (lines.isEmpty() && !aRecord.Message.empty()) {
				lines << text_of(aRecord.Message);
			}
			return lines;
		}

		[[nodiscard]] auto refusal_text(const core::workflow_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::workflow_refusal::stale:            return QObject::tr("The draft changed meanwhile; try again.");
			case core::workflow_refusal::unknown:          return QObject::tr("That workflow or action is no longer here.");
			case core::workflow_refusal::state:            return QObject::tr("That is not possible in the action's current state.");
			case core::workflow_refusal::mismatched:       return QObject::tr("The answer is for another handoff or draft; nothing was taken from it.");
			case core::workflow_refusal::invalid:          return QObject::tr("The answer has something BetterPullReview cannot record.");
			case core::workflow_refusal::publication_open: return QObject::tr("A pull request is being created, is uncertain or is linked already.");
			case core::workflow_refusal::no_plan:          return QObject::tr("Check the plan on GitHub first, then hand off the plan shown.");
			case core::workflow_refusal::checks_unmet:     return QObject::tr("A required check is not met, so its publisher is not handed the plan.");
			case core::workflow_refusal::not_linked:       return QObject::tr("The pull request is not linked to this draft yet.");
			case core::workflow_refusal::other_publisher:  return QObject::tr("The built-in publisher is chosen for this draft.");
			case core::workflow_refusal::external_open:    return QObject::tr("A publication handoff is unresolved: check GitHub before anything else.");
			}
			return { };
		}

		/// A row's short state, in the author's words.
		[[nodiscard]] auto short_state(const core::checklist_row& aRow, const application::local_draft_workflow_action_view* aAction) -> QString
		{
			switch (aRow.Wait) {
			case core::checklist_wait::pull_request:   return QObject::tr("After the pull request exists");
			case core::checklist_wait::required_check: return QObject::tr("Waits for the required check");
			case core::checklist_wait::worktree:       return QObject::tr("Author works in your worktree");
			case core::checklist_wait::none:           break;
			}
			if (aAction != nullptr && aAction->Prerequisite != core::workflow_prerequisite::ready) {
				return text_of(core::describe(aAction->Prerequisite));
			}
			const auto* const record = aAction != nullptr && aAction->Record ? &*aAction->Record : nullptr;
			switch (aRow.Mark) {
			case core::checklist_mark::not_yet: return aRow.Id == "branch" ? QObject::tr("Not pushed as this candidate") : QObject::tr("Not run");
			case core::checklist_mark::waiting: return QObject::tr("Waiting");
			case core::checklist_mark::in_hand:
				if (record != nullptr && record->State == core::workflow_action_state::awaiting_input) {
					return QObject::tr("It asks: %1").arg(text_of(record->Question));
				}
				return aRow.Runner == core::checklist_runner::agent ? QObject::tr("Asked") : QObject::tr("Handed off");
			case core::checklist_mark::observed:
				if (aRow.Runner == core::checklist_runner::agent) {
					return QObject::tr("Answered");
				}
				return aRow.Id == "branch" ? QObject::tr("Seen on GitHub") : QObject::tr("Done");
			case core::checklist_mark::reported: return QObject::tr("Reported");
			case core::checklist_mark::reported_unmet:
				if (record != nullptr && record->State == core::workflow_action_state::uncertain) {
					return QObject::tr("Reported: uncertain");
				}
				if (record != nullptr && (record->State == core::workflow_action_state::failed || record->State == core::workflow_action_state::interrupted)) {
					return QObject::tr("Reported: failed");
				}
				return QObject::tr("Reported: not met");
			}
			return { };
		}
	} // namespace

	auto workflow_state_text(const application::local_draft_workflow_action_view& aAction) -> QString
	{
		if (aAction.Prerequisite != core::workflow_prerequisite::ready) {
			return text_of(core::describe(aAction.Prerequisite)) + QLatin1Char('.');
		}
		if (!aAction.Record || aAction.Record->Invocation == 0U) {
			return QObject::tr("Not run on this draft.");
		}
		const auto& record   = *aAction.Record;
		const auto  reported = record.Message.empty() ? QString{ } : QStringLiteral(" “") + text_of(record.Message) + QStringLiteral("”");
		switch (record.State) {
		case core::workflow_action_state::idle:           return QObject::tr("Reconciled; it can be handed off again.");
		case core::workflow_action_state::handed_off:     return QObject::tr("Handed off; waiting for its answer.");
		case core::workflow_action_state::awaiting_input: return QObject::tr("It asks: %1").arg(text_of(record.Question));
		case core::workflow_action_state::reported:
			return (record.Reconciled ? QObject::tr("Reported by the companion, and read on GitHub.") : QObject::tr("Reported by the companion; not observed by BetterPullReview.")) + reported;
		case core::workflow_action_state::failed:      return QObject::tr("The companion reported a failure.") + reported;
		case core::workflow_action_state::interrupted: return QObject::tr("The companion reported it was stopped.") + reported;
		case core::workflow_action_state::uncertain:   return QObject::tr("Uncertain: nobody knows what happened yet.") + reported;
		}
		return { };
	}

	local_draft_workflow_area::local_draft_workflow_area(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftWorkflowArea"));
		setAccessibleName(QObject::tr("Workflow"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 10, 12, 12);
		column->setSpacing(10);

		// The headline: where the pull request stands, and what is left.
		auto* const top = new QHBoxLayout{ };
		top->setSpacing(8);
		HeadlineMark = new QLabel{ this };
		HeadlineMark->setFixedWidth(mark_size);
		top->addWidget(HeadlineMark);
		// Both wrap: no drawer pans sideways (owner, look workflow-v002).
		Headline = weighted(label({ }, this, QStringLiteral("localDraftWorkflowHeadline")), QFont::DemiBold);
		top->addWidget(Headline, 1);
		HeadlineAside = secondary(label({ }, this, QStringLiteral("localDraftWorkflowHeadlineAside")));
		HeadlineAside->setAlignment(Qt::AlignRight | Qt::AlignTop);
		top->addWidget(HeadlineAside, 0, Qt::AlignTop);
		column->addLayout(top);

		Summary = label({ }, this, QStringLiteral("localDraftWorkflowSummary"));
		Summary->hide();
		column->addWidget(Summary);

		PublisherRow             = new QWidget{ this };
		auto* const publisherRow = new detail::wrapping_row{ PublisherRow, 10, 4 };
		publisherRow->addWidget(secondary(new QLabel{ QObject::tr("Created by"), PublisherRow }));
		BuiltIn = new QRadioButton{ QObject::tr("BetterPullReview"), PublisherRow };
		BuiltIn->setObjectName(QStringLiteral("localDraftWorkflowPublisherBuiltIn"));
		BuiltIn->setToolTip(QObject::tr("It does not run this workflow's checks."));
		External = new QRadioButton{ PublisherRow };
		External->setObjectName(QStringLiteral("localDraftWorkflowPublisherExternal"));
		PublisherGroup = new QButtonGroup{ PublisherRow };
		PublisherGroup->addButton(BuiltIn, 0);
		PublisherGroup->addButton(External, 1);
		publisherRow->addWidget(BuiltIn);
		publisherRow->addWidget(External);
		PublisherRow->hide();

		Actions = new QWidget{ this };
		Actions->setObjectName(QStringLiteral("localDraftWorkflowActions"));
		Rows = new QVBoxLayout{ Actions };
		Rows->setContentsMargins(0, 0, 0, 0);
		Rows->setSpacing(10);
		column->addWidget(Actions);

		// The workflow itself, quiet at the foot: which one, and its revision.
		Footer                = new QWidget{ this };
		auto* const footerRow = new detail::wrapping_row{ Footer, 8, 4 };
		footerRow->setContentsMargins(0, 6, 0, 0);
		Chooser = new QComboBox{ Footer };
		Chooser->setObjectName(QStringLiteral("localDraftWorkflowChooser"));
		Chooser->setAccessibleName(QObject::tr("Workflow for this draft"));
		Chooser->setToolTip(QObject::tr("Optional, and selecting one runs nothing."));
		// The full name when there is room; elided, never wider, when not.
		Chooser->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		Chooser->setMinimumWidth(96);
		footerRow->addWidget(Chooser);
		Revision = secondary(label({ }, Footer, QStringLiteral("localDraftWorkflowRevision")));
		Revision->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		footerRow->addWidget(Revision);
		Update = new QPushButton{ QObject::tr("Use the Newer Revision"), Footer };
		Update->setObjectName(QStringLiteral("localDraftWorkflowUpdate"));
		Update->hide();
		footerRow->addWidget(Update);
		column->addWidget(Footer);

		connect(Chooser, &QComboBox::activated, this, [this](const int aIndex) {
			if (Presenting || !Dispatch.Select) {
				return;
			}
			const auto id = Chooser->itemData(aIndex).toString();
			Dispatch.Select(id.isEmpty() ? std::nullopt : std::optional{ id.toStdString() });
		});
		connect(Update, &QPushButton::clicked, this, [this] {
			if (Dispatch.AcceptUpdate) {
				Dispatch.AcceptUpdate();
			}
		});
		connect(PublisherGroup, &QButtonGroup::idClicked, this, [this](const int aId) {
			if (!Presenting && Dispatch.Publisher) {
				Dispatch.Publisher(aId == 1 ? core::workflow_publisher::external : core::workflow_publisher::built_in);
			}
		});
	}

	void local_draft_workflow_area::set_dispatch(local_draft_workflow_dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void local_draft_workflow_area::set_worktree_dispatch(std::function<void()> aChoose) { ChooseWorktree = std::move(aChoose); }

	void local_draft_workflow_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection = aProjection;
		present_footer();
		rebuild();
	}

	void local_draft_workflow_area::present_checklist(const core::author_checklist& aChecklist, const core::author_case aCase)
	{
		if (Checklist == aChecklist && Case == aCase) {
			return;
		}
		Checklist = aChecklist;
		Case      = aCase;
		present_footer();
		rebuild();
	}

	void local_draft_workflow_area::present_footer()
	{
		const auto& view  = Projection.Workflow;
		const auto* draft = Projection.Draft ? &*Projection.Draft : nullptr;
		const auto  guard = QSignalBlocker{ Chooser };
		Presenting        = true;
		Chooser->clear();
		Chooser->addItem(QObject::tr("No workflow"), QString{ });
		for (const auto& option : view.Available) {
			Chooser->addItem(text_of(option.Name), text_of(option.Id));
			Chooser->setItemData(Chooser->count() - 1, option.Origin.empty() ? QString{ } : QObject::tr("From %1").arg(text_of(option.Origin)), Qt::ToolTipRole);
		}
		const auto* selected = draft != nullptr && draft->Workflow ? &*draft->Workflow : nullptr;
		if (selected != nullptr) {
			auto index = Chooser->findData(text_of(selected->Workflow.PortableId));
			if (index < 0) {
				Chooser->addItem(QObject::tr("%1 (no longer in Settings)").arg(text_of(selected->Workflow.Name)), text_of(selected->Workflow.PortableId));
				index = Chooser->count() - 1;
			}
			Chooser->setCurrentIndex(index);
		} else {
			Chooser->setCurrentIndex(0);
		}
		// With nothing to choose the picker is one quiet line, not a control.
		Chooser->setEnabled(draft != nullptr && (selected != nullptr || !view.Available.empty()));
		Chooser->setVisible(draft != nullptr);
		Update->setVisible(view.UpdateAvailable);
		Revision->setText(selected == nullptr ? (view.Available.empty() ? QObject::tr("None is set up. Changes, Description and Tour need none.") : QString{ })
											  : QObject::tr("From %1, %2").arg(selected->Workflow.Origin.empty() ? QObject::tr("an unnamed source") : text_of(selected->Workflow.Origin), text_of(selected->Revision.substr(0U, 7U))));
		Revision->setToolTip(selected == nullptr ? QString{ } : QObject::tr("Revision %1").arg(text_of(selected->Revision.substr(0U, 40U))));
		const auto hasPublisher = selected != nullptr
			&& std::ranges::any_of(selected->Workflow.Actions, [](const core::workflow_action& aAction) { return aAction.Slot == core::workflow_slot::publish_draft; });
		if (hasPublisher) {
			const auto publish = std::ranges::find(selected->Workflow.Actions, core::workflow_slot::publish_draft, &core::workflow_action::Slot);
			// A radio button does not elide: a long name is shortened here, whole in its tooltip.
			const auto publisher = text_of(selected->Workflow.Origin.empty() ? publish->Task : selected->Workflow.Origin);
			External->setText(shortened(publisher, publisher_name_length));
			External->setToolTip(publisher.size() > publisher_name_length ? QObject::tr("%1. Handed to your terminal once its checks are met.").arg(publisher)
																		  : QObject::tr("Handed to your terminal once its checks are met."));
			(selected->Publisher == core::workflow_publisher::external ? External : BuiltIn)->setChecked(true);
		}
		PublisherShown = hasPublisher && Case == core::author_case::local_draft;
		auto notes     = QStringList{ };
		if (view.Refusal) {
			notes << refusal_text(*view.Refusal);
		}
		if (!view.Failure.empty()) {
			notes << text_of(view.Failure);
		}
		Summary->setText(notes.join(QLatin1Char('\n')));
		Summary->setVisible(!notes.isEmpty());
		Presenting = false;
	}

	void local_draft_workflow_area::rebuild()
	{
		while (auto* const item = Rows->takeAt(0)) {
			if (auto* const widget = item->widget(); widget == PublisherRow) {
				// The publisher choice is the tab's own; it only visits the rows.
				widget->hide();
				widget->setParent(this);
			} else if (widget != nullptr) {
				// A button's own click may be presenting: it goes once that returns.
				widget->hide();
				widget->setParent(nullptr);
				widget->deleteLater();
			}
			delete item;
		}
		const auto& view     = Projection.Workflow;
		const auto  linked   = Projection.Draft && Projection.Draft->Publication && Projection.Draft->Publication->Journal.Phase == core::local_publication_phase::linked;
		const auto  external = Projection.Draft && Projection.Draft->Workflow && Projection.Draft->Workflow->Publisher == core::workflow_publisher::external;
		const auto  digest   = Projection.Publishing.Digest;

		// The headline.
		const auto onGithub = std::ranges::any_of(Checklist.Rows, [](const core::checklist_row& aRow) {
			return (aRow.Id == "create" || aRow.Id == "pull-request") && aRow.Mark == core::checklist_mark::observed;
		});
		switch (Case) {
		case core::author_case::local_draft:        Headline->setText(onGithub || linked ? QObject::tr("On GitHub") : QObject::tr("Not on GitHub")); break;
		case core::author_case::draft_pull_request: Headline->setText(QObject::tr("Draft pull request")); break;
		case core::author_case::open_pull_request:  Headline->setText(QObject::tr("Revising my pull request")); break;
		}
		HeadlineAside->setText(Checklist.RequiredUnmet > 0U ? (Checklist.RequiredUnmet == 1U ? QObject::tr("1 check not met") : QObject::tr("%1 checks not met").arg(Checklist.RequiredUnmet))
								   : Checklist.Ready        ? QObject::tr("Ready")
															: QString{ });
		auto asidePalette = HeadlineAside->palette();
		asidePalette.setColor(QPalette::WindowText, Checklist.RequiredUnmet > 0U ? danger_ink(palette()) : palette().color(QPalette::PlaceholderText));
		HeadlineAside->setPalette(asidePalette);
		auto* const headMark = static_cast<checklist_mark_view*>(HeadlineMark->findChild<QWidget*>(QStringLiteral("localDraftChecklistMark"), Qt::FindDirectChildrenOnly));
		const auto  headKind = onGithub || linked ? core::checklist_mark::observed : Checklist.RequiredUnmet > 0U ? core::checklist_mark::reported_unmet
																												  : core::checklist_mark::not_yet;
		if (headMark == nullptr) {
			new checklist_mark_view{ headKind, HeadlineMark };
		} else {
			headMark->set_mark(headKind);
		}

		auto section = std::optional<core::checklist_section>{ };
		for (const auto& entry : Checklist.Rows) {
			// One heading per section; follow-up sits under publishing.
			const auto heading = entry.Section == core::checklist_section::follow_up ? core::checklist_section::publish : entry.Section;
			if (section != heading) {
				section             = heading;
				const auto  title   = heading == core::checklist_section::prepare ? QObject::tr("Prepare")
					: Case == core::author_case::local_draft                      ? QObject::tr("Create draft pull request")
																				  : QObject::tr("On GitHub");
				auto* const caption = secondary(weighted(label(title, Actions), QFont::DemiBold));
				caption->setContentsMargins(0, 6, 0, 0);
				Rows->addWidget(caption);
				if (heading == core::checklist_section::publish && PublisherShown) {
					// The publisher choice heads publishing, where it applies.
					Rows->addWidget(PublisherRow);
					PublisherRow->show();
				}
			}
			const auto  found  = std::ranges::find(view.Actions, entry.Id, [](const application::local_draft_workflow_action_view& aView) { return aView.Action.Id; });
			const auto* action = found == view.Actions.end() ? nullptr : &*found;
			const auto  id     = entry.Id;
			auto* const group  = new QWidget{ Actions };
			group->setObjectName(action != nullptr ? QStringLiteral("localDraftWorkflowAction_") + text_of(id) : QStringLiteral("localDraftChecklist_") + text_of(id));
			group->setAccessibleName(text_of(entry.Task));
			group->setProperty("bprMark", text_of(core::stable_name(entry.Mark)));
			auto* const rows = new QVBoxLayout{ group };
			rows->setContentsMargins(0, 0, 0, 0);
			rows->setSpacing(3);

			auto* const first = new QHBoxLayout{ };
			first->setSpacing(8);
			first->addWidget(new checklist_mark_view{ entry.Mark, group }, 0, Qt::AlignTop);
			auto* const task = weighted(label(text_of(entry.Task), group, QStringLiteral("localDraftWorkflowTask")), QFont::Medium);
			if (action != nullptr) {
				task->setToolTip(declared_text(action->Action));
			}
			const auto waiting = entry.Mark == core::checklist_mark::waiting;
			if (waiting) {
				static_cast<void>(secondary(task));
			}
			first->addWidget(task, 1);
			if (entry.Required) {
				auto* const required = secondary(new QLabel{ QObject::tr("Required"), group });
				required->setObjectName(QStringLiteral("localDraftWorkflowRequired"));
				required->setToolTip(QObject::tr("The workflow's own publisher requires it before it acts."));
				first->addWidget(required, 0, Qt::AlignTop);
			}
			if (entry.Id == "branch" && entry.Mark == core::checklist_mark::observed) {
				first->addWidget(secondary(new QLabel{ QObject::tr("Candidate %1").arg(Projection.Shown), group }), 0, Qt::AlignTop);
			}
			rows->addLayout(first);

			// The second line: who runs it, where it stands, one button.
			// It wraps inside the drawer: the state first, the buttons after
			// it, or below it when the row is narrow.
			auto* const second = new detail::wrapping_row{ nullptr, 6, 4 };
			second->setContentsMargins(row_indent, 0, 0, 0);
			auto* const where   = new QWidget{ group };
			auto* const whereAt = new QHBoxLayout{ where };
			whereAt->setContentsMargins(0, 0, 0, 0);
			whereAt->setSpacing(6);
			where->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
			second->addWidget(where);
			if (const auto glyph = runner_glyph(entry.Runner); !glyph.isEmpty()) {
				auto* const who = new QLabel{ glyph, group };
				who->setObjectName(QStringLiteral("localDraftWorkflowRunner"));
				who->setToolTip(runner_text(entry.Runner, action != nullptr ? text_of(action->AgentName) : QString{ }));
				who->setAccessibleName(who->toolTip());
				auto whoPalette = who->palette();
				whoPalette.setColor(QPalette::WindowText, entry.Runner == core::checklist_runner::agent ? agent_accent(palette()) : palette().color(QPalette::PlaceholderText));
				who->setPalette(whoPalette);
				whereAt->addWidget(who, 0, Qt::AlignTop);
			}
			auto* const state = label(short_state(entry, action), group, QStringLiteral("localDraftWorkflowState"));
			if (entry.Mark == core::checklist_mark::reported_unmet) {
				auto statePalette = state->palette();
				statePalette.setColor(QPalette::WindowText, danger_ink(palette()));
				state->setPalette(statePalette);
			} else {
				static_cast<void>(secondary(state));
			}
			auto tip = QStringList{ };
			if (action != nullptr) {
				tip << workflow_state_text(*action);
				// The selected context the request carried, and where it is.
				if (action->Record && action->Record->Invocation != 0U && !action->RequestPath.empty() && Projection.Draft) {
					const auto& draft = *Projection.Draft;
					const auto* shown = core::latest_candidate(draft);
					tip << QObject::tr("Sent candidate %1: the title, the description, the tour's problem and solution, %2 notes and %3 changed paths%4. In %5.")
							   .arg(action->Record->Candidate)
							   .arg(draft.Notes.size())
							   .arg(shown == nullptr ? 0U : shown->Changes.size())
							   .arg(action->Record->PlanDigest.empty() ? QString{ } : QObject::tr(", and the plan %1").arg(text_of(action->Record->PlanDigest.substr(0U, 12U))))
							   .arg(text_of(action->RequestPath));
				}
			}
			state->setToolTip(tip.join(QLatin1Char('\n')));
			whereAt->addWidget(state, 1);

			const auto button = [this, group, second](const QString& aText, const QString& aName, auto aDo) {
				auto* const control = new QPushButton{ aText, group };
				control->setObjectName(aName);
				connect(control, &QPushButton::clicked, this, aDo);
				second->addWidget(control);
				return control;
			};
			if (entry.Id == "worktree" && ChooseWorktree) {
				button(QObject::tr("Choose Worktree…"), QStringLiteral("authorChooseWorktree"), [this] { ChooseWorktree(); })
					->setToolTip(QObject::tr("Where this pull request's branch is checked out."));
			}
			const auto recordState = action != nullptr && action->Record ? action->Record->State : core::workflow_action_state::idle;
			const auto ready       = action != nullptr && action->Prerequisite == core::workflow_prerequisite::ready;
			if (action != nullptr) {
				switch (action->Action.Slot) {
				case core::workflow_slot::gather_context:
				case core::workflow_slot::propose_description:
				case core::workflow_slot::check_preparation:
					if (action->Action.Binding == core::workflow_binding::agent) {
						const auto  agent = action->AgentName.empty() ? QObject::tr("the Agent") : text_of(action->AgentName);
						auto* const ask   = button(QObject::tr("Ask %1").arg(shortened(agent, agent_name_length)), QStringLiteral("localDraftWorkflowAsk"), [this, id] {
							if (Dispatch.Ask) {
								Dispatch.Ask(id);
							}
						});
						ask->setEnabled(ready && !Projection.Asking && Projection.AgentAvailable);
						if (agent.size() > agent_name_length) {
							ask->setToolTip(QObject::tr("Ask %1").arg(agent));
						}
						wear_agent_accent(*ask, palette());
					} else if (recordState != core::workflow_action_state::handed_off && recordState != core::workflow_action_state::awaiting_input) {
						button(action->Record && action->Record->Invocation != 0U ? QObject::tr("Hand Off Again") : QObject::tr("Hand Off"), QStringLiteral("localDraftWorkflowHandOff"), [this, id] {
							if (Dispatch.HandOff) {
								Dispatch.HandOff(id, { });
							}
						})->setEnabled(ready);
					}
					break;
				case core::workflow_slot::publish_draft:
					if (!linked && recordState != core::workflow_action_state::handed_off) {
						button(QObject::tr("Hand Off Plan"), QStringLiteral("localDraftWorkflowPublish"), [this, id, digest] {
							if (Dispatch.Publish) {
								Dispatch.Publish(id, digest);
							}
						})->setEnabled(ready && external && !digest.empty());
					}
					if (recordState != core::workflow_action_state::idle && action->Record && !action->Record->Reconciled && !linked) {
						button(QObject::tr("Check GitHub"), QStringLiteral("localDraftWorkflowReconcile"), [this, id] {
							if (Dispatch.Reconcile) {
								Dispatch.Reconcile(id);
							}
						});
					}
					break;
				case core::workflow_slot::finish_publication:
					if (linked && recordState != core::workflow_action_state::handed_off) {
						button(QObject::tr("Hand Off"), QStringLiteral("localDraftWorkflowFollowUp"), [this, id] {
							if (Dispatch.FollowUp) {
								Dispatch.FollowUp(id);
							}
						})->setEnabled(ready);
					}
					if (action->Record && action->Record->Invocation != 0U && !action->Record->Reconciled && recordState != core::workflow_action_state::awaiting_input) {
						button(QObject::tr("I Checked What It Made"), QStringLiteral("localDraftWorkflowReconcileFollowUp"), [this, id] {
							if (Dispatch.ReconcileFollowUp) {
								Dispatch.ReconcileFollowUp(id);
							}
						});
					}
					break;
				}
				if (action->Action.Binding == core::workflow_binding::command && (recordState == core::workflow_action_state::handed_off || recordState == core::workflow_action_state::uncertain)) {
					button(QObject::tr("Read Its Answer"), QStringLiteral("localDraftWorkflowRead"), [this, id] {
						if (Dispatch.ReadAnswer) {
							Dispatch.ReadAnswer(id);
						}
					});
				}
			}
			rows->addLayout(second);

			if (action != nullptr && action->Record && entry.Mark != core::checklist_mark::in_hand) {
				if (const auto lines = finding_lines(*action->Record); !lines.isEmpty()) {
					auto* const detail = secondary(label(lines.join(QLatin1Char('\n')), group, QStringLiteral("localDraftWorkflowFindings")));
					detail->setContentsMargins(row_indent, 0, 0, 0);
					rows->addWidget(detail);
				}
			}
			// A handed-off command: the exact line to paste, and Copy.
			if (action != nullptr && !action->CommandLine.empty() && (recordState == core::workflow_action_state::handed_off || recordState == core::workflow_action_state::uncertain)) {
				auto* const commandRow = new detail::wrapping_row{ nullptr, 6, 4 };
				commandRow->setContentsMargins(row_indent, 0, 0, 0);
				auto* const command = new QLineEdit{ text_of(action->CommandLine), group };
				command->setObjectName(QStringLiteral("localDraftWorkflowCommand"));
				command->setAccessibleName(QObject::tr("Command to run in your terminal"));
				command->setReadOnly(true);
				command->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
				command->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
				commandRow->addWidget(command);
				auto* const copy = new QPushButton{ QObject::tr("Copy"), group };
				copy->setObjectName(QStringLiteral("localDraftWorkflowCopy"));
				commandRow->addWidget(copy);
				connect(copy, &QPushButton::clicked, this, [this, line = action->CommandLine] {
					if (Dispatch.Copy) {
						Dispatch.Copy(line);
					}
				});
				if (recordState == core::workflow_action_state::handed_off) {
					auto* const lost = new QPushButton{ QObject::tr("Its Answer Was Lost"), group };
					lost->setObjectName(QStringLiteral("localDraftWorkflowLost"));
					commandRow->addWidget(lost);
					connect(lost, &QPushButton::clicked, this, [this, id] {
						if (Dispatch.Lost) {
							Dispatch.Lost(id);
						}
					});
				}
				rows->addLayout(commandRow);
			}
			if (recordState == core::workflow_action_state::awaiting_input) {
				auto* const answerRow = new detail::wrapping_row{ nullptr, 6, 4 };
				answerRow->setContentsMargins(row_indent, 0, 0, 0);
				auto* const answer = new QLineEdit{ group };
				answer->setObjectName(QStringLiteral("localDraftWorkflowAnswer"));
				answer->setAccessibleName(QObject::tr("Your answer"));
				answer->setPlaceholderText(QObject::tr("Your answer"));
				answer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
				answerRow->addWidget(answer);
				auto* const proceed = new QPushButton{ QObject::tr("Continue"), group };
				proceed->setObjectName(QStringLiteral("localDraftWorkflowContinue"));
				answerRow->addWidget(proceed);
				connect(proceed, &QPushButton::clicked, this, [this, id, answer] {
					if (Dispatch.HandOff && !answer->text().isEmpty()) {
						Dispatch.HandOff(id, answer->text().toStdString());
					}
				});
				rows->addLayout(answerRow);
			}
			Rows->addWidget(group);
		}
		Actions->setVisible(!Checklist.Rows.empty());
	}

	auto local_draft_workflow_area::text() const -> QString
	{
		auto lines = QStringList{ Headline->text(), HeadlineAside->text(), Summary->text() };
		for (auto* const child : Actions->findChildren<QLabel*>()) {
			lines << child->text();
			if (!child->toolTip().isEmpty()) {
				lines << child->toolTip();
			}
		}
		for (auto* const child : Actions->findChildren<QLineEdit*>()) {
			lines << child->text();
		}
		lines << Chooser->currentText() << Revision->text();
		return lines.join(QLatin1Char('\n'));
	}
} // namespace bpr::qt
