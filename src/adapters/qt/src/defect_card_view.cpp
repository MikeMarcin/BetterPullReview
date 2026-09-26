#include "bpr/qt/defect_card_view.hpp"

#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto make_action(
			QWidget*       aParent,
			const QString& aText,
			const QString& aObjectName) -> QAction*
		{
			auto* const action = new QAction{ aText, aParent };
			action->setObjectName(aObjectName);
			aParent->addAction(action);
			return action;
		}
	} // namespace

	auto badge_for(const core::defect_state aState) -> QString
	{
		switch (aState) {
		case core::defect_state::open:
			return QStringLiteral("[open]");
		case core::defect_state::fix_claimed:
			return QStringLiteral("[fix claimed]");
		case core::defect_state::needs_verification:
			return QStringLiteral("[needs verification]");
		case core::defect_state::verified:
			return QStringLiteral("[verified]");
		case core::defect_state::reopened:
			return QStringLiteral("[reopened]");
		case core::defect_state::withdrawn:
			return QStringLiteral("[withdrawn]");
		case core::defect_state::accepted_as_is:
			return QStringLiteral("[accepted as is]");
		}
		return QStringLiteral("[open]");
	}

	auto explanation_for(const core::defect_state aState) -> QString
	{
		switch (aState) {
		case core::defect_state::open:
			return QStringLiteral("Nobody has addressed this yet.");
		case core::defect_state::fix_claimed:
			return QStringLiteral("The author says this is fixed. You have not "
								  "checked it.");
		case core::defect_state::needs_verification:
			return QStringLiteral("Something suggests this was addressed. You "
								  "have not checked it.");
		case core::defect_state::verified:
			return QStringLiteral("You checked this and accepted the fix.");
		case core::defect_state::reopened:
			return QStringLiteral("You reopened this after verifying it.");
		case core::defect_state::withdrawn:
			return QStringLiteral("You withdrew this.");
		case core::defect_state::accepted_as_is:
			return QStringLiteral("You accepted this as it stands.");
		}
		return QStringLiteral("Nobody has addressed this yet.");
	}

	defect_card_view::defect_card_view(dispatch aDispatch, QWidget* const aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		auto* const layout = new QVBoxLayout{ this };

		StateLabel = new QLabel{ this };
		StateLabel->setObjectName(QStringLiteral("defectState"));
		StateLabel->setAccessibleName(QStringLiteral("Defect state"));
		StateLabel->setWordWrap(true);
		layout->addWidget(StateLabel);

		ClassificationLabel = new QLabel{ this };
		ClassificationLabel->setObjectName(QStringLiteral("defectClassification"));
		ClassificationLabel->setAccessibleName(
			QStringLiteral("Local severity and category"));
		layout->addWidget(ClassificationLabel);
		ContextLabel = new QLabel{ this };
		ContextLabel->setObjectName(QStringLiteral("defectContextAvailability"));
		ContextLabel->setAccessibleName(QStringLiteral("Defect context availability"));
		ContextLabel->setWordWrap(true);
		layout->addWidget(ContextLabel);

		Severity = new QComboBox{ this };
		Severity->setObjectName(QStringLiteral("defectSeverity"));
		Severity->setAccessibleName(QStringLiteral("Local defect severity"));
		Severity->setAccessibleDescription(
			QStringLiteral("Choose a private severity for the selected defect"));
		for (const auto value : { core::defect_severity::trivial,
								  core::defect_severity::minor, core::defect_severity::major,
								  core::defect_severity::critical }) {
			Severity->addItem(QString::fromUtf8(core::name(value)), static_cast<int>(value));
		}
		layout->addWidget(Severity);
		Category = new QComboBox{ this };
		Category->setObjectName(QStringLiteral("defectCategory"));
		Category->setAccessibleName(QStringLiteral("Local defect category"));
		Category->setAccessibleDescription(
			QStringLiteral("Choose a private category for the selected defect"));
		for (const auto value : { core::defect_category::correctness,
								  core::defect_category::security, core::defect_category::performance,
								  core::defect_category::maintainability, core::defect_category::style,
								  core::defect_category::documentation, core::defect_category::other }) {
			Category->addItem(QString::fromUtf8(core::name(value)), static_cast<int>(value));
		}
		layout->addWidget(Category);
		Reclassify = make_action(
			this, tr("Apply local classification"), QStringLiteral("defectReclassify"));

		Verify = make_action(
			this,
			tr("Verify this fix"),
			QStringLiteral("defectVerify"));
		Reopen = make_action(
			this,
			tr("Reopen this defect"),
			QStringLiteral("defectReopen"));
		Withdraw = make_action(
			this,
			tr("Withdraw this defect"),
			QStringLiteral("defectWithdraw"));
		AcceptAsIs = make_action(
			this,
			tr("Accept as is"),
			QStringLiteral("defectAcceptAsIs"));

		connect(Verify, &QAction::triggered, this, [this]() {
			emit_command(core::defect_command_kind::verify);
		});
		connect(Reopen, &QAction::triggered, this, [this]() {
			emit_command(core::defect_command_kind::reopen);
		});
		connect(Withdraw, &QAction::triggered, this, [this]() {
			emit_command(core::defect_command_kind::withdraw);
		});
		connect(AcceptAsIs, &QAction::triggered, this, [this]() {
			emit_command(core::defect_command_kind::accept_as_is);
		});
		connect(Reclassify, &QAction::triggered, this, [this]() {
			emit_reclassification();
		});

		refresh();
	}

	auto defect_card_view::state_label() const noexcept -> QLabel*
	{
		return StateLabel;
	}

	auto defect_card_view::classification_label() const noexcept -> QLabel*
	{
		return ClassificationLabel;
	}

	auto defect_card_view::context_label() const noexcept -> QLabel*
	{
		return ContextLabel;
	}

	auto defect_card_view::severity_selector() const noexcept -> QComboBox*
	{
		return Severity;
	}

	auto defect_card_view::category_selector() const noexcept -> QComboBox*
	{
		return Category;
	}

	auto defect_card_view::reclassify_action() const noexcept -> QAction*
	{
		return Reclassify;
	}

	auto defect_card_view::verify_action() const noexcept -> QAction*
	{
		return Verify;
	}

	auto defect_card_view::reopen_action() const noexcept -> QAction*
	{
		return Reopen;
	}

	auto defect_card_view::withdraw_action() const noexcept -> QAction*
	{
		return Withdraw;
	}

	auto defect_card_view::accept_as_is_action() const noexcept -> QAction*
	{
		return AcceptAsIs;
	}

	auto defect_card_view::shown_defect() const noexcept
		-> const std::optional<core::defect_record>&
	{
		return ShownDefect;
	}

	auto defect_card_view::statistics() const noexcept -> defect_card_statistics
	{
		return Statistics;
	}

	void defect_card_view::emit_command(const core::defect_command_kind aKind)
	{
		if (!ShownDefect || !Dispatch) {
			return;
		}
		++Statistics.Intents;
		Dispatch(core::defect_command{ .Id = ShownDefect->Id, .Kind = aKind });
	}

	void defect_card_view::emit_reclassification()
	{
		if (!ShownDefect || !Dispatch) {
			return;
		}
		++Statistics.Intents;
		Dispatch(core::defect_command{
			.Id       = ShownDefect->Id,
			.Kind     = core::defect_command_kind::reclassify,
			.Severity = static_cast<core::defect_severity>(Severity->currentData().toInt()),
			.Category = static_cast<core::defect_category>(Category->currentData().toInt()),
		});
	}

	void defect_card_view::present(const core::defect_record& aDefect)
	{
		++Statistics.Presentations;
		ShownDefect  = aDefect;
		ShownHistory = core::defect_history{
			.Defect = aDefect, .OriginalAnchor = aDefect.Anchor
		};
		refresh();
	}

	void defect_card_view::present(const core::defect_history& aHistory)
	{
		++Statistics.Presentations;
		ShownDefect  = aHistory.Defect;
		ShownHistory = aHistory;
		refresh();
	}

	void defect_card_view::refresh()
	{
		if (!ShownDefect) {
			StateLabel->clear();
			ClassificationLabel->clear();
			ContextLabel->clear();
			Severity->setEnabled(false);
			Category->setEnabled(false);
			Reclassify->setEnabled(false);
			Verify->setEnabled(false);
			Reopen->setEnabled(false);
			Withdraw->setEnabled(false);
			AcceptAsIs->setEnabled(false);
			return;
		}

		// The badge leads, so the state reads without consulting a colour.
		StateLabel->setText(
			badge_for(ShownDefect->State) + QLatin1Char{ ' ' }
			+ explanation_for(ShownDefect->State));

		// Severity and category are named as local, because they are never in
		// the published comment and a reviewer should not expect a
		// GitHub-only collaborator to have seen them.
		ClassificationLabel->setText(
			tr("Local classification: %1, %2")
				.arg(QString::fromUtf8(core::name(ShownDefect->Severity).data(),
									   static_cast<qsizetype>(
										   core::name(ShownDefect->Severity).size())))
				.arg(QString::fromUtf8(core::name(ShownDefect->Category).data(),
									   static_cast<qsizetype>(
										   core::name(ShownDefect->Category).size()))));
		ContextLabel->setText(
			ShownHistory.has_value() && !core::context_available(*ShownHistory)
				? tr("Shared context unavailable; the original semantic anchor is retained.")
				: tr("Shared context available at the retained semantic anchor."));
		Severity->setCurrentIndex(Severity->findData(static_cast<int>(ShownDefect->Severity)));
		Category->setCurrentIndex(Category->findData(static_cast<int>(ShownDefect->Category)));

		// Enablement mirrors the core's legality rather than restating it: an
		// action offered where the core would refuse it teaches the reviewer
		// that the tool is unreliable.
		const auto state = ShownDefect->State;
		Verify->setEnabled(
			state == core::defect_state::fix_claimed
			|| state == core::defect_state::needs_verification);
		Reopen->setEnabled(state == core::defect_state::verified);
		const auto settled = state == core::defect_state::withdrawn
			|| state == core::defect_state::accepted_as_is;
		Withdraw->setEnabled(!settled);
		AcceptAsIs->setEnabled(!settled);
		Severity->setEnabled(!settled);
		Category->setEnabled(!settled);
		Reclassify->setEnabled(!settled);
		for (auto* action : { Verify, Reopen, Withdraw, AcceptAsIs, Reclassify }) {
			action->setToolTip(action->isEnabled() ? action->text() : tr("Unavailable for a defect in state %1. %2").arg(badge_for(state), explanation_for(state)));
		}
	}
} // namespace bpr::qt
