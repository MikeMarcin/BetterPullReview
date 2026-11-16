#include "bpr/qt/local_check_card.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "wrapping_row.hpp"

#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		auto label(const QString& aText, QWidget* aParent, const QString& aName = { }) -> QLabel*
		{
			auto* const line = new QLabel{ aText, aParent };
			line->setTextFormat(Qt::PlainText);
			line->setWordWrap(true);
			line->setTextInteractionFlags(Qt::TextSelectableByMouse);
			if (!aName.isEmpty()) {
				line->setObjectName(aName);
			}
			return line;
		}

		auto quieter(QLabel* aLabel) -> QLabel*
		{
			aLabel->setForegroundRole(QPalette::PlaceholderText);
			auto face = aLabel->font();
			face.setPointSizeF(face.pointSizeF() * 0.92);
			aLabel->setFont(face);
			return aLabel;
		}

		auto from(const std::string& aText) -> QString { return QString::fromStdString(aText); }
	} // namespace

	local_check_card::local_check_card(const core::local_check& aCheck, const QString& aBlocked, const std::uint64_t aLatest, const local_check_toggles& aToggles, local_check_card_dispatch aDispatch,
									   QWidget* const aParent)
		: QFrame{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setFrameShape(QFrame::StyledPanel);
		setAutoFillBackground(true);
		setBackgroundRole(QPalette::Base);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 10, 12, 10);
		column->setSpacing(6);
		if (aCheck.Plan) {
			setObjectName(QStringLiteral("localCheckPlanCard"));
			build_plan(*aCheck.Plan, aBlocked, aToggles);
		} else if (aCheck.Report) {
			setObjectName(QStringLiteral("localCheckReportCard"));
			build_report(*aCheck.Report, aLatest);
		}
	}

	auto local_check_card::outcome_colour(const core::local_check_outcome aOutcome, const QPalette& aPalette) -> QColor
	{
		const auto dark = aPalette.color(QPalette::Base).lightnessF() < 0.5;
		switch (aOutcome) {
		case core::local_check_outcome::passed:       return dark ? QColor{ 0x7c, 0xc8, 0x94 } : QColor{ 0x1f, 0x7a, 0x45 };
		case core::local_check_outcome::failed:       return dark ? QColor{ 0xf2, 0x8b, 0x82 } : QColor{ 0xb4, 0x23, 0x18 };
		case core::local_check_outcome::couldnt_tell:
		case core::local_check_outcome::didnt_run:
		case core::local_check_outcome::stopped:      return dark ? QColor{ 0xe0, 0xb4, 0x5c } : QColor{ 0x8a, 0x5a, 0x00 };
		}
		return aPalette.color(QPalette::Text);
	}

	void local_check_card::build_plan(const core::local_check_plan& aPlan, const QString& aBlocked, const local_check_toggles& aToggles)
	{
		auto* const column = static_cast<QVBoxLayout*>(layout());
		auto* const title  = label(tr("Check plan for candidate %1").arg(aPlan.Candidate), this, QStringLiteral("localCheckPlanTitle"));
		auto        bold   = title->font();
		bold.setBold(true);
		title->setFont(bold);
		column->addWidget(title);
		// The commands run in this order: their numbers are the sequence.
		const auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		for (std::size_t index = 0U; index < aPlan.Steps.size(); ++index) {
			const auto& step    = aPlan.Steps[index];
			auto* const command = label(QStringLiteral("%1. %2").arg(index + 1U).arg(from(step.Command)), this, QStringLiteral("localCheckPlanCommand"));
			command->setFont(mono);
			column->addWidget(command);
			if (!step.Shows.empty()) {
				auto* const shows = quieter(label(from(step.Shows), this, QStringLiteral("localCheckPlanShows")));
				shows->setContentsMargins(16, 0, 0, 2);
				column->addWidget(shows);
			}
		}
		if (!aPlan.NotCovered.empty()) {
			column->addWidget(quieter(label(tr("Won't cover: %1").arg(from(aPlan.NotCovered)), this, QStringLiteral("localCheckPlanNotCovered"))));
		}
		// RFC-BPR-0104's two toggles, for this draft and agent; the plan says
		// which it needs.
		const auto needs = aPlan.NeedsScratch && aPlan.NeedsBuilds ? tr("Needs Use scratch folder and Allow build and run tests.")
			: aPlan.NeedsScratch                                   ? tr("Needs Use scratch folder.")
			: aPlan.NeedsBuilds                                    ? tr("Needs Allow build and run tests.")
																   : tr("Needs neither grant: it only reads.");
		column->addWidget(quieter(label(needs, this, QStringLiteral("localCheckPlanNeeds"))));
		const auto planned = aPlan.State == core::local_check_state::planned;
		if (planned) {
			auto* const scratch = new QCheckBox{ tr("Use scratch folder"), this };
			scratch->setObjectName(QStringLiteral("localCheckScratch"));
			scratch->setChecked(aToggles.Scratch);
			scratch->setEnabled(aToggles.Enabled);
			scratch->setToolTip(tr("Lets the agent write scripts in a folder of its own beside the check copy, for this local draft and agent."));
			auto* const builds = new QCheckBox{ tr("Allow build and run tests"), this };
			builds->setObjectName(QStringLiteral("localCheckBuilds"));
			builds->setChecked(aToggles.Builds);
			builds->setEnabled(aToggles.Enabled);
			builds->setToolTip(tr("Lets the agent build and run this change's tests in the check copy, with your account's rights, for this local draft and agent. "
								  "The build programs run with any arguments, including commands that install or publish."));
			// The side chat can be narrower than both toggles: they wrap rather
			// than widen the card past the chat.
			auto* const toggles = new detail::wrapping_row{ nullptr, 12, 4 };
			toggles->setContentsMargins(0, 0, 0, 0);
			toggles->addWidget(scratch);
			toggles->addWidget(builds);
			column->addLayout(toggles);
			const auto changed = [this, scratch, builds] {
				if (Dispatch.Grants) {
					Dispatch.Grants(scratch->isChecked(), builds->isChecked());
				}
			};
			connect(scratch, &QCheckBox::toggled, this, changed);
			connect(builds, &QCheckBox::toggled, this, changed);
		}
		auto* const actions = new QHBoxLayout{ };
		actions->setContentsMargins(0, 2, 0, 0);
		if (planned) {
			auto* const run = new quiet_push_button{ tr("Run"), this };
			run->setObjectName(QStringLiteral("localCheckRun"));
			run->setEnabled(aBlocked.isEmpty());
			run->setToolTip(aBlocked.isEmpty() ? tr("Run these commands in a throwaway copy of the latest candidate. Your folder is not touched.") : aBlocked);
			wear_agent_accent(*run, palette());
			auto* const discard = new quiet_push_button{ tr("Discard"), this };
			discard->setObjectName(QStringLiteral("localCheckDiscard"));
			discard->setFlat(true);
			discard->setToolTip(tr("Set this plan aside. To change it, say what to change in the conversation."));
			connect(run, &QPushButton::clicked, this, [this] {
				if (Dispatch.Run) {
					Dispatch.Run();
				}
			});
			connect(discard, &QPushButton::clicked, this, [this] {
				if (Dispatch.Discard) {
					Dispatch.Discard();
				}
			});
			actions->addWidget(run);
			actions->addWidget(discard);
		}
		actions->addStretch(1);
		layout()->addItem(actions);
		const auto state = !planned ? aPlan.State == core::local_check_state::ran ? tr("Ran. Its report follows.") : tr("Discarded.") : aBlocked;
		if (!state.isEmpty()) {
			column->addWidget(quieter(label(state, this, QStringLiteral("localCheckPlanState"))));
		}
	}

	void local_check_card::build_report(const core::local_check_report& aReport, const std::uint64_t aLatest)
	{
		auto* const column  = static_cast<QVBoxLayout*>(layout());
		auto* const outcome = label(from(std::string{ core::outcome_words(aReport.Outcome) }), this, QStringLiteral("localCheckOutcome"));
		auto        face    = outcome->font();
		face.setBold(true);
		face.setPointSizeF(face.pointSizeF() * 1.25);
		outcome->setFont(face);
		auto colours = outcome->palette();
		colours.setColor(QPalette::WindowText, outcome_colour(aReport.Outcome, palette()));
		outcome->setPalette(colours);
		column->addWidget(outcome);
		column->addWidget(quieter(label(from(core::check_candidate_words(aReport.Candidate, aLatest)) + QStringLiteral("."), this, QStringLiteral("localCheckCandidate"))));
		if (!aReport.Summary.empty()) {
			column->addWidget(label(from(aReport.Summary), this, QStringLiteral("localCheckSummary")));
		}
		const auto ran   = static_cast<int>(aReport.Commands.size());
		auto       tally = ran == 0 ? tr("Nothing ran.") : tr("%n command(s) ran.", nullptr, ran);
		if (!aReport.Changed.empty()) {
			tally += QStringLiteral(" ") + tr("%n tracked file(s) changed in the copy.", nullptr, static_cast<int>(aReport.Changed.size()));
		}
		column->addWidget(quieter(label(tally, this, QStringLiteral("localCheckTally"))));
		auto* const actions = new QHBoxLayout{ };
		actions->setContentsMargins(0, 2, 0, 0);
		auto* const open = new quiet_push_button{ tr("Open report"), this };
		open->setObjectName(QStringLiteral("localCheckOpen"));
		open->setToolTip(tr("Read the report in a tab beside the files"));
		connect(open, &QPushButton::clicked, this, [this] {
			if (Dispatch.Open) {
				Dispatch.Open();
			}
		});
		actions->addWidget(open);
		if (core::offers_run_again(aReport, aLatest)) {
			auto* const again = new quiet_push_button{ tr("Run again"), this };
			again->setObjectName(QStringLiteral("localCheckAgain"));
			again->setToolTip(tr("Run the same plan on candidate %1").arg(aLatest));
			wear_agent_accent(*again, palette());
			connect(again, &QPushButton::clicked, this, [this] {
				if (Dispatch.Again) {
					Dispatch.Again();
				}
			});
			actions->addWidget(again);
		}
		actions->addStretch(1);
		column->addLayout(actions);
	}
}
