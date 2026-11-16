#include "bpr/qt/local_draft_publication_area.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "wrapping_row.hpp"

#include "bpr/core/git_objects.hpp"
#include "bpr/core/local_draft_tour.hpp"

#include <QFontDatabase>
#include <QFormLayout>
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

		/// The three steps the accepted design draws in the Status tab: push
		/// the branch, create the draft pull request, publish the tour (when
		/// the draft has one). Link it to this draft shows only when it failed.
		[[nodiscard]] auto steps_of(const application::local_draft_workbench_projection& aProjection) -> std::vector<application::tour_publication_step_view>
		{
			auto out = std::vector<application::tour_publication_step_view>{ };
			if (!aProjection.Draft) {
				return out;
			}
			const auto& draft   = *aProjection.Draft;
			const auto& view    = aProjection.Publishing;
			const auto* journal = draft.Publication ? &draft.Publication->Journal : nullptr;
			const auto  created = journal != nullptr && journal->Phase == core::local_publication_phase::linked;
			auto        push    = application::tour_publication_step_view{ .Label = "Push " + draft.Branch };
			push.State          = created || (view.Checked && view.Branch == core::hosted_branch_state::matches) ? application::tour_step_state::sent : application::tour_step_state::waiting;
			out.push_back(std::move(push));
			auto create = application::tour_publication_step_view{ .Label = "Create the draft pull request" };
			if (journal != nullptr && journal->Phase != core::local_publication_phase::prepared) {
				const auto adopted = journal->Link && journal->Link->Adopted;
				create.Label       = adopted ? "Create the draft pull request (one was open already)" : create.Label;
				create.State       = step_of(journal->Create);
				create.Reason      = journal->Create == core::local_step_state::failed ? journal->Reason : std::string{ };
			} else if (view.Creating) {
				create.State = application::tour_step_state::sending;
			}
			out.push_back(std::move(create));
			if (journal != nullptr) {
				for (const auto& step : journal->FollowUps) {
					if (step.Kind == core::local_follow_up::association && step.State == core::local_step_state::failed) {
						out.push_back({ .Label = "Link it to this draft", .State = application::tour_step_state::failed, .Reason = step.Reason });
					}
				}
			}
			if (!draft.Preparation.Tour.Stops.empty()) {
				const auto head = core::decide_local_tour_head(draft);
				auto       tour = application::tour_publication_step_view{ .Label = "Publish the tour" };
				if (head.ToAccept > 0U) {
					tour.Label += " (" + std::to_string(head.ToAccept) + " to accept)";
				} else if (created) {
					tour.Label += " (from the Tour tab)";
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
		// The accepted design's Status tab: where the draft stands, the
		// publish steps, the push to run, and what is sent.
		setObjectName(QStringLiteral("localDraftPublicationArea"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(14, 14, 14, 14);
		column->setSpacing(8);

		Headline = new QLabel{ this };
		Headline->setObjectName(QStringLiteral("localDraftPublicationHeadline"));
		Headline->setTextFormat(Qt::PlainText);
		auto headlineFont = Headline->font();
		headlineFont.setWeight(QFont::DemiBold);
		headlineFont.setPointSizeF(headlineFont.pointSizeF() + 1.0);
		Headline->setFont(headlineFont);
		Headline->setWordWrap(true);
		column->addWidget(Headline);

		State = new QLabel{ this };
		State->setObjectName(QStringLiteral("localDraftPublicationState"));
		State->setTextFormat(Qt::PlainText);
		State->setWordWrap(true);
		State->setTextInteractionFlags(Qt::TextSelectableByMouse);
		State->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(State);

		const auto caption = [this](const QString& aText) {
			auto* const label = new QLabel{ aText, this };
			label->setTextFormat(Qt::PlainText);
			auto font = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		};
		column->addSpacing(6);
		column->addWidget(caption(tr("Create draft pull request")));
		Steps = new tour_step_list{ this };
		Steps->setObjectName(QStringLiteral("localDraftPublicationSteps"));
		column->addWidget(Steps);

		Push = new QLineEdit{ this };
		Push->setObjectName(QStringLiteral("localDraftPublicationPush"));
		Push->setReadOnly(true);
		Push->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		Push->setAccessibleName(tr("The push to run"));
		Push->setToolTip(tr("Run this in a terminal to push the candidate."));
		column->addWidget(Push);

		// The buttons wrap inside the drawer rather than widen it.
		auto* const actions = new detail::wrapping_row{ nullptr, 6, 4 };
		Copy                = new quiet_push_button{ tr("Copy"), this };
		Copy->setObjectName(QStringLiteral("localDraftPublicationCopy"));
		Copy->setToolTip(tr("Copy the push."));
		Check = new quiet_push_button{ tr("Check GitHub"), this };
		Check->setObjectName(QStringLiteral("localDraftPublicationCheck"));
		Check->setToolTip(tr("Check that the pushed branch is this candidate."));
		Create = new QPushButton{ tr("Create Draft PR"), this };
		Create->setObjectName(QStringLiteral("localDraftPublicationCreate"));
		Create->setToolTip(tr("Create the draft pull request as shown under What is sent."));
		actions->addWidget(Copy);
		actions->addWidget(Check);
		actions->addWidget(Create);
		column->addLayout(actions);

		Existing           = new QWidget{ this };
		auto* const exists = new QHBoxLayout{ Existing };
		exists->setContentsMargins(0, 0, 0, 0);
		exists->setSpacing(6);
		ExistingText = new QLabel{ Existing };
		ExistingText->setTextFormat(Qt::PlainText);
		ExistingText->setWordWrap(true);
		exists->addWidget(ExistingText, 1);
		Associate = new quiet_push_button{ tr("Link to this draft"), Existing };
		Associate->setObjectName(QStringLiteral("localDraftPublicationAssociate"));
		Associate->setToolTip(tr("Link the open pull request to this draft."));
		exists->addWidget(Associate);
		column->addWidget(Existing);

		Banner = new tour_publication_banner_view{ this };
		Banner->setObjectName(QStringLiteral("localDraftPublicationBanner"));
		column->addWidget(Banner);

		column->addSpacing(6);
		SentCaption = caption(tr("What is sent"));
		column->addWidget(SentCaption);
		Sent = new QWidget{ this };
		Sent->setObjectName(QStringLiteral("localDraftPublicationSent"));
		SentRows = new QFormLayout{ Sent };
		SentRows->setContentsMargins(0, 0, 0, 0);
		SentRows->setHorizontalSpacing(14);
		SentRows->setVerticalSpacing(4);
		SentRows->setLabelAlignment(Qt::AlignLeft);
		column->addWidget(Sent);

		Link = new QLabel{ this };
		Link->setObjectName(QStringLiteral("localDraftPublicationLink"));
		Link->setTextFormat(Qt::PlainText);
		Link->setWordWrap(true);
		Link->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		column->addWidget(Link);
		column->addStretch(1);

		connect(Check, &QPushButton::clicked, this, [this] {
			if (Dispatch.Check) {
				Dispatch.Check(follows_tour());
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
				Dispatch.Check(follows_tour());
			}
		});
	}

	auto local_draft_publication_area::follows_tour() const -> bool
	{
		return Projection.Draft && !Projection.Draft->Preparation.Tour.Stops.empty();
	}

	void local_draft_publication_area::set_dispatch(local_draft_publication_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto local_draft_publication_area::sent_text() const -> QString
	{
		auto lines = QStringList{ };
		for (int row = 0; row < SentRows->rowCount(); ++row) {
			const auto* const label = qobject_cast<QLabel*>(SentRows->itemAt(row, QFormLayout::LabelRole) != nullptr ? SentRows->itemAt(row, QFormLayout::LabelRole)->widget() : nullptr);
			const auto* const field = qobject_cast<QLabel*>(SentRows->itemAt(row, QFormLayout::FieldRole) != nullptr ? SentRows->itemAt(row, QFormLayout::FieldRole)->widget() : nullptr);
			if (label != nullptr && field != nullptr) {
				lines.push_back(label->text() + QStringLiteral(" ") + field->text());
			}
		}
		return lines.join(QLatin1Char{ '\n' });
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
		const auto* link   = linked && draft->Publication->Journal.Link ? &*draft->Publication->Journal.Link : nullptr;
		Headline->setText(link != nullptr ? tr("Draft pull request #%1").arg(link->Number) : tr("Not on GitHub"));
		State->setText(local_publication_state_text(aProjection));
		const auto pushNeeded = view.Push.has_value() && !linked;
		Push->setVisible(pushNeeded);
		if (const auto command = view.Push ? text_of(view.Push->Command) : QString{ }; Push->text() != command) {
			Push->setText(command);
			Push->setCursorPosition(0);
		}
		Copy->setVisible(pushNeeded);
		Check->setText(view.Checked ? tr("Check Again") : tr("Check GitHub"));
		Check->setVisible(!linked);
		Check->setEnabled(draft != nullptr && !busy && phase != core::local_publication_phase::uncertain);
		const auto ready = draft != nullptr && view.Checked && !view.Refusal && !view.Digest.empty() && view.AccountAvailable && phase == core::local_publication_phase::prepared
			&& view.Branch == core::hosted_branch_state::matches;
		Create->setVisible(ready || view.Creating);
		Create->setEnabled(ready && !busy);
		Create->setDefault(ready);
		Existing->setVisible(view.Existing.has_value() && !linked);
		ExistingText->setText(view.Existing ? tr("#%1 is open from this branch.").arg(view.Existing->Number) : QString{ });
		Associate->setEnabled(!busy && view.AccountAvailable);
		Steps->present(steps_of(aProjection));

		// What is sent: the plan when one was made, else what the draft would send.
		while (SentRows->rowCount() > 0) {
			SentRows->removeRow(0);
		}
		if (draft != nullptr && !linked) {
			const auto  repository = text_of(draft->Target.Target.Repository.Owner) + QLatin1Char{ '/' } + text_of(draft->Target.Target.Repository.Name);
			const auto* latest     = core::latest_candidate(*draft);
			const auto  row        = [this](const QString& aLabel, const QString& aValue) {
				auto* const label = new QLabel{ aLabel, Sent };
				label->setForegroundRole(QPalette::PlaceholderText);
				auto* const value = new QLabel{ aValue, Sent };
				value->setTextFormat(Qt::PlainText);
				value->setWordWrap(true);
				value->setTextInteractionFlags(Qt::TextSelectableByMouse);
				SentRows->addRow(label, value);
			};
			row(tr("From"), repository + QLatin1Char{ ' ' } + text_of(draft->Branch));
			row(tr("Into"), repository + QLatin1Char{ ' ' } + text_of(draft->Target.Target.Branch));
			if (plan != nullptr && !plan->Destination.Login.empty()) {
				row(tr("As"), text_of(plan->Destination.Login));
			}
			if (latest != nullptr) {
				row(tr("Version"), tr("Candidate %1, %2").arg(latest->Ordinal).arg(QString::fromStdString(std::string{ core::canonical_hex(latest->Head).view() }).left(7)));
			}
			row(tr("Title"), text_of(draft->Title));
			if (plan != nullptr && !plan->Unaccepted.empty()) {
				row(tr("Unresolved"), tr("%n stop(s) not accepted", nullptr, static_cast<int>(plan->Unaccepted.size())));
			}
			row(tr("Kept here"), tr("Local comments and conversations"));
		}
		SentCaption->setVisible(SentRows->rowCount() > 0);
		Sent->setVisible(SentRows->rowCount() > 0);
		Link->setVisible(linked);
		Link->setText(linked ? text_of(view.Linked) : QString{ });
		Banner->present(draft != nullptr ? banner_of(*draft) : std::nullopt);
	}
}
