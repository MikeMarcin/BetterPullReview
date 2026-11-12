#include "bpr/qt/local_draft_publication_area.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"

#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto step_of(const core::local_step_state aState) -> application::tour_step_state
		{
			switch (aState) {
			case core::local_step_state::waiting:  return application::tour_step_state::waiting;
			case core::local_step_state::sending:  return application::tour_step_state::sending;
			case core::local_step_state::sent:     return application::tour_step_state::sent;
			case core::local_step_state::failed:   return application::tour_step_state::failed;
			case core::local_step_state::not_sent: return application::tour_step_state::not_sent;
			}
			return application::tour_step_state::waiting;
		}

		[[nodiscard]] auto steps_of(const core::local_draft& aDraft) -> std::vector<application::tour_publication_step_view>
		{
			auto out = std::vector<application::tour_publication_step_view>{ };
			if (!aDraft.Publication) {
				return out;
			}
			const auto& publication = *aDraft.Publication;
			const auto& journal     = publication.Journal;
			const auto  adopted     = journal.Link && journal.Link->Adopted;
			out.push_back({ .Label  = adopted ? "Create the draft pull request (one was open already)" : "Create the draft pull request",
							.State  = step_of(journal.Create),
							.Reason = journal.Create == core::local_step_state::failed ? journal.Reason : std::string{ } });
			auto link = application::tour_publication_step_view{ .Label = "Link it to this draft" };
			auto tour = application::tour_publication_step_view{ .Label = "Publish the tour" };
			for (const auto& step : journal.FollowUps) {
				auto& row  = step.Kind == core::local_follow_up::association ? link : tour;
				row.State  = step_of(step.State);
				row.Reason = step.State == core::local_step_state::failed ? step.Reason : std::string{ };
			}
			if (journal.Phase == core::local_publication_phase::refused || journal.Phase == core::local_publication_phase::cancelled) {
				link.State = application::tour_step_state::not_sent;
				tour.State = application::tour_step_state::not_sent;
			}
			out.push_back(std::move(link));
			if (publication.Plan.TourFollowUp) {
				if (tour.State == application::tour_step_state::waiting) {
					if (const auto blocked = core::tour_follow_up_blocked(aDraft); blocked == core::tour_follow_up_block::unaccepted) {
						tour.Reason = "Waits until every stop is accepted.";
					} else if (blocked == core::tour_follow_up_block::drifted) {
						tour.Reason = "The pull request's head is not this candidate.";
					} else if (!blocked) {
						tour.Reason = "Publish it from the pull request's Tour.";
					}
				}
				out.push_back(std::move(tour));
			}
			return out;
		}

		[[nodiscard]] auto banner_of(const core::local_draft& aDraft) -> std::optional<application::tour_publication_banner>
		{
			using remedy = application::tour_publication_remedy;
			if (!aDraft.Publication) {
				return std::nullopt;
			}
			const auto& journal = aDraft.Publication->Journal;
			switch (journal.Phase) {
			case core::local_publication_phase::uncertain:
				return application::tour_publication_banner{ .Title    = "The draft pull request may have been created.",
															 .Why      = journal.Reason,
															 .OnGitHub = "Not known yet. Nothing is sent again until GitHub has been read.",
															 .Actions  = { remedy::check_status } };
			case core::local_publication_phase::refused:
				return application::tour_publication_banner{
					.Title = "GitHub refused the draft pull request.", .Why = journal.Reason, .OnGitHub = "Nothing was created.", .Actions = { remedy::preview_again }
				};
			case core::local_publication_phase::linked:
				for (const auto& step : journal.FollowUps) {
					if (step.State == core::local_step_state::failed) {
						return application::tour_publication_banner{ .Title    = step.Kind == core::local_follow_up::tour ? "The tour did not publish." : "The link was not recorded.",
																	 .Why      = step.Reason,
																	 .OnGitHub = "Draft pull request #" + std::to_string(journal.Link->Number) + " is on GitHub and linked to this draft.",
																	 .Actions  = { } };
					}
				}
				return std::nullopt;
			case core::local_publication_phase::prepared:
				if (journal.Create == core::local_step_state::not_sent && !journal.Reason.empty()) {
					return application::tour_publication_banner{
						.Title = "The draft pull request was not created.", .Why = journal.Reason, .OnGitHub = "Nothing is on GitHub.", .Actions = { remedy::preview_again }
					};
				}
				return std::nullopt;
			case core::local_publication_phase::authorized:
			case core::local_publication_phase::creating:
			case core::local_publication_phase::cancelled:  return std::nullopt;
			}
			return std::nullopt;
		}
	}

	auto local_publication_state_text(const application::local_draft_workbench_projection& aProjection) -> QString
	{
		const auto& view = aProjection.Publishing;
		if (!aProjection.Draft) {
			return { };
		}
		const auto& draft  = *aProjection.Draft;
		const auto  branch = text_of(draft.Branch);
		if (draft.Publication && draft.Publication->Journal.Link) {
			const auto& link = *draft.Publication->Journal.Link;
			auto        line = QObject::tr("Draft pull request #%1 is on GitHub, linked to this draft.").arg(link.Number);
			if (!link.Draft) {
				line = QObject::tr("Pull request #%1 is on GitHub, linked to this draft.").arg(link.Number);
			}
			if (link.Drifted) {
				line += QObject::tr(" Its head is not candidate %1: the stops stay with the candidate.").arg(draft.Publication->Plan.Candidate);
			}
			return line;
		}
		if (view.Creating) {
			return QObject::tr("Creating the draft pull request…");
		}
		if (view.Checking) {
			return QObject::tr("Reading GitHub…");
		}
		if (draft.Publication && draft.Publication->Journal.Phase == core::local_publication_phase::uncertain) {
			return QObject::tr("The last creation's answer was lost.");
		}
		if (!view.Checked) {
			return QObject::tr("Not checked on GitHub yet.");
		}
		if (view.Refusal == core::local_publication_refusal::pull_request_exists && view.Existing) {
			return QObject::tr("#%1 is already open from %2.").arg(view.Existing->Number).arg(branch);
		}
		switch (view.Branch) {
		case core::hosted_branch_state::missing:     return QObject::tr("%1 is not on GitHub. Push it, then check again.").arg(branch);
		case core::hosted_branch_state::differs:     return QObject::tr("%1 on GitHub is at another commit. Push this candidate, then check again.").arg(branch);
		case core::hosted_branch_state::uncommitted: return QObject::tr("This candidate includes uncommitted files. Commit them and refresh.");
		case core::hosted_branch_state::unobserved:
			return !view.Failure.empty() ? text_of(view.Failure)
				: !view.AccountAvailable ? QObject::tr("Sign in to GitHub to check %1.").arg(branch)
										 : QObject::tr("%1 could not be read on GitHub.").arg(branch);
		case core::hosted_branch_state::matches: break;
		}
		if (view.Refusal == core::local_publication_refusal::no_permission) {
			return QObject::tr("This account cannot push to %1/%2.").arg(text_of(draft.Target.Target.Repository.Owner), text_of(draft.Target.Target.Repository.Name));
		}
		if (view.Refusal) {
			return QObject::tr("%1 on GitHub is this candidate, but the plan was refused (%2).").arg(branch, text_of(core::name(*view.Refusal)));
		}
		return QObject::tr("%1 on GitHub is this candidate.").arg(branch);
	}

	local_draft_publication_area::local_draft_publication_area(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftPublicationArea"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 8, 0, 0);
		column->setSpacing(8);

		auto* const head = new QHBoxLayout{ };
		head->setSpacing(6);
		State = new QLabel{ this };
		State->setObjectName(QStringLiteral("localDraftPublicationState"));
		State->setTextFormat(Qt::PlainText);
		State->setWordWrap(true);
		State->setTextInteractionFlags(Qt::TextSelectableByMouse);
		head->addWidget(State, 1);
		Check = new quiet_push_button{ tr("Check GitHub"), this };
		Check->setObjectName(QStringLiteral("localDraftPublicationCheck"));
		Check->setToolTip(tr("Reads whether the branch you pushed is this candidate, and records the exact plan"));
		Create = new QPushButton{ tr("Create draft PR"), this };
		Create->setObjectName(QStringLiteral("localDraftPublicationCreate"));
		Create->setToolTip(tr("Creates the draft pull request exactly as previewed below. BPR pushes nothing."));
		const auto width = std::max(Check->sizeHint().width(), Create->sizeHint().width());
		Check->setMinimumWidth(width);
		Create->setMinimumWidth(width);
		head->addWidget(Check);
		head->addWidget(Create);
		column->addLayout(head);

		PushRow          = new QWidget{ this };
		auto* const push = new QHBoxLayout{ PushRow };
		push->setContentsMargins(0, 0, 0, 0);
		push->setSpacing(6);
		Push = new QLineEdit{ PushRow };
		Push->setObjectName(QStringLiteral("localDraftPublicationPush"));
		Push->setReadOnly(true);
		Push->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		Push->setAccessibleName(tr("The push to run"));
		Push->setToolTip(tr("Run this in a terminal: your own Git, credentials and hooks push the exact candidate to the hosted repository. BPR pushes nothing."));
		push->addWidget(Push, 1);
		Copy = new quiet_push_button{ tr("Copy"), PushRow };
		Copy->setObjectName(QStringLiteral("localDraftPublicationCopy"));
		Copy->setMinimumWidth(width);
		push->addWidget(Copy);
		column->addWidget(PushRow);

		Existing           = new QWidget{ this };
		auto* const exists = new QHBoxLayout{ Existing };
		exists->setContentsMargins(0, 0, 0, 0);
		exists->setSpacing(6);
		ExistingText = new QLabel{ Existing };
		ExistingText->setTextFormat(Qt::PlainText);
		exists->addWidget(ExistingText, 1);
		Associate = new quiet_push_button{ tr("Link to this draft"), Existing };
		Associate->setObjectName(QStringLiteral("localDraftPublicationAssociate"));
		Associate->setToolTip(tr("Links the open pull request to this draft instead of creating another. Its description is left as it is."));
		Associate->setMinimumWidth(width);
		exists->addWidget(Associate);
		column->addWidget(Existing);

		Tour = new QCheckBox{ tr("Publish the tour afterwards"), this };
		Tour->setObjectName(QStringLiteral("localDraftPublicationTour"));
		Tour->setToolTip(tr("The tour is its own publication once the pull request exists. Stops not accepted are listed and wait; they do not stop the draft pull request."));
		column->addWidget(Tour);

		Preview = new QPlainTextEdit{ this };
		Preview->setObjectName(QStringLiteral("localDraftPublicationPreview"));
		Preview->setReadOnly(true);
		Preview->setAccessibleName(tr("What Create draft PR sends"));
		Preview->setMaximumHeight(Preview->fontMetrics().lineSpacing() * 12);
		column->addWidget(Preview);

		Link = new QLabel{ this };
		Link->setObjectName(QStringLiteral("localDraftPublicationLink"));
		Link->setTextFormat(Qt::PlainText);
		Link->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		column->addWidget(Link);

		Steps = new tour_step_list{ this };
		Steps->setObjectName(QStringLiteral("localDraftPublicationSteps"));
		column->addWidget(Steps);
		Banner = new tour_publication_banner_view{ this };
		Banner->setObjectName(QStringLiteral("localDraftPublicationBanner"));
		column->addWidget(Banner);
		column->addStretch(1);

		connect(Check, &QPushButton::clicked, this, [this] {
			if (Dispatch.Check) {
				Dispatch.Check(Tour->isChecked());
			}
		});
		connect(Tour, &QCheckBox::toggled, this, [this](const bool aOn) {
			// The tour choice is part of the plan: a changed choice is checked again.
			if (Dispatch.Check && Projection.Publishing.Checked && !Projection.Publishing.Checking && !Projection.Publishing.Creating) {
				Dispatch.Check(aOn);
			}
		});
		connect(Create, &QPushButton::clicked, this, [this] {
			if (Dispatch.Create && !Projection.Publishing.Digest.empty()) {
				Create->setEnabled(false);
				Dispatch.Create(Projection.Publishing.Digest);
			}
		});
		connect(Copy, &QPushButton::clicked, this, [this] {
			if (Dispatch.Copy && !Push->text().isEmpty()) {
				Dispatch.Copy(Push->text().toStdString());
			}
		});
		connect(Associate, &QPushButton::clicked, this, [this] {
			if (Dispatch.Associate && Projection.Publishing.Existing) {
				Dispatch.Associate(Projection.Publishing.Existing->Number);
			}
		});
		Banner->set_dispatch([this](const application::tour_publication_remedy aRemedy, core::tour_stop_id) {
			if (aRemedy == application::tour_publication_remedy::check_status && Dispatch.Recover) {
				Dispatch.Recover();
			} else if (aRemedy == application::tour_publication_remedy::preview_again && Dispatch.Check) {
				Dispatch.Check(Tour->isChecked());
			}
		});
	}

	void local_draft_publication_area::set_dispatch(local_draft_publication_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void local_draft_publication_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection         = aProjection;
		const auto& view   = aProjection.Publishing;
		const auto* draft  = aProjection.Draft ? &*aProjection.Draft : nullptr;
		const auto  busy   = view.Checking || view.Creating || aProjection.Busy;
		const auto* plan   = draft != nullptr && draft->Publication ? &draft->Publication->Plan : nullptr;
		const auto  phase  = draft != nullptr && draft->Publication ? draft->Publication->Journal.Phase : core::local_publication_phase::prepared;
		const auto  linked = phase == core::local_publication_phase::linked;
		State->setText(local_publication_state_text(aProjection));
		Check->setEnabled(draft != nullptr && !busy && !linked && phase != core::local_publication_phase::uncertain);
		Create->setEnabled(draft != nullptr && !busy && view.Checked && !view.Refusal && !view.Digest.empty() && view.AccountAvailable && phase == core::local_publication_phase::prepared
						   && view.Branch == core::hosted_branch_state::matches);
		Create->setToolTip(Create->isEnabled() ? tr("Creates the draft pull request exactly as previewed below. BPR pushes nothing.")
											   : tr("Check GitHub first: the branch you pushed must be this candidate."));
		PushRow->setVisible(view.Push.has_value() && !linked);
		Push->setText(view.Push ? text_of(view.Push->Command) : QString{ });
		Existing->setVisible(view.Existing.has_value() && !linked);
		ExistingText->setText(view.Existing ? tr("#%1 is open from this branch.").arg(view.Existing->Number) : QString{ });
		Associate->setEnabled(!busy && view.AccountAvailable);
		const auto stops = draft != nullptr && !draft->Preparation.Tour.Stops.empty();
		Tour->setEnabled(stops && !busy && !linked);
		{
			const auto blocker = QSignalBlocker{ Tour };
			Tour->setChecked(plan != nullptr ? plan->TourFollowUp : Tour->isChecked() && stops);
		}
		Preview->setVisible(!view.Preview.empty() && !linked);
		if (Preview->toPlainText() != text_of(view.Preview)) {
			Preview->setPlainText(text_of(view.Preview));
		}
		Link->setVisible(linked);
		Link->setText(linked ? text_of(view.Linked) : QString{ });
		const auto rows = draft != nullptr && phase != core::local_publication_phase::prepared ? steps_of(*draft) : std::vector<application::tour_publication_step_view>{ };
		Steps->setVisible(!rows.empty());
		Steps->present(rows);
		Banner->present(draft != nullptr ? banner_of(*draft) : std::nullopt);
	}
}
