#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_publication.hpp"
#include "bpr/application/review_cycle_publisher.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "publication_course.hpp"
#include "simulated_github.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1178]]. The publication crash, drift and recovery matrix
/// (RC59-07, RFC-BPR-0059), run as real processes by
/// `tests/scripts/review_cycle_publication_matrix_tests.py`.
/// \details Each phase is one process over one course root: the desktop's
/// cycle host (`review_cycle_workbench`) on the real store, a real Git
/// checkout and the product's installed-Git catalog, publishing to a
/// simulated GitHub whose whole state is a file in the course root, so it
/// outlives the process. A phase given `--kill POINT` stops at that boundary
/// of the publication catalog — before a request leaves, or after GitHub took
/// the effect and before its answer reached the journal — writes `kill-ready`
/// and waits on standard input to be killed; the driver kills it there
/// (SIGKILL, TerminateProcess). The next process is a restart: it finds what
/// another actor changed on GitHub while nothing ran, reads before it writes,
/// and sends a remaining step only after the reviewer confirms the plan again.
/// No live GitHub is contacted.

namespace {
	using namespace bpr;
	using application::review_cycle_publication_stage;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::simulated_github;
	using tests::workspace_contract::write_file;

	constexpr auto profile    = core::bpr_profile_id{ 6 };
	constexpr auto reviewer   = core::github_account_id{ 1001U };
	constexpr auto login      = std::string_view{ "octo-reviewer" };
	constexpr auto new_branch = std::string_view{ "bpr/matrix" };
	constexpr auto base       = std::string_view{ "main" };

	/// What another actor does on GitHub while no BPR process runs.
	enum class drift : std::uint8_t {
		none,
		/// Pushes a commit to the base branch.
		base_moved,
		/// Creates the plan's new branch first, at P.
		branch_taken,
		/// Force-moves the branch the plan created back to P.
		branch_moved,
		/// Opens a pull request for the plan's head and base first.
		pull_raced,
		/// Closes the plan's pull request and opens another for the same head
		/// and base, at the same commit, under the reviewer's own login.
		pull_ambiguous,
		/// Pushes a commit to the head of the plan's pull request.
		head_pushed,
	};

	/// Where the course ends once recovery has read GitHub.
	enum class ending : std::uint8_t {
		/// Confirmed again and published: linked, every step written once.
		linked,
		/// Linked after recovery alone; nothing more was sent.
		recovered_linked,
		/// Linked, with the head another actor pushed recorded as drift.
		linked_drifted,
		/// The base moved: refused before any further write, then cancelled.
		target_moved,
		/// Something else holds the branch or the pull request.
		target_drift,
		/// Two candidates: uncertain until the reviewer links one by hand.
		ambiguous,
		/// The reviewer cancels after recovery; confirmed steps stay.
		cancelled,
	};

	struct course_case
	{
		std::string_view Name;
		/// The boundary the publishing process dies at; empty: it does not.
		std::string_view Kill;
		drift            While = drift::none;
		ending           End   = ending::linked;
		/// A fault the publishing process meets without dying.
		std::string_view Fault;
	};

	constexpr auto cases = std::array{
		course_case{ .Name = "authorized", .Kill = "before-read-repository" },
		course_case{ .Name = "before-blob", .Kill = "before-blob" },
		course_case{ .Name = "after-blob", .Kill = "after-blob" },
		course_case{ .Name = "before-tree", .Kill = "before-tree" },
		course_case{ .Name = "after-tree", .Kill = "after-tree" },
		course_case{ .Name = "before-commit", .Kill = "before-commit" },
		course_case{ .Name = "after-commit", .Kill = "after-commit" },
		course_case{ .Name = "before-branch", .Kill = "before-branch" },
		course_case{ .Name = "after-branch", .Kill = "after-branch" },
		course_case{ .Name = "before-pull", .Kill = "before-pull" },
		course_case{ .Name = "after-pull", .Kill = "after-pull", .End = ending::recovered_linked },
		course_case{ .Name = "after-pull-reread", .Kill = "before-read-pull", .End = ending::recovered_linked },
		course_case{ .Name = "recovery-killed", .Kill = "after-blob" },
		course_case{ .Name = "lost-pull-answer", .End = ending::recovered_linked, .Fault = "pull" },
		course_case{ .Name = "not-sent-branch", .End = ending::recovered_linked, .Fault = "branch" },
		course_case{ .Name = "base-moved", .Kill = "after-commit", .While = drift::base_moved, .End = ending::target_moved },
		course_case{ .Name = "branch-taken", .Kill = "before-branch", .While = drift::branch_taken, .End = ending::target_drift },
		course_case{ .Name = "branch-moved", .Kill = "after-branch", .While = drift::branch_moved, .End = ending::target_drift },
		course_case{ .Name = "pull-raced", .Kill = "before-pull", .While = drift::pull_raced, .End = ending::target_drift },
		course_case{ .Name = "pull-ambiguous", .Kill = "after-pull", .While = drift::pull_ambiguous, .End = ending::ambiguous },
		course_case{ .Name = "head-pushed", .Kill = "after-pull", .While = drift::head_pushed, .End = ending::linked_drifted },
		course_case{ .Name = "cancel-after-tree", .Kill = "after-tree", .End = ending::cancelled },
	};

