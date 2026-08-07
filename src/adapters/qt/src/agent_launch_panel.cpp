#include "bpr/qt/agent_launch_panel.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>

namespace bpr::qt {
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

		Disclosure = new QLabel{ this };
		Disclosure->setObjectName(QStringLiteral("agentScopeDisclosure"));
		Disclosure->setAccessibleName(QStringLiteral("What the agent will read"));
		Disclosure->setWordWrap(true);
		Disclosure->setTextFormat(Qt::PlainText);

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
		layout->addLayout(form);
		layout->addWidget(Disclosure);

		QObject::connect(StartAction, &QAction::triggered, this, [this] {
			const auto profile = selected_profile();
			if (!profile.has_value() || !Dispatch) {
				return;
			}
			++Statistics.StartIntents;
			Dispatch(start_agent_intent{
				.Profile                = *profile,
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

	void agent_launch_panel::set_dispatch(agent_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void agent_launch_panel::offer_profiles(
		const std::vector<core::agent_profile>& aProfiles)
	{
		ProfileSelector->clear();
		for (const auto& profile : aProfiles) {
			ProfileSelector->addItem(
				QString::fromStdString(profile.DisplayName),
				QVariant::fromValue<qulonglong>(profile.Id.value()));
		}
		update_actions();
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
	}
} // namespace bpr::qt
