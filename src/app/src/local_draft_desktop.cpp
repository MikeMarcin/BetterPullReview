#include "local_draft_desktop.hpp"

#include "bpr/adapters/git/installed_author_git.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/syntax_classification_application_host.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/local_draft_workflow_area.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_search.hpp"
#include "local_draft_workflow_exchange.hpp"
#include "pr_search_controller.hpp"
#include "syntax_text_sources.hpp"

#include <QClipboard>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMessageBox>
#include <QRandomGenerator>

#include <algorithm>
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
											 std::optional<std::filesystem::path> aWindowsSystemRoot, application::effect_executor& aEffects, application::decision_executor& aDecisions,
											 local_draft_syntax aSyntax, application::effect_executor* const aAuthorGitLane)
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
		// The window shows the draft in its own review surface; these are its additions.
		View = Window.local_draft();
		if (aSyntax.Definitions != nullptr && aSyntax.Effects != nullptr) {
			// The comparison's sides and the Edit view's text, classified by the
			// packaged definitions the review panes use, on their lane.
			SyntaxDefinitions = aSyntax.Definitions;
			SyntaxTexts       = std::make_unique<suggestion_text_source>();
			SyntaxClassifier  = std::make_unique<adapters::syntax::packaged_syntax_classifier>(*SyntaxDefinitions, *SyntaxTexts);
			SyntaxRelay       = std::make_unique<code_syntax_relay>([this](const application::token_span_publication* aSpans) { syntax_done(aSpans); });
			SyntaxHost        = std::make_unique<application::syntax_classification_application_host>(*SyntaxClassifier, *aSyntax.Effects, aDecisions, *SyntaxRelay, core::application_session_id{ 3U },
																									  application::appearance_host_limits{ .OutstandingRequests = 1U, .TokenSpanCacheEntries = 8U });
			SyntaxHost->observe_work(aSyntax.Insights);
			Window.set_local_syntax_dispatch([this](const std::string& aPath, const std::string& aDocument) { want_syntax(aPath, aDocument); });
		}
		// Find in files: the pull request's own search host and panel, over the shown candidate.
		SearchCorpus = std::make_unique<local_draft_corpus>([root = aProfileRoot] { return adapters::sqlite::open_local_draft_store(root); });
		SearchLane   = std::make_unique<worker_executor>();
		if (Window.pr_search() != nullptr && Window.comparison_surface() != nullptr) {
			Find = std::make_unique<pr_search_controller>(
				*Window.pr_search(), *Window.comparison_surface(), *SearchCorpus, core::default_source_regex_engine(), aDecisions, *SearchLane,
				[this]() -> std::optional<core::source_search_scope> {
					if (Closing || View == nullptr || !Window.showing_local_draft() || !View->projection().Draft) {
						return std::nullopt;
					}
					const auto& draft = *View->projection().Draft;
					const auto  shown = std::ranges::find(draft.Candidates, View->projection().Shown, &core::local_draft_candidate::Ordinal);
					if (shown == draft.Candidates.end()) {
						return std::nullopt;
					}
					auto scope = local_search_scope(draft, *shown);
					SearchCorpus->set_candidate(scope, local_candidate_subject(draft, shown->Ordinal), shown->Changes);
					return scope;
				},
				[this](const core::source_search_scope&, const application::source_corpus_entry& aEntry, pr_search_controller::result_tab) -> std::optional<core::document_tab_id> {
					const auto path = std::string{ aEntry.path() };
					if (!Window.open_local_file(QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())))) {
						return std::nullopt;
					}
					return local_search_tab;
				},
				[this](const core::source_search_scope&, core::file_identity, const std::string& aPath, const core::source_match_location& aLocation) {
					const auto& shown = Window.local_comparison();
					return shown && shown->Path == aPath && shown->Context.Key.File != core::file_identity{ }
					&& (aLocation.Side == core::diff_side::left ? shown->Context.Key.LeftBlob : shown->Context.Key.RightBlob) == std::optional{ aLocation.Blob };
				},
				[this]() -> std::optional<core::document_tab_id> { return Window.showing_local_draft() ? std::optional{ local_search_tab } : std::nullopt; });
			Find->set_scope_name(QStringLiteral("Local draft"));
			// The panel is the window's; a desktop outliving it leaves it alone.
			FindPanelGone = QObject::connect(Window.pr_search(), &QObject::destroyed, [find = Find.get()] { find->abandon_panel(); });
		}
		Window.set_local_change_dispatch([this](const std::string& aPath, const core::local_change_hunk aChange, const bool aStage) {
			if (aStage) {
				Changes->stage_change(aPath, aChange);
			} else {
				Changes->unstage_change(aPath, aChange);
			}
		});
		// Author state: the author's own Git in their own worktree, separate
		// from the isolated reader (RFC-BPR-0114 as amended 2026-11-12).
		AuthorGit     = adapters::git::make_installed_author_git(aGitExecutable.empty() ? std::filesystem::path{ } : aGitExecutable);
		WorktreeFiles = adapters::git::make_worktree_files();
		ConsentFile   = aProfileRoot / "author-git-consent.txt";
		if (aAuthorGitLane == nullptr) {
			OwnAuthorGitLane = std::make_unique<worker_executor>();
		}
		AuthorGitLane = aAuthorGitLane != nullptr ? aAuthorGitLane : OwnAuthorGitLane.get();
		Changes       = std::make_unique<application::local_changes_host>(
			*AuthorGit, *WorktreeFiles, core::make_xdiff_line_matcher(), *AuthorGitLane, aDecisions, *this,
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
						Changes->suggest_message({ .Agent = choice->Identity, .AgentName = choice->Name, .Profile = choice->Profile, .Instructions = choice->CommitMessage, .Delivery = choice->Delivery });
					}
				},
			.Consent = [this](const bool aAllow) { Changes->answer_consent(aAllow); },
		});
		// RFC-BPR-0112 ([[BPR-2032]] to [[BPR-2034]]). Optional workflows: the
		// profile's, read on the effect lane, and the exchange files under it.
		Exchange = std::make_unique<local_draft_workflow_exchange>(aProfileRoot);
		Host->set_workflows([root = aProfileRoot] { return read_workflow_profile(root); }, Exchange.get());
		View->workflow()->set_dispatch({
			.Select            = [this](std::optional<std::string> aId) { Host->select_workflow(std::move(aId)); },
			.AcceptUpdate      = [this] { Host->accept_workflow_update(); },
			.Publisher         = [this](const core::workflow_publisher aPublisher) { Host->choose_publisher(aPublisher); },
			.HandOff           = [this](const std::string& aAction, const std::string& aAnswer) { Host->hand_off(aAction, aAnswer); },
			.Ask               = [this](const std::string& aAction) { ask_for_action(aAction); },
			.ReadAnswer        = [this](const std::string& aAction) { Host->read_answer(aAction); },
			.Lost              = [this](const std::string& aAction) { Host->mark_lost(aAction); },
			.Publish           = [this](const std::string& aAction, const std::string& aDigest) { Host->hand_off_publication(aAction, aDigest); },
			.Reconcile         = [this](const std::string& aAction) { Host->reconcile_publication(aAction); },
			.FollowUp          = [this](const std::string& aAction) { Host->hand_off_follow_up(aAction); },
			.ReconcileFollowUp = [this](const std::string& aAction) { Host->reconcile_follow_up(aAction); },
			.Copy =
				[this](const std::string& aCommand) {
					if (Copier) {
						Copier(aCommand);
					} else if (auto* const clipboard = QGuiApplication::clipboard()) {
						clipboard->setText(QString::fromStdString(aCommand));
					}
				},
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
			.OpenLocalFile     = [this](const std::string& aPath) { Changes->open_file(aPath); },
			.Save              = [this](const std::string& aPath, std::vector<std::uint8_t> aRead, std::vector<std::uint8_t> aEdited) { Changes->save(aPath, std::move(aRead), std::move(aEdited)); },
			.CreatePullRequest = [this] { open_publication(); },
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
		// [[BPR-2215]]. A window destroyed after the desktop (as a course's
		// session does) would otherwise run the panel's destroyed handler on
		// Find once it is gone: an access violation on Windows.
		QObject::disconnect(FindPanelGone);
		shutdown();
	}

	void local_draft_desktop::publish(const application::local_draft_workbench_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		if (aProjection.Stage == application::local_draft_stage::closed) {
			Window.present_local_draft(aProjection);
			follow_author_state(aProjection);
			Window.leave_local_draft();
			return;
		}
		if (std::exchange(BringForward, false) || Window.showing_local_draft()) {
			View->present(aProjection);
			Window.show_local_draft(View->window_title());
		}
		Window.present_local_draft(aProjection);
		follow_author_state(aProjection);
		present_tour();
		// RFC-BPR-0126 Q2 ([[BPR-2154]]). A question's start and end, for the
		// discussion's queue: its next message goes only once this one ended.
		if (aProjection.Asking != AskingSeen) {
			AskingSeen       = aProjection.Asking;
			const auto stop  = !AskingSeen && std::exchange(StopAsked, false);
			const auto fails = !AskingSeen && !aProjection.AskFailure.empty();
			if (AskingObserver) {
				AskingObserver(AskingSeen, fails, stop);
			}
		}
		// Created here: the window becomes the pull request.
		if (aProjection.Draft) {
			const auto& draft          = *aProjection.Draft;
			const auto* link           = draft.Publication && draft.Publication->Journal.Link ? &*draft.Publication->Journal.Link : nullptr;
			const auto  linked         = link != nullptr && draft.Publication->Journal.Phase == core::local_publication_phase::linked;
			const auto  seen           = LinkedSeen.find(draft.Id.Value);
			const auto  became         = seen != LinkedSeen.end() && !seen->second && linked;
			LinkedSeen[draft.Id.Value] = linked;
			if (became && OpenPullRequest && Window.showing_local_draft()) {
				const auto& repository = draft.Target.Target.Repository;
				OpenPullRequest(repository.Host, repository.Owner, repository.Name, link->Number);
			}
		}
		if (!aProjection.Busy) {
			TourEditInFlight = false;
			flush_tour_edits();
		}
	}

	auto local_draft_desktop::tour_shown() const -> bool
	{
		return !Closing && View != nullptr && Window.showing_local_draft() && View->projection().Draft.has_value();
	}

	void local_draft_desktop::present_tour()
	{
		auto* const editor = Window.tour_editor();
		if (!tour_shown() || editor == nullptr) {
			return;
		}
		const auto& projection = View->projection();
		auto        asking     = std::optional<core::local_tour_asking>{ };
		if (projection.Asking) {
			asking = core::local_tour_asking{ .Thread = projection.AskingThread, .Text = projection.AskingText, .Steered = projection.AskingSteered };
		}
		auto view = core::view_local_tour(*projection.Draft, asking);
		present_checks(view);
		auto shown          = application::tour_presentation{ };
		shown.Draft         = std::make_shared<const core::review_tour>(view.Tour);
		shown.SavedRevision = view.Tour.Revision;
		shown.ChangedFiles  = view.Files;
		shown.Saving        = !TourEdits.empty() || TourEditInFlight;
		// The draft's own words: a candidate, never a commit; its reasons.
		auto reasons = std::make_shared<std::vector<std::pair<std::uint64_t, core::local_stop_reason>>>(view.Reasons);
		auto stops   = std::make_shared<std::vector<core::local_tour_stop>>(projection.Draft->Preparation.Tour.Stops);
		editor->set_local_words(qt::tour_local_words{
			.Reason =
				[reasons](const core::tour_stop_id aStop) {
					const auto found = std::ranges::find(*reasons, aStop.value(), &std::pair<std::uint64_t, core::local_stop_reason>::first);
					if (found == reasons->end() || found->second == core::local_stop_reason::not_accepted || found->second == core::local_stop_reason::none) {
						return QString{ };
					}
					return QStringLiteral("Changed since accepted: %1.").arg(QString::fromUtf8(core::name(found->second).data(), static_cast<qsizetype>(core::name(found->second).size())));
				},
			.Source =
				[stops](const core::tour_stop_id aStop) {
					const auto found = std::ranges::find(*stops, aStop.value(), &core::local_tour_stop::Id);
					if (found == stops->end()) {
						return QString{ };
					}
					const auto& source = found->Source;
					return QStringLiteral("%1 · lines %2–%3 · candidate %4").arg(QString::fromStdString(source.Path)).arg(source.FirstLine).arg(source.LastLine).arg(source.Candidate);
				},
		});
		editor->set_description_publication_default(core::update_description_by_default(true));
		editor->present(shown);
		editor->present_tour_session(view.Session, view.Proposal);
		editor->set_tour_running(std::nullopt, false, projection.Asking);
		// A local draft's runs are its own lane's: the chat may send whenever none runs.
		editor->set_tour_host_idle(!projection.Asking);
		if (!projection.AskFailure.empty()) {
			editor->show_writing_notice(QString::fromStdString(projection.AskFailure));
		}
		TourView = std::move(view);
		if (TourObserver) {
			TourObserver();
		}
	}

	auto local_draft_desktop::shown_draft() const -> std::string { return tour_shown() ? View->projection().Draft->Id.Value : std::string{ }; }

	auto local_draft_desktop::asking() const -> bool { return Host && Host->projection().Asking; }

	auto local_draft_desktop::asking_question() const -> std::optional<std::pair<core::tour_thread, std::uint64_t>>
	{
		if (!Host || !tour_shown()) {
			return std::nullopt;
		}
		const auto& projection = Host->projection();
		if (!projection.Asking || projection.AskingRun == 0U || projection.Checking != application::local_check_mode::none) {
			return std::nullopt;
		}
		return std::pair{ core::local_tour_thread(projection.AskingThread), projection.AskingRun };
	}

	auto local_draft_desktop::answered_steers() const -> std::pair<std::uint64_t, std::vector<application::local_draft_steered>>
	{
		if (!Host) {
			return { };
		}
		return { Host->projection().AnsweredRun, Host->projection().AnsweredSteers };
	}

	auto local_draft_desktop::agent_ready() const -> bool { return AgentChooser && AgentChooser().has_value(); }

	auto local_draft_desktop::agent_name() const -> std::string
	{
		const auto choice = AgentChooser ? AgentChooser() : std::nullopt;
		return choice ? choice->Name : std::string{ };
	}

	// --- RFC-BPR-0120: a check ------------------------------------------------------

	void local_draft_desktop::present_checks(core::local_tour_view& aView)
	{
		auto* const editor = Window.tour_editor();
		if (editor == nullptr || editor->side_chat() == nullptr) {
			return;
		}
		const auto& projection = View->projection();
		const auto& draft      = *projection.Draft;
		if (CheckAgent.empty()) {
			for (const auto& conversation : draft.Preparation.Conversations) {
				if (conversation.Thread == core::local_change_thread) {
					CheckAgent     = conversation.Agent;
					CheckAgentName = conversation.AgentName;
				}
			}
		}
		const auto grants = core::grants_for(draft.Preparation.CheckGrants, CheckAgent);
		// The core decides whether each plan card's Run is available, and why not.
		for (auto& run : aView.Session.Runs) {
			if (run.Check && run.Check->Plan) {
				const auto block = core::decide_check_run(*run.Check->Plan, { .Grants = grants, .Running = projection.Asking, .Candidate = core::latest_candidate(draft) != nullptr });
				run.CheckBlocked = block == core::local_check_block::discarded || block == core::local_check_block::ran ? std::string{ } : core::check_block_words(block);
			}
		}
		auto* const chat = editor->side_chat();
		if (projection.ChecksAvailable) {
			chat->set_check_dispatch({ .Plan    = [this](const QString& aWords) { plan_check(aWords); },
									   .Run     = [this](const core::tour_agent_run_id aRun) { run_check(aRun, false); },
									   .Discard = [this](const core::tour_agent_run_id aRun) {
										   if (tour_shown()) {
											   Host->discard_check(aRun.value() / 1024U, static_cast<std::size_t>(aRun.value() % 1024U));
										   } },
									   .Open    = [this](const core::tour_agent_run_id aRun) { open_check_report(aRun); },
									   .Again   = [this](const core::tour_agent_run_id aRun) { run_check(aRun, true); },
									   .Grants  = [this](const bool aScratch, const bool aBuilds) { check_grants(aScratch, aBuilds); } });
		} else {
			chat->set_check_dispatch({ });
		}
		chat->set_check_state(grants.Scratch, grants.Builds, projection.ChecksAvailable && !projection.Asking,
							  projection.Asking ? QStringLiteral("A question or a check is with the agent. Plan the next check when it ends.") : QString{ });
	}

	auto local_draft_desktop::check_question(const std::string& aWords) -> std::optional<application::local_check_question>
	{
		const auto choice = AgentChooser ? AgentChooser() : std::nullopt;
		if (!choice) {
			return std::nullopt;
		}
		CheckAgent     = choice->Identity;
		CheckAgentName = choice->Name;
		return application::local_check_question{ .Words        = aWords,
												  .Agent        = choice->Identity,
												  .AgentName    = choice->Name,
												  .Profile      = choice->Profile,
												  .Instructions = choice->Check,
												  .Version      = choice->CheckVersion,
												  .Delivery     = choice->Delivery };
	}

	void local_draft_desktop::plan_check(const QString& aWords)
	{
		if (!tour_shown()) {
			return;
		}
		if (auto question = check_question(aWords.toStdString())) {
			CheckStartedMs = 0;
			Host->plan_check(std::move(*question));
		}
	}

	void local_draft_desktop::run_check(const core::tour_agent_run_id aRun, const bool aAgain)
	{
		if (!tour_shown()) {
			return;
		}
		if (auto question = check_question({ })) {
			CheckStartedMs = 0;
			Host->run_check(aRun.value() / 1024U, static_cast<std::size_t>(aRun.value() % 1024U), std::move(*question), aAgain);
		}
	}

	void local_draft_desktop::check_grants(const bool aScratch, bool aBuilds)
	{
		if (!tour_shown() || CheckAgent.empty()) {
			return;
		}
		auto grants = core::grants_for(View->projection().Draft->Preparation.CheckGrants, CheckAgent);
		// RFC-BPR-0104: the first build in a draft asks.
		if (aBuilds && !grants.Builds && !grants.Asked) {
			const auto allowed = BuildQuestion ? BuildQuestion()
											   : QMessageBox::question(Window.tour_editor(), QStringLiteral("Allow build and run tests"), QString::fromUtf8(core::local_check_build_question.data(), static_cast<qsizetype>(core::local_check_build_question.size())))
					== QMessageBox::Yes;
			if (!allowed) {
				aBuilds = false;
				if (auto* const editor = Window.tour_editor(); editor != nullptr && editor->side_chat() != nullptr) {
					editor->side_chat()->set_check_state(grants.Scratch, grants.Builds, true, { }, true);
				}
			} else {
				grants.Asked = true;
			}
		}
		grants.Scratch = aScratch;
		grants.Builds  = aBuilds;
		Host->set_check_grants(std::move(grants));
	}

	void local_draft_desktop::open_check_report(const core::tour_agent_run_id aRun)
	{
		if (!tour_shown()) {
			return;
		}
		const auto& draft         = *View->projection().Draft;
		const auto& conversations = draft.Preparation.Conversations;
		const auto  found         = std::ranges::find(conversations, aRun.value() / 1024U, &core::local_conversation::Id);
		const auto  turn          = static_cast<std::size_t>(aRun.value() % 1024U);
		if (found == conversations.end() || turn >= found->Turns.size() || turn == 0U || !found->Turns[turn].Check || !found->Turns[turn].Check->Report) {
			return;
		}
		const auto& report = *found->Turns[turn].Check->Report;
		const auto* latest = core::latest_candidate(draft);
		ReportCandidate    = report.Candidate;
		auto document      = qt::agent_report_document{ };
		document.Turn      = aRun.value();
		document.Title     = QStringLiteral("Check: %1").arg(QString::fromUtf8(core::outcome_words(report.Outcome).data(), static_cast<qsizetype>(core::outcome_words(report.Outcome).size())));
		document.Markdown  = QString::fromStdString(core::render_check_report(report, latest == nullptr ? report.Candidate : latest->Ordinal));
		document.Question  = QString::fromStdString(found->Turns[turn - 1U].Text);
		Window.open_report("check:" + draft.Id.Value + ":" + std::to_string(aRun.value()), document);
	}

	auto local_draft_desktop::follow_check_reference(const std::string& aLink) -> bool
	{
		const auto reference = core::read_check_reference_link(aLink);
		if (!reference || !tour_shown()) {
			return false;
		}
		const auto* latest = core::latest_candidate(*View->projection().Draft);
		const auto  line   = std::max<std::uint32_t>(reference->Line, 1U);
		View->reveal(reference->Path, line, line, latest != nullptr && latest->Ordinal == ReportCandidate ? 0U : ReportCandidate);
		return true;
	}

	void local_draft_desktop::publish_check_activity(const std::optional<core::agent_turn_activity>& aActivity)
	{
		auto* const editor = Window.tour_editor();
		if (Closing || editor == nullptr || !tour_shown()) {
			return;
		}
		if (!aActivity) {
			editor->present_run_status(std::nullopt);
			return;
		}
		if (CheckStartedMs == 0) {
			CheckStartedMs = qt::agent_activity_clock_ms();
		}
		editor->present_run_status(qt::agent_run_status{ .Agent = QString::fromStdString(CheckAgentName), .Stage = qt::agent_run_stage::thinking, .Activity = aActivity, .ActivityStartedMs = CheckStartedMs });
	}

	void local_draft_desktop::set_check_copies(std::unique_ptr<application::local_check_copy_port> aCopies, std::function<std::uint64_t()> aLimitBytes)
	{
		CheckCopies = std::move(aCopies);
		Host->set_check_copies(CheckCopies.get(), std::move(aLimitBytes));
	}

	void local_draft_desktop::set_build_question(std::function<bool()> aQuestion)
	{
		BuildQuestion = std::move(aQuestion);
	}

	void local_draft_desktop::queue_tour_edit(std::string aKey, std::function<void()> aApply)
	{
		if (!aKey.empty() && !TourEdits.empty() && TourEdits.back().Key == aKey) {
			TourEdits.back().Apply = std::move(aApply);
		} else {
			TourEdits.push_back({ .Key = std::move(aKey), .Apply = std::move(aApply) });
		}
		flush_tour_edits();
	}

	void local_draft_desktop::flush_tour_edits()
	{
		if (Closing || TourEditInFlight || TourEdits.empty() || Host->projection().Busy) {
			return;
		}
		auto edit = std::move(TourEdits.front());
		TourEdits.pop_front();
		TourEditInFlight = true;
		edit.Apply();
	}

	void local_draft_desktop::tour_command(const core::tour_command& aCommand)
	{
		if (!tour_shown()) {
			return;
		}
		for (auto& edit : core::translate_local_tour_command(*View->projection().Draft, aCommand)) {
			switch (edit.Kind) {
			case core::local_tour_edit_kind::brief:
				// The Problem and Solution as typed, each kept until sent; the
				// one waiting edit sends both, the other as the draft has it.
				(aCommand.Kind == core::tour_command_kind::problem ? TypedProblem : TypedSolution) = aCommand.Text;
				queue_tour_edit("brief", [this] {
					const auto& tour = Host->projection().Draft->Preparation.Tour;
					Host->save_brief(std::exchange(TypedProblem, std::nullopt).value_or(tour.Problem), std::exchange(TypedSolution, std::nullopt).value_or(tour.Solution));
				});
				break;
			case core::local_tour_edit_kind::stop_text:
				queue_tour_edit("stop:" + std::to_string(edit.Stop), [this, edit] { Host->edit_stop(edit.Stop, edit.Title, edit.Text); });
				break;
			case core::local_tour_edit_kind::accept:   queue_tour_edit({ }, [this, stop = edit.Stop] { Host->accept_stop(stop); }); break;
			case core::local_tour_edit_kind::unaccept: queue_tour_edit({ }, [this, stop = edit.Stop] { Host->unaccept_stop(stop); }); break;
			case core::local_tour_edit_kind::remove:   queue_tour_edit({ }, [this, stop = edit.Stop] { Host->remove_stop(stop); }); break;
			case core::local_tour_edit_kind::move:     queue_tour_edit({ }, [this, stop = edit.Stop, index = edit.Index] { Host->move_stop(stop, index); }); break;
			case core::local_tour_edit_kind::none:     break;
			}
		}
	}

	void local_draft_desktop::tour_action(const qt::tour_editor_action aAction)
	{
		auto* const editor = Window.tour_editor();
		if (!tour_shown() || editor == nullptr) {
			return;
		}
		const auto* const stop      = editor->selected_stop();
		const auto        draftStop = [&]() -> const core::local_tour_stop* {
			if (stop == nullptr) {
				return nullptr;
			}
			const auto& stops = View->projection().Draft->Preparation.Tour.Stops;
			const auto  found = std::ranges::find(stops, stop->Id.value(), &core::local_tour_stop::Id);
			return found == stops.end() ? nullptr : &*found;
		}();
		switch (aAction) {
		case qt::tour_editor_action::add_stop:
		case qt::tour_editor_action::insert_after:
			// A stop on the lines selected in the latest candidate's comparison.
			if (const auto lines = View->selected_lines()) {
				queue_tour_edit({ }, [this, chosen = *lines] { Host->add_stop(chosen.Path, chosen.First, chosen.Last, { }, { }); });
			} else {
				editor->show_notice(QStringLiteral("Select lines of the latest candidate in Review, then add the stop."));
			}
			return;
		case qt::tour_editor_action::retarget_stop:
			if (draftStop == nullptr) {
				return;
			}
			if (const auto lines = View->selected_lines()) {
				queue_tour_edit({ }, [this, id = draftStop->Id, title = draftStop->Title, text = draftStop->Text, chosen = *lines] { Host->edit_stop(id, title, text, chosen.Path, chosen.First, chosen.Last); });
			} else {
				editor->show_notice(QStringLiteral("Select lines of the latest candidate in Review first."));
			}
			return;
		case qt::tour_editor_action::navigate_stop:
			if (draftStop != nullptr) {
				// Its lines where they are exactly on the latest candidate, else where it was bound.
				const auto exact = draftStop->Current.Correspondence == core::local_correspondence::exact;
				View->reveal(draftStop->Source.Path, exact ? draftStop->Current.FirstLine : draftStop->Source.FirstLine, exact ? draftStop->Current.LastLine : draftStop->Source.LastLine,
							 exact ? std::uint64_t{ 0U } : draftStop->Source.Candidate);
			}
			return;
		case qt::tour_editor_action::ask_code:
			editor->ask_about_code(std::nullopt);
			return;
		default:
			// Publication, import, conflicts and undo belong to a pull
			// request's tour; a local draft has none of them.
			return;
		}
	}

	void local_draft_desktop::ask_local(application::local_draft_question aQuestion)
	{
		const auto choice = AgentChooser ? AgentChooser() : std::nullopt;
		if (!choice) {
			if (auto* const editor = Window.tour_editor(); editor != nullptr) {
				editor->show_notice(QStringLiteral("Choose an agent in the status bar to ask about this draft."));
			}
			return;
		}
		const auto describing  = aQuestion.Subject == core::local_subject::title || aQuestion.Subject == core::local_subject::description;
		aQuestion.Agent        = choice->Identity;
		aQuestion.AgentName    = choice->Name;
		aQuestion.Profile      = choice->Profile;
		aQuestion.Instructions = describing ? choice->Description : choice->Discussion;
		aQuestion.Version      = describing ? choice->DescriptionVersion : choice->DiscussionVersion;
		aQuestion.Delivery     = choice->Delivery;
		Host->ask(std::move(aQuestion));
	}

	void local_draft_desktop::ask_for_action(const std::string& aAction)
	{
		const auto& projection = Host->projection();
		const auto  action     = std::ranges::find(projection.Workflow.Actions, aAction, [](const application::local_draft_workflow_action_view& aView) { return aView.Action.Id; });
		const auto  choice     = AgentChooser ? AgentChooser() : std::nullopt;
		// The action runs as the agent it is bound to: the window's choice must be that one.
		if (action == projection.Workflow.Actions.end() || !choice || choice->Name != action->AgentName) {
			if (auto* const editor = Window.tour_editor(); editor != nullptr && action != projection.Workflow.Actions.end()) {
				editor->show_notice(QStringLiteral("This action runs as %1. Choose it in the status bar, then ask again.").arg(QString::fromStdString(action->AgentName)));
			}
			return;
		}
		Host->ask_for_action(aAction, { .Agent = choice->Identity, .AgentName = choice->Name, .Profile = choice->Profile, .Instructions = choice->Description, .Version = choice->DescriptionVersion, .Delivery = choice->Delivery });
	}

	void local_draft_desktop::reload_workflows()
	{
		if (Host) {
			Host->reload_workflows();
		}
	}

	void local_draft_desktop::tour_ask(const qt::tour_ask& aAsk)
	{
		if (!tour_shown()) {
			return;
		}
		auto       question = application::local_draft_question{ .Thread = core::local_thread_of(aAsk.Thread), .Subject = core::local_subject_of(aAsk.Thread), .Stop = aAsk.Thread.Kind == core::tour_thread_kind::stop ? aAsk.Thread.Stop.value() : 0U };
		auto       text     = aAsk.Question.toStdString();
		const auto part     = aAsk.Thread.Kind == core::tour_thread_kind::problem ? std::string{ "Problem" } : aAsk.Thread.Kind == core::tour_thread_kind::solution ? std::string{ "Solution" }
																																									: std::string{ "stop" };
		if (aAsk.Fresh) {
			// Enhance: a rewrite of the part, proposed for the author to accept.
			text = "Rewrite this " + part + " so it reads clearly and accurately. Propose the rewrite; do not change anything else.";
		} else if (aAsk.Update) {
			text = "Rewrite this " + part + " from our conversation. Propose the rewrite.";
		}
		if (aAsk.Passage) {
			if (aAsk.Passage->Field == core::tour_passage_field::stop_code && !aAsk.Passage->Path.empty()) {
				question.Subject   = core::local_subject::lines;
				question.LinesPath = aAsk.Passage->Path;
				question.FirstLine = aAsk.Passage->FirstLine;
				question.LastLine  = aAsk.Passage->LastLine;
			} else if (!aAsk.Passage->Text.empty()) {
				text += "\n\nAbout this passage:\n> " + aAsk.Passage->Text;
			}
		}
		if (text.empty()) {
			return;
		}
		question.Text   = std::move(text);
		const auto self = std::exchange(WritingAsk, false);
		ask_local(std::move(question));
		AskingForWriting = self && Host->projection().Asking;
	}

	void local_draft_desktop::tour_writing(const core::writing_hook aHook)
	{
		if (!tour_shown()) {
			return;
		}
		const auto thread = aHook == core::writing_hook::problem ? core::tour_thread{ .Kind = core::tour_thread_kind::problem }
			: aHook == core::writing_hook::solution              ? core::tour_thread{ .Kind = core::tour_thread_kind::solution }
																 : core::tour_thread{ };
		auto       ask    = qt::tour_ask{ .Thread = thread, .Fresh = true };
		if (auto* const editor = Window.tour_editor(); editor != nullptr && aHook != core::writing_hook::problem && aHook != core::writing_hook::solution && editor->selected_stop() != nullptr) {
			ask.Thread = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = editor->selected_stop()->Id };
		}
		WritingAsk = true;
		tour_ask(ask);
		WritingAsk = false;
	}

	void local_draft_desktop::writing_stop()
	{
		// [[BPR-2154]]. Only the one-shot writing request's question: a
		// question of the discussion goes on, as the pull request's tour
		// chat's does (the tour editor lets writing go on every hide,
		// refresh and focus change).
		if (tour_shown() && Host->projection().Checking == application::local_check_mode::none && AskingForWriting && Host->projection().Asking) {
			StopAsked = true;
			Host->stop_asking();
		}
	}

	void local_draft_desktop::tour_stop()
	{
		if (tour_shown()) {
			StopAsked = Host->projection().Asking;
			Host->stop_asking();
		}
	}

	void local_draft_desktop::tour_decide(const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText)
	{
		if (!tour_shown() || !TourView || aIndex >= TourView->ProposalItems.size() || aDecision == core::tour_proposal_decision::pending) {
			return;
		}
		const auto proposal = TourView->ProposalItems[aIndex];
		const auto accepted = aDecision == core::tour_proposal_decision::accepted;
		queue_tour_edit({ }, [this, proposal, accepted] { Host->decide_proposal(proposal, accepted); });
		// The author's edit of the proposed text, after it is accepted.
		const auto& item = TourView->Proposal->Items[aIndex];
		const auto  text = aText.toStdString();
		if (!accepted || aText.isNull() || text == item.Text) {
			return;
		}
		if (item.Part == core::tour_proposal_part::problem || item.Part == core::tour_proposal_part::solution) {
			const auto problem = item.Part == core::tour_proposal_part::problem;
			queue_tour_edit({ }, [this, problem, text] {
				const auto& tour = Host->projection().Draft->Preparation.Tour;
				Host->save_brief(problem ? text : tour.Problem, problem ? tour.Solution : text);
			});
		} else if (item.Part == core::tour_proposal_part::stop_text) {
			queue_tour_edit({ }, [this, stop = item.StopId.value(), title = item.Stop.Title, text] { Host->edit_stop(stop, title, text); });
		}
	}

	void local_draft_desktop::save_description(const std::string& aBody)
	{
		if (tour_shown()) {
			Host->save_description(View->projection().Draft->Title, aBody);
		}
	}

	void local_draft_desktop::set_pull_request_opener(pull_request_opener aOpener)
	{
		OpenPullRequest = std::move(aOpener);
	}

	void local_draft_desktop::open_publication()
	{
		if (!tour_shown()) {
			return;
		}
		Window.show_local_publication();
		const auto& view = View->projection().Publishing;
		if (!view.Checking && !view.Creating) {
			Host->check_publication(!View->projection().Draft->Preparation.Tour.Stops.empty());
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
		if (!Closing && View != nullptr) {
			Window.present_local_changes(aProjection);
		}
	}

	void local_draft_desktop::associate_for_look(const std::uint64_t aNumber)
	{
		const auto& shown = View->projection();
		if (!shown.Draft) {
			return;
		}
		auto store = adapters::sqlite::open_local_draft_store(ConsentFile.parent_path());
		auto draft = store ? store->find(shown.Draft->Id) : std::unexpected{ application::local_draft_store_error{ } };
		if (!draft) {
			return;
		}
		const auto& repository = draft->Target.Target.Repository;
		auto        associated = core::associate_draft(*draft, draft->Version,
													   { .Host = repository.Host, .Owner = repository.Owner, .Name = repository.Name, .Number = aNumber, .HeadRef = draft->Branch },
													   draft->Target.Target.Branch, static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch()));
		if (associated && store->put(*associated, draft->Version, { })) {
			BringForward = true;
			Host->resume(associated->Id);
		}
	}

	auto local_draft_desktop::author_git_lane() noexcept -> application::effect_executor&
	{
		return *AuthorGitLane;
	}

	auto local_draft_desktop::publication_lane() noexcept -> application::effect_executor*
	{
		return PublishLane.get();
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
			auto answer = aRunner({ .Agent = aAgent.Agent, .AgentName = aAgent.AgentName, .Profile = aAgent.Profile, .Prompt = aPrompt, .Instructions = aAgent.Instructions, .Path = core::agent_run_path::local_commit_message, .Delivery = aAgent.Delivery }, aStop);
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

	auto local_draft_desktop::view() noexcept -> qt::local_draft_parts*
	{
		return View;
	}

	void local_draft_desktop::search(const int aGesture)
	{
		if (Closing || !Find) {
			return;
		}
		const auto gesture = static_cast<qt::source_find_gesture>(aGesture);
		if (gesture == qt::source_find_gesture::document_changed) {
			Find->open();
		} else if (gesture == qt::source_find_gesture::next || gesture == qt::source_find_gesture::previous) {
			Find->navigate(gesture == qt::source_find_gesture::next ? core::change_navigation_direction::next : core::change_navigation_direction::previous);
		} else {
			Find->dispatch(gesture);
		}
	}

	void local_draft_desktop::attach_search()
	{
		if (Find) {
			Find->attach();
			Find->observe_scope();
		}
	}

	void local_draft_desktop::adopt_appearance(const qt::appearance_profile& aProfile)
	{
		if (View != nullptr) {
			View->adopt_appearance(aProfile);
		}
	}

	void local_draft_desktop::want_syntax(const std::string& aPath, const std::string& aDocument)
	{
		if (Closing || !SyntaxHost) {
			return;
		}
		auto       document = std::make_shared<const std::string>(aDocument);
		const auto bytes    = std::span{ reinterpret_cast<const std::uint8_t*>(document->data()), document->size() };
		const auto blob     = core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
		if (!blob) {
			return;
		}
		auto wanted = wanted_syntax{ .Key = { .Blob = *blob, .Definition = SyntaxDefinitions->definition_for_path(aPath) }, .Document = std::move(document) };
		if ((SyntaxClassifying && SyntaxClassifying->Key == wanted.Key) || std::ranges::find(SyntaxWanted, wanted.Key, &wanted_syntax::Key) != SyntaxWanted.end()) {
			return;
		}
		// A known answer is adopted at once.
		if (auto spans = SyntaxHost->cached(wanted.Key)) {
			Window.adopt_local_syntax(*wanted.Document, spans);
			return;
		}
		// The newest texts are the ones on screen; the oldest waiting gives way.
		constexpr auto waiting = std::size_t{ 6U };
		if (SyntaxWanted.size() >= waiting) {
			SyntaxWanted.pop_front();
		}
		SyntaxWanted.push_back(std::move(wanted));
		submit_syntax();
	}

	void local_draft_desktop::submit_syntax()
	{
		if (Closing || !SyntaxHost || SyntaxClassifying || SyntaxWanted.empty()) {
			return;
		}
		SyntaxClassifying = std::move(SyntaxWanted.front());
		SyntaxWanted.pop_front();
		SyntaxTexts->hold(SyntaxClassifying->Key.Blob, SyntaxClassifying->Document);
		SyntaxHost->classify(SyntaxClassifying->Key);
	}

	void local_draft_desktop::syntax_done(const application::token_span_publication* const aSpans)
	{
		if (SyntaxClassifying) {
			SyntaxTexts->release(SyntaxClassifying->Key.Blob);
			if (!Closing && aSpans != nullptr && aSpans->Key == SyntaxClassifying->Key) {
				Window.adopt_local_syntax(*SyntaxClassifying->Document, aSpans->Spans);
			}
		}
		SyntaxClassifying.reset();
		submit_syntax();
	}

	void local_draft_desktop::shutdown()
	{
		Closing = true;
		// The page outlives this desktop in the window; a gesture it sends
		// while it is torn down (a title losing focus) reaches nothing.
		// A window already gone took the page with it: nothing to detach.
		if (View != nullptr) {
			Window.set_local_syntax_dispatch({ });
			Window.set_local_change_dispatch({ });
			View->set_dispatch({ });
			View->publication()->set_dispatch({ });
			View->workflow()->set_dispatch({ });
			View->local_changes()->set_dispatch({ });
			Window.set_local_draft_entry_dispatch({ }, { });
		}
		if (SyntaxHost) {
			SyntaxHost->shutdown();
		}
		if (Find) {
			Find->shutdown();
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
		return (!Host || Host->is_quiescent()) && (!Changes || Changes->is_quiescent()) && (!Find || Find->is_quiescent());
	}
}
