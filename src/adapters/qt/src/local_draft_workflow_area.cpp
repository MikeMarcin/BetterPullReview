#include "bpr/qt/local_draft_workflow_area.hpp"

#include "bpr/qt/agent_feature.hpp"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
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

		/// Who does the work and what it says it does: a declaration, never a
		/// restriction BPR enforces on a program.
		[[nodiscard]] auto scope_text(const application::local_draft_workflow_action_view& aAction) -> QString
		{
			auto effects = QStringList{ };
			for (const auto effect : aAction.Action.Effects) {
				effects << text_of(core::describe(effect));
			}
			const auto declared = QObject::tr("It says it %1.").arg(effects.join(QStringLiteral(", ")));
			if (aAction.Action.Binding == core::workflow_binding::agent) {
				return QObject::tr("Asks %1 in BetterPullReview's read-only writing lane; it can only propose. %2")
					.arg(aAction.AgentName.empty() ? QObject::tr("its agent") : text_of(aAction.AgentName), declared);
			}
			return QObject::tr("You run %1 in your own terminal or harness, which approves what it does. BetterPullReview does not start or restrict it. %2")
				.arg(text_of(aAction.Action.ProgramName), declared);
		}

		[[nodiscard]] auto finding_lines(const core::workflow_action_record& aRecord) -> QStringList
		{
			auto lines = QStringList{ };
			for (const auto& finding : aRecord.Findings) {
				const auto state = finding.State == core::workflow_finding_state::met ? QObject::tr("met")
					: finding.State == core::workflow_finding_state::unmet            ? QObject::tr("not met")
																					  : QObject::tr("unknown");
				lines << QObject::tr("%1: %2%3").arg(text_of(finding.Check), state, finding.Detail.empty() ? QString{ } : QStringLiteral(" (") + text_of(finding.Detail) + QLatin1Char(')'));
			}
			for (const auto& limitation : aRecord.Limitations) {
				lines << text_of(core::describe(limitation.Kind)) + (limitation.Detail.empty() ? QString{ } : QStringLiteral(" (") + text_of(limitation.Detail) + QLatin1Char(')'));
			}
			for (const auto& reference : aRecord.References) {
				lines << QObject::tr("%1 %2: %3%4")
							 .arg(text_of(core::stable_name(reference.Kind)).replace(QLatin1Char('_'), QLatin1Char(' ')), text_of(reference.Id), text_of(core::stable_name(reference.State)),
								  reference.Detail.empty() ? QString{ } : QStringLiteral(" (") + text_of(reference.Detail) + QLatin1Char(')'));
			}
			if (aRecord.Upstream) {
				lines << QObject::tr("Consulted %1 at %2").arg(text_of(aRecord.Upstream->Name), text_of(aRecord.Upstream->Fingerprint));
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
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 12);
		auto* const top     = new QHBoxLayout{ };
		auto* const caption = new QLabel{ QObject::tr("Workflow"), this };
		auto        font    = caption->font();
		font.setWeight(QFont::DemiBold);
		caption->setFont(font);
		top->addWidget(caption);
		Chooser = new QComboBox{ this };
		Chooser->setObjectName(QStringLiteral("localDraftWorkflowChooser"));
		Chooser->setAccessibleName(QObject::tr("Workflow for this draft"));
		Chooser->setToolTip(QObject::tr("Optional, and selecting one runs nothing."));
		top->addWidget(Chooser, 1);
		column->addLayout(top);
		Summary = secondary(label({ }, this, QStringLiteral("localDraftWorkflowSummary")));
		column->addWidget(Summary);
		Update = new QPushButton{ QObject::tr("Use the newer revision"), this };
		Update->setObjectName(QStringLiteral("localDraftWorkflowUpdate"));
		Update->hide();
		column->addWidget(Update, 0, Qt::AlignLeft);
		PublisherRow             = new QWidget{ this };
		auto* const publisherRow = new QHBoxLayout{ PublisherRow };
		publisherRow->setContentsMargins(0, 0, 0, 0);
		Publisher = new QComboBox{ PublisherRow };
		Publisher->setObjectName(QStringLiteral("localDraftWorkflowPublisher"));
		Publisher->setAccessibleName(QObject::tr("Who creates the pull request"));
		publisherRow->addWidget(new QLabel{ QObject::tr("Publisher"), PublisherRow });
		publisherRow->addWidget(Publisher, 1);
		column->addWidget(PublisherRow);
		Actions = new QWidget{ this };
		Actions->setObjectName(QStringLiteral("localDraftWorkflowActions"));
		Rows = new QVBoxLayout{ Actions };
		Rows->setContentsMargins(0, 0, 0, 0);
		Rows->setSpacing(14);
		column->addWidget(Actions);

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
		connect(Publisher, &QComboBox::activated, this, [this](const int aIndex) {
			if (!Presenting && Dispatch.Publisher) {
				Dispatch.Publisher(aIndex == 1 ? core::workflow_publisher::external : core::workflow_publisher::built_in);
			}
		});
	}

	void local_draft_workflow_area::set_dispatch(local_draft_workflow_dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void local_draft_workflow_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection        = aProjection;
		const auto& view  = Projection.Workflow;
		const auto* draft = Projection.Draft ? &*Projection.Draft : nullptr;
		const auto  guard = QSignalBlocker{ Chooser };
		Presenting        = true;
		Chooser->clear();
		Chooser->addItem(QObject::tr("None (built-in preparation only)"), QString{ });
		for (const auto& option : view.Available) {
			Chooser->addItem(option.Origin.empty() ? text_of(option.Name) : QObject::tr("%1, from %2").arg(text_of(option.Name), text_of(option.Origin)), text_of(option.Id));
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
		Chooser->setEnabled(draft != nullptr && (selected != nullptr || !view.Available.empty()));
		Update->setVisible(view.UpdateAvailable);
		const auto hasPublisher = selected != nullptr
			&& std::ranges::any_of(selected->Workflow.Actions, [](const core::workflow_action& aAction) { return aAction.Slot == core::workflow_slot::publish_draft; });
		Publisher->clear();
		Publisher->addItem(QObject::tr("Built in: BetterPullReview creates the draft pull request"));
		if (hasPublisher) {
			const auto publish = std::ranges::find(selected->Workflow.Actions, core::workflow_slot::publish_draft, &core::workflow_action::Slot);
			Publisher->addItem(QObject::tr("%1, handed to your terminal").arg(text_of(publish->Task)));
			Publisher->setCurrentIndex(selected->Publisher == core::workflow_publisher::external ? 1 : 0);
		}
		PublisherRow->setVisible(hasPublisher);
		auto summary = QStringList{ };
		if (selected == nullptr) {
			summary << (view.Available.empty() ? QObject::tr("No workflow is set up. Changes, Description and Tour need none.")
											   : QObject::tr("No workflow selected. Changes, Description and Tour need none."));
		} else {
			summary << QObject::tr("From %1. Revision %2.").arg(selected->Workflow.Origin.empty() ? QObject::tr("an unnamed source") : text_of(selected->Workflow.Origin), text_of(selected->Revision.substr(0U, 12U)));
			for (const auto& check : view.Checks) {
				summary << (check.Gap ? QObject::tr("Required check “%1”: %2.").arg(text_of(check.Task), text_of(core::describe(*check.Gap)))
									  : QObject::tr("Required check “%1”: met, as reported.").arg(text_of(check.Task)));
			}
			if (hasPublisher && selected->Publisher == core::workflow_publisher::built_in && !view.Checks.empty()) {
				summary << QObject::tr("The built-in publisher does not run this workflow's checks.");
			}
		}
		if (view.Refusal) {
			summary << refusal_text(*view.Refusal);
		}
		if (!view.Failure.empty()) {
			summary << text_of(view.Failure);
		}
		Summary->setText(summary.join(QLatin1Char('\n')));
		Presenting = false;
		rebuild();
	}

	void local_draft_workflow_area::rebuild()
	{
		while (auto* const item = Rows->takeAt(0)) {
			if (auto* const widget = item->widget()) {
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
		for (const auto& action : view.Actions) {
			const auto  id    = action.Action.Id;
			auto* const group = new QWidget{ Actions };
			group->setObjectName(QStringLiteral("localDraftWorkflowAction_") + text_of(id));
			group->setAccessibleName(text_of(action.Action.Task));
			auto* const column = new QVBoxLayout{ group };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(4);
			auto* const task = label(text_of(action.Action.Task), group);
			auto        font = task->font();
			font.setWeight(QFont::Medium);
			task->setFont(font);
			column->addWidget(task);
			column->addWidget(secondary(label(scope_text(action), group)));
			column->addWidget(label(workflow_state_text(action), group, QStringLiteral("localDraftWorkflowState")));
			if (action.Record) {
				if (const auto lines = finding_lines(*action.Record); !lines.isEmpty()) {
					column->addWidget(label(lines.join(QLatin1Char('\n')), group, QStringLiteral("localDraftWorkflowFindings")));
				}
			}
			const auto state = action.Record ? action.Record->State : core::workflow_action_state::idle;
			// The selected context the request carried, and where it is.
			if (action.Record && action.Record->Invocation != 0U && !action.RequestPath.empty() && Projection.Draft) {
				const auto& draft = *Projection.Draft;
				const auto* shown = core::latest_candidate(draft);
				column->addWidget(secondary(label(QObject::tr("Sent candidate %1: the title, the description, the tour's problem and solution, %2 notes and %3 changed paths%4. In %5.")
													  .arg(action.Record->Candidate)
													  .arg(draft.Notes.size())
													  .arg(shown == nullptr ? 0U : shown->Changes.size())
													  .arg(action.Record->PlanDigest.empty() ? QString{ } : QObject::tr(", and the plan %1").arg(text_of(action.Record->PlanDigest.substr(0U, 12U))))
													  .arg(text_of(action.RequestPath)),
												  group, QStringLiteral("localDraftWorkflowSent"))));
			}
			if (!action.CommandLine.empty() && (state == core::workflow_action_state::handed_off || state == core::workflow_action_state::uncertain)) {
				auto* const commandRow = new QHBoxLayout{ };
				auto* const command    = new QLineEdit{ text_of(action.CommandLine), group };
				command->setObjectName(QStringLiteral("localDraftWorkflowCommand"));
				command->setAccessibleName(QObject::tr("Command to run in your terminal"));
				command->setReadOnly(true);
				command->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
				commandRow->addWidget(command, 1);
				auto* const copy = new QPushButton{ QObject::tr("Copy"), group };
				commandRow->addWidget(copy);
				connect(copy, &QPushButton::clicked, this, [this, line = action.CommandLine] {
					if (Dispatch.Copy) {
						Dispatch.Copy(line);
					}
				});
				column->addLayout(commandRow);
			}
			auto* const buttons = new QHBoxLayout{ };
			const auto  ready   = action.Prerequisite == core::workflow_prerequisite::ready;
			const auto  button  = [this, group, buttons](const QString& aText, const QString& aName, auto aDo) {
				auto* const control = new QPushButton{ aText, group };
				control->setObjectName(aName);
				connect(control, &QPushButton::clicked, this, aDo);
				buttons->addWidget(control);
				return control;
			};
			if (state == core::workflow_action_state::awaiting_input) {
				auto* const answer = new QLineEdit{ group };
				answer->setObjectName(QStringLiteral("localDraftWorkflowAnswer"));
				answer->setAccessibleName(QObject::tr("Your answer"));
				answer->setPlaceholderText(QObject::tr("Your answer"));
				column->addWidget(answer);
				button(QObject::tr("Continue"), QStringLiteral("localDraftWorkflowContinue"), [this, id, answer] {
					if (Dispatch.HandOff && !answer->text().isEmpty()) {
						Dispatch.HandOff(id, answer->text().toStdString());
					}
				});
			}
			switch (action.Action.Slot) {
			case core::workflow_slot::gather_context:
			case core::workflow_slot::propose_description:
			case core::workflow_slot::check_preparation:
				if (action.Action.Binding == core::workflow_binding::agent) {
					auto* const ask = button(QObject::tr("Ask %1").arg(action.AgentName.empty() ? QObject::tr("the agent") : text_of(action.AgentName)), QStringLiteral("localDraftWorkflowAsk"), [this, id] {
						if (Dispatch.Ask) {
							Dispatch.Ask(id);
						}
					});
					ask->setEnabled(ready && !Projection.Asking && Projection.AgentAvailable);
					wear_agent_accent(*ask, palette());
				} else {
					button(action.Record && action.Record->Invocation != 0U ? QObject::tr("Hand off again") : QObject::tr("Hand off"), QStringLiteral("localDraftWorkflowHandOff"), [this, id] {
						if (Dispatch.HandOff) {
							Dispatch.HandOff(id, { });
						}
					})->setEnabled(ready);
				}
				break;
			case core::workflow_slot::publish_draft:
				button(QObject::tr("Hand off the plan"), QStringLiteral("localDraftWorkflowPublish"), [this, id, digest] {
					if (Dispatch.Publish) {
						Dispatch.Publish(id, digest);
					}
				})->setEnabled(ready && external && !digest.empty() && !linked);
				if (state != core::workflow_action_state::idle && action.Record && !action.Record->Reconciled && !linked) {
					button(QObject::tr("Check GitHub"), QStringLiteral("localDraftWorkflowReconcile"), [this, id] {
						if (Dispatch.Reconcile) {
							Dispatch.Reconcile(id);
						}
					});
				}
				break;
			case core::workflow_slot::finish_publication:
				button(QObject::tr("Hand off"), QStringLiteral("localDraftWorkflowFollowUp"), [this, id] {
					if (Dispatch.FollowUp) {
						Dispatch.FollowUp(id);
					}
				})->setEnabled(ready && linked);
				if (action.Record && action.Record->Invocation != 0U && !action.Record->Reconciled && state != core::workflow_action_state::awaiting_input) {
					button(QObject::tr("I checked what it made"), QStringLiteral("localDraftWorkflowReconcileFollowUp"), [this, id] {
						if (Dispatch.ReconcileFollowUp) {
							Dispatch.ReconcileFollowUp(id);
						}
					});
				}
				break;
			}
			if (action.Action.Binding == core::workflow_binding::command && (state == core::workflow_action_state::handed_off || state == core::workflow_action_state::uncertain)) {
				button(QObject::tr("Read its answer"), QStringLiteral("localDraftWorkflowRead"), [this, id] {
					if (Dispatch.ReadAnswer) {
						Dispatch.ReadAnswer(id);
					}
				});
				if (state == core::workflow_action_state::handed_off) {
					button(QObject::tr("Its answer was lost"), QStringLiteral("localDraftWorkflowLost"), [this, id] {
						if (Dispatch.Lost) {
							Dispatch.Lost(id);
						}
					});
				}
			}
			buttons->addStretch(1);
			column->addLayout(buttons);
			Rows->addWidget(group);
		}
		Actions->setVisible(!view.Actions.empty());
	}

	auto local_draft_workflow_area::text() const -> QString
	{
		auto lines = QStringList{ Chooser->currentText(), Summary->text() };
		for (auto* const child : Actions->findChildren<QLabel*>()) {
			lines << child->text();
		}
		for (auto* const child : Actions->findChildren<QLineEdit*>()) {
			lines << child->text();
		}
		return lines.join(QLatin1Char('\n'));
	}
} // namespace bpr::qt
