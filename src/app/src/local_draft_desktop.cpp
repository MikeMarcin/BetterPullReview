#include "local_draft_desktop.hpp"

#include "bpr/adapters/git/installed_author_git.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/local_draft_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QClipboard>
#include <QFile>
#include <QDateTime>
#include <QFileDialog>
#include <QGuiApplication>
#include <QRandomGenerator>

#include <array>
#include <utility>

namespace bpr::app {
	namespace {
		auto random_seed() -> std::array<std::uint8_t, 32>
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			for (auto& byte : seed) {
				byte = static_cast<std::uint8_t>(QRandomGenerator::global()->bounded(256U));
			}
			return seed;
		}
	}

	local_draft_desktop::local_draft_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, std::filesystem::path aGitExecutable,
											 std::optional<std::filesystem::path> aWindowsSystemRoot, application::effect_executor& aEffects, application::decision_executor& aDecisions)
		: Window{ aWindow }
		, Source{ adapters::workspace::make_local_workspace_source() }
	{
		auto git = [root = aProfileRoot, executable = aGitExecutable, windows = std::move(aWindowsSystemRoot)](
					   const std::stop_token aStop) -> application::publication_git_result<std::unique_ptr<application::local_draft_git_port>> {
			const auto isolation = root / "local-draft-git-v1";
			if (executable.empty() || !adapters::sqlite::create_storage_root_owner_private(isolation)) {
				return std::unexpected{ application::publication_git_error{ .Failure = application::publication_git_failure::unavailable } };
			}
			auto made = adapters::git::make_installed_git_local_draft_reader({ .GitExecutable = executable, .IsolationRoot = isolation, .WindowsSystemRoot = windows }, aStop);
			if (!made) {
				return std::unexpected{ application::publication_git_error{ .Failure = application::publication_git_failure::unavailable, .Detail = made.error().Detail } };
			}
			return std::move(*made);
		};
		Host = std::make_unique<application::local_draft_workbench>(
			[root = aProfileRoot] { return adapters::sqlite::open_local_draft_store(root); }, *Source, std::move(git), aEffects, aDecisions, *this, random_seed(),
			[] { return static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch()); }, core::make_xdiff_line_matcher());
		View = new qt::local_draft_view{ };
		Window.adopt_local_draft(View);
		// Author state: the author's own Git in their own worktree, separate
		// from the isolated reader (RFC-BPR-0114 as amended 2026-11-12).
		AuthorGit     = adapters::git::make_installed_author_git(aGitExecutable.empty() ? std::filesystem::path{ } : aGitExecutable);
		WorktreeFiles = adapters::git::make_worktree_files();
		ConsentFile   = aProfileRoot / "author-git-consent.txt";
		Changes       = std::make_unique<application::local_changes_host>(
            *AuthorGit, *WorktreeFiles, core::make_xdiff_line_matcher(), aEffects, aDecisions, *this,
            application::local_changes_consent{
                .Allowed =
                    [this](const std::string& aKey) {
                        auto file = QFile{ QString::fromStdString(ConsentFile.string()) };
                        if (aKey.empty() || !file.open(QIODevice::ReadOnly)) {
                            return false;
                        }
                        return file.readAll().split('\n').contains(QByteArray::fromStdString(aKey));
                    },
                .Remember =
                    [this](const std::string& aKey) {
                        auto file = QFile{ QString::fromStdString(ConsentFile.string()) };
                        if (!aKey.empty() && file.open(QIODevice::Append)) {
                            file.write(QByteArray::fromStdString(aKey + "\n"));
                        }
                    },
            });
		View->local_changes()->set_dispatch({
			.Choose     = [this](const core::local_comparison aComparison) { Changes->choose(aComparison); },
			.OpenFile   = [this](const std::string& aPath) { Changes->open_file(aPath); },
			.Stage      = [this](std::vector<std::string> aPaths) { Changes->stage(std::move(aPaths)); },
			.Unstage    = [this](std::vector<std::string> aPaths) { Changes->unstage(std::move(aPaths)); },
			.Commit     = [this](core::local_commit_request aRequest) { Changes->commit(std::move(aRequest)); },
			.Push       = [this] { Changes->push(); },
			.CheckAgain = [this] { Changes->check_again(); },
			.Stop       = [this] { Changes->stop(); },
			.Suggest =
				[this] {
					const auto choice = AgentChooser ? AgentChooser() : std::nullopt;
					if (choice) {
						Changes->suggest_message({ .Agent = choice->Identity, .AgentName = choice->Name, .Profile = choice->Profile, .Instructions = choice->CommitMessage });
					}
				},
			.Consent = [this](const bool aAllow) { Changes->answer_consent(aAllow); },
		});
		View->set_dispatch({
			.OpenFile      = [this](const std::string& aPath) { Host->open_file(aPath); },
			.Include       = [this](const std::string& aPath, const bool aIncluded) { Host->include(aPath, aIncluded); },
			.Refresh       = [this] { Host->refresh(); },
			.ShowCandidate = [this](const std::uint64_t aOrdinal) { Host->show_candidate(aOrdinal); },
			.AddNote       = [this](const std::string& aText, const std::string& aPath) { Host->add_note(aText, aPath); },
			.RemoveNote    = [this](const std::uint64_t aNote) { Host->remove_note(aNote); },
			.Rename        = [this](const std::string& aTitle) { Host->rename(aTitle); },
			.ChooseTarget  = [this](core::local_draft_target aTarget) { Host->choose_target(std::move(aTarget)); },
			.Rebind        = [this] { Host->rebind(); },
			.Discard       = [this] { Host->discard(); },
			.Close =
				[this] {
					Host->close();
					Window.leave_local_draft();
				},
			.StageChange =
				[this](const std::string& aPath, const core::local_change_hunk aChange, const bool aStage) {
					if (aStage) {
						Changes->stage_change(aPath, aChange);
					} else {
						Changes->unstage_change(aPath, aChange);
					}
				},
			.OpenLocalFile = [this](const std::string& aPath) { Changes->open_file(aPath); },
			.Save = [this](const std::string& aPath, std::vector<std::uint8_t> aRead, std::vector<std::uint8_t> aEdited) { Changes->save(aPath, std::move(aRead), std::move(aEdited)); },
			.CreatePullRequest = [this] { Host->check_publication(false); },
		});
		// RFC-BPR-0110: the Description and Tour areas. Nothing here publishes.
		View->set_preparation_dispatch({
			.SaveDescription = [this](const std::string& aTitle, const std::string& aBody) { Host->save_description(aTitle, aBody); },
			.SaveBrief       = [this](const std::string& aProblem, const std::string& aSolution) { Host->save_brief(aProblem, aSolution); },
			.Transfer        = [this](const std::string& aPassage) { Host->transfer_to_description(aPassage); },
			.AddStop         = [this](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const std::string& aTitle,
									  const std::string& aText) { Host->add_stop(aPath, aFirst, aLast, aTitle, aText); },
			.SaveStop        = [this](const std::uint64_t aStop, const std::string& aTitle, const std::string& aText, const std::string& aPath, const std::uint32_t aFirst,
									  const std::uint32_t aLast) { Host->save_stop(aStop, aTitle, aText, aPath, aFirst, aLast); },
			.AcceptStop      = [this](const std::uint64_t aStop) { Host->accept_stop(aStop); },
			.RemoveStop      = [this](const std::uint64_t aStop) { Host->remove_stop(aStop); },
			.MoveStop        = [this](const std::uint64_t aStop, const std::size_t aIndex) { Host->move_stop(aStop, aIndex); },
			.DecideProposal  = [this](const std::uint64_t aProposal, const bool aAccept) { Host->decide_proposal(aProposal, aAccept); },
			.Ask =
				[this](application::local_draft_question aQuestion) {
					const auto choice = AgentChooser ? AgentChooser() : std::nullopt;
					if (!choice) {
						return;
					}
					const auto describing  = aQuestion.Subject == core::local_subject::title || aQuestion.Subject == core::local_subject::description;
					aQuestion.Agent        = choice->Identity;
					aQuestion.AgentName    = choice->Name;
					aQuestion.Profile      = choice->Profile;
					aQuestion.Instructions = describing ? choice->Description : choice->Discussion;
					aQuestion.Version      = describing ? choice->DescriptionVersion : choice->DiscussionVersion;
					Host->ask(std::move(aQuestion));
				},
			.StopAsking = [this] { Host->stop_asking(); },
			.NewSession = [this](const std::uint64_t aConversation) { Host->new_session(aConversation); },
		});
		Window.set_local_draft_entry_dispatch(
			[this] {
				const auto chosen = Chooser ? Chooser() : QFileDialog::getExistingDirectory(&Window, QStringLiteral("Prepare Pull Request"));
				if (!chosen.isEmpty()) {
					open_location(chosen.toStdString());
				}
			},
			[this](const core::local_draft_id& aId) {
				BringForward = true;
				Host->resume(aId);
			});
		Host->refresh_home();
	}

	local_draft_desktop::~local_draft_desktop()
	{
		shutdown();
	}

	void local_draft_desktop::publish(const application::local_draft_workbench_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		View->present(aProjection);
		follow_author_state(aProjection);
		if (aProjection.Stage == application::local_draft_stage::closed) {
			Window.leave_local_draft();
			return;
		}
		if (std::exchange(BringForward, false) || Window.showing_local_draft()) {
			Window.show_local_draft(View->window_title());
		}
	}

	void local_draft_desktop::follow_author_state(const application::local_draft_workbench_projection& aProjection)
	{
		// Author state is the window on the author's own worktree: a draft
		// whose worktree is where it was, on its branch.
		const auto* const draft  = aProjection.Draft ? &*aProjection.Draft : nullptr;
		const auto        author = draft != nullptr && aProjection.Stage == application::local_draft_stage::open && !aProjection.WorktreeMissing && aProjection.Match == core::worktree_match::same;
		if (!author) {
			if (Changes->projection().Active) {
				Changes->close();
			}
			return;
		}
		const auto* const latest     = core::latest_candidate(*draft);
		const auto&       repository = draft->Target.Target.Repository;
		auto              subject    = application::local_changes_subject{
						.Worktree    = draft->Worktree.Location,
						.Branch      = draft->Branch,
						.Clone       = draft->Chain.empty() ? draft->Worktree.Location : draft->Chain.back().Location,
						.Remote      = draft->Target.Target.Remote,
						.FallbackUrl = "https://" + repository.Host + "/" + repository.Owner + "/" + repository.Name + ".git",
						.Target      = draft->Target.Target.Branch,
						.Base        = latest != nullptr ? std::optional{ latest->Base } : std::nullopt,
						.PullRequest = draft->Association ? std::optional{ draft->Association->Number } : std::nullopt,
						.ConsentKey  = draft->Id.Value,
		};
		if (!Changes->projection().Active || Changes->projection().Subject != subject) {
			Changes->open(std::move(subject));
		}
	}

	void local_draft_desktop::publish(const application::local_changes_projection& aProjection)
	{
		if (!Closing && View) {
			View->present_local(aProjection);
		}
	}

	auto local_draft_desktop::changes() noexcept -> application::local_changes_host&
	{
		return *Changes;
	}

	void local_draft_desktop::set_consent_question(std::function<bool()> aQuestion)
	{
		View->local_changes()->set_consent_question(std::move(aQuestion));
	}

	void local_draft_desktop::publish(const application::local_draft_home_projection& aHome)
	{
		if (!Closing) {
			Window.present_local_drafts(aHome);
		}
	}

	void local_draft_desktop::open_location(const std::string& aLocation, std::optional<std::string> aContext)
	{
		BringForward = true;
		Host->open_location(aLocation, std::move(aContext));
	}

	void local_draft_desktop::set_folder_chooser(folder_chooser aChooser)
	{
		Chooser = std::move(aChooser);
	}

	void local_draft_desktop::show_draft(const core::local_draft_id& aId)
	{
		if (Closing) {
			return;
		}
		BringForward = true;
		Host->resume(aId);
		Window.raise();
		Window.activateWindow();
		Host->refresh_home();
	}

	void local_draft_desktop::set_code_font(const QFont& aFont)
	{
		View->set_code_font(aFont);
	}

	void local_draft_desktop::set_publishing(publication_account aAccount)
	{
		if (!PublishLane) {
			PublishLane = std::make_unique<worker_executor>();
		}
		Host->set_publishing(std::move(aAccount), PublishLane.get());
		View->publication()->set_dispatch({
			.Check     = [this](const bool aTour) { Host->check_publication(aTour); },
			.Create    = [this](const std::string& aDigest) { Host->create_pull_request(aDigest); },
			.Recover   = [this] { Host->recover_publication(); },
			.Associate = [this](const std::uint64_t aNumber) { Host->associate_pull_request(aNumber); },
			.Copy =
				[this](const std::string& aCommand) {
					if (Copier) {
						Copier(aCommand);
					} else if (auto* const clipboard = QGuiApplication::clipboard()) {
						clipboard->setText(QString::fromStdString(aCommand));
					}
				},
		});
	}

	void local_draft_desktop::set_copier(std::function<void(const std::string&)> aCopier)
	{
		Copier = std::move(aCopier);
	}

	void local_draft_desktop::set_agent(application::local_draft_agent_runner aRunner, application::effect_executor* const aLane, agent_chooser aChooser)
	{
		AgentChooser = std::move(aChooser);
		// The commit message is one fresh, tool-free run of the same agent:
		// the request is its only input, and nothing resumes.
		auto commit = aRunner ? application::local_commit_message_runner{ [aRunner](const application::local_commit_message_agent& aAgent, const std::string& aPrompt,
																					 const std::stop_token aStop) -> std::expected<std::string, std::string> {
			auto answer = aRunner({ .Agent = aAgent.Agent, .AgentName = aAgent.AgentName, .Profile = aAgent.Profile, .Prompt = aPrompt }, aStop);
			if (!answer.Answered) {
				return std::unexpected{ answer.Failure.empty() ? std::string{ "The agent did not answer." } : answer.Failure };
			}
			return answer.Text;
		} }
							  : application::local_commit_message_runner{ };
		if (!CommitAgentLane) {
			CommitAgentLane = std::make_unique<worker_executor>();
		}
		Changes->set_agent(std::move(commit), CommitAgentLane.get());
		View->local_changes()->set_agent_name(QStringLiteral("the agent"));
		Host->set_agent(std::move(aRunner), aLane);
	}

	auto local_draft_desktop::host() noexcept -> application::local_draft_workbench&
	{
		return *Host;
	}

	auto local_draft_desktop::view() noexcept -> qt::local_draft_view*
	{
		return View;
	}

	void local_draft_desktop::shutdown()
	{
		Closing = true;
		// The page outlives this desktop in the window; a gesture it sends
		// while it is torn down (a title losing focus) reaches nothing.
		// A window already gone took the page with it: nothing to detach.
		if (View) {
			View->set_dispatch({ });
			View->set_preparation_dispatch({ });
			View->publication()->set_dispatch({ });
			View->local_changes()->set_dispatch({ });
			Window.set_local_draft_entry_dispatch({ }, { });
		}
		if (Changes) {
			Changes->shutdown();
		}
		if (Host) {
			Host->shutdown();
		}
	}

	auto local_draft_desktop::is_quiescent() const noexcept -> bool
	{
		return (!Host || Host->is_quiescent()) && (!Changes || Changes->is_quiescent());
	}
}
