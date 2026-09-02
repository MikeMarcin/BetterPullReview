#include "bpr/qt/finding_queue_view.hpp"

#include "bpr/core/defect_workflow.hpp"

#include <QAction>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <array>
#include <string_view>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Renders a stable name as presentable text.
		[[nodiscard]] auto text(const std::string_view aName) -> QString
		{
			return QString::fromUtf8(
				aName.data(), static_cast<qsizetype>(aName.size()));
		}

		/// \brief Renders one stated string as presentable text.
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromStdString(aValue);
		}

		/// \brief How sure the agent said it was, in the core's own spelling.
		///
		/// \details A Qt-local rendering because the core deliberately
		/// publishes no name for confidence: nothing in `agent_finding.hpp`
		/// reads it, so nothing there needed to say it. The switch is covered
		/// and has no `default`, so a fifth declared confidence fails to
		/// compile here rather than being drawn as the wrong one.
		[[nodiscard]] auto confidence_name(
			const core::agent_finding_confidence aConfidence) noexcept
			-> std::string_view
		{
			switch (aConfidence) {
			case core::agent_finding_confidence::unstated:
				return "unstated";
			case core::agent_finding_confidence::low:
				return "low";
			case core::agent_finding_confidence::medium:
				return "medium";
			case core::agent_finding_confidence::high:
				return "high";
			}
			return "unstated";
		}

		/// \brief Whether the anchor held, in the core's own spelling.
		[[nodiscard]] auto anchor_state_name(
			const core::agent_finding_anchor_state aState) noexcept
			-> std::string_view
		{
			switch (aState) {
			case core::agent_finding_anchor_state::anchored:
				return "anchored";
			case core::agent_finding_anchor_state::unanchored:
				return "unanchored";
			case core::agent_finding_anchor_state::stale:
				return "stale";
			}
			return "unanchored";
		}

		/// \brief Why the anchor did not hold, in the core's own spelling.
		[[nodiscard]] auto refusal_name(
			const core::agent_finding_refusal aRefusal) noexcept
			-> std::string_view
		{
			switch (aRefusal) {
			case core::agent_finding_refusal::none:
				return "none";
			case core::agent_finding_refusal::unknown_path:
				return "unknown_path";
			case core::agent_finding_refusal::span_past_end:
				return "span_past_end";
			case core::agent_finding_refusal::malformed:
				return "malformed";
			}
			return "malformed";
		}

		/// \brief One action as it is offered: what it is, and what it says.
		struct offered_action
		{
			finding_queue_action Action     = finding_queue_action::open_evidence;
			const char*          Label      = "";
			const char*          ObjectName = "";
		};

		/// \brief The six actions, written out rather than sampled.
		///
		/// \details Each label is distinct because a `QAction` carries no
		/// accessible name of its own and assistive technology reads its text.
		/// "Convert" twice would be one gesture as far as a screen reader is
		/// concerned.
		constexpr auto offered_actions = std::array{
			offered_action{
				finding_queue_action::open_evidence,
				"Open evidence",
				"findingOpenEvidence" },
			offered_action{
				finding_queue_action::ask_follow_up,
				"Ask follow-up",
				"findingAskFollowUp" },
			offered_action{
				finding_queue_action::convert_to_draft_comment,
				"Convert to local draft comment",
				"findingConvertToDraftComment" },
			offered_action{
				finding_queue_action::convert_to_draft_defect,
				"Convert to local draft defect",
				"findingConvertToDraftDefect" },
			offered_action{
				finding_queue_action::add_to_private_notes,
				"Add to private notes",
				"findingAddToPrivateNotes" },
			offered_action{
				finding_queue_action::dismiss,
				"Dismiss",
				"findingDismiss" },
		};

		static_assert(
			offered_actions.size() == finding_queue_action_count,
			"every declared finding action must be offered");

		/// \brief Creates one named field label owned by the view.
		[[nodiscard]] auto make_field(
			QWidget*       aParent,
			const QString& aObjectName,
			const QString& aAccessibleName) -> QLabel*
		{
			auto* const field = new QLabel{ aParent };
			field->setObjectName(aObjectName);
			field->setAccessibleName(aAccessibleName);
			field->setTextFormat(Qt::PlainText);
			field->setWordWrap(true);
			return field;
		}

		/// \brief The row that identifies one finding in the queue.
		///
		/// \details The place, not the claim. The seven disclosed facts are
		/// fields of their own below, and repeating one of them here would
		/// give it two spellings that could disagree.
		[[nodiscard]] auto row_label(
			const std::size_t aOrdinal, const core::agent_finding& aFinding)
			-> QString
		{
			return QStringLiteral("%1. %2:%3-%4")
				.arg(aOrdinal + 1U)
				.arg(text(aFinding.Path))
				.arg(aFinding.Lines.Begin)
				.arg(aFinding.Lines.End);
		}
	} // namespace

	auto stable_name(const finding_queue_action aAction) noexcept
		-> std::string_view
	{
		switch (aAction) {
		case finding_queue_action::open_evidence:
			return "open_evidence";
		case finding_queue_action::ask_follow_up:
			return "ask_follow_up";
		case finding_queue_action::convert_to_draft_comment:
			return "convert_to_draft_comment";
		case finding_queue_action::convert_to_draft_defect:
			return "convert_to_draft_defect";
		case finding_queue_action::add_to_private_notes:
			return "add_to_private_notes";
		case finding_queue_action::dismiss:
			return "dismiss";
		}
		return "open_evidence";
	}

	finding_queue_view::finding_queue_view(
		finding_queue_intent_dispatch aDispatch,
		QWidget* const                aParent)
		: QWidget{ aParent }
	{
		Members.Dispatch = std::move(aDispatch);

		setObjectName(QStringLiteral("findingQueueView"));
		setAccessibleName(QStringLiteral("AI finding queue"));

		auto* const layout = new QVBoxLayout{ this };

		Members.Findings = new QListWidget{ this };
		Members.Findings->setObjectName(QStringLiteral("findingQueueList"));
		Members.Findings->setAccessibleName(QStringLiteral("Delivered findings"));
		layout->addWidget(Members.Findings);

		// One field per disclosed fact. A reviewer who has to read an anchor
		// state out of a sentence about a claim has been shown neither.
		Members.Severity = make_field(
			this,
			QStringLiteral("findingSeverity"),
			QStringLiteral("Finding severity"));
		Members.Confidence = make_field(
			this,
			QStringLiteral("findingConfidence"),
			QStringLiteral("Stated agent confidence"));
		Members.Category = make_field(
			this,
			QStringLiteral("findingCategory"),
			QStringLiteral("Finding category"));
		Members.Claim = make_field(
			this,
			QStringLiteral("findingClaim"),
			QStringLiteral("What the agent claims"));
		Members.Evidence = make_field(
			this,
			QStringLiteral("findingEvidence"),
			QStringLiteral("Evidence quoted for the claim"));
		Members.Consequence = make_field(
			this,
			QStringLiteral("findingConsequence"),
			QStringLiteral("Consequence the agent describes"));
		Members.AnchorState = make_field(
			this,
			QStringLiteral("findingAnchorState"),
			QStringLiteral("Whether the anchor held"));

		auto* const details = new QFormLayout{ };
		details->addRow(QStringLiteral("Severity"), Members.Severity);
		details->addRow(QStringLiteral("Confidence"), Members.Confidence);
		details->addRow(QStringLiteral("Category"), Members.Category);
		details->addRow(QStringLiteral("Claim"), Members.Claim);
		details->addRow(QStringLiteral("Evidence"), Members.Evidence);
		details->addRow(QStringLiteral("Consequence"), Members.Consequence);
		details->addRow(QStringLiteral("Anchor"), Members.AnchorState);
		layout->addLayout(details);

		for (const auto& offered : offered_actions) {
			auto* const action = new QAction{
				QString::fromUtf8(offered.Label), this
			};
			action->setObjectName(QString::fromUtf8(offered.ObjectName));
			addAction(action);
			const auto chosen = offered.Action;
			connect(action, &QAction::triggered, this, [this, chosen]() {
				emit_action(chosen);
			});
			Members.Actions.at(static_cast<std::size_t>(offered.Action))
				= action;
		}

		connect(
			Members.Findings,
			&QListWidget::currentRowChanged,
			this,
			[this](const int aRow) {
				if (Members.Applying) {
					return;
				}
				++Members.Statistics.Selections;
				Members.Selected = aRow < 0
					? std::optional<std::size_t>{ }
					: std::optional<std::size_t>{
						  static_cast<std::size_t>(aRow)
					  };
				refresh_details();
				refresh_enablement();
			});

		refresh_details();
		refresh_enablement();
	}

	void finding_queue_view::set_dispatch(finding_queue_intent_dispatch aDispatch)
	{
		Members.Dispatch = std::move(aDispatch);
	}

	void finding_queue_view::present(
		const application::agent_finding_delivery& aDelivery)
	{
		++Members.Statistics.Presentations;
		Members.Shown = aDelivery;

		Members.Applying = true;
		Members.Findings->clear();
		for (auto index = std::size_t{ 0U }; index < aDelivery.Findings.size();
			 ++index) {
			Members.Findings->addItem(
				row_label(index, aDelivery.Findings.at(index).Finding));
		}
		// The selection survives a delivery that still has a row at it, so an
		// accepted refresh does not move the reviewer somewhere they did not
		// ask to be.
		if (Members.Selected && *Members.Selected >= aDelivery.Findings.size()) {
			Members.Selected.reset();
		}
		Members.Findings->setCurrentRow(
			Members.Selected ? static_cast<int>(*Members.Selected) : -1);
		Members.Applying = false;

		refresh_details();
		refresh_enablement();
	}

	void finding_queue_view::select(const std::size_t aOrdinal)
	{
		if (!Members.Shown || aOrdinal >= Members.Shown->Findings.size()) {
			return;
		}
		++Members.Statistics.Selections;
		Members.Selected = aOrdinal;
		Members.Applying = true;
		Members.Findings->setCurrentRow(static_cast<int>(aOrdinal));
		Members.Applying = false;
		refresh_details();
		refresh_enablement();
	}

	auto finding_queue_view::selected_ordinal() const noexcept
		-> std::optional<std::size_t>
	{
		return Members.Selected;
	}

	auto finding_queue_view::finding_count() const noexcept -> std::size_t
	{
		return Members.Shown ? Members.Shown->Findings.size() : std::size_t{ 0U };
	}

	auto finding_queue_view::findings_text() const -> QString
	{
		auto rows = QStringList{ };
		for (auto row = 0; row < Members.Findings->count(); ++row) {
			rows.append(Members.Findings->item(row)->text());
		}
		return rows.join(QLatin1Char{ '\n' });
	}

	auto finding_queue_view::severity_text() const -> QString
	{
		return Members.Severity->text();
	}

	auto finding_queue_view::confidence_text() const -> QString
	{
		return Members.Confidence->text();
	}

	auto finding_queue_view::category_text() const -> QString
	{
		return Members.Category->text();
	}

	auto finding_queue_view::claim_text() const -> QString
	{
		return Members.Claim->text();
	}

	auto finding_queue_view::evidence_text() const -> QString
	{
		return Members.Evidence->text();
	}

	auto finding_queue_view::consequence_text() const -> QString
	{
		return Members.Consequence->text();
	}

	auto finding_queue_view::anchor_state_text() const -> QString
	{
		return Members.AnchorState->text();
	}

	auto finding_queue_view::findings_list() const noexcept -> QListWidget*
	{
		return Members.Findings;
	}

	auto finding_queue_view::action(
		const finding_queue_action aAction) const noexcept -> QAction*
	{
		return Members.Actions.at(static_cast<std::size_t>(aAction));
	}

	auto finding_queue_view::statistics() const noexcept
		-> finding_queue_view_statistics
	{
		return Members.Statistics;
	}

	auto finding_queue_view::selected_finding() const
		-> std::optional<queued_finding>
	{
		if (!Members.Shown || !Members.Selected
			|| *Members.Selected >= Members.Shown->Findings.size()) {
			return std::nullopt;
		}
		const auto& delivered = Members.Shown->Findings.at(*Members.Selected);
		return queued_finding{
			.Session = Members.Shown->Session,
			.Ordinal = *Members.Selected,
			.Finding = delivered.Finding,
			.Verdict = delivered.Verdict,
		};
	}

	void finding_queue_view::emit_action(const finding_queue_action aAction)
	{
		const auto subject = selected_finding();
		if (!subject || !Members.Dispatch) {
			return;
		}
		++Members.Statistics.ActionIntents;
		// The whole of it: this widget asks, and the core decides what the
		// answer becomes. Nothing is promoted here and nothing is stored.
		Members.Dispatch(finding_action_intent{
			.Action  = aAction,
			.Subject = *subject,
		});
	}

	void finding_queue_view::refresh_details()
	{
		const auto subject = selected_finding();
		if (!subject) {
			Members.Severity->clear();
			Members.Confidence->clear();
			Members.Category->clear();
			Members.Claim->clear();
			Members.Evidence->clear();
			Members.Consequence->clear();
			Members.AnchorState->clear();
			return;
		}

		const auto& finding = subject->Finding;
		Members.Severity->setText(text(core::name(finding.Severity)));
		Members.Confidence->setText(text(confidence_name(finding.Confidence)));
		Members.Category->setText(text(core::name(finding.Category)));
		Members.Claim->setText(text(finding.Claim));
		Members.Evidence->setText(text(finding.Evidence));
		Members.Consequence->setText(text(finding.Consequence));
		Members.AnchorState->setText(
			QStringLiteral("%1 (%2)")
				.arg(text(anchor_state_name(subject->Verdict.State)))
				.arg(text(refusal_name(subject->Verdict.Refusal))));
	}

	void finding_queue_view::refresh_enablement()
	{
		// Every action is offered against every finding. Whether a promotion
		// is legal is `core::promote_agent_finding`'s answer, and a widget
		// that pre-empted it could disagree with it.
		const auto selected = selected_finding().has_value();
		for (auto* const action : Members.Actions) {
			action->setEnabled(selected);
		}
	}
} // namespace bpr::qt
