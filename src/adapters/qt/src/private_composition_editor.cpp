#include "bpr/qt/private_composition_editor.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
namespace bpr::qt {
	namespace {
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		auto digest(const core::private_body_digest& aValue) -> QString
		{
			QString result;
			for (auto byte : aValue.bytes()) {
				result += QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0'));
			}
			return result;
		}
	}
	private_composition_editor::private_composition_editor(QWidget* aParent)
		: QDialog(aParent)
	{
		if (aParent != nullptr) {
			setPalette(aParent->palette());
			aParent->installEventFilter(this);
		}
		setObjectName(QStringLiteral("privateCompositionEditor"));
		setWindowTitle(tr("Squash to Draft · private composition"));
		setModal(false);
		resize(640, 720);
		auto* outer  = new QVBoxLayout(this);
		auto* scroll = new QScrollArea(this);
		scroll->setWidgetResizable(true);
		auto* page   = new QWidget(scroll);
		auto* layout = new QVBoxLayout(page);
		scroll->setWidget(page);
		outer->addWidget(scroll);
		auto* rail = new QLabel(tr("Local only · not visible on GitHub. Accept as Draft saves locally; Publish is a separate action in the draft editor."), page);
		rail->setWordWrap(true);
		rail->setTextFormat(Qt::PlainText);
		layout->addWidget(rail);
		Status = new QLabel(page);
		Status->setObjectName(QStringLiteral("privateCompositionStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		layout->addWidget(Status);
		Sources = new QPlainTextEdit(page);
		Sources->setObjectName(QStringLiteral("privateCompositionSources"));
		Sources->setAccessibleName(tr("Exact sources, currentness and body digests"));
		Sources->setReadOnly(true);
		layout->addWidget(Sources);
		auto* form = new QFormLayout;
		layout->addLayout(form);
		Mode = new QComboBox(page);
		Mode->setObjectName(QStringLiteral("privateCompositionMode"));
		Mode->addItems({ tr("Reviewer authored"), tr("Exact copy of one source"), tr("Explicit agent-assisted composition") });
		Mode->setAccessibleName(tr("Composition mode"));
		form->addRow(tr("Mode"), Mode);
		Workspace = new QComboBox(page);
		Workspace->setObjectName(QStringLiteral("privateCompositionWorkspace"));
		Workspace->setAccessibleName(tr("Split composition workspace"));
		Workspace->setMinimumWidth(220);
		form->addRow(tr("Workspace"), Workspace);
		Target = new QComboBox(page);
		Target->setObjectName(QStringLiteral("privateCompositionTarget"));
		Target->setAccessibleName(tr("Explicit exact target, never a guessed line"));
		Target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		Target->setMinimumWidth(360);
		form->addRow(tr("Target"), Target);
		Profile = new QComboBox(page);
		Profile->setObjectName(QStringLiteral("privateCompositionProfile"));
		Profile->setAccessibleName(tr("Agent profile for explicit composition"));
		form->addRow(tr("Agent profile"), Profile);
		Model = new QLineEdit(page);
		Model->setObjectName(QStringLiteral("privateCompositionModel"));
		Model->setAccessibleName(tr("Requested model, optional; not a runtime claim"));
		form->addRow(tr("Requested model"), Model);
		Effort = new QLineEdit(page);
		Effort->setObjectName(QStringLiteral("privateCompositionEffort"));
		Effort->setAccessibleName(tr("Requested effort, optional; not a runtime claim"));
		form->addRow(tr("Requested effort"), Effort);
		Actor = new QLabel(page);
		Actor->setTextFormat(Qt::PlainText);
		Actor->setWordWrap(true);
		layout->addWidget(Actor);
		Body = new QPlainTextEdit(page);
		Body->setObjectName(QStringLiteral("privateCompositionBody"));
		Body->setAccessibleName(tr("Candidate prose, no automatic public metadata"));
		layout->addWidget(Body);
		auto button = [&](const QString& aLabel, const QString& aName, application::private_composition_action aAction) {
			QPushButton* result = aAction == application::private_composition_action::target || aAction == application::private_composition_action::cancel
				? static_cast<QPushButton*>(new quiet_push_button{ aLabel, page })
				: new QPushButton{ aLabel, page };
			result->setObjectName(aName);
			result->setAccessibleName(aLabel);
			layout->addWidget(result);
			connect(result, &QPushButton::clicked, this, [this, aAction] { dispatch(aAction); });
			return result;
		};
		button(tr("Create local candidate"), QStringLiteral("privateCompositionCreate"), application::private_composition_action::create);
		button(tr("Split into exact target drafts"), QStringLiteral("privateCompositionSplit"), application::private_composition_action::split);
		button(tr("Use selected exact target"), QStringLiteral("privateCompositionSetTarget"), application::private_composition_action::target);
		button(tr("Save reviewer revision"), QStringLiteral("privateCompositionRevise"), application::private_composition_action::revise);
		button(tr("Ask selected agent to compose"), QStringLiteral("privateCompositionAsk"), application::private_composition_action::ask_agent);
		Accept = button(tr("Accept as Draft · save locally"), QStringLiteral("privateCompositionAccept"), application::private_composition_action::accept);
		button(tr("Cancel composition view"), QStringLiteral("privateCompositionCancel"), application::private_composition_action::cancel);
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { if (!Applying) { update_accept(); } });
		connect(Workspace, &QComboBox::activated, this, [this] { dispatch(application::private_composition_action::select); });
		connect(this, &QDialog::rejected, this, [this] { dispatch(application::private_composition_action::cancel); });
	}
	bool private_composition_editor::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (auto* parent = parentWidget(); aWatched == parent && aEvent->type() == QEvent::PaletteChange) {
			setPalette(parent->palette());
		}
		return QDialog::eventFilter(aWatched, aEvent);
	}
	void private_composition_editor::set_dispatch(application::private_composition_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void private_composition_editor::show_status(std::string_view aStatus) { Status->setText(text(aStatus)); }
	void private_composition_editor::present(const application::private_composition_projection& aProjection)
	{
		Applying                    = true;
		const auto candidateChanged = !Shown.Candidate || !aProjection.Candidate || Shown.Candidate->Identity != aProjection.Candidate->Identity;
		Shown                       = aProjection;
		QString sources;
		for (const auto& source : aProjection.Sources) {
			sources += tr("Source %1 · %2 · %3 bytes\nDigest %4\n").arg(source.Source.Order.value()).arg(text(core::stable_name(source.Availability))).arg(source.Source.BodyBytes).arg(digest(source.Source.Digest));
		}
		Sources->setPlainText(sources);
		Status->setText(text(aProjection.Status));
		Workspace->clear();
		for (const auto& state : aProjection.Workspaces) {
			Workspace->addItem(tr("Composition %1").arg(state.Id.value()), QVariant::fromValue<qulonglong>(state.Id.value()));
			if (aProjection.Current && aProjection.Current->Id == state.Id) {
				Workspace->setCurrentIndex(Workspace->count() - 1);
			}
		}
		Target->clear();
		Target->addItem(tr("Choose an exact target or split; unplaceable sources need a new selection"));
		for (const auto& target : aProjection.Targets) {
			const auto head = text(core::canonical_hex(target.Binding.Revision.HeadOid).view());
			Target->addItem(tr("%1 side, line %2 · head %3").arg(target.Line.Side == core::diff_side::right ? tr("Right") : tr("Left")).arg(target.Line.Line + 1).arg(head.left(12)));
			Target->setItemData(Target->count() - 1, tr("Full head: %1").arg(head), Qt::ToolTipRole);
			if (aProjection.Current && aProjection.Current->Target == target) {
				Target->setCurrentIndex(Target->count() - 1);
			}
		}
		const auto profile = Profile->currentData();
		Profile->clear();
		for (const auto& item : aProjection.Profiles) {
			Profile->addItem(text(item.second), QVariant::fromValue<qulonglong>(item.first.value()));
		}
		if (Profile->findData(profile) >= 0) {
			Profile->setCurrentIndex(Profile->findData(profile));
		}
		if (aProjection.Current) {
			Mode->setCurrentIndex(static_cast<int>(aProjection.Current->Mode));
		}
		if (candidateChanged) {
			Body->setPlainText(aProjection.Candidate ? text(aProjection.Candidate->Body) : QString{ });
		}
		if (aProjection.Candidate) {
			const auto* agent = std::get_if<core::private_agent_actor>(&aProjection.Candidate->Actor);
			Actor->setText(agent ? tr("Candidate by agent run %1; %2 immutable derivation sources").arg(agent->Run.value()).arg(aProjection.Candidate->Derivation.size()) : tr("Reviewer candidate; %1 immutable derivation sources").arg(aProjection.Candidate->Derivation.size()));
		} else {
			Actor->setText(tr("No candidate yet. Create one using the chosen mode."));
		}
		setEnabled(!aProjection.Busy);
		update_accept();
		Applying = false;
	}
	void private_composition_editor::update_accept()
	{
		const auto pendingEdit = Shown.Candidate && Body->toPlainText().toStdString() != Shown.Candidate->Body;
		Accept->setEnabled(!Shown.Busy && Shown.Candidate && Shown.Current && Shown.Current->Target && Shown.Current->Lifecycle == core::review_composition_lifecycle::editing && !pendingEdit);
		Status->setText(pendingEdit ? tr("Save reviewer revision before Accept as Draft. Your visible edit is still local to this editor.") : text(Shown.Status));
	}
	void private_composition_editor::dispatch(application::private_composition_action aAction)
	{
		if (Applying || !Dispatch || Shown.Busy) {
			return;
		}
		if (aAction == application::private_composition_action::accept && !Accept->isEnabled()) {
			return;
		}
		auto intent = application::private_composition_intent{ .Action = aAction, .Generation = Shown.Generation, .Mode = static_cast<core::review_composition_mode>(Mode->currentIndex()), .Composition = Shown.Current ? std::optional{ Shown.Current->Id } : std::nullopt, .ExpectedVersion = Shown.Version, .Candidate = Shown.Candidate ? std::optional{ Shown.Candidate->Identity } : std::nullopt, .Profile = core::agent_profile_id{ Profile->currentData().toULongLong() }, .Body = Body->toPlainText().toStdString() };
		if (aAction == application::private_composition_action::select) {
			intent.Composition = core::review_composition_id{ Workspace->currentData().toULongLong() };
		}
		if (Target->currentIndex() > 0 && static_cast<std::size_t>(Target->currentIndex()) <= Shown.Targets.size()) {
			intent.Target = Shown.Targets[static_cast<std::size_t>(Target->currentIndex() - 1)];
		}
		if (!Model->text().isEmpty()) {
			intent.Requested.Model = Model->text().toStdString();
		}
		if (!Effort->text().isEmpty()) {
			intent.Requested.Effort = Effort->text().toStdString();
		}
		Dispatch(std::move(intent));
		if (aAction == application::private_composition_action::cancel) {
			hide();
		}
	}
}
