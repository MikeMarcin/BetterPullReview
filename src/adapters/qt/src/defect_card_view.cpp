#include "bpr/qt/defect_card_view.hpp"

#include <QAction>
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

	void defect_card_view::present(const core::defect_record& aDefect)
	{
		++Statistics.Presentations;
		ShownDefect = aDefect;
		refresh();
	}

	void defect_card_view::refresh()
	{
		if (!ShownDefect) {
			StateLabel->clear();
			ClassificationLabel->clear();
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
	}
} // namespace bpr::qt