	/// The order the plan's steps are written in: two blobs, the tree, the
	/// commit, the new branch, the pull request.
	constexpr auto steps = std::array<std::pair<std::string_view, int>, 5>{ { { "blob", 2 }, { "tree", 1 }, { "commit", 1 }, { "branch", 1 }, { "pull", 1 } } };

	/// The writes GitHub received when a process died at `aKill`.
	auto writes_at(const std::string_view aKill) -> std::map<std::string, int>
	{
		auto writes = std::map<std::string, int>{ };
		if (aKill.empty() || aKill == "before-read-repository") {
			return writes;
		}
		if (aKill == "before-read-pull") {
			for (const auto& [operation, count] : steps) {
				writes[std::string{ operation }] = count;
			}
			return writes;
		}
		const auto after     = aKill.starts_with("after-");
		const auto operation = aKill.substr(aKill.find('-') + 1U);
		for (const auto& [step, count] : steps) {
			if (step == operation) {
				if (after) {
					// The first of the step's writes took effect.
					writes[std::string{ step }] = 1;
				}
				break;
			}
			writes[std::string{ step }] = count;
		}
		return writes;
	}

	auto full_writes() -> std::map<std::string, int>
	{
		auto writes = std::map<std::string, int>{ };
		for (const auto& [operation, count] : steps) {
			writes[std::string{ operation }] = count;
		}
		return writes;
	}

	auto describe(const std::map<std::string, int>& aWrites) -> std::string
	{
		auto out = std::string{ };
		for (const auto& [operation, count] : aWrites) {
			out += operation + "=" + std::to_string(count) + " ";
		}
		return out;
	}

	auto contains(const std::string_view aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string_view::npos;
	}

	/// One course root: the checkout, the profile, the catalog's isolation
	/// root and the simulated server's state.
	struct course
	{
		std::filesystem::path Root;
		std::string           Phase;
		std::string           Kill;

		[[nodiscard]] auto project() const -> std::filesystem::path
		{
			return Root / "project";
		}
		[[nodiscard]] auto profile_root() const -> std::filesystem::path
		{
			return Root / "profile";
		}
		[[nodiscard]] auto isolation() const -> std::filesystem::path
		{
			return Root / "git-isolation";
		}
		[[nodiscard]] auto server() const -> std::filesystem::path
		{
			return Root / "server-state";
		}
	};

