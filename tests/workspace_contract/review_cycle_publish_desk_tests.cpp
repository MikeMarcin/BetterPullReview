#include "../sqlite_contract/scratch_root.hpp"
#include "product_diff.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "publication_course.hpp"
#include "simulated_github.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1177]]. Publish as Pull Request through the desktop's cycle
/// host: the real store, the real folder and Git checkout, the product's
/// installed-Git catalog, and an in-memory GitHub behind the publication
/// catalog's port (no live GitHub). RC61-08: the preview keeps R0, Rn, P and
/// T apart and lists starting work, improvements, deletions and what stays
/// out; any edit after a plan makes its digest worthless. RC59-09: the
/// published pull request is the association, the description is the
/// reviewer's Problem and Solution in the form a Review Tour maps, nothing
/// private reaches GitHub, and a pull request published with one's own tools
/// is linked only after a read of exactly it, as externally published.
/// RC59-08: after the squashed publication and a reclaim, every revision,
/// round, response and decision is still there, across a restart, until the
/// cycle is removed explicitly.

namespace {
	using namespace bpr;
	using application::review_cycle_publication_stage;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::simulated_github;
	using tests::workspace_contract::write_file;

	constexpr auto profile  = core::bpr_profile_id{ 6 };
	constexpr auto reviewer = core::github_account_id{ 1001U };

	/// Words only the cycle holds; none of them may reach GitHub.
	constexpr auto private_note  = std::string_view{ "PRIVATE-NOTE-7f3a: why check here?" };
	constexpr auto private_reply = std::string_view{ "PRIVATE-REPLY-91b2: checked, it stays." };

	struct recording_port final : application::review_cycle_workbench_port
	{
		application::review_cycle_workbench_projection Last;
		void                                           publish(const application::review_cycle_workbench_projection& aProjection) override
		{
			Last = aProjection;
		}
		void publish(const std::vector<application::review_cycle_recent>&) override
		{
		}
	};

	struct clipboard_double final : application::review_cycle_clipboard
	{
		std::vector<std::string> Written;
		auto                     write(const std::string_view aText) -> outcome override
		{
			Written.emplace_back(aText);
			return outcome::confirmed;
		}
	};

	/// The cycle host over a profile root, drained on manual lanes, with the
	/// publication environment a desktop composes: the installed-Git catalog
	/// in its own isolation root, an account and a fixed clock.
	struct desk
	{
		application::manual_executor                         Lanes;
		std::unique_ptr<application::workspace_source_port>  Source = adapters::workspace::make_local_workspace_source();
		recording_port                                       Port;
		clipboard_double                                     Clipboard;
		std::unique_ptr<application::review_cycle_workbench> Host;
		/// Whose account the window is signed in as now; absent when signed out.
		std::optional<application::review_cycle_publication_account> Account;

