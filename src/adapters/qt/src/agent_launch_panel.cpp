#include "bpr/qt/agent_launch_panel.hpp"

#include "bpr/core/agent_setup.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-1738]]. What a reviewer is told when the agent's
		/// workspace could not be made: what happened, then what to do. Git's
		/// own words stay in the log.
		[[nodiscard]] auto workspace_stop_reason(const application::agent_workspace_unavailable_reason aReason) -> QString
		{
			using reason = application::agent_workspace_unavailable_reason;
			switch (aReason) {
			case reason::copy_failed:
				return QStringLiteral("The agent's copy of this pull request could not be made. Reopen the pull request, then ask again.");
			case reason::download_failed:
				return QStringLiteral("The agent needs every file at this head, and this review holds only the files it shows. Downloading the rest failed. Ask again, or show the log for the cause.");
			case reason::connection_failed:
				return QStringLiteral("The agent needs every file at this head, and GitHub could not be reached to download them. Check the connection, then ask again.");
			case reason::sign_in_refused:
				return QStringLiteral("The agent needs every file at this head, and GitHub refused to send them to the signed-in account. Check that gh is signed in with access to this repository, then ask again.");
			case reason::too_large:
				return QStringLiteral("The agent needs every file at this head, and this head is larger than the agent's private copy allows.");
			}
			return { };
		}
	} // namespace

	agent_launch_panel::agent_launch_panel(
		agent_intent_dispatch aDispatch,
		QWidget* const        aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setObjectName(QStringLiteral("agentLaunchPanel"));
		setAccessibleName(QStringLiteral("AI agent"));

		ProfileSelector = new QComboBox{ this };
		ProfileSelector->setObjectName(QStringLiteral("agentProfileSelector"));
		ProfileSelector->setAccessibleName(QStringLiteral("AI agent provider"));
		ProfileSelector->setPlaceholderText(QStringLiteral("No agent configured"));

		Disclosure = new QLabel{ this };
		Disclosure->setObjectName(QStringLiteral("agentScopeDisclosure"));
		Disclosure->setAccessibleName(QStringLiteral("What the agent will read"));
		Disclosure->setWordWrap(true);
		Disclosure->setTextFormat(Qt::PlainText);
		NoteSelector = new QListWidget{ this };
		NoteSelector->setObjectName(QStringLiteral("agentPrivateNoteSelector"));
		NoteSelector->setAccessibleName(
			QStringLiteral("Private notes included with this request"));
		DraftSelector = new QCheckBox{ this };
		DraftSelector->setObjectName(QStringLiteral("agentDraftSelector"));
		DraftSelector->setAccessibleName(
			QStringLiteral("Include current private draft"));
		DraftSelector->setText(QStringLiteral("No current draft"));
		DraftSelector->setEnabled(false);

		StartAction = new QAction{ QStringLiteral("&Ask the agent"), this };
		StartAction->setObjectName(QStringLiteral("startAgentAction"));
		CancelAction = new QAction{ QStringLiteral("&Stop the agent"), this };
		CancelAction->setObjectName(QStringLiteral("cancelAgentAction"));
		ExistingCheckoutAction = new QAction{
			QStringLiteral("Use my &working copy"),
			this,
		};
		ExistingCheckoutAction->setObjectName(
			QStringLiteral("agentExistingCheckoutAction"));
		ExistingCheckoutAction->setCheckable(true);
		ExistingCheckoutAction->setToolTip(
			QStringLiteral(
				"Let the agent see uncommitted work in your checkout. "
				"Off by default; chosen for this run only."));

		auto* const layout = new QVBoxLayout{ this };
		auto* const form   = new QFormLayout{ };
		form->addRow(QStringLiteral("Provider"), ProfileSelector);
		form->addRow(QStringLiteral("Private notes"), NoteSelector);
		form->addRow(QStringLiteral("Private draft"), DraftSelector);
		layout->addLayout(form);
		layout->addWidget(Disclosure);

		// BPR-0527. Stacked, for the reason BPR-0506 stacked the finding
		// actions and BPR-0520 the defect rows. This panel is mounted in the
		// conversations drawer, which scrolls down and never sideways, so a row
		// wider than the drawer does not push a button offscreen - it puts the
		// button where no gesture reaches. Measured at the drawer's default
		// width, *Stop the agent* began 8 px past the right edge. A column
		// cannot clip: each button is as wide as its own label.
		//
		// This row was also the widest thing in that column - 366 px against
		// the 226 px the stacked version asks for - so it set the width the
		// drawer opened at and then overflowed it by what the vertical scroll
		// bar took, and it stretched the panels above it past the same edge.
		// `workbench_window.cpp` now states the opening width rather than
		// inheriting it from whichever row is widest, which is the other half
		// of this repair.
		//
		// The stretch goes with the row. It held the two run controls at the
		// far edge, away from the run-scope toggle, and a column has nothing to
		// push against. The three actions, their order, their labels, their
		// object names, and the single intent each emits are untouched - this
		// is where they are drawn, not what they are.
		auto* const actions    = new QVBoxLayout{ };
		ExistingCheckoutButton = new quiet_tool_button{ this };
		ExistingCheckoutButton->setDefaultAction(ExistingCheckoutAction);
		ExistingCheckoutButton->setAccessibleName(
			QStringLiteral("Use existing working copy for this run"));
		StartButton = new QToolButton{ this };
		StartButton->setDefaultAction(StartAction);
		StartButton->setAccessibleName(QStringLiteral("Ask selected AI agent"));
		CancelButton = new quiet_tool_button{ this };
		CancelButton->setDefaultAction(CancelAction);
		CancelButton->setAccessibleName(QStringLiteral("Stop running AI agent"));
		actions->addWidget(ExistingCheckoutButton);
		actions->addWidget(StartButton);
		actions->addWidget(CancelButton);
		layout->addLayout(actions);

		SessionCatalog = new QLabel{ QStringLiteral("Agent sessions unavailable"), this };
		SessionCatalog->setObjectName(QStringLiteral("agentSessionCatalog"));
		SessionCatalog->setAccessibleName(QStringLiteral("Latest AI agent session"));
		SessionCatalog->setTextFormat(Qt::PlainText);
		SessionCatalog->setWordWrap(true);
		layout->addWidget(SessionCatalog);
		Status = new QLabel{ QStringLiteral("No agent invocation"), this };
		Status->setObjectName(QStringLiteral("agentInvocationStatus"));
		Status->setAccessibleName(QStringLiteral("AI agent status"));
		Status->setTextFormat(Qt::PlainText);
		layout->addWidget(Status);
		Output = new QPlainTextEdit{ this };
		Output->setObjectName(QStringLiteral("agentInvocationOutput"));
		Output->setAccessibleName(QStringLiteral("AI agent output"));
		Output->setReadOnly(true);
		Output->setPlaceholderText(QStringLiteral("Agent output appears here"));
		layout->addWidget(Output);

		QObject::connect(StartAction, &QAction::triggered, this, [this] {
			const auto profile = selected_profile();
			if (!profile.has_value() || !Dispatch) {
				return;
			}
			++Statistics.StartIntents;
			auto notes = std::vector<application::local_note_id>{ };
			for (auto row = 0; row < NoteSelector->count(); ++row) {
				const auto* const item = NoteSelector->item(row);
				if (item->checkState() == Qt::Checked) {
					notes.emplace_back(item->data(Qt::UserRole).toULongLong());
				}
			}
			Dispatch(start_agent_intent{
				.Profile                = *profile,
				.IncludedNotes          = std::move(notes),
				.IncludedDraft          = DraftSelector->isChecked()
					? OfferedDraft
					: std::nullopt,
				.ExistingCheckoutChosen = ExistingCheckoutAction->isChecked(),
			});
		});
		QObject::connect(CancelAction, &QAction::triggered, this, [this] {
			if (!Dispatch) {
				return;
			}
			++Statistics.CancelIntents;
			Dispatch(cancel_agent_intent{ });
		});
		QObject::connect(
			ProfileSelector,
			&QComboBox::currentIndexChanged,
			this,
			[this](int) { update_actions(); });

		update_actions();
	}

	void agent_launch_panel::set_configuration_observer(std::function<void()> aObserver)
	{
		ConfigurationObserver = std::move(aObserver);
		if (ConfigurationObserver) {
			ConfigurationObserver();
		}
	}

	void agent_launch_panel::set_dispatch(agent_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void agent_launch_panel::offer_profiles(
		const std::span<const core::agent_profile> aProfiles)
	{
		ProfileSelector->clear();
		for (const auto& profile : aProfiles) {
			// [[BPR-PENDING-AQ6]], RFC-BPR-0092 Q4: the menu says the agent's
			// plain name, as the setup does.
			const auto plain = core::plain_name(core::setup_agent_of(profile.Protocol));
			auto       name  = QString::fromUtf8(plain.data(), static_cast<qsizetype>(plain.size()));
			if (profile.Protocol == core::agent_protocol::codex_jsonl_exec) {
				name += QStringLiteral(" (JSONL)");
			}
			ProfileSelector->addItem(name, QVariant::fromValue<qulonglong>(profile.Id.value()));
			ProfileSelector->setItemData(ProfileSelector->count() - 1, QString::fromStdString(profile.DisplayName), Qt::ToolTipRole);
		}
		// A placeholder keeps QComboBox at -1; retain the offered first choice,
		// or the profile setup saved when it is offered.
		if (!aProfiles.empty()) {
			const auto preferred = PreferredProfile ? ProfileSelector->findData(QVariant::fromValue<qulonglong>(PreferredProfile->value())) : -1;
			ProfileSelector->setCurrentIndex(preferred >= 0 ? preferred : 0);
		}
		update_actions();
	}

	void agent_launch_panel::prefer_profile(const std::optional<core::agent_profile_id> aProfile)
	{
		PreferredProfile = aProfile;
		if (!aProfile) {
			return;
		}
		const auto index = ProfileSelector->findData(QVariant::fromValue<qulonglong>(aProfile->value()));
		if (index >= 0 && index != ProfileSelector->currentIndex()) {
			ProfileSelector->setCurrentIndex(index);
			update_actions();
		}
	}

	void agent_launch_panel::offer(
		const std::span<const core::agent_profile> aProfiles)
	{
		offer_profiles(aProfiles);
	}

	void agent_launch_panel::offer_private_context(
		const std::span<const application::local_note_id> aNotes,
		std::optional<application::agent_draft_choice>    aDraft)
	{
		NoteSelector->clear();
		for (const auto id : aNotes) {
			auto* const item = new QListWidgetItem{
				QStringLiteral("Private note #%1").arg(id.value()), NoteSelector
			};
			item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(id.value()));
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Unchecked);
		}
		OfferedDraft = std::move(aDraft);
		DraftSelector->setChecked(false);
		if (OfferedDraft.has_value()) {
			DraftSelector->setText(
				QStringLiteral("Current draft #%1, revision %2")
					.arg(OfferedDraft->Id.value())
					.arg(OfferedDraft->Revision));
		} else {
			DraftSelector->setText(QStringLiteral("No current draft"));
		}
		update_actions();
	}

	void agent_launch_panel::refuse(const std::string_view aReason)
	{
		if (ActiveInvocation.has_value()) {
			return;
		}
		Running = false;
		Output->clear();
		StopReason.clear();
		Status->setText(
			QStringLiteral("Not started: %1")
				.arg(QString::fromUtf8(aReason.data(), static_cast<qsizetype>(aReason.size()))));
		update_actions();
	}

	void agent_launch_panel::admit(
		const application::agent_invocation_projection_identity& aIdentity)
	{
		if (
			ActiveInvocation.has_value()
			&& ActiveInvocation != std::optional{ aIdentity }) {
			return;
		}
		ActiveInvocation = aIdentity;
		Output->clear();
		StopReason.clear();
		Status->setText(QStringLiteral("Preparing agent invocation"));
		Phase = phase::preparing;
		set_running(true);
		progressed();
	}

	auto agent_launch_panel::latest_output_line() const -> QString
	{
		const auto text      = Output->toPlainText().trimmed();
		const auto lineBreak = text.lastIndexOf(QLatin1Char{ '\n' });
		return lineBreak < 0 ? text : text.mid(lineBreak + 1);
	}

	void agent_launch_panel::add_progress_observer(std::function<void()> aObserver)
	{
		if (aObserver) {
			ProgressObservers.push_back(std::move(aObserver));
		}
	}

	void agent_launch_panel::progressed()
	{
		for (const auto& observer : ProgressObservers) {
			observer();
		}
	}

	void agent_launch_panel::start(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::prepared_workspace&                   aWorkspace)
	{
		if (ActiveInvocation != std::optional{ aIdentity }) {
			return;
		}
		Output->clear();
		Status->setText(
			QStringLiteral("Running in %1")
				.arg(QString::fromStdString(aWorkspace.Path)));
		Phase = phase::running;
		set_running(true);
		progressed();
	}

	void agent_launch_panel::stream(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_stream_event&                   aEvent)
	{
		if (
			ActiveInvocation != std::optional{ aIdentity }
			|| aEvent.Session != aIdentity.Session.Id) {
			return;
		}
		Output->appendPlainText(QString::fromStdString(aEvent.Text));
		progressed();
	}

	void agent_launch_panel::finish(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_run_result&                     aResult)
	{
		if (
			ActiveInvocation != std::optional{ aIdentity }
			|| aResult.Session != aIdentity.Session.Id) {
			return;
		}
		if (aResult.Outcome != application::agent_run_outcome::success) {
			Output->clear();
			if (!aResult.StandardError.empty()) {
				Output->setPlainText(
					QString::fromStdString(aResult.StandardError));
			}
		}
		LastOutcome = aResult.Outcome;
		StopReason  = aResult.Outcome == application::agent_run_outcome::workspace_unavailable
			? workspace_stop_reason(aResult.WorkspaceReason.value_or(application::agent_workspace_unavailable_reason::copy_failed))
			: QString{ };
		Status->setText(
			QStringLiteral("Finished: %1")
				.arg(QString::fromUtf8(application::name(aResult.Outcome))));
		Phase = phase::idle;
		set_running(false);
		ActiveInvocation.reset();
		progressed();
	}

	void agent_launch_panel::present_session_catalog(
		const application::agent_session_catalog_projection& aProjection)
	{
		if (!aProjection.Latest.has_value()) {
			SessionCatalog->setText(
				aProjection.Next.has_value()
					? QStringLiteral("No retained agent session; next session #%1")
						  .arg(aProjection.Next->value())
					: QStringLiteral("No retained agent session"));
			return;
		}
		const auto& session = *aProjection.Latest;
		// The views below read these values' own storage, so they are kept for
		// the whole presentation (a view of a temporary dangled: [[BPR-1179]]'s
		// Release run read freed bytes here).
		const auto baseHex = core::canonical_hex(session.Binding.BaseOid);
		const auto headHex = core::canonical_hex(session.Binding.HeadOid);
		const auto base    = baseHex.view();
		const auto head    = headHex.view();
		const auto state   = aProjection.LatestStatus.has_value()
			? core::stable_name(aProjection.LatestStatus->Staleness)
			: std::string_view{ "unclassified" };
		const auto next    = aProjection.Next.has_value()
			? QStringLiteral(" · next #%1").arg(aProjection.Next->value())
			: QString{ };
		SessionCatalog->setText(
			QStringLiteral(
				"Latest session #%1 · profile #%2 · %3 · base %4 · head %5 · "
				"epochs %6–%7%8")
				.arg(session.Id.value())
				.arg(session.Profile.value())
				.arg(QString::fromUtf8(state.data(), static_cast<qsizetype>(state.size())))
				.arg(QString::fromUtf8(base.data(), static_cast<qsizetype>(base.size())))
				.arg(QString::fromUtf8(head.data(), static_cast<qsizetype>(head.size())))
				.arg(session.Binding.LeftEpoch.value())
				.arg(session.Binding.RightEpoch.value())
				.arg(next));
	}

	void agent_launch_panel::disclose_scope(const QString& aScope)
	{
		Disclosure->setText(aScope);
		Disclosed = !aScope.isEmpty();
		++Statistics.Disclosures;
		update_actions();
	}

	void agent_launch_panel::set_running(const bool aRunning)
	{
		Running = aRunning;
		update_actions();
	}

	auto agent_launch_panel::profile_selector() const noexcept -> QComboBox*
	{
		return ProfileSelector;
	}

	auto agent_launch_panel::start_action() const noexcept -> QAction*
	{
		return StartAction;
	}

	auto agent_launch_panel::cancel_action() const noexcept -> QAction*
	{
		return CancelAction;
	}

	auto agent_launch_panel::existing_checkout_action() const noexcept -> QAction*
	{
		return ExistingCheckoutAction;
	}

	auto agent_launch_panel::disclosure_text() const -> QString
	{
		return Disclosure->text();
	}

	auto agent_launch_panel::status_text() const -> QString
	{
		return Status->text();
	}

	auto agent_launch_panel::stop_reason_text() const -> QString
	{
		return StopReason;
	}

	auto agent_launch_panel::output_text() const -> QString
	{
		return Output->toPlainText();
	}

	auto agent_launch_panel::session_catalog_text() const -> QString
	{
		return SessionCatalog->text();
	}

	auto agent_launch_panel::start_button() const noexcept -> QToolButton*
	{
		return StartButton;
	}

	auto agent_launch_panel::note_selector() const noexcept -> QListWidget*
	{
		return NoteSelector;
	}

	auto agent_launch_panel::draft_selector() const noexcept -> QCheckBox*
	{
		return DraftSelector;
	}

	auto agent_launch_panel::selected_profile() const
		-> std::optional<core::agent_profile_id>
	{
		const auto index = ProfileSelector->currentIndex();
		if (index < 0) {
			return std::nullopt;
		}
		const auto value = ProfileSelector->itemData(index);
		if (!value.isValid()) {
			return std::nullopt;
		}
		return core::agent_profile_id{ value.toULongLong() };
	}

	auto agent_launch_panel::statistics() const noexcept
		-> agent_launch_panel_statistics
	{
		return Statistics;
	}

	void agent_launch_panel::update_actions()
	{
		// Start requires a provider *and* a disclosure. A reviewer who cannot
		// see which tool is about to read their repository has not been asked,
		// so the action is unavailable until they can.
		StartAction->setEnabled(
			!Running && Disclosed && selected_profile().has_value());
		// Cancellation is its own action and is offered only while something
		// is running, so stopping is never the same gesture as starting.
		CancelAction->setEnabled(Running);
		ExistingCheckoutAction->setEnabled(!Running);
		NoteSelector->setEnabled(!Running);
		DraftSelector->setEnabled(!Running && OfferedDraft.has_value());
		if (ConfigurationObserver) {
			ConfigurationObserver();
		}
	}
} // namespace bpr::qt
