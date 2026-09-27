#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
namespace bpr::qt {
	namespace {
		auto paragraph(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			return label;
		}
		void update_text(QPlainTextEdit* aEditor, const std::string& aText)
		{
			// QTextDocument uses paragraph separators internally. A save acknowledgement
			// must not replace an unchanged document just because its raw source uses CRLF.
			const auto text = QString::fromStdString(aText).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			if (aEditor->toPlainText() != text) {
				const auto block = QSignalBlocker{ aEditor };
				aEditor->setPlainText(text);
			}
		}
	}
	review_tour_editor::review_tour_editor(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewTourEditor"));
		setAccessibleName(tr("Prepare review tour"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(8, 8, 8, 8);
		layout->addWidget(paragraph(tr("Prepare a private review tour. Publishing requires Preview and Publish."), this));
		Imports = new QComboBox{ this };
		Imports->setObjectName(QStringLiteral("tourImportChoices"));
		Imports->setAccessibleName(tr("GitHub tour index and list"));
		Imports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		Import = new QPushButton{ tr("Load selected GitHub tour"), this };
		Import->setObjectName(QStringLiteral("tourImport"));
		connect(Import, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::import_selected); });
		layout->addWidget(Imports);
		layout->addWidget(Import);
		Roadmap = new QTabWidget{ this };
		Roadmap->setObjectName(QStringLiteral("tourRoadmap"));
		Roadmap->setAccessibleName(tr("Review tour sections"));
		const auto field = [&](const QString& aTitle, const QString& aPrompt, const QString& aHelp, const QString& aName, core::tour_command_kind aKind, tour_editor_action aMapping) {
			auto* page   = new QWidget{ Roadmap };
			auto* column = new QVBoxLayout{ page };
			column->addWidget(paragraph(aPrompt, page));
			column->addWidget(paragraph(aHelp, page));
			auto* editor = new scoped_prose_editor{ page };
			editor->setObjectName(aName);
			editor->setAccessibleName(aTitle + tr(" Markdown source"));
			editor->setTabChangesFocus(false);
			editor->setMinimumHeight(130);
			column->addWidget(editor, 1);
			add_writing_button(editor, aKind == core::tour_command_kind::problem ? core::writing_hook::problem : core::writing_hook::solution, column);
			auto* map = new QPushButton{ tr("Use selected description text"), page };
			map->setObjectName(aName + QStringLiteral("Map"));
			map->setToolTip(tr("Select text in the pull request description's Source view, then use it in this field."));
			column->addWidget(map);
			connect(map, &QPushButton::clicked, this, [this, aMapping] { trigger(aMapping); });
			connect(editor, &QPlainTextEdit::textChanged, this, [this, editor, aKind] { edit(aKind, editor->raw()); });
			Roadmap->addTab(page, aTitle);
			return editor;
		};
		Problem  = field(tr("Problem"), tr("What problem does this change solve?"), tr("Describe what was wrong, missing, or unnecessarily difficult before this change."), QStringLiteral("tourProblem"), core::tour_command_kind::problem, tour_editor_action::map_problem);
		Solution = field(tr("Solution"), tr("How does this change solve the problem?"), tr("Explain the central approach and any important limits. Save detailed implementation decisions for the tour."), QStringLiteral("tourSolution"), core::tour_command_kind::solution, tour_editor_action::map_solution);
		build_stops();
		Preview = new markdown_body_view{ Roadmap };
		Preview->setObjectName(QStringLiteral("tourPreview"));
		auto* previewPage   = new QWidget{ Roadmap };
		auto* previewLayout = new QVBoxLayout{ previewPage };
		previewLayout->addWidget(paragraph(tr("Local preview. No remote operations are scheduled. HTML and disclosures may differ from GitHub; original Markdown remains the source."), previewPage));
		previewLayout->addWidget(Preview, 1);
		PublicationMode = new QComboBox{ previewPage };
		PublicationMode->setObjectName(QStringLiteral("tourPublicationMode"));
		PublicationMode->addItems({ tr("Keep single-review publication"), tr("Publish with compatibility comments (non-atomic)") });
		previewLayout->addWidget(PublicationMode);
		DescriptionEdit = new QCheckBox{ tr("Replace PR description after publishing"), previewPage };
		DescriptionEdit->setObjectName(QStringLiteral("tourDescriptionEdit"));
		previewLayout->addWidget(DescriptionEdit);
		previewLayout->addWidget(paragraph(tr("The optional description edit replaces it with Problem/Solution and a tour backlink. Compatibility comments become visible one at a time; interrupted publication may remain partial."), previewPage));
		PublicationStatus = paragraph({ }, previewPage);
		PublicationStatus->setObjectName(QStringLiteral("tourPublicationStatus"));
		previewLayout->addWidget(PublicationStatus);
		PublicationPreview = new markdown_body_view{ previewPage };
		PublicationPreview->setObjectName(QStringLiteral("tourPublicationPreview"));
		previewLayout->addWidget(PublicationPreview, 1);
		const auto button = [&](const QString& label, const QString& name, tour_editor_action action) {auto* b=new QPushButton{label,previewPage};b->setObjectName(name);connect(b,&QPushButton::clicked,this,[this,action]{trigger(action);});previewLayout->addWidget(b);return b; };
		button(tr("Prepare publication preview"), QStringLiteral("tourPreparePublication"), tour_editor_action::preview_publication);
		PublishTour   = button(tr("Publish tour"), QStringLiteral("tourPublish"), tour_editor_action::publish_publication);
		ResumeTour    = button(tr("Resume captured publication"), QStringLiteral("tourResume"), tour_editor_action::resume_publication);
		ReconcileTour = button(tr("Check publication status"), QStringLiteral("tourReconcile"), tour_editor_action::reconcile_publication);
		StopTour      = button(tr("Stop publication"), QStringLiteral("tourStop"), tour_editor_action::stop_publication);
		connect(PublicationMode, &QComboBox::currentIndexChanged, this, [this] { PublishTour->setEnabled(false); });
		connect(DescriptionEdit, &QCheckBox::toggled, this, [this] { PublishTour->setEnabled(false); });
		present_publication({ });
		Roadmap->addTab(previewPage, tr("Preview"));
		connect(Roadmap, &QTabWidget::currentChanged, this, [this] { if(WritingCancel){ WritingCancel();
} refresh_preview(); });
		layout->addWidget(Roadmap, 1);
		Status = paragraph(tr("Open a pull request with an accepted account to prepare a private tour."), this);
		Status->setObjectName(QStringLiteral("tourSaveStatus"));
		RevisionChanges = new QLabel{ this };
		RevisionChanges->setObjectName(QStringLiteral("tourRevisionChanges"));
		RevisionChanges->setWordWrap(true);
		layout->addWidget(RevisionChanges);
		NextChanged = new QPushButton{ tr("Next changed stop"), this };
		NextChanged->setObjectName(QStringLiteral("tourNextChanged"));
		layout->addWidget(NextChanged);
		connect(NextChanged, &QPushButton::clicked, this, [this] { next_changed_stop(); });

		Status->setAccessibleName(tr("Tour save status"));
		layout->addWidget(Status);
		Conflict = new QPlainTextEdit{ this };
		Conflict->setObjectName(QStringLiteral("tourConflict"));
		Conflict->setAccessibleName(tr("Saved copy from another window"));
		Conflict->setReadOnly(true);
		Conflict->hide();
		layout->addWidget(Conflict);
		Retry = new QPushButton{ tr("Retry local save"), this };
		connect(Retry, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::retry_save);} });
		layout->addWidget(Retry);
		UseSaved = new QPushButton{ tr("Discard my edits and use saved copy"), this };
		connect(UseSaved, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::use_saved_copy);} });
		layout->addWidget(UseSaved);
		KeepLocalConflict = new QPushButton{ tr("Keep my proposed version"), this };
		KeepLocalConflict->setObjectName(QStringLiteral("tourKeepLocalConflict"));
		connect(KeepLocalConflict, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::keep_local_conflict); });
		layout->addWidget(KeepLocalConflict);
		UseRemoteConflict = new QPushButton{ tr("Use website version"), this };
		UseRemoteConflict->setObjectName(QStringLiteral("tourUseRemoteConflict"));
		connect(UseRemoteConflict, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::use_remote_conflict); });
		layout->addWidget(UseRemoteConflict);
		present({ });
	}
	auto review_tour_editor::compatibility_publication() const -> bool { return PublicationMode->currentIndex() == 1; }
	auto review_tour_editor::description_publication() const -> bool { return DescriptionEdit->isChecked(); }
	void review_tour_editor::present_publication(const application::tour_publication_presentation& aProjection)
	{
		PublicationStatus->setText(QString::fromStdString(aProjection.Status));
		PublicationPreview->set_body(aProjection.Markdown, "tour-publication");
		PublishTour->setEnabled(aProjection.Publish);
		ResumeTour->setEnabled(aProjection.Resume);
		ReconcileTour->setEnabled(aProjection.Reconcile);
		StopTour->setEnabled(aProjection.Stop);
	}

	void review_tour_editor::set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction)
	{
		Dispatch = std::move(aDispatch);
		Action   = std::move(aAction);
	}
	auto review_tour_editor::import_choice() const -> std::size_t { return static_cast<std::size_t>(Imports->currentIndex()); }
	void review_tour_editor::edit(core::tour_command_kind aKind, const std::string& aText)
	{
		if (Applying || !Shown.Draft || !Dispatch) {
			return;
		}
		EditingKind = aKind;
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = aKind, .Text = aText });
	}
	void review_tour_editor::present(const application::tour_presentation& aProjection)
	{
		Applying        = true;
		const bool same = Shown.Draft && aProjection.Draft && Shown.Draft->Owner == aProjection.Draft->Owner && Shown.Draft->Id == aProjection.Draft->Id;
		if (!same) {
			Selected = { };
		}
		if (aProjection.Selected != Shown.Selected && aProjection.Selected.value()) {
			Selected = aProjection.Selected;
		}
		if (Shown.ImportChoices != aProjection.ImportChoices) {
			const auto block = QSignalBlocker{ Imports };
			Imports->clear();
			for (const auto& choice : aProjection.ImportChoices) {
				Imports->addItem(QString::fromStdString(choice));
			}
			// Multiple plausible roots require an explicit choice, even when a link is preferred.
			Imports->setCurrentIndex(Imports->count() == 1 ? 0 : -1);
		}
		Imports->setVisible(!aProjection.ImportChoices.empty());
		Import->setVisible(!aProjection.ImportChoices.empty());
		Shown               = aProjection;
		const bool editable = Shown.Draft && !Shown.Loading && !Shown.Draft->ReadOnly;
		const bool bounded  = same && Shown.Refusal == core::tour_refusal::bounds;
		Problem->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::problem));
		Solution->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::solution));
		Title->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::edit_stop));
		Body->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::edit_stop));
		if (Shown.Draft && (Shown.Refusal == core::tour_refusal::none || !same)) {
			Problem->present(Shown.Draft->Problem, !same);
			Solution->present(Shown.Draft->Solution, !same);
		} else if (!Shown.Draft) {
			Problem->present({ }, !same);
			Solution->present({ }, !same);
		}
		auto status = tr("Open a pull request with an accepted account to prepare a private tour.");
		if (Shown.Loading) {
			status = tr("Loading private tour…");
		} else if (Shown.Refusal != core::tour_refusal::none) {
			status = Shown.Refusal == core::tour_refusal::bounds ? tr("This edit exceeds the local tour limit. Your editor text is retained but has not been saved.") : Shown.Refusal == core::tour_refusal::incomplete ? tr("Add a title, explanation and available source before marking this stop Ready. Incomplete text is saved locally.")
																																																						: tr("This edit was refused because its draft or source changed. Your text is retained.");
		} else if (Shown.Conflict) {
			status = tr("Another window saved this draft. Your edits are retained above; the saved copy is below. Choose which copy to keep.");
		} else if (Shown.Failure) {
			status = tr("Local save unavailable. Your edits remain in this window and are not yet durable. Retry the save.");
		} else if (Shown.Draft) {
			status = Shown.Saving ? tr("Saving locally…") : Shown.SavedRevision == Shown.Draft->Revision ? tr("Saved locally. Nothing published.")
																										 : tr("Private draft. Nothing published.");
		}
		if (Shown.Draft && !Shown.Draft->ImportNotice.empty()) {
			status += " " + QString::fromStdString(Shown.Draft->ImportNotice);
		}
		if (!Shown.ImportNotice.empty()) {
			status += " " + QString::fromStdString(Shown.ImportNotice);
		}
		Status->setText(status);
		QString changes = tr("No prior local tour version.");
		if (Shown.Draft && Shown.Draft->HasPriorObservation) {
			changes = tr("Changes at the last differing local website observation:");
			if (Shown.Draft->ObservedChanges.empty()) {
				changes = tr("No website differences recorded locally.");
			}
			for (const auto& change : Shown.Draft->ObservedChanges) {
				const auto kind = change.Kind == core::tour_change_kind::added ? tr("Added") : change.Kind == core::tour_change_kind::removed ? tr("Removed")
					: change.Kind == core::tour_change_kind::reordered                                                                        ? tr("Reordered")
					: change.Kind == core::tour_change_kind::source                                                                           ? tr("Source changed")
																																			  : tr("Text changed");
				changes += "\n" + kind + ": " + QString::fromStdString(change.Title);
			}
		}
		RevisionChanges->setText(changes);
		NextChanged->setEnabled(Shown.Draft && !Shown.Draft->ObservedChanges.empty());

		Retry->setVisible(Shown.Failure.has_value() && !Shown.Conflict);
		UseSaved->setVisible(static_cast<bool>(Shown.Conflict));
		const bool remoteConflict = Shown.Draft && !Shown.Draft->Conflicts.empty();
		KeepLocalConflict->setVisible(remoteConflict && !Shown.Conflict);
		UseRemoteConflict->setVisible(remoteConflict && !Shown.Conflict);
		Conflict->setVisible(static_cast<bool>(Shown.Conflict) || remoteConflict);
		if (remoteConflict && !Shown.Conflict) {
			const auto& conflict = Shown.Draft->Conflicts.front();
			update_text(Conflict, "Website conflict — saved versions\n\nBASELINE\n" + conflict.Baseline + "\n\nMY PROPOSAL\n" + conflict.Local + "\n\nWEBSITE\n" + conflict.Remote);
			Conflict->setAccessibleName(tr("Baseline, my proposal and website versions"));
		}
		if (Shown.Conflict) {
			auto text = "## Problem\n" + Shown.Conflict->Problem + "\n\n## Solution\n" + Shown.Conflict->Solution;
			for (const auto& stop : Shown.Conflict->Stops) {
				text += "\n\n### " + stop.Title + "\n" + stop.Body;
			}
			update_text(Conflict, text);
		}
		present_stops(!same);
		refresh_preview();
		Applying = false;
	}
	void review_tour_editor::refresh_preview()
	{
		if (Roadmap->currentIndex() != Roadmap->count() - 1 || !Shown.Draft) {
			return;
		}
		auto body = "## Problem\n\n" + Shown.Draft->Problem + "\n\n## Solution\n\n" + Shown.Draft->Solution;
		body += "\n\n## Tour stops\n";
		for (std::size_t i = 0; i < Shown.Draft->Stops.size(); ++i) {
			const auto& stop = Shown.Draft->Stops[i];
			body += "\n### " + std::to_string(i + 1) + " — " + stop.Title + "\n\n" + stop.Body + "\n\n";
			if (stop.Original) {
				body += "Source: " + stop.Original->Path + " · " + (stop.Original->First.Side == core::diff_side::left ? "base" : "head") + " lines " + std::to_string(stop.Original->First.Line + 1U) + "–" + std::to_string(stop.Original->Last.Line + 1U) + "\n\n";
			} else if (!stop.Destination) {
				body += "Source unavailable.\n\n";
			}
			if (stop.Destination && !stop.Original) {
				const auto& source = *stop.Destination;
				body += "Pinned source: " + source.Path + " · " + std::string{ core::canonical_hex(source.Commit).view() } + " · lines " + std::to_string(source.FirstLine + 1U) + "–" + std::to_string(source.LastLine + 1U) + "\n\n";
			}
			const auto plan = std::ranges::find(Shown.Destinations, stop.Id, &application::tour_stop_destination::Stop);
			if (plan != Shown.Destinations.end()) {
				if (!plan->SourceLink.empty()) {
					body += "[Original source](" + plan->SourceLink + ")\n\n";
				}
				if (plan->Plan.IncompatibleBody) {
					body += "Publication requires a native line subject for this suggestion.\n\n";
				} else if (plan->Plan.RequiresCompatibility) {
					body += "This destination requires an explicit compatibility publication plan. It cannot join one pending review.\n\n";
				}
				if (plan->Plan.NeedsQualification) {
					body += "Publication support for this exact source and operation is not yet qualified.\n\n";
				}
			}
			for (const auto& related : stop.Related) {
				body += "Related: " + related.Path + " · lines " + std::to_string(related.First.Line + 1U) + "–" + std::to_string(related.Last.Line + 1U) + "\n\n";
			}
		}
		Preview->set_body(std::move(body), std::to_string(Shown.Draft->Revision));
	}
	void review_tour_editor::set_writing_dispatch(std::function<void(core::writing_hook, prose_capture)> aRequest, std::function<void()> aCancel)
	{
		WritingRequest = std::move(aRequest);
		WritingCancel  = std::move(aCancel);
	}
	auto review_tour_editor::writing_editor(core::writing_hook aHook) const -> scoped_prose_editor* { return aHook == core::writing_hook::problem ? Problem : aHook == core::writing_hook::solution ? Solution
																										  : aHook == core::writing_hook::comment                                                    ? Body
																																																	: nullptr; }
	auto review_tour_editor::writing_visible(core::writing_hook aHook) const -> bool { return aHook == core::writing_hook::problem ? Roadmap->currentIndex() == 0 : aHook == core::writing_hook::solution ? Roadmap->currentIndex() == 1
																																																		  : aHook == core::writing_hook::comment && Roadmap->currentIndex() == 2 && selected_stop(); }
	void review_tour_editor::add_writing_button(scoped_prose_editor* aEditor, core::writing_hook aHook, QVBoxLayout* aLayout)
	{
		auto* button = new QPushButton{ aEditor->parentWidget() };
		button->setObjectName(aHook == core::writing_hook::problem ? QStringLiteral("tourProblemMagic") : aHook == core::writing_hook::solution ? QStringLiteral("tourSolutionMagic")
																																				: QStringLiteral("tourStopMagic"));
		button->setFocusPolicy(Qt::NoFocus);
		button->setToolTip(tr("Use the selected agent once. The result replaces only the captured text and can be undone."));
		aLayout->addWidget(button);
		const auto label = [aEditor, aHook, button] { button->setText(aEditor->textCursor().hasSelection() ? tr("Enhance selection") : core::writing_blank(aEditor->raw()) ? (aHook == core::writing_hook::comment ? tr("Generate comment") : tr("Recover explanation"))
																																										   : tr("Enhance text")); };
		connect(aEditor, &QPlainTextEdit::selectionChanged, this, label);
		connect(aEditor, &QPlainTextEdit::textChanged, this, label);
		connect(button, &QPushButton::clicked, this, [this, aEditor, aHook] {if(!aEditor->isReadOnly() && WritingRequest) {if(auto captured=aEditor->capture()){ WritingRequest(aHook,std::move(*captured));
}} });
		aEditor->Invalidated = [this, label] {label();if(WritingCancel){ WritingCancel();
} };
		label();
	}
	void review_tour_editor::show_problem()
	{
		Roadmap->setCurrentIndex(0);
		Problem->setFocus();
	}
	void review_tour_editor::show_notice(const QString& aText) { Status->setText(aText); }
	void review_tour_editor::trigger(tour_editor_action aAction)
	{
		if (Shown.Refusal == core::tour_refusal::bounds && aAction != tour_editor_action::retry_save && aAction != tour_editor_action::use_saved_copy && aAction != tour_editor_action::navigate_stop && aAction != tour_editor_action::navigate_related) {
			show_notice(tr("Shorten the unsaved text or use native Undo before changing the tour. Your full text is retained."));
			return;
		}
		if (Action) {
			Action(aAction);
		}
	}
	void review_tour_editor::show_stops()
	{
		Roadmap->setCurrentIndex(2);
		Body->setFocus();
	}
	auto review_tour_editor::selected_stop() const -> const core::tour_stop*
	{
		if (!Shown.Draft) {
			return nullptr;
		}
		const auto found = std::ranges::find(Shown.Draft->Stops, Selected, &core::tour_stop::Id);
		return found == Shown.Draft->Stops.end() ? nullptr : &*found;
	}
	auto review_tour_editor::selected_related() const -> std::optional<core::tour_source_range>
	{
		const auto* stop = selected_stop();
		const auto  row  = Related->currentRow();
		return stop && row >= 0 && static_cast<std::size_t>(row) < stop->Related.size() ? std::optional{ stop->Related[static_cast<std::size_t>(row)] } : std::nullopt;
	}
	void review_tour_editor::build_stops()
	{
		auto* page   = new QWidget{ Roadmap };
		auto* column = new QVBoxLayout{ page };
		column->addWidget(paragraph(tr("Arrange the important decisions in conceptual order."), page));
		auto* top = new QHBoxLayout;
		column->addLayout(top);
		auto* add = new QPushButton{ tr("Add tour stop"), page };
		add->setObjectName(QStringLiteral("tourAddStop"));
		top->addWidget(add);
		connect(add, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::add_stop); });
		auto* more = new QToolButton{ page };
		more->setText(tr("Arrange"));
		more->setPopupMode(QToolButton::InstantPopup);
		top->addWidget(more);
		auto* menu = new QMenu{ more };
		more->setMenu(menu);
		const auto action = [&](const QString& aText, const QString& aName, const QString& aKeys, auto aCallback) {
			auto* value = menu->addAction(aText);
			value->setObjectName(aName);
			value->setShortcut(QKeySequence{ aKeys });
			value->setShortcutContext(Qt::WidgetWithChildrenShortcut);
			page->addAction(value);
			connect(value, &QAction::triggered, this, aCallback);
			return value;
		};
		action(tr("Insert before selected"), QStringLiteral("tourInsertBefore"), QStringLiteral("Alt+Insert"), [this] { trigger(tour_editor_action::insert_before); });
		MoveUp   = action(tr("Move stop up"), QStringLiteral("tourMoveUp"), QStringLiteral("Alt+Shift+Up"), [this] {if(Outline->currentRow()>0){structural(core::tour_command_kind::move,static_cast<std::size_t>(Outline->currentRow()-1));} });
		MoveDown = action(tr("Move stop down"), QStringLiteral("tourMoveDown"), QStringLiteral("Alt+Shift+Down"), [this] {if(Outline->currentRow()+1<Outline->count()){structural(core::tour_command_kind::move,static_cast<std::size_t>(Outline->currentRow()+1));} });
		action(tr("Duplicate stop"), QStringLiteral("tourDuplicate"), QStringLiteral("Alt+D"), [this] { structural(core::tour_command_kind::duplicate); });
		action(tr("Split at text cursor"), QStringLiteral("tourSplit"), QStringLiteral("Alt+S"), [this] { structural(core::tour_command_kind::split); });
		action(tr("Merge with next stop"), QStringLiteral("tourMerge"), QStringLiteral("Alt+M"), [this] { structural(core::tour_command_kind::merge_next); });
		action(tr("Use selected source as replacement"), QStringLiteral("tourRetarget"), QStringLiteral("Alt+R"), [this] { trigger(tour_editor_action::retarget_stop); });
		action(tr("Remove stop"), QStringLiteral("tourRemove"), QStringLiteral("Alt+Delete"), [this] { structural(core::tour_command_kind::remove); });
		menu->addSeparator();
		UndoStructure = action(tr("Undo arrangement"), QStringLiteral("tourUndo"), QStringLiteral("Ctrl+Alt+Z"), [this] { trigger(tour_editor_action::undo_structure); });
		RedoStructure = action(tr("Redo arrangement"), QStringLiteral("tourRedo"), QStringLiteral("Ctrl+Alt+Shift+Z"), [this] { trigger(tour_editor_action::redo_structure); });
		Outline       = new QListWidget{ page };
		Outline->setObjectName(QStringLiteral("tourOutline"));
		Outline->setAccessibleName(tr("Ordered tour stops; Alt+Shift+Up and Alt+Shift+Down move the selected stop"));
		Outline->setWordWrap(true);
		Outline->setTextElideMode(Qt::ElideNone);
		Outline->setMinimumHeight(85);
		Outline->setMaximumHeight(120);
		column->addWidget(Outline);
		StopFields   = new QWidget{ page };
		auto* fields = new QVBoxLayout{ StopFields };
		fields->setContentsMargins(0, 0, 0, 0);
		column->addWidget(StopFields, 1);
		Title = new QLineEdit{ StopFields };
		Title->setObjectName(QStringLiteral("tourStopTitle"));
		Title->setAccessibleName(tr("Stop title"));
		Title->setPlaceholderText(tr("What should the reviewer notice?"));
		Title->setMaxLength(std::numeric_limits<int>::max());
		fields->addWidget(Title);
		Source = paragraph({ }, StopFields);
		Source->setObjectName(QStringLiteral("tourStopSource"));
		fields->addWidget(Source);
		Body = new scoped_prose_editor{ StopFields };
		Body->setObjectName(QStringLiteral("tourStopBody"));
		Body->setAccessibleName(tr("Stop explanation Markdown source"));
		Body->setMinimumHeight(130);
		fields->addWidget(Body, 1);
		add_writing_button(Body, core::writing_hook::comment, fields);
		Related = new QListWidget{ StopFields };
		Related->setObjectName(QStringLiteral("tourRelatedSources"));
		Related->setAccessibleName(tr("Related source destinations"));
		Related->setWordWrap(true);
		Related->setTextElideMode(Qt::ElideNone);
		Related->setMinimumHeight(55);
		Related->setMaximumHeight(90);
		fields->addWidget(Related);
		auto* relatedButtons = new QHBoxLayout;
		fields->addLayout(relatedButtons);
		const auto button = [this](const QString& aText, const QString& aName, QHBoxLayout* aRow, auto aCallback) {auto* value=new QPushButton{aText,this}; value->setObjectName(aName); aRow->addWidget(value); connect(value,&QPushButton::clicked,this,aCallback); return value; };
		button(tr("Add selected source"), QStringLiteral("tourAddRelated"), relatedButtons, [this] { trigger(tour_editor_action::add_related); });
		button(tr("Remove link"), QStringLiteral("tourRemoveRelated"), relatedButtons, [this] {
			if (!selected_related() || !Dispatch || Shown.Refusal == core::tour_refusal::bounds) {
				return;
			}
			auto value = *selected_stop();
			value.Related.erase(value.Related.begin() + Related->currentRow());
			Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = value.Id, .Value = std::move(value) });
		});
		Related->setToolTip(tr("Double-click a related source to inspect it. Return to tour restores this stop."));
		connect(Related, &QListWidget::itemActivated, this, [this] { trigger(tour_editor_action::navigate_related); });
		auto* nav = new QHBoxLayout;
		column->addLayout(nav);
		button(tr("Previous"), QStringLiteral("tourPrevious"), nav, [this] { next_stop(-1); });
		button(tr("Next"), QStringLiteral("tourNext"), nav, [this] { next_stop(1); });
		auto* returnToTour = new QPushButton{ tr("Return to tour"), page };
		returnToTour->setObjectName(QStringLiteral("tourReturn"));
		column->addWidget(returnToTour);
		connect(returnToTour, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::navigate_stop); });
		auto* complete = new QPushButton{ tr("Save and next"), page };
		complete->setObjectName(QStringLiteral("tourComplete"));
		complete->setShortcut(QKeySequence{ QStringLiteral("Alt+Return") });
		auto* reaffirm = new QPushButton{ tr("Reaffirm explanation for current head"), page };
		reaffirm->setObjectName(QStringLiteral("tourReaffirm"));
		column->addWidget(reaffirm);
		connect(reaffirm, &QPushButton::clicked, this, [this] { structural(core::tour_command_kind::reaffirm); });

		column->addWidget(complete);
		connect(complete, &QPushButton::clicked, this, [this] {structural(core::tour_command_kind::complete); if(const auto* stop=selected_stop();stop && stop->Preparation==core::tour_preparation::ready){next_stop(1);} });
		connect(Title, &QLineEdit::textEdited, this, [this] { edit_stop(); });
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { edit_stop(); });
		connect(Outline, &QListWidget::currentRowChanged, this, [this](int aRow) {
			if (Applying || aRow < 0 || !Shown.Draft || static_cast<std::size_t>(aRow) >= Shown.Draft->Stops.size()) {
				return;
			}
			if (Shown.Refusal == core::tour_refusal::bounds) {
				const auto block = QSignalBlocker{ Outline };
				if (const auto* stop = selected_stop()) {
					Outline->setCurrentRow(static_cast<int>(stop - Shown.Draft->Stops.data()));
				}
				show_notice(tr("Shorten the unsaved text or use native Undo before selecting another stop. Your full text is retained."));
				return;
			}
			if (WritingCancel) {
				WritingCancel();
			}
			Selected = Shown.Draft->Stops[static_cast<std::size_t>(aRow)].Id;
			Applying = true;
			present_stops(false);
			Applying = false;
			trigger(tour_editor_action::navigate_stop);
		});
		Roadmap->addTab(page, tr("Tour stops"));
	}
	void review_tour_editor::present_stops(bool aOwnerChanged)
	{
		if (Editing.value()) {
			BodyPositions[Editing] = Body->textCursor().position();
		}
		if (aOwnerChanged) {
			// Detach the native document before destroying another owner's private text.
			auto* blank = new QTextDocument{ Body };
			blank->setDocumentLayout(new QPlainTextDocumentLayout{ blank });
			Body->setDocument(blank);
			for (const auto& [id, document] : BodyDocuments) {
				static_cast<void>(id);
				delete document;
			}
			BodyDocuments.clear();
			BodyPositions.clear();
			Editing = { };
		}
		const auto block = QSignalBlocker{ Outline };
		const auto count = Shown.Draft ? Shown.Draft->Stops.size() : 0U;
		for (std::size_t index = 0; index < count; ++index) {
			const auto& stop  = Shown.Draft->Stops[index];
			const auto  row   = static_cast<int>(index);
			auto        found = row;
			while (found < Outline->count() && Outline->item(found)->data(Qt::UserRole).toULongLong() != stop.Id.value()) {
				++found;
			}
			if (found == Outline->count()) {
				auto* item = new QListWidgetItem;
				item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(stop.Id.value()));
				Outline->insertItem(row, item);
			} else if (found != row) {
				Outline->insertItem(row, Outline->takeItem(found));
			}
			auto*      item  = Outline->item(row);
			const auto state = stop.Preparation == core::tour_preparation::ready ? tr("Ready") : stop.Preparation == core::tour_preparation::needs_attention ? tr("Needs attention")
																																							 : tr("Editing");
			const auto title = stop.Title.empty() ? tr("Untitled stop") : QString::fromStdString(stop.Title);
			item->setText(tr("%1. %2 — %3").arg(row + 1).arg(title, state));
			item->setToolTip(item->text());
		}
		while (Outline->count() > static_cast<int>(count)) {
			delete Outline->takeItem(Outline->count() - 1);
		}
		if (!selected_stop()) {
			Selected = count ? Shown.Draft->Stops.front().Id : core::tour_stop_id{ };
		}
		const auto* stop = selected_stop();
		StopFields->setEnabled(stop != nullptr && !Shown.Loading);
		UndoStructure->setEnabled(Shown.CanUndo);
		RedoStructure->setEnabled(Shown.CanRedo);
		if (!stop) {
			Title->clear();
			Body->clear();
			Source->setText(tr("Select source in the comparison, then add a tour stop. You can keep writing the brief while source is unavailable."));
			Related->clear();
			Editing = { };
			return;
		}
		const auto row = static_cast<int>(stop - Shown.Draft->Stops.data());
		Outline->setCurrentRow(row);
		MoveUp->setEnabled(row > 0);
		MoveDown->setEnabled(row + 1 < Outline->count());
		const bool changed = Editing != stop->Id;
		if (changed) {
			auto& document = BodyDocuments[stop->Id];
			if (!document) {
				document = new QTextDocument{ this };
				document->setDocumentLayout(new QPlainTextDocumentLayout{ document });
				document->setPlainText(QString::fromStdString(stop->Body));
			}
			Body->setDocument(document);
			Editing = stop->Id;
		}
		if (changed || Shown.Refusal == core::tour_refusal::none) {
			if (Title->text() != QString::fromStdString(stop->Title)) {
				Title->setText(QString::fromStdString(stop->Title));
			}
			Body->present(stop->Body);
		}
		if (changed) {
			auto cursor = Body->textCursor();
			cursor.setPosition(std::min(BodyPositions[stop->Id], Body->document()->characterCount() - 1));
			Body->setTextCursor(cursor);
		}
		const auto label = [](const core::tour_source_range& aSource) { return QStringLiteral("%1 · %2:%3–%4 · %5").arg(QString::fromStdString(aSource.Path), aSource.First.Side == core::diff_side::left ? QStringLiteral("base") : QStringLiteral("head")).arg(aSource.First.Line + 1U).arg(aSource.Last.Line + 1U).arg(QString::fromStdString(std::string{ core::canonical_hex(aSource.Commit).view() }).left(12)); };
		Source->setText(stop->Original ? tr("Original: ") + label(*stop->Original) : tr("Source unavailable — your text remains editable."));
		if (!stop->Original && stop->Destination) {
			const auto& destination = *stop->Destination;
			Source->setText(QStringLiteral("%1 · %2–%3 · %4").arg(QString::fromStdString(destination.Path)).arg(destination.FirstLine + 1U).arg(destination.LastLine + 1U).arg(QString::fromStdString(std::string{ core::canonical_hex(destination.Commit).view() }).left(12)));
		}
		if (stop->Mapped) {
			Source->setText(Source->text() + tr("\nProposed mapping: ") + label(*stop->Mapped));
		}
		const auto mapping = stop->Mapping == core::tour_mapping::exact_original ? tr("Exact original") : stop->Mapping == core::tour_mapping::unchanged_material ? tr("Moved with unchanged selected material")
			: stop->Mapping == core::tour_mapping::changed_material                                                                                               ? tr("Changed material")
			: stop->Mapping == core::tour_mapping::ambiguous                                                                                                      ? tr("Ambiguous")
			: stop->Mapping == core::tour_mapping::missing                                                                                                        ? tr("Missing")
			: stop->Mapping == core::tour_mapping::author_selected                                                                                                ? tr("Author-selected replacement")
																																								  : tr("Source unavailable");
		Source->setText(Source->text() + "\n" + mapping);
		if (Shown.Draft->ObservedHead && stop->ReaffirmedHead != Shown.Draft->ObservedHead) {
			Source->setText(Source->text() + tr("\nHead or surrounding contract changed. Reaffirm or revise the explanation before publishing; identical selected bytes are not proof."));
		}
		if (stop->Mapping != core::tour_mapping::exact_original && stop->Mapping != core::tour_mapping::unchanged_material) {
			Source->setText(Source->text() + tr("\nSource needs attention; original destination is retained."));
		}
		for (std::size_t index = 0; index < stop->Related.size(); ++index) {
			if (static_cast<int>(index) >= Related->count()) {
				Related->addItem(label(stop->Related[index]));
			} else {
				Related->item(static_cast<int>(index))->setText(label(stop->Related[index]));
			}
		}
		while (Related->count() > static_cast<int>(stop->Related.size())) {
			delete Related->takeItem(Related->count() - 1);
		}
		for (auto it = BodyDocuments.begin(); it != BodyDocuments.end();) {
			if (std::ranges::find(Shown.Draft->Stops, it->first, &core::tour_stop::Id) == Shown.Draft->Stops.end()) {
				delete it->second;
				BodyPositions.erase(it->first);
				it = BodyDocuments.erase(it);
			} else {
				++it;
			}
		}
	}
	void review_tour_editor::edit_stop()
	{
		if (Applying || !Dispatch || !selected_stop()) {
			return;
		}
		auto value  = *selected_stop();
		value.Title = Title->text().toUtf8().toStdString();
		value.Body  = Body->raw();
		EditingKind = core::tour_command_kind::edit_stop;
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = value.Id, .Value = std::move(value) });
	}
	void review_tour_editor::structural(core::tour_command_kind aKind, std::size_t aPosition)
	{
		if (!Dispatch || !selected_stop()) {
			return;
		}
		if (Shown.Refusal == core::tour_refusal::bounds) {
			show_notice(tr("Shorten the unsaved text or use native Undo before arranging stops. Your full text is retained."));
			return;
		}
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = aKind, .Stop = Selected, .Position = aPosition, .SplitByte = static_cast<std::size_t>(Body->toPlainText().left(Body->textCursor().position()).toUtf8().size()) });
	}
	void review_tour_editor::next_changed_stop()
	{
		if (!Shown.Draft) {
			return;
		}
		const auto& stops   = Shown.Draft->Stops;
		const auto  current = std::ranges::find(stops, Selected, &core::tour_stop::Id);
		const auto  start   = current == stops.end() ? 0U : static_cast<std::size_t>(current - stops.begin() + 1);
		for (std::size_t offset = 0; offset < stops.size(); ++offset) {
			const auto index   = (start + offset) % stops.size();
			const bool changed = std::ranges::any_of(Shown.Draft->ObservedChanges, [&](const auto& change) { return change.Stop == stops[index].Id && change.Kind != core::tour_change_kind::removed; });
			if (changed) {
				show_stops();
				Outline->setCurrentRow(static_cast<int>(index));
				return;
			}
		}
	}

	void review_tour_editor::next_stop(int aOffset)
	{
		const auto row = Outline->currentRow() + aOffset;
		if (row >= 0 && row < Outline->count()) {
			Outline->setCurrentRow(row);
		}
	}
	auto review_tour_editor::minimumSizeHint() const -> QSize { return { 280, 240 }; }
}