		desk(const std::filesystem::path& aProfileRoot, const std::filesystem::path& aGit, const std::filesystem::path& aIsolation, std::shared_ptr<simulated_github> aRemote)
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			seed[0]   = 0x77;
			Host      = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, profile), *Source, Lanes, Lanes, Port, seed, nullptr, bpr::tests::product_line_matcher());
			Host->set_clipboard(&Clipboard);
			Host->set_destination_factory([](const std::string& aPath) { return adapters::workspace::make_local_export_destination(std::filesystem::path{ aPath }); });
			Account = application::review_cycle_publication_account{ .Provider = std::move(aRemote), .Account = reviewer, .Login = "octo-reviewer", .Host = "github.com" };
			Host->set_publication_environment({
				.Candidates =
					[aGit, aIsolation](std::stop_token aStop) -> application::publication_git_result<std::unique_ptr<application::publication_candidate_port>> {
					auto made = adapters::git::make_installed_git_publication({ .GitExecutable     = std::filesystem::canonical(aGit),
																				.IsolationRoot     = std::filesystem::canonical(aIsolation),
																				.WindowsSystemRoot = tests::git_contract::windows_system_root() },
																			  std::move(aStop));
					if (!made) {
						return std::unexpected(application::publication_git_error{ .Failure = application::publication_git_failure::unavailable });
					}
					return std::move(*made);
				},
				.Account = [this] { return Account; },
				.Now     = [] { return core::git_signature{ .Seconds = 1'760'000'000, .OffsetMinutes = -420 }; },
			});
		}
		desk(const desk&)                    = delete;
		auto operator=(const desk&) -> desk& = delete;
		desk(desk&&)                         = delete;
		auto operator=(desk&&) -> desk&      = delete;
		~desk()
		{
			Host->shutdown();
			Lanes.drain();
		}
		auto view() -> const application::review_cycle_workbench_projection&
		{
			Lanes.drain();
			return Host->projection();
		}
		auto sheet() -> const application::review_cycle_publication_view&
		{
			const auto& shown = view();
			expect(shown.Publication.has_value(), "Publish as Pull Request is open");
			return *shown.Publication;
		}
		void edit(const std::function<void(application::review_cycle_publication_form&)>& aChange)
		{
			auto form = sheet().Form;
			aChange(form);
			Host->edit_publication(std::move(form));
			Lanes.drain();
		}
	};

	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	auto row(const core::outgoing_plan& aPlan, const std::string_view aPath) -> const core::outgoing_row*
	{
		const auto found = std::ranges::find(aPlan.Rows, aPath, &core::outgoing_row::Path);
		return found == aPlan.Rows.end() ? nullptr : &*found;
	}

	auto contains(const std::string_view aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string_view::npos;
	}

	/// Every byte that reached the simulated server: blobs and pull request text.
	auto uploaded(const simulated_github& aRemote) -> std::string
	{
		auto all = std::string{ };
		for (const auto& [id, bytes] : aRemote.Blobs) {
			all += text(bytes);
		}
		for (const auto& draft : aRemote.Drafts) {
			all += draft.Title + draft.Body + draft.Head + draft.Base;
		}
		return all;
	}

	void publishes_links_and_keeps_history(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-publish-profile" };
		tests::sqlite_contract::scratch_root isolation{ "cycle-publish-git" };
		const auto&                          profileRoot = profileScratch.path();

		// A checkout at its base commit, with starting work: a dirty tracked
		// file and an untracked scratch file nothing in the cycle touches.
		const auto project = aScratch / "publish-project";
		std::filesystem::create_directories(project);
		aGit(project, { "init", "-q", "-b", "main" });
		write_file(project / "README.md", "# Parser\n");
		write_file(project / "src" / "parse.cpp", "int parse(const char* aText) { return check(aText); }\n");
		write_file(project / "src" / "old.cpp", "int old() { return 0; }\n");
		aGit(project, { "add", "." });
		aGit(project, { "commit", "-q", "-m", "base" });
		aGit(project, { "update-ref", "refs/remotes/origin/main", "HEAD" });
		write_file(project / "src" / "parse.cpp", "int parse(const char* aText) { return aText ? check(aText) : 0; }\n");
		write_file(project / "scratch.txt", "my own scratch, unrelated\n");

		auto remote   = std::make_shared<simulated_github>();
		remote->Login = "octo-reviewer";
		auto subject  = core::local_cycle_subject{ };
		auto history  = std::vector<core::cycle_source_revision_id>{ };
		{
			auto work = desk{ profileRoot, aExecutable, isolation.path(), remote };
			work.Host->open_project(project.string());
			work.Lanes.drain();
			subject = *work.Host->selected();

			// R0, a private note, the first round copied, and a reply to it.
			work.Host->open_file("src/parse.cpp", false);
			const auto bytes = work.view().Source->Bytes;
			const auto at    = text(bytes).find("check(aText)");
			work.Host->comment({ .Primary = *core::make_anchor(work.view().Source->Revision, "src/parse.cpp", bytes, at, at + 12U), .Body = std::string{ private_note } });
			work.Lanes.drain();
			work.Host->compose();
			work.Lanes.drain();
			work.Host->share(core::cycle_share_route::copy_document);
			work.Lanes.drain();
			work.Host->close_compose();
			work.Host->attach_reply(core::cycle_round_id{ 1 }, { .RoundId = "F1", .From = "colleague", .Body = std::string{ private_reply } });
			expect(work.view().Facts.Rounds.size() == 1U && work.view().Responses.size() == 1U && work.view().Message.empty(), "R0, a note, F1 and a reply");

			// R1: the cycle's improvements, an addition and a deletion; then the
			// reviewer's decision; R2: one more change.
			write_file(project / "src" / "parse.cpp", "#include \"validate.hpp\"\nint parse(const char* aText) { return aText && validate(aText) ? 0 : 1; }\n");
			write_file(project / "src" / "validate.cpp", "bool validate(const char*) { return true; }\n");
			std::filesystem::remove(project / "src" / "old.cpp");
			work.Host->capture_revision();
			work.Lanes.drain();
			work.Host->decide(work.view().Notes[0].Item, core::cycle_reviewer_decision::verified);
			work.Lanes.drain();
			write_file(project / "src" / "validate.cpp", "bool validate(const char* aText) { return *aText != '\\0'; }\n");
			work.Host->capture_revision();
			history = work.view().Revisions;
			expect(history.size() == 3U && work.view().Message.empty(), "R0, R1 and R2: Capture revision after a round was frozen");
			expect(work.view().Notes[0].Reconciliation && work.view().Notes[0].Reconciliation->Decision && work.view().Notes[0].Reconciliation->Decision->Decision == core::cycle_reviewer_decision::verified, "the reviewer's decision");
			const auto before = tests::workspace_contract::snapshot(project);

			// RC61-08: the preview keeps the baselines apart and says what goes out.
			work.Host->open_publication();
			const auto& opened = work.sheet();
			expect(opened.Account && opened.Account->Login == "octo-reviewer" && opened.Account->Host == "github.com", "the account it would act as");
			expect(opened.Form.NewBranch == "bpr/cycle-" + std::to_string(subject.Cycle.value()) + "-r3" && opened.Form.Title == "publish-project", "a new branch and title proposed, not chosen");
			expect(opened.Stage == review_cycle_publication_stage::choosing && !opened.Current && remote->writes() == 0, "nothing recorded or sent by opening it");
			expect(opened.Outgoing && opened.OutgoingProblem.empty(), "what would go out is planned");
			work.edit([](auto& aForm) {
				aForm.Owner      = "octo-org";
				aForm.Repository = "review-repository";
				aForm.Target     = "refs/remotes/origin/main";
			});
			const auto& plan = *work.sheet().Outgoing;
			expect(plan.StartRevision == history.front() && plan.FinalRevision == history.back(), "R0 and Rn are the first and latest captures");
			expect(plan.TargetHead && *plan.TargetHead == plan.BaseCommit && !plan.TargetMoved && plan.TargetReference == "refs/remotes/origin/main", "P and T named apart");
			const auto* parse   = row(plan, "src/parse.cpp");
			const auto* added   = row(plan, "src/validate.cpp");
			const auto* removed = row(plan, "src/old.cpp");
			const auto* scratch = row(plan, "scratch.txt");
			expect(parse && parse->StartingWork == core::outgoing_change::modified && parse->Improvement == core::outgoing_change::modified && parse->Selected, "starting work and an improvement, kept apart");
			expect(added && added->StartingWork == core::outgoing_change::unchanged && added->Improvement == core::outgoing_change::added && added->Selected, "an addition the cycle made");
			expect(removed && removed->Outgoing == core::outgoing_change::deleted && removed->Selected, "a deletion is explicit");
			expect(scratch && scratch->Unrelated && !scratch->Selected, "unrelated scratch stays out");
			expect(!row(plan, "README.md"), "untouched paths keep P's entries, unlisted");

			// Improvements only: the parser's improvement depends on starting work.
			work.edit([](auto& aForm) { aForm.Mode = core::outgoing_selection_mode::improvements_only; });
			const auto* depends = row(*work.sheet().Outgoing, "src/parse.cpp");
			expect(depends && depends->Gap == core::outgoing_gap::depends_on_starting_work && work.sheet().Outgoing->Undecided == 1U, "a dependency is a decision, never a clean cherry-pick");
			work.edit([](auto& aForm) { aForm.Mode = core::outgoing_selection_mode::starting_work_and_improvements; });

			// The description reuses the manual Problem and Solution path.
			work.edit([](auto& aForm) {
				aForm.Title    = "Validate parser input";
				aForm.Problem  = "Parsing accepted a null pointer.";
				aForm.Solution = "Validate the text before checking it.";
				aForm.Draft    = true;
			});
			expect(remote->writes() == 0, "no edit sends anything");

			// Signed out: planning works, a plan cannot be made, nothing is sent.
			const auto signedIn = work.Account;
			work.Account.reset();
			work.Host->prepare_publication();
			expect(!work.sheet().Current && contains(work.sheet().Status, "Sign in"), "a plan needs an account");
			work.Account = signedIn;

			// Review Plan: the candidate in the private store and a finite plan.
			work.Host->prepare_publication();
			const auto& planned = work.sheet();
			expect(planned.Stage == review_cycle_publication_stage::planned && planned.Current && planned.Digest.size() == 64U, "a plan recorded, waiting for confirmation");
			expect(contains(planned.Preview, "Private cycle history, notes and rounds are not uploaded") && contains(planned.Preview, "From cycle revisions R1 to R3")
					   && contains(planned.Preview, "New branch bpr/cycle-") && contains(planned.Preview, "as a draft") && contains(planned.Preview, planned.Digest),
				   "the preview says what a confirmation authorizes and what stays private");
			const auto body = *core::publication_body("Parsing accepted a null pointer.", "Validate the text before checking it.");
			expect(planned.Current->Plan.PullBody == body && planned.Current->Plan.PullTitle == "Validate parser input", "the description is the Problem and Solution brief");
			expect(planned.Current->Plan.Content.Author.Name == "octo-reviewer" && planned.Current->Plan.Content.Author.OffsetMinutes == -420 && remote->writes() == 0, "attribution as confirmed; nothing sent");
			expect(tests::workspace_contract::snapshot(project) == before, "planning and building left the checkout byte-for-byte as it was");

			// Any edit after the plan: another plan, and the old digest authorizes nothing.
			const auto first = planned.Current->Id;
			work.edit([](auto& aForm) { aForm.Title = "Validate parser input before checking"; });
			expect(work.sheet().Stage == review_cycle_publication_stage::choosing && !work.sheet().Current && work.sheet().Digest.empty(), "edited: the plan is gone");
			work.Host->publish_publication();
			expect(remote->writes() == 0 && work.sheet().Stage == review_cycle_publication_stage::choosing, "publishing without a current plan sends nothing");
			work.Host->prepare_publication();
			expect(work.sheet().Current && work.sheet().Current->Id != first, "a new plan");
			const auto second = work.sheet().Current->Id;
			work.Host->revise_publication();
			expect(work.sheet().Stage == review_cycle_publication_stage::choosing && !work.sheet().Current, "Change drops the plan");
			work.Host->publish_publication();
			expect(remote->writes() == 0, "a dropped plan publishes nothing");
			work.Host->prepare_publication();
			expect(work.sheet().Current && work.sheet().Current->Id != second, "and another plan is made");

			// Publish: blobs, tree, commit, branch and pull request on the simulated server.
			auto       lister = publication_git{ aExecutable };
			const auto parent = lister.Port->list_commit({ .Checkout = project.string(), .Algorithm = core::git_hash_algorithm::sha1 }, work.sheet().Current->Plan.Parent, { });
			expect(parent.has_value(), "P listed");
			remote->seed(work.sheet().Current->Plan.Parent, parent->Tree, parent->Entries, "main");
			const auto commit = work.sheet().Current->Plan.Commit;
			work.Host->publish_publication();
			const auto& linked = work.sheet();
			expect(linked.Stage == review_cycle_publication_stage::linked && contains(linked.Status, "Pull request #17 is open"), "published and linked");
			expect(work.view().Links.size() == 1U && work.view().Links[0].Origin == core::cycle_pull_origin::published && work.view().Links[0].Address.Number == 17U
					   && work.view().Links[0].Head == commit && work.view().Links[0].Url == "https://github.com/octo-org/review-repository/pull/17",
				   "the cycle is associated with the actual pull request");

			// RC59-09: only the plan's objects, branch and pull request left; nothing private.
			for (const auto& [operation, count] : remote->Writes) {
				expect(operation == "blob" || operation == "tree" || operation == "commit" || operation == "branch" || operation == "pull", "only the finite catalog was written");
			}
			expect(remote->Drafts.size() == 1U && remote->Drafts[0].Body == body && remote->Drafts[0].Draft, "the pull request carries exactly the confirmed text and draft choice");
			const auto sent = uploaded(*remote);
			expect(!contains(sent, "PRIVATE-NOTE") && !contains(sent, "PRIVATE-REPLY") && !contains(sent, "scratch, unrelated"), "no note, reply or unselected file reached GitHub");
			expect(!contains(linked.Current->Plan.Content.Message, "PRIVATE"), "nor the commit message");
			const auto mapped = core::map_tour_markdown(remote->Drafts[0].Body);
			expect(std::ranges::count_if(mapped.Sections, [](const core::tour_markdown_section& aSection) { return aSection.Name == "Problem" || aSection.Name == "Solution"; }) == 2,
				   "a Review Tour prepared on the pull request finds Problem and Solution");

			// External publication: a pull request made with one's own tools.
			auto theirs    = remote->Pulls.front();
			theirs.Number  = 21;
			theirs.NodeId  = "PR_21";
			theirs.Url     = "https://github.com/octo-org/review-repository/pull/21";
			theirs.HeadRef = "harden-by-hand";
			remote->Pulls.push_back(theirs);
			const auto reads = remote->writes();
			work.Host->link_pull_request("https://github.com/octo-org/review-repository/pull/404");
			expect(contains(work.sheet().Status, "No such pull request") && work.view().Links.size() == 1U, "a pull request that is not there is not linked");
			work.Host->link_pull_request("https://ghe.example.com/octo-org/review-repository/pull/21");
			expect(contains(work.sheet().Status, "not on github.com") && work.view().Links.size() == 1U, "another host is not read");
			work.Host->link_pull_request("not a pull request");
			expect(contains(work.sheet().Status, "not a pull request address"), "an address is required");
			remote->Pulls.back().Url = "https://github.com/octo-org/elsewhere/pull/21";
			work.Host->link_pull_request("https://github.com/octo-org/review-repository/pull/21");
			expect(contains(work.sheet().Status, "something other than that pull request") && work.view().Links.size() == 1U, "an answer about another pull request is refused");
			remote->Pulls.back().Url = theirs.Url;
			work.Account.reset();
			work.Host->link_pull_request("https://github.com/octo-org/review-repository/pull/21");
			expect(contains(work.sheet().Status, "Sign in") && work.view().Links.size() == 1U, "signed out, nothing is read or linked");
			work.Account = signedIn;
			// The Home bar's shorthand names a pull request on the account's host.
			work.Host->link_pull_request("octo-org/review-repository#21");
			expect(work.view().Links.size() == 2U && work.view().Links[1].Origin == core::cycle_pull_origin::external && !work.view().Links[1].Publication
					   && work.view().Links[1].HeadRef == "harden-by-hand" && contains(work.sheet().Status, "externally published"),
				   "linked after a read of exactly it, as externally published");
			work.Host->link_pull_request("https://github.com/octo-org/review-repository/pull/17");
			expect(contains(work.sheet().Status, "already linked") && work.view().Links.size() == 2U, "the published pull request is not linked twice");
			expect(remote->writes() == reads, "linking wrote nothing to GitHub");

			// Publishing again sends nothing: the plan is linked.
			work.Host->publish_publication();
			expect(remote->Writes.at("pull") == 1 && remote->Drafts.size() == 1U, "no second pull request");
			work.Host->close_publication();
			expect(!work.view().Publication, "closed");
			expect(work.view().Revisions == history && work.view().Facts.Rounds.size() == 1U && work.view().Responses.size() == 1U, "the cycle's history is unchanged on the page");
		}

		// RC59-08: after a reclaim and a restart, every revision's bytes, the
		// round, the reply, the decision and the note are still retained.
		{
			auto       store = adapters::sqlite::make_review_cycle_store(profileRoot, profile);
			const auto kept  = store->reclaim({ });
			expect(kept.has_value(), "a reclaim of unreferenced objects");
			const auto revisions = store->revisions(subject, std::nullopt, { });
			expect(revisions && revisions->Revisions.size() == 3U, "R1, R2 and R3 remain");
			for (const auto& revision : revisions->Revisions) {
				const auto manifest = store->read_object(revision.Manifest, { });
				expect(manifest.has_value(), "the revision's manifest is retained");
				const auto decoded = core::decode_manifest(*manifest);
				expect(decoded.has_value(), "and decodes");
				for (const auto& entry : decoded->Entries) {
					if (entry.Content) {
						expect(store->read_object({ .Digest = *entry.Content, .Size = entry.Size }, { }).has_value(), "every captured byte is retained");
					}
				}
			}
			expect(store->rounds(subject, std::nullopt, { })->Rounds.size() == 1U, "the frozen round remains");
			expect(store->responses(subject, std::nullopt, { })->Responses.size() == 1U, "the reply remains");
			expect(store->verifications(subject, std::nullopt, { })->Verifications.size() == 1U, "the decision remains");
			expect(store->feedback(subject, std::nullopt, { })->Threads.size() == 1U, "the note remains");
			expect(store->deliveries(subject, std::nullopt, { })->Deliveries.size() == 1U, "the copy observation remains");
		}
		{
			auto again = desk{ profileRoot, aExecutable, isolation.path(), remote };
			again.Host->resume(subject);
			const auto& resumed = again.view();
			expect(resumed.Revisions == history && resumed.Facts.Rounds.size() == 1U && resumed.Responses.size() == 1U && resumed.Notes.size() == 1U && resumed.Notes[0].Body == private_note,
				   "after a restart the cycle reads as it did");
			expect(resumed.Links.size() == 2U && resumed.Links[0].Origin == core::cycle_pull_origin::published && resumed.Links[1].Origin == core::cycle_pull_origin::external, "and keeps both associations");
			again.Host->open_publication();
			expect(again.sheet().Stage == review_cycle_publication_stage::linked && again.sheet().Current, "the linked publication is shown again");
		}
		{
			// Only explicit removal removes history.
			auto store   = adapters::sqlite::make_review_cycle_store(profileRoot, profile);
			auto preview = store->removal_preview(subject, { });
			expect(preview && store->remove(application::cycle_operation_id{ core::sha256(std::vector<std::uint8_t>{ 1, 7, 7 }) }, *preview, { }).has_value(), "explicit removal");
			expect(!store->load(subject, { })->has_value(), "removed only then");
		}
	}

	/// A step the host refuses or cannot take leaves GitHub untouched and says why.
	void refuses_without_sending(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-publish-refusals" };
		tests::sqlite_contract::scratch_root isolation{ "cycle-publish-refusals-git" };
		const auto                           project = aScratch / "refusal-project";
		std::filesystem::create_directories(project);
		aGit(project, { "init", "-q", "-b", "main" });
		write_file(project / "a.txt", "one\n");
		aGit(project, { "add", "." });
		aGit(project, { "commit", "-q", "-m", "base" });

		auto remote   = std::make_shared<simulated_github>();
		remote->Login = "octo-reviewer";
		auto work     = desk{ profileScratch.path(), aExecutable, isolation.path(), remote };
		work.Host->open_project(project.string());
		work.Lanes.drain();
		work.Host->open_publication();
		expect(work.sheet().Outgoing && work.sheet().Outgoing->NoChange, "an unchanged checkout plans no change");
		work.edit([](auto& aForm) {
			aForm.Owner      = "octo-org";
			aForm.Repository = "review-repository";
		});
		work.Host->prepare_publication();
		expect(!work.sheet().Current && contains(work.sheet().Status, "nothing to publish"), "a blank candidate is a clear no-change result, not an empty pull request");
		work.edit([](auto& aForm) { aForm.Base = "refs/heads/nowhere"; });
		expect(contains(work.sheet().OutgoingProblem, "does not name a commit"), "a base that is not there");

		write_file(project / "a.txt", "two\n");
		work.Host->close_publication();
		work.Host->capture_revision();
		work.Lanes.drain();
		work.Host->open_publication();
		work.edit([](auto& aForm) {
			aForm.Owner      = "octo-org";
			aForm.Repository = "review-repository";
			aForm.NewBranch  = "main";
		});
		work.Host->prepare_publication();
		expect(!work.sheet().Current && contains(work.sheet().Status, "differs from it"), "the new branch must differ from the base");
		work.edit([](auto& aForm) { aForm.NewBranch = "bpr/try"; });
		work.Host->prepare_publication();
		expect(work.sheet().Current.has_value(), "planned");
		auto       lister = publication_git{ aExecutable };
		const auto parent = lister.Port->list_commit({ .Checkout = project.string(), .Algorithm = core::git_hash_algorithm::sha1 }, work.sheet().Current->Plan.Parent, { });
		remote->seed(work.sheet().Current->Plan.Parent, parent->Tree, parent->Entries, "main");
		remote->Branches["bpr/try"] = work.sheet().Current->Plan.Parent;
		work.Host->publish_publication();
		expect(remote->writes() == 0 && contains(work.sheet().Status, "already exists on GitHub") && work.sheet().Stage == review_cycle_publication_stage::planned,
			   "an existing branch stops it before any write, and says so");
		auto other    = *work.Account;
		other.Account = core::github_account_id{ 9U };
		work.Account  = other;
		work.Host->publish_publication();
		expect(remote->writes() == 0 && contains(work.sheet().Status, "not the one this plan names"), "another account signed in now is refused");
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	tests::sqlite_contract::scratch_root scratch{ "cycle-publish-desk" };
	publishes_links_and_keeps_history(scratch.path(), *fixtures, *executable);
	refuses_without_sending(scratch.path(), *fixtures, *executable);
	std::cout << "review-cycle-publish-desk-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle publish desk failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return tests::failing_exit();
}