	/// The publication catalog in front of the simulated server: it saves
	/// the server after every call and dies at the course's kill boundary,
	/// once, before or after the call it names.
	class checkpoint_port final : public application::github_publication_port
	{
	public:
		checkpoint_port(simulated_github& aServer, const course& aCourse)
			: Server{ aServer }
			, Course{ aCourse }
		{
		}

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token aStop)
			-> application::github_publication_result<application::github_publication_capability> override
		{
			return around("read-repository", [&] { return Server.observe_repository(aTarget, aStop); });
		}
		auto observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			return around("read-branch", [&] { return Server.observe_branch(aTarget, aBranch, aStop); });
		}
		auto observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return around("read-commit", [&] { return Server.observe_commit(aTarget, aCommit, aStop); });
		}
		auto observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree, std::stop_token aStop) -> application::github_publication_result<bool> override
		{
			return around("read-tree", [&] { return Server.observe_tree(aTarget, aTree, aStop); });
		}
		auto observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob, std::stop_token aStop) -> application::github_publication_result<bool> override
		{
			return around("read-blob", [&] { return Server.observe_blob(aTarget, aBlob, aStop); });
		}
		auto create_blob(const application::github_publication_target& aTarget, std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override
		{
			return around("blob", [&] { return Server.create_blob(aTarget, aBytes, aExpected, aStop); });
		}
		auto create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
						 const std::vector<std::string>& aDeletions, const core::git_tree_oid& aExpected, std::stop_token aStop) -> application::github_publication_result<void> override
		{
			return around("tree", [&] { return Server.create_tree(aTarget, aBase, aChanges, aDeletions, aExpected, aStop); });
		}
		auto create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override
		{
			return around("commit", [&] { return Server.create_commit(aTarget, aCommit, aExpected, aStop); });
		}
		auto create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			return around("branch", [&] { return Server.create_branch(aTarget, aBranch, aCommit, aStop); });
		}
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token aStop)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			return around("pull", [&] { return Server.create_pull_request(aTarget, aDraft, aStop); });
		}
		auto find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			return around("read-pulls", [&] { return Server.find_pull_requests(aTarget, aHead, aBase, aStop); });
		}
		auto observe_pull_request(const application::github_publication_target& aTarget, const std::uint64_t aNumber, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			return around("read-pull", [&] { return Server.observe_pull_request(aTarget, aNumber, aStop); });
		}

		/// Every catalog call this process made, in order.
		std::vector<std::string> Calls;

	private:
		auto around(const std::string& aOperation, const auto& aCall) -> decltype(aCall())
		{
			boundary("before-" + aOperation);
			auto result = aCall();
			Calls.push_back(aOperation);
			boundary("after-" + aOperation);
			return result;
		}

		void boundary(const std::string& aPoint)
		{
			Server.save(Course.server());
			if (Fired || aPoint != Course.Kill) {
				return;
			}
			Fired = true;
			// The server is on disk; the journal holds whatever it held. The
			// driver kills this process now, while it waits.
			std::ofstream{ Course.Root / "kill-ready", std::ios::binary | std::ios::trunc } << Course.Phase;
			static_cast<void>(std::cin.get());
			expect(false, "the process was killed at " + aPoint);
		}

		simulated_github& Server;
		const course&     Course;
		bool              Fired = false;
	};

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

	/// The desktop's cycle host over the course's profile, with the
	/// publication environment a desktop composes.
	struct desk
	{
		application::manual_executor                         Lanes;
		std::unique_ptr<application::workspace_source_port>  Source = adapters::workspace::make_local_workspace_source();
		recording_port                                       Port;
		std::unique_ptr<application::review_cycle_workbench> Host;

		desk(const course& aCourse, const std::filesystem::path& aGit, std::shared_ptr<application::github_publication_port> aRemote)
		{
			// Each process names its operations from its own random seed, as a
			// desktop does; a fixed seed would reuse the killed process's names.
			auto seed   = std::array<std::uint8_t, 32>{ };
			auto random = std::random_device{ };
			for (auto& byte : seed) {
				byte = static_cast<std::uint8_t>(random());
			}
			Host         = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aCourse.profile_root(), profile), *Source, Lanes, Lanes, Port, seed);
			auto account = application::review_cycle_publication_account{ .Provider = std::move(aRemote), .Account = reviewer, .Login = std::string{ login }, .Host = "github.com" };
			Host->set_publication_environment({
				.Candidates =
					[aGit, isolation = aCourse.isolation()](std::stop_token aStop) -> application::publication_git_result<std::unique_ptr<application::publication_candidate_port>> {
					auto made = adapters::git::make_installed_git_publication(
						{ .GitExecutable = std::filesystem::canonical(aGit), .IsolationRoot = std::filesystem::canonical(isolation), .WindowsSystemRoot = tests::git_contract::windows_system_root() },
						std::move(aStop));
					if (!made) {
						return std::unexpected(application::publication_git_error{ .Failure = application::publication_git_failure::unavailable });
					}
					return std::move(*made);
				},
				.Account = [account] { return std::optional{ account }; },
				.Now     = [] { return core::git_signature{ .Seconds = 1'760'000'000, .OffsetMinutes = 330 }; },
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
		auto journal() -> const core::publication_journal&
		{
			expect(sheet().Current.has_value(), "the recorded publication is shown");
			return sheet().Current->Journal;
		}
	};

	/// The subject the setup created, kept for the restarts' direct reads.
	void save_subject(const course& aCourse, const core::local_cycle_subject& aSubject)
	{
		auto out = std::ofstream{ aCourse.Root / "subject", std::ios::binary | std::ios::trunc };
		out << aSubject.Authority.Profile.value() << ' ' << aSubject.Cycle.value() << ' ' << aSubject.Generation;
		for (const auto byte : aSubject.Authority.Incarnation.bytes()) {
			out << ' ' << static_cast<unsigned>(byte);
		}
		out << '\n';
	}

	auto load_subject(const course& aCourse) -> core::local_cycle_subject
	{
		auto in      = std::ifstream{ aCourse.Root / "subject", std::ios::binary };
		auto owner   = std::uint64_t{ };
		auto cycle   = std::uint64_t{ };
		auto subject = core::local_cycle_subject{ };
		in >> owner >> cycle >> subject.Generation;
		auto bytes = std::array<std::uint8_t, 32>{ };
		for (auto& byte : bytes) {
			auto value = 0U;
			in >> value;
			byte = static_cast<std::uint8_t>(value);
		}
		expect(!in.fail(), "the course's subject");
		subject.Authority = { .Profile = core::bpr_profile_id{ owner }, .Incarnation = core::cycle_catalog_incarnation{ bytes } };
		subject.Cycle     = core::review_cycle_id{ cycle };
		return subject;
	}

	/// GitHub's invariants after any phase: nothing was written more often
	/// than the plan has steps, no reference the plan does not own was moved
	/// by BPR, and there is never more than one open pull request of ours
	/// for the head.
	void expect_no_duplicates(const simulated_github& aServer, const std::string_view aCase)
	{
		for (const auto& [operation, count] : aServer.Writes) {
			const auto planned = std::ranges::find(steps, operation, &std::pair<std::string_view, int>::first);
			expect(planned != steps.end() && count <= planned->second, std::string{ aCase } + ": no step written twice (" + operation + "=" + std::to_string(count) + ")");
		}
		const auto ours = std::ranges::count_if(aServer.Pulls, [](const auto& aPull) { return aPull.HeadRef == new_branch && aPull.Author == login && aPull.State == "open"; });
		expect(ours <= 1, std::string{ aCase } + ": never two open pull requests of ours for the head");
		expect(aServer.Drafts.size() <= 1U, std::string{ aCase } + ": one pull request creation at most");
	}

	auto pull_for(simulated_github& aServer, const core::git_commit_oid& aHead, std::string aAuthor) -> application::github_pull_request_observation
	{
		const auto number = aServer.NextPull++;
		return { .Number         = number,
				 .NodeId         = "PR_" + std::to_string(number),
				 .Url            = "https://github.com/" + aServer.Owner + "/" + aServer.Repository + "/pull/" + std::to_string(number),
				 .State          = "open",
				 .Title          = "Opened elsewhere",
				 .HeadRef        = std::string{ new_branch },
				 .HeadOwner      = aServer.Owner,
				 .HeadRepository = aServer.Repository,
				 .HeadCommit     = aHead,
				 .BaseRef        = std::string{ base },
				 .BaseCommit     = aServer.Branches.at(std::string{ base }),
				 .Author         = std::move(aAuthor) };
	}

	/// A commit another actor pushes: P's tree again, on top of `aParent`.
	auto another_commit(simulated_github& aServer, const core::git_commit_oid& aParent) -> core::git_commit_oid
	{
		const auto parent  = aServer.Commits.at(simulated_github::hex(aParent));
		const auto someone = core::git_signature{ .Name = "Someone Else", .Email = "someone@example.invalid", .Seconds = 1'760'000'500, .OffsetMinutes = 0 };
		const auto content = core::git_commit_content{ .Tree = parent.Tree, .Parents = { aParent }, .Author = someone, .Committer = someone, .Message = "Pushed by another actor\n" };
		const auto id      = core::git_commit_identity(content);
		expect(id.has_value(), "another actor's commit");
		aServer.Commits[simulated_github::hex(*id)] = { .Tree = parent.Tree, .Parents = { aParent } };
		return *id;
	}

	/// What another actor did while nothing ran. These are GitHub's own
	/// changes, not BPR's writes, so they count as no write.
	void apply_drift(simulated_github& aServer, const drift aDrift)
	{
		const auto branch = std::string{ new_branch };
		switch (aDrift) {
		case drift::none: return;
		case drift::base_moved:
			aServer.Branches[std::string{ base }] = another_commit(aServer, aServer.Branches.at(std::string{ base }));
			return;
		case drift::branch_taken:
			expect(!aServer.Branches.contains(branch), "the plan's branch was not created before the kill");
			aServer.Branches[branch] = aServer.Branches.at(std::string{ base });
			return;
		case drift::branch_moved:
			expect(aServer.Branches.contains(branch), "the plan's branch was created before the kill");
			aServer.Branches[branch] = aServer.Branches.at(std::string{ base });
			return;
		case drift::pull_raced:     aServer.Pulls.push_back(pull_for(aServer, aServer.Branches.at(branch), "someone-else")); return;
		case drift::pull_ambiguous: {
			expect(aServer.Pulls.size() == 1U, "the plan's pull request was created before the kill");
			const auto head             = aServer.Pulls.front().HeadCommit;
			aServer.Pulls.front().State = "closed";
			aServer.Pulls.push_back(pull_for(aServer, head, std::string{ login }));
			return;
		}
		case drift::head_pushed: {
			expect(aServer.Pulls.size() == 1U, "the plan's pull request was created before the kill");
			const auto pushed                = another_commit(aServer, aServer.Branches.at(branch));
			aServer.Branches[branch]         = pushed;
			aServer.Pulls.front().HeadCommit = pushed;
			return;
		}
		}
	}

	auto find_case(const std::string_view aName) -> const course_case&
	{
		const auto found = std::ranges::find(cases, aName, &course_case::Name);
		expect(found != cases.end(), "a known course case: " + std::string{ aName });
		return *found;
	}

	/// The first process: a checkout, a cycle with R1 and R2, a plan, then
	/// Publish Pull Request — which dies at the case's boundary, or meets its
	/// fault and stops.
	void publish_phase(const course& aCourse, const course_case& aCase, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		const auto project = aCourse.project();
		std::filesystem::create_directories(project);
		aGit(project, { "init", "-q", "-b", "main" });
		write_file(project / "README.md", "# Parser\n");
		write_file(project / "src" / "parse.cpp", "int parse(const char* aText) { return check(aText); }\n");
		write_file(project / "src" / "old.cpp", "int old() { return 0; }\n");
		aGit(project, { "add", "." });
		aGit(project, { "commit", "-q", "-m", "base" });
		aGit(project, { "update-ref", "refs/remotes/origin/main", "HEAD" });

		auto server  = simulated_github{ };
		server.Login = std::string{ login };
		auto catalog = std::make_shared<checkpoint_port>(server, aCourse);
		auto work    = desk{ aCourse, aExecutable, catalog };
		work.Host->open_project(project.string());
		work.Lanes.drain();
		save_subject(aCourse, *work.Host->selected());

		write_file(project / "src" / "parse.cpp", "#include \"validate.hpp\"\nint parse(const char* aText) { return aText && validate(aText) ? 0 : 1; }\n");
		write_file(project / "src" / "validate.cpp", "bool validate(const char* aText) { return *aText != '\\0'; }\n");
		std::filesystem::remove(project / "src" / "old.cpp");
		work.Host->capture_revision();
		work.Lanes.drain();
		expect(work.view().Revisions.size() == 2U && work.view().Message.empty(), "R1 and R2: " + std::to_string(work.view().Revisions.size()) + " " + work.view().Message);

		work.Host->open_publication();
		auto form       = work.sheet().Form;
		form.Owner      = server.Owner;
		form.Repository = server.Repository;
		form.Target     = "refs/remotes/origin/main";
		form.NewBranch  = std::string{ new_branch };
		form.Title      = "Validate parser input";
		form.Problem    = "Parsing accepted a null pointer.";
		form.Solution   = "Validate the text before checking it.";
		work.Host->edit_publication(form);
		work.Lanes.drain();
		work.Host->prepare_publication();
		const auto& planned = work.sheet();
		expect(planned.Stage == review_cycle_publication_stage::planned && planned.Current, "a plan recorded, waiting for confirmation");
		expect(planned.Current->Plan.Blobs.size() == 2U && planned.Current->Plan.Deleted.size() == 1U, "two blobs and one deletion");

		auto       lister = tests::workspace_contract::publication_git{ aExecutable };
		const auto parent = lister.Port->list_commit({ .Checkout = project.string(), .Algorithm = core::git_hash_algorithm::sha1 }, planned.Current->Plan.Parent, { });
		expect(parent.has_value(), "P listed");
		server.seed(planned.Current->Plan.Parent, parent->Tree, parent->Entries, std::string{ base });
		if (!aCase.Fault.empty()) {
			server.Faults[std::string{ aCase.Fault }] = aCase.Fault == "pull" ? tests::workspace_contract::simulated_fault::lose_answer : tests::workspace_contract::simulated_fault::not_sent;
		}
		server.save(aCourse.server());

		work.Host->publish_publication();
		work.Lanes.drain();
		// Only a case without a kill point comes back here.
		expect(aCase.Kill.empty(), std::string{ aCase.Name } + ": the process survived its kill point");
		if (aCase.Fault == "pull") {
			// A lost answer without a death: uncertain, and nothing more is sent.
			expect(work.sheet().Stage == review_cycle_publication_stage::stopped && work.journal().Phase == core::publication_phase::uncertain && work.journal().Generation == 0U,
				   "a lost pull request answer is uncertain, with no authorization left");
			expect(contains(work.sheet().Status, "did not answer"), "and says so");
			work.Host->publish_publication();
			expect(server.Writes.at("pull") == 1, "no blind retry of the pull request");
		} else {
			// A branch request that never left: the plan waits for the reviewer
			// again, says what GitHub holds, and sends only the rest.
			const auto& stopped = work.sheet();
			expect(stopped.Stage == review_cycle_publication_stage::planned && work.journal().Phase == core::publication_phase::uploading_objects,
				   "a step that never left leaves the plan waiting, not stuck publishing: " + stopped.Status);
			expect(contains(stopped.Status, "GitHub holds the 4 steps already confirmed") && contains(stopped.Status, "sends only what GitHub does not hold"), "and says what GitHub holds: " + stopped.Status);
			expect(!server.Writes.contains("branch"), "the branch request never reached GitHub");
			// The journal changes under the sheet (here another connection's
			// restart spends the authorization): the confirmation is refused
			// by the store, nothing is sent, and the sheet is not left stuck
			// publishing. Reopened, it reads the journal again.
			{
				auto other = adapters::sqlite::make_review_cycle_store(aCourse.profile_root(), profile);
				expect(application::recover_publications_after_restart(*other, *work.Host->selected(), { }).has_value(), "another connection changes the journal");
			}
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(work.sheet().Stage == review_cycle_publication_stage::planned && !work.sheet().Busy && contains(work.sheet().Status, "changed"),
				   "a refused confirmation leaves the plan waiting, not stuck publishing: " + work.sheet().Status);
			expect(!server.Writes.contains("branch"), "and sends nothing");
			work.Host->close_publication();
			work.Host->open_publication();
			work.Lanes.drain();
			expect(work.sheet().Stage == review_cycle_publication_stage::planned, "reopened, the plan waits for the reviewer");
			work.Host->publish_publication();
			expect(work.sheet().Stage == review_cycle_publication_stage::linked && server.Writes == full_writes(), "confirmed again, the rest is sent once: " + describe(server.Writes));
		}
		expect_no_duplicates(server, aCase.Name);
		server.save(aCourse.server());
	}

	/// The kill left exactly what the boundary promises on GitHub.
	void expect_boundary(const simulated_github& aServer, const course_case& aCase)
	{
		// A case that met a fault instead of dying finished publishing.
		const auto expected = aCase.Fault.empty() ? writes_at(aCase.Kill) : full_writes();
		expect(aServer.Writes == expected, std::string{ aCase.Name } + ": GitHub holds exactly what the kill boundary left (" + describe(aServer.Writes) + "; expected " + describe(expected) + ")");
	}

	/// A restart: what the journal says without reading GitHub, then the
	/// desktop's recovery, a fresh confirmation and the rest.
	void restart_phase(const course& aCourse, const course_case& aCase, const std::filesystem::path& aExecutable, const bool aFirst)
	{
		auto server = simulated_github{ };
		server.load(aCourse.server());
		if (aFirst) {
			expect_boundary(server, aCase);
			apply_drift(server, aCase.While);
			server.save(aCourse.server());
		}
		const auto before  = server.Writes;
		const auto subject = load_subject(aCourse);
		auto       catalog = std::make_shared<checkpoint_port>(server, aCourse);

		// The restart, without any read of GitHub: unobserved attempts are
		// uncertain and no authorization survived. A publish without a fresh
		// confirmation sends nothing.
		{
			auto store     = adapters::sqlite::make_review_cycle_store(aCourse.profile_root(), profile);
			auto recovered = application::recover_publications_after_restart(*store, subject, { });
			expect(recovered.has_value(), "the restart reads the journal");
			const auto open = std::ranges::find_if(*recovered, [](const auto& aStored) { return aStored.Journal.Phase != core::publication_phase::superseded; });
			expect(open != recovered->end(), "the publication is there after the restart");
			expect(open->Journal.Generation == 0U, std::string{ aCase.Name } + ": no authorization survives a restart");
			expect(std::ranges::none_of(open->Journal.Effects, [](const auto& aEffect) { return aEffect.State == core::publication_effect_state::intended; }),
				   "no attempt is left without an outcome or uncertainty");
			// An attempt was in flight when the process died at a write, or
			// its answer was lost; not after authorizing or after linking.
			const auto pending = aCase.Fault == "pull" || (!aCase.Kill.empty() && aCase.Kill != "before-read-repository" && aCase.Kill != "before-read-pull");
			if (aFirst) {
				expect((open->Journal.Phase == core::publication_phase::uncertain) == pending,
					   std::string{ aCase.Name } + ": uncertain exactly when an attempt was in flight (" + std::string{ core::name(open->Journal.Phase) } + ")");
			}

			auto source = adapters::workspace::make_local_workspace_source();
			auto git    = adapters::git::make_installed_git_publication({ .GitExecutable     = std::filesystem::canonical(aExecutable),
																		  .IsolationRoot     = std::filesystem::canonical(aCourse.isolation()),
																		  .WindowsSystemRoot = tests::git_contract::windows_system_root() });
			expect(git.has_value(), "the installed-Git catalog");
			auto       outgoing  = application::review_cycle_outgoing_host{ *source, *store, **git };
			auto       journal   = application::review_cycle_publication_host{ outgoing, *store };
			auto       publisher = application::review_cycle_publisher{ journal, *store, *catalog, application::github_server_contract::github_com_2022_11_28, reviewer };
			const auto attempt   = publisher.publish(subject, open->Id.Id, { });
			expect(attempt.Status == application::cycle_publish_status::unauthorized, std::string{ aCase.Name } + ": nothing is sent without a fresh confirmation");
			expect(catalog->Calls.empty() && server.Writes == before, "not even a read");
		}

		auto work = desk{ aCourse, aExecutable, catalog };
		work.Host->open_project(aCourse.project().string());
		work.Lanes.drain();
		work.Host->open_publication();
		const auto  phase  = work.journal().Phase;
		const auto& opened = work.sheet();
		if (phase == core::publication_phase::uncertain) {
			expect(opened.Stage == review_cycle_publication_stage::stopped && contains(opened.Status, "did not answer"), "the window says GitHub did not answer: " + opened.Status);
			work.Host->recover_publication();
			work.Lanes.drain();
			expect(server.Writes == before, std::string{ aCase.Name } + ": recovery only reads (" + describe(server.Writes) + ")");
		}
		const auto& recovered = work.sheet();
		const auto& read      = work.journal();
		switch (aCase.End) {
		case ending::linked:
			expect(recovered.Stage == review_cycle_publication_stage::planned,
				   std::string{ aCase.Name } + ": after recovery the plan waits for the reviewer, not stuck publishing (" + std::string{ core::name(read.Phase) } + "): " + recovered.Status);
			if (!read.Effects.empty() && std::ranges::any_of(read.Effects, [](const auto& aEffect) { return aEffect.State == core::publication_effect_state::confirmed; })) {
				expect(contains(recovered.Status, "already confirmed; nothing is undone") && contains(recovered.Status, "Publish Pull Request confirms this plan again"),
					   std::string{ aCase.Name } + ": says what GitHub holds and what confirming does: " + recovered.Status);
			}
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(work.sheet().Stage == review_cycle_publication_stage::linked && contains(work.sheet().Status, "is open"), std::string{ aCase.Name } + ": confirmed again and published: " + work.sheet().Status);
			expect(server.Writes == full_writes(), std::string{ aCase.Name } + ": every step written exactly once (" + describe(server.Writes) + ")");
			expect(server.Branches.at(std::string{ new_branch }) == work.journal().Link->Head && !work.journal().Link->Drifted, "the branch at the planned commit, not drifted");
			break;
		case ending::recovered_linked:
			expect(recovered.Stage == review_cycle_publication_stage::linked && read.Link && !read.Link->Drifted,
				   std::string{ aCase.Name } + ": the pull request already created is associated, not created again: " + recovered.Status);
			expect(server.Writes == full_writes() && server.Pulls.size() == 1U, std::string{ aCase.Name } + ": one pull request (" + describe(server.Writes) + ")");
			break;
		case ending::linked_drifted:
			expect(recovered.Stage == review_cycle_publication_stage::linked && read.Link && read.Link->Drifted && read.Link->Head == server.Branches.at(std::string{ new_branch }),
				   std::string{ aCase.Name } + ": our pull request, with the pushed head recorded as drift: " + recovered.Status);
			expect(contains(recovered.Status, "someone pushed"), "and says so");
			expect(server.Writes == before && server.Pulls.size() == 1U, "nothing more was sent");
			break;
		case ending::target_moved:
			expect(recovered.Stage == review_cycle_publication_stage::planned, std::string{ aCase.Name } + ": the plan waits for the reviewer: " + recovered.Status);
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(server.Writes == before, std::string{ aCase.Name } + ": the moved base stops it before any further write");
			expect(contains(work.sheet().Status, "moved since the plan was made") && contains(work.sheet().Status, "Nothing more was sent")
					   && contains(work.sheet().Status, "GitHub holds the 4 steps already confirmed; nothing is undone") && !contains(work.sheet().Status, "Nothing was sent"),
				   std::string{ aCase.Name } + ": the words never claim nothing was sent: " + work.sheet().Status);
			work.Host->cancel_publication();
			work.Lanes.drain();
			expect(work.sheet().Stage == review_cycle_publication_stage::stopped && work.journal().Phase == core::publication_phase::cancelled
					   && contains(work.sheet().Status, "Publishing was cancelled. GitHub holds the 4 steps already confirmed; nothing is undone."),
				   "cancelled, and what GitHub holds stays: " + work.sheet().Status);
			expect(server.Blobs.size() == 2U && server.Commits.size() == 3U && !server.Branches.contains(std::string{ new_branch }), "nothing on GitHub was deleted or rewritten");
			break;
		case ending::target_drift:
			expect(recovered.Stage == review_cycle_publication_stage::stopped && read.Phase == core::publication_phase::target_drift && contains(recovered.Status, "Nothing was overwritten"),
				   std::string{ aCase.Name } + ": something else holds the name; nothing is overwritten: " + recovered.Status);
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(server.Writes == before, std::string{ aCase.Name } + ": no write after drift");
			if (aCase.While == drift::pull_raced) {
				expect(server.Pulls.size() == 1U && server.Pulls.front().Author == "someone-else", "only the other actor's pull request");
			} else {
				expect(server.Branches.at(std::string{ new_branch }) == server.Branches.at(std::string{ base }), "the other actor's branch is where they left it");
			}
			break;
		case ending::ambiguous: {
			expect(recovered.Stage == review_cycle_publication_stage::stopped && read.Phase == core::publication_phase::uncertain,
				   std::string{ aCase.Name } + ": two candidates stay uncertain: " + recovered.Status);
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(server.Writes == before && server.Pulls.size() == 2U, "no third pull request");
			// The reviewer's explicit association: one read of exactly it.
			work.Host->link_pull_request(server.Pulls.back().Url);
			work.Lanes.drain();
			expect(work.view().Links.size() == 1U && work.view().Links[0].Origin == core::cycle_pull_origin::external && work.view().Links[0].Address.Number == server.Pulls.back().Number,
				   "the reviewer links the one they mean, recorded as their own link");
			expect(server.Writes == before, "linking writes nothing");
			break;
		}
		case ending::cancelled:
			expect(recovered.Stage == review_cycle_publication_stage::planned, std::string{ aCase.Name } + ": the plan waits for the reviewer: " + recovered.Status);
			work.Host->cancel_publication();
			work.Lanes.drain();
			expect(work.sheet().Stage == review_cycle_publication_stage::stopped && work.journal().Phase == core::publication_phase::cancelled
					   && contains(work.sheet().Status, "GitHub holds the 3 steps already confirmed; nothing is undone."),
				   "cancelled; what GitHub holds stays: " + work.sheet().Status);
			work.Host->publish_publication();
			work.Lanes.drain();
			expect(server.Writes == before && server.Blobs.size() == 2U && server.Trees.size() == 2U, "nothing sent, deleted or rewritten after cancelling");
			break;
		}
		expect_no_duplicates(server, aCase.Name);
		server.save(aCourse.server());
	}

	/// A last process: the outcome survives another restart, and reading it
	/// sends nothing.
	void verify_phase(const course& aCourse, const course_case& aCase, const std::filesystem::path& aExecutable)
	{
		auto server = simulated_github{ };
		server.load(aCourse.server());
		const auto before = server.Writes;
		// The journal as stored: a cancelled publication is not shown again
		// when the sheet opens, only lends its destination.
		auto stored = core::publication_phase::prepared;
		{
			auto       store = adapters::sqlite::make_review_cycle_store(aCourse.profile_root(), profile);
			const auto page  = store->publications(load_subject(aCourse), std::nullopt, { });
			expect(page.has_value() && page->Publications.size() == 1U, "one publication recorded");
			stored = page->Publications.front().Journal.Phase;
		}
		auto catalog = std::make_shared<checkpoint_port>(server, aCourse);
		auto work    = desk{ aCourse, aExecutable, catalog };
		work.Host->open_project(aCourse.project().string());
		work.Lanes.drain();
		work.Host->open_publication();
		const auto& shown = work.sheet();
		switch (aCase.End) {
		case ending::linked:
		case ending::recovered_linked:
		case ending::linked_drifted:
			expect(shown.Stage == review_cycle_publication_stage::linked && work.view().Links.size() == 1U && work.view().Links[0].Origin == core::cycle_pull_origin::published,
				   std::string{ aCase.Name } + ": still linked to the one pull request after another restart");
			break;
		case ending::target_moved:
		case ending::cancelled:
			expect(stored == core::publication_phase::cancelled && !shown.Current && shown.Stage == review_cycle_publication_stage::choosing,
				   std::string{ aCase.Name } + ": still cancelled; the sheet starts a new choice");
			break;
		case ending::target_drift: expect(stored == core::publication_phase::target_drift && work.journal().Phase == stored, std::string{ aCase.Name } + ": still drifted"); break;
		case ending::ambiguous:
			expect(stored == core::publication_phase::uncertain && work.journal().Phase == stored && work.view().Links.size() == 1U,
				   std::string{ aCase.Name } + ": still uncertain, with the reviewer's link");
			break;
		}
		work.Host->publish_publication();
		work.Lanes.drain();
		expect(catalog->Calls.empty() && server.Writes == before, std::string{ aCase.Name } + ": reopening and publishing again sent nothing");
		expect_no_duplicates(server, aCase.Name);
	}

	auto argument(const int aArgc, char** aArgv, const std::string_view aName) -> std::string
	{
		for (int index = 1; index + 1 < aArgc; ++index) {
			if (std::string_view{ aArgv[index] } == aName) {
				return aArgv[index + 1];
			}
		}
		return { };
	}
}

/// `test_review_cycle_publication_matrix PHASE --git GIT --case NAME [--kill POINT] ROOT`,
/// PHASE one of `--publish`, `--restart`, `--restart-again`, `--verify`, `--cases`.
auto main(const int aArgc, char** aArgv) -> int
try {
	const auto arguments = std::span{ aArgv, static_cast<std::size_t>(std::max(aArgc, 0)) };
	expect(arguments.size() >= 2U, "a phase");
	const auto phase = std::string{ arguments[1] };
	if (phase == "--cases") {
		for (const auto& entry : cases) {
			std::cout << entry.Name << ' ' << (entry.Kill.empty() ? "-" : entry.Kill) << '\n';
		}
		return EXIT_SUCCESS;
	}
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	const auto& chosen = find_case(argument(aArgc, aArgv, "--case"));
	const auto  root   = course{ .Root = std::filesystem::path{ arguments.back() }, .Phase = phase, .Kill = argument(aArgc, aArgv, "--kill") };
	expect(std::filesystem::is_directory(root.Root) && std::filesystem::is_directory(root.profile_root()) && std::filesystem::is_directory(root.isolation()), "the driver's course root");
	if (phase == "--publish") {
		publish_phase(root, chosen, *fixtures, *executable);
	} else if (phase == "--restart") {
		restart_phase(root, chosen, *executable, true);
	} else if (phase == "--restart-again") {
		restart_phase(root, chosen, *executable, false);
	} else if (phase == "--verify") {
		verify_phase(root, chosen, *executable);
	} else {
		expect(false, "a known phase: " + phase);
	}
	std::cout << phase << ':' << chosen.Name << ":pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle publication matrix failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return tests::failing_exit();
}
