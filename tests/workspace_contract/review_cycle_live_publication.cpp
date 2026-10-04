#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/review_cycle_publisher.hpp"
#include "cycle_course.hpp"
#include "github_host_profile.hpp"
#include "github_publication_contract.hpp"
#include "native_https_transport.hpp"
#include "publication_course.hpp"
#include "workspace_fixture.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1176]] RC59-10: the live publication course on the one
/// owner-designated disposable repository, github.com/MikeMarcin/BPR-Test.
/// \details Not a test row, and built only with
/// `-DBPR_BUILD_LIVE_PUBLICATION_COURSE=ON`. It composes what ships — the real
/// cycle store, the installed-Git publication catalog, the publication host,
/// `review_cycle_publisher`, the production `github_publication_session` and
/// the production native HTTPS transport with system trust and proxy — and
/// points it at the designated repository only; the owner and repository are
/// constants, not arguments. The credential is read from standard input and
/// never printed. The one seam is a port decorator that, when a case asks,
/// makes a real write and then reports its answer lost, lets "another
/// actor" (the same account through the same catalog) take a name first, or
/// ([[BPR-1178]]) cancels a real write while it is in flight.
///
/// The driver prepares the checkout: a clone of BPR-Test at a base branch it
/// pushed, and a branch named `--foreign` that another client pushed.
namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_publish_limit;
	using application::cycle_publish_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::write_file;

	constexpr std::string_view designated_owner      = "MikeMarcin";
	constexpr std::string_view designated_repository = "BPR-Test";

	struct arguments
	{
		std::filesystem::path Git;
		std::filesystem::path Checkout;
		std::string           Base;
		std::string           Stamp;
		std::string           Login;
		std::uint64_t         Account = 0;
		std::string           Foreign;
	};

	auto parse(const int aArgc, char** aArgv) -> arguments
	{
		auto parsed = arguments{ };
		for (int index = 1; index + 1 < aArgc; index += 2) {
			const auto name  = std::string_view{ aArgv[index] };
			const auto value = std::string{ aArgv[index + 1] };
			if (name == "--git") {
				parsed.Git = value;
			} else if (name == "--checkout") {
				parsed.Checkout = value;
			} else if (name == "--base") {
				parsed.Base = value;
			} else if (name == "--stamp") {
				parsed.Stamp = value;
			} else if (name == "--login") {
				parsed.Login = value;
			} else if (name == "--account") {
				parsed.Account = std::stoull(value);
			} else if (name == "--foreign") {
				parsed.Foreign = value;
			} else {
				tests::fail("unknown argument " + std::string{ name });
			}
		}
		expect(!parsed.Git.empty() && !parsed.Checkout.empty() && !parsed.Base.empty() && !parsed.Stamp.empty() && !parsed.Login.empty() && parsed.Account != 0U
				   && !parsed.Foreign.empty(),
			   "usage: --git --checkout --base --stamp --login --account --foreign, credential on standard input");
		return parsed;
	}

	/// \brief What a case asks the decorator to do to its next write.
	enum class live_fault : std::uint8_t {
		none,
		/// Make the branch, then report its answer lost.
		lose_branch_answer,
		/// Make the pull request, then report its answer lost.
		lose_pull_answer,
		/// Another actor creates the branch at P just before ours.
		race_branch,
		/// Another actor opens a pull request for the head just before ours.
		race_pull,
		/// [[BPR-1178]]. Cancel the branch's request while it is in flight.
		cancel_branch,
		/// [[BPR-1178]]. Cancel the pull request's request while it is in flight.
		cancel_pull,
	};

	/// \brief The production session, with one fault at a time and a count of writes.
	class live_port final : public application::github_publication_port
	{
	public:
		explicit live_port(application::github_publication_port& aInner)
			: Inner{ aInner }
		{
		}

		live_fault                   Fault = live_fault::none;
		core::git_commit_oid         RaceAt;
		std::size_t                  Writes = 0;
		std::optional<std::uint64_t> RacedPull;
		/// How long after a cancelled write starts its stop is requested.
		std::chrono::milliseconds CancelAfter{ 40 };
		/// How long the last uncancelled branch and pull request writes took,
		/// so a cancellation can aim inside a request's flight.
		std::chrono::milliseconds             BranchTook{ 0 };
		std::chrono::milliseconds             PullTook{ 0 };
		application::github_publication_error Lost{ .Failure = application::github_publication_failure::unavailable, .Transmission = application::github_publication_transmission::indeterminate };

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token aStop)
			-> application::github_publication_result<application::github_publication_capability> override
		{
			return Inner.observe_repository(aTarget, aStop);
		}
		auto observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			return Inner.observe_branch(aTarget, aBranch, aStop);
		}
		auto observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return Inner.observe_commit(aTarget, aCommit, aStop);
		}
		auto observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree, std::stop_token aStop) -> application::github_publication_result<bool> override
		{
			return Inner.observe_tree(aTarget, aTree, aStop);
		}
		auto observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob, std::stop_token aStop) -> application::github_publication_result<bool> override
		{
			return Inner.observe_blob(aTarget, aBlob, aStop);
		}
		auto create_blob(const application::github_publication_target& aTarget, std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override
		{
			++Writes;
			return Inner.create_blob(aTarget, aBytes, aExpected, aStop);
		}
		auto create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
						 const std::vector<std::string>& aDeletions, const core::git_tree_oid& aExpected, std::stop_token aStop) -> application::github_publication_result<void> override
		{
			++Writes;
			return Inner.create_tree(aTarget, aBase, aChanges, aDeletions, aExpected, aStop);
		}
		auto create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected, std::stop_token aStop)
			-> application::github_publication_result<void> override
		{
			++Writes;
			return Inner.create_commit(aTarget, aCommit, aExpected, aStop);
		}
		auto create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			++Writes;
			if (Fault == live_fault::cancel_branch) {
				Fault = live_fault::none;
				return midway([&](std::stop_token aMidway) { return Inner.create_branch(aTarget, aBranch, aCommit, std::move(aMidway)); });
			}
			const auto fault = take(live_fault::race_branch, live_fault::lose_branch_answer);
			if (fault == live_fault::race_branch) {
				const auto theirs = Inner.create_branch(aTarget, aBranch, RaceAt, aStop);
				expect(theirs && *theirs == application::github_branch_creation::created, "another actor's branch is created at P first");
			}
			const auto started = std::chrono::steady_clock::now();
			auto       created = Inner.create_branch(aTarget, aBranch, aCommit, aStop);
			BranchTook         = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
			if (fault == live_fault::lose_branch_answer) {
				expect(created && *created == application::github_branch_creation::created, "the branch was really created before its answer is lost");
				return std::unexpected{ Lost };
			}
			return created;
		}
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token aStop)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			++Writes;
			if (Fault == live_fault::cancel_pull) {
				Fault = live_fault::none;
				return midway([&](std::stop_token aMidway) { return Inner.create_pull_request(aTarget, aDraft, std::move(aMidway)); });
			}
			const auto fault = take(live_fault::race_pull, live_fault::lose_pull_answer);
			if (fault == live_fault::race_pull) {
				auto theirs       = aDraft;
				theirs.Title      = "Another actor's pull request for the same head";
				theirs.Body       = "Opened just before the planned one (RC59-10 live course).";
				const auto opened = Inner.create_pull_request(aTarget, theirs, aStop);
				expect(opened.has_value(), "another actor's pull request is opened first");
				RacedPull = opened->Number;
			}
			const auto started = std::chrono::steady_clock::now();
			auto       created = Inner.create_pull_request(aTarget, aDraft, aStop);
			PullTook           = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
			if (fault == live_fault::lose_pull_answer) {
				expect(created.has_value(), "the pull request was really created before its answer is lost");
				return std::unexpected{ Lost };
			}
			return created;
		}
		auto find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			return Inner.find_pull_requests(aTarget, aHead, aBase, aStop);
		}
		auto observe_pull_request(const application::github_publication_target& aTarget, std::uint64_t aNumber, std::stop_token aStop)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			return Inner.observe_pull_request(aTarget, aNumber, aStop);
		}

	private:
		/// One real write whose stop is requested `CancelAfter` after it
		/// starts, from another thread, as a reviewer's Cancel would be.
		auto midway(const auto& aWrite) -> decltype(aWrite(std::stop_token{ }))
		{
			auto source = std::stop_source{ };
			auto timer  = std::jthread{ [&source, delay = CancelAfter] {
				std::this_thread::sleep_for(delay);
				source.request_stop();
			} };
			auto result = aWrite(source.get_token());
			timer.join();
			return result;
		}

		/// The pending fault, if it is one of this operation's; a branch
		/// write never consumes a fault meant for the pull request.
		auto take(const live_fault aFirst, const live_fault aSecond) -> live_fault
		{
			return Fault == aFirst || Fault == aSecond ? std::exchange(Fault, live_fault::none) : live_fault::none;
		}

		application::github_publication_port& Inner;
	};

	auto operation(const std::uint16_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x5E;
		bytes[30]  = static_cast<std::uint8_t>(aSeed >> 8U);
		bytes[31]  = static_cast<std::uint8_t>(aSeed & 0xFFU);
		return application::cycle_operation_id{ bytes };
	}

	struct live_course
	{
		const arguments&                           Arguments;
		application::review_cycle_outgoing_host    Outgoing;
		application::review_cycle_publication_host Journal;
		std::uint16_t                              Next = 1;

		live_course(const arguments& aArguments, application::workspace_source_port& aSource, course& aCycle, publication_git& aGit)
			: Arguments{ aArguments }
			, Outgoing{ aSource, *aCycle.Store, *aGit.Port }
			, Journal{ Outgoing, *aCycle.Store }
		{
		}

		/// \brief Prepares and authorizes one plan for a new branch.
		auto authorized(course& aCycle, const std::string& aBranch, const bool aDraft) -> application::stored_cycle_publication
		{
			// Non-zero offsets on both signatures: the server's identity for
			// the commit equals the local one only if the commit endpoint keeps them.
			const auto author                    = core::git_signature{ .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1790000000, .OffsetMinutes = 330 };
			const auto committer                 = core::git_signature{ .Name = "BPR live course", .Email = "bpr-live-course@example.invalid", .Seconds = 1790000060, .OffsetMinutes = -420 };
			auto       request                   = application::cycle_publication_request{ };
			request.Candidate.Outgoing           = { .Base = "HEAD", .Target = "refs/heads/" + Arguments.Base };
			request.Candidate.Candidate.Metadata = { .Author = author, .Committer = committer, .Title = "Validate before parsing", .Body = "RC59-10 live course " + Arguments.Stamp + "." };
			request.Destination                  = { .Host       = "github.com",
													 .Account    = core::github_account_id{ Arguments.Account },
													 .Login      = Arguments.Login,
													 .Owner      = std::string{ designated_owner },
													 .Repository = std::string{ designated_repository },
													 .BaseBranch = Arguments.Base,
													 .NewBranch  = aBranch };
			request.PullTitle                    = std::string{ aDraft ? "[draft] " : "" } + "Validate before parsing (RC59-10 " + Arguments.Stamp + ")";
			request.PullBody                     = "Disposable pull request from the BetterPullReview RC59-10 live publication course. Nothing to review.";
			request.Draft                        = aDraft;
			const auto prepared                  = Journal.prepare(aCycle.state(), operation(Next++), request, { });
			expect(prepared.Publication.has_value(), "plan prepared for " + aBranch);
			const auto confirmed = Journal.authorize(aCycle.Subject, operation(Next++), prepared.Publication->Id.Id, prepared.Publication->Revision,
													 { .Actor = core::cycle_actor::reviewer, .PlanDigest = prepared.Publication->Journal.PlanDigest, .Account = core::github_account_id{ Arguments.Account } }, { });
			expect(confirmed.has_value(), "plan authorized for " + aBranch);
			return confirmed->Publication;
		}

		auto reauthorize(course& aCycle, const application::stored_cycle_publication& aPublication) -> application::stored_cycle_publication
		{
			const auto confirmed = Journal.authorize(aCycle.Subject, operation(Next++), aPublication.Id.Id, aPublication.Revision,
													 { .Actor = core::cycle_actor::reviewer, .PlanDigest = aPublication.Journal.PlanDigest, .Account = core::github_account_id{ Arguments.Account } }, { });
			expect(confirmed.has_value(), "fresh authorization");
			return confirmed->Publication;
		}
	};

	auto hex(const core::git_commit_oid& aCommit) -> std::string
	{
		return std::string{ core::canonical_hex(aCommit).view() };
	}

	void report(const std::string_view aCase, const application::stored_cycle_publication& aPublication)
	{
		std::cout << "LIVE " << aCase << ": branch " << aPublication.Plan.Destination.NewBranch << " commit " << hex(aPublication.Plan.Commit) << " tree "
				  << core::canonical_hex(aPublication.Plan.Tree).view() << " phase " << static_cast<int>(aPublication.Journal.Phase);
		if (aPublication.Journal.Link) {
			std::cout << " pull #" << aPublication.Journal.Link->Number << " " << aPublication.Journal.Link->Url << " head " << hex(aPublication.Journal.Link->Head)
					  << (aPublication.Journal.Link->Drifted ? " drifted" : " not drifted");
		}
		std::cout << '\n';
	}

	auto read_credential() -> std::string
	{
		auto credential = std::string{ };
		std::getline(std::cin, credential);
		while (!credential.empty() && (credential.back() == '\r' || credential.back() == '\n' || credential.back() == ' ')) {
			credential.pop_back();
		}
		expect(!credential.empty(), "a credential on standard input");
		return credential;
	}

	void wipe(std::string& aSecret)
	{
		std::ranges::fill(aSecret, '\0');
		aSecret.clear();
	}

	void run(const arguments& aArguments)
	{
		using adapters::github::github_host_profile_candidate;

		// The GitHub.com profile under native HTTPS with system proxy and trust.
		const auto profile = adapters::github::validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 1 },
			.Host           = core::git_host_id{ 1 },
			.Revision       = application::github_profile_revision{ 1 },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = adapters::github::github_profile_provenance::builtin,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		expect(profile.has_value(), "the GitHub.com native HTTPS profile validates");
		const auto context = application::github_credential_operation_context{
			.Request            = core::request_id{ 1 },
			.ApplicationSession = core::application_session_id{ 1 },
			.Authority          = { .Profile         = profile->profile(),
									.ProfileRevision = profile->revision(),
									.Host            = profile->host(),
									.Hostname        = application::github_canonical_hostname{ profile->hostname() },
									.Account         = core::github_account_id{ aArguments.Account },
									.ServerContract  = profile->server_contract() },
			.IdentityGeneration = core::github_identity_generation{ 1 },
		};
		auto secret   = read_credential();
		auto material = application::github_credential_material::create(std::as_bytes(std::span<const char>{ secret.data(), secret.size() }));
		wipe(secret);
		expect(material.has_value(), "credential material");
		auto transport = adapters::github::detail::make_native_https_account_transport(*profile, { .Context = context, .Credential = std::move(*material) });
		expect(transport.has_value(), "the production native HTTPS account transport");
		auto       session = adapters::github::detail::github_publication_session{ **transport, context };
		auto       remote  = live_port{ session };
		const auto target  = application::github_publication_target{ .Host       = "github.com",
																	 .Server     = application::github_server_contract::github_com_2022_11_28,
																	 .Account    = core::github_account_id{ aArguments.Account },
																	 .Login      = aArguments.Login,
																	 .Owner      = std::string{ designated_owner },
																	 .Repository = std::string{ designated_repository } };

		// R0 at the pushed base; R1 modifies, adds and deletes.
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aArguments.Git };
		const auto root   = aArguments.Checkout;
		auto       cycle  = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0 at the base");
		write_file(root / "src" / "parse.cpp", "#include \"validate.hpp\"\n\nint parse() { return validate() ? 0 : 1; }\n");
		write_file(root / "src" / "validate.cpp", "// RC59-10 live course " + aArguments.Stamp + "\nbool validate() { return true; }\n");
		std::filesystem::remove(root / "src" / "old.cpp");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1 with a change, an addition and a deletion");

		auto       live      = live_course{ aArguments, *source, cycle, git };
		auto       publisher = application::review_cycle_publisher{ live.Journal, *cycle.Store, remote, application::github_server_contract::github_com_2022_11_28, core::github_account_id{ aArguments.Account } };
		const auto branch    = [&](const std::string_view aCase) { return "bpr-rc59-10/" + aArguments.Stamp + "/" + std::string{ aCase }; };
		const auto pulls     = [&](const std::string& aHead) {
			const auto found = session.find_pull_requests(target, aHead, aArguments.Base, { });
			expect(found.has_value(), "pull requests read for " + aHead);
			return *found;
		};

		// 1. One plan end to end: blobs, tree (the deletion as a null identity),
		// the commit under non-zero offsets, the new branch, a ready pull request.
		auto       ready     = live.authorized(cycle, branch("ready"), false);
		const auto published = publisher.publish(cycle.Subject, ready.Id.Id, { });
		report("ready", *published.Publication);
		expect(published.Status == cycle_publish_status::linked && published.Publication->Journal.Phase == core::publication_phase::linked,
			   "published end to end and linked (a server identity other than the local one would have failed the publication)");
		expect(std::ranges::find(ready.Plan.Deleted, std::string{ "src/old.cpp" }) != ready.Plan.Deleted.end(), "the plan deletes src/old.cpp");
		const auto& link = *published.Publication->Journal.Link;
		expect(link.Head == ready.Plan.Commit && link.BaseBranch == aArguments.Base && !link.Drifted, "the pull request's head is the planned commit");
		const auto readyPull = session.observe_pull_request(target, link.Number, { });
		expect(readyPull && *readyPull && !(*readyPull)->Draft && (*readyPull)->Author == aArguments.Login, "a ready pull request under the designated login");
		const auto writes = remote.Writes;
		expect(publisher.publish(cycle.Subject, ready.Id.Id, { }).Status == cycle_publish_status::unauthorized && remote.Writes == writes && pulls(branch("ready")).size() == 1U,
			   "a linked publication never writes again");

		// 2. The same candidate as a draft pull request.
		auto       drafted = live.authorized(cycle, branch("draft"), true);
		const auto draft   = publisher.publish(cycle.Subject, drafted.Id.Id, { });
		report("draft", *draft.Publication);
		expect(draft.Status == cycle_publish_status::linked, "the draft publication is linked");
		const auto draftPull = session.observe_pull_request(target, draft.Publication->Journal.Link->Number, { });
		expect(draftPull && *draftPull && (*draftPull)->Draft, "the server reports a draft pull request");

		// 3. The branch's answer is lost: recovery reads it as this plan's, and
		// the pull request waits for a fresh confirmation.
		auto lostBranch       = live.authorized(cycle, branch("lost-branch"), false);
		remote.Fault          = live_fault::lose_branch_answer;
		const auto unanswered = publisher.publish(cycle.Subject, lostBranch.Id.Id, { });
		report("lost-branch (answer lost)", *unanswered.Publication);
		expect(unanswered.Status == cycle_publish_status::stopped && unanswered.Publication->Journal.Phase == core::publication_phase::uncertain, "an unanswered branch is uncertain");
		expect(publisher.publish(cycle.Subject, lostBranch.Id.Id, { }).Status == cycle_publish_status::unauthorized, "no blind retry");
		const auto adopted = publisher.recover(cycle.Subject, lostBranch.Id.Id, { });
		report("lost-branch (recovered)", *adopted.Publication);
		expect(adopted.Status == cycle_publish_status::recovered && adopted.Publication->Journal.Phase == core::publication_phase::branch_created, "read back as this plan's branch at the planned commit");
		live.reauthorize(cycle, *adopted.Publication);
		const auto continued = publisher.publish(cycle.Subject, lostBranch.Id.Id, { });
		report("lost-branch (continued)", *continued.Publication);
		expect(continued.Status == cycle_publish_status::linked && pulls(branch("lost-branch")).size() == 1U, "continued to one pull request without a second branch write");

		// 4. The pull request's answer is lost, then a restart: recovery
		// associates exactly the one pull request of ours.
		auto lostPull     = live.authorized(cycle, branch("lost-pull"), false);
		remote.Fault      = live_fault::lose_pull_answer;
		const auto silent = publisher.publish(cycle.Subject, lostPull.Id.Id, { });
		report("lost-pull (answer lost)", *silent.Publication);
		expect(silent.Status == cycle_publish_status::stopped && silent.Publication->Journal.Phase == core::publication_phase::uncertain, "an unanswered pull request is uncertain");
		expect(publisher.publish(cycle.Subject, lostPull.Id.Id, { }).Status == cycle_publish_status::unauthorized, "never created again blindly");
		expect(live.Journal.recover_after_restart(cycle.Subject, { }).has_value(), "restart");
		const auto reconciled = publisher.recover(cycle.Subject, lostPull.Id.Id, { });
		report("lost-pull (recovered)", *reconciled.Publication);
		expect(reconciled.Publication->Journal.Phase == core::publication_phase::linked && reconciled.Publication->Journal.Link->Head == lostPull.Plan.Commit
				   && pulls(branch("lost-pull")).size() == 1U,
			   "the one pull request of ours is associated, not recreated");

		// 5. A branch another client pushed before the plan: refused before any write.
		auto       foreign = live.authorized(cycle, aArguments.Foreign, false);
		const auto before  = remote.Writes;
		const auto refused = publisher.publish(cycle.Subject, foreign.Id.Id, { });
		std::cout << "LIVE foreign-branch: " << aArguments.Foreign << " limit " << (refused.Limit ? application::name(*refused.Limit) : "none") << " writes " << remote.Writes - before << '\n';
		expect(refused.Status == cycle_publish_status::limited && refused.Limit == cycle_publish_limit::branch_exists && remote.Writes == before, "an existing branch is refused with no write");

		// 6. Another actor takes the name between the check and the write: the
		// server's 422 is reported as foreign and nothing is overwritten.
		auto raced       = live.authorized(cycle, branch("raced-branch"), false);
		remote.Fault     = live_fault::race_branch;
		remote.RaceAt    = raced.Plan.Parent;
		const auto taken = publisher.publish(cycle.Subject, raced.Id.Id, { });
		report("raced-branch", *taken.Publication);
		const auto kept = session.observe_branch(target, branch("raced-branch"), { });
		expect(taken.Status == cycle_publish_status::stopped && taken.Publication->Journal.Phase == core::publication_phase::target_drift && kept && *kept && **kept == raced.Plan.Parent,
			   "the other actor's branch is reported, never overwritten or adopted");

		// 7. Another actor opens a pull request for the head just before ours:
		// the server's 422 is read again and recorded as foreign, no second one.
		auto racedPull    = live.authorized(cycle, branch("raced-pull"), false);
		remote.Fault      = live_fault::race_pull;
		const auto theirs = publisher.publish(cycle.Subject, racedPull.Id.Id, { });
		report("raced-pull", *theirs.Publication);
		const auto open = pulls(branch("raced-pull"));
		std::cout << "LIVE raced-pull: another actor's #" << remote.RacedPull.value_or(0U) << ", pull requests for the head " << open.size() << '\n';
		expect(theirs.Status == cycle_publish_status::stopped && theirs.Publication->Journal.Phase == core::publication_phase::target_drift && open.size() == 1U
				   && open.front().Number == remote.RacedPull.value_or(0U),
			   "a foreign pull request is never duplicated or adopted");

		// 8 and 9. [[BPR-1178]]. A write cancelled while it is in flight. Its
		// outcome is GitHub's timing: done before the stop (confirmed), sent
		// but unanswered (uncertain), or stopped before it left (not sent).
		// Whatever it was, a request that left is never called not sent,
		// nothing is retried blindly, and one pull request results.
		const auto cancelled = [&](const std::string_view aCase, const live_fault aFault, const core::publication_effect_kind aKind) {
			auto plan = live.authorized(cycle, branch(aCase), false);
			// Three quarters of the same write's last round trip: after the
			// request has most likely left, before its answer has come back.
			const auto took    = aKind == core::publication_effect_kind::branch ? remote.BranchTook : remote.PullTook;
			remote.CancelAfter = took * 3 / 4;
			std::cout << "LIVE " << aCase << ": the last uncancelled write took " << took.count() << " ms; the stop is requested after " << remote.CancelAfter.count() << " ms\n";
			remote.Fault       = aFault;
			const auto stopped = publisher.publish(cycle.Subject, plan.Id.Id, { });
			report(std::string{ aCase } + " (cancelled)", *stopped.Publication);
			const auto& effects = stopped.Publication->Journal.Effects;
			const auto  effect  = std::ranges::find(effects, aKind, &core::publication_effect::Kind);
			expect(effect != effects.end(), "the cancelled write was journaled before it was attempted");
			const auto state  = effect->State;
			const auto held   = session.observe_branch(target, branch(aCase), { });
			const auto opened = pulls(branch(aCase));
			expect(held.has_value(), "the branch read after the cancellation");
			const auto present = aKind == core::publication_effect_kind::branch ? held->has_value() : !opened.empty();
			std::cout << "LIVE " << aCase << ": the cancelled " << core::name(aKind) << " is " << core::name(state) << "; GitHub " << (present ? "holds it" : "does not hold it") << '\n';
			expect(state == core::publication_effect_state::confirmed || state == core::publication_effect_state::uncertain || state == core::publication_effect_state::not_sent,
				   "a cancelled write is confirmed, uncertain or not sent, never refused or foreign");
			expect(state != core::publication_effect_state::not_sent || !present, "a write GitHub took is never reported as not sent");
			expect(state != core::publication_effect_state::confirmed || present, "a confirmed write is there");
			if (state == core::publication_effect_state::uncertain) {
				expect(publisher.publish(cycle.Subject, plan.Id.Id, { }).Status == cycle_publish_status::unauthorized, "no blind retry after a cancelled write");
				const auto read = publisher.recover(cycle.Subject, plan.Id.Id, { });
				report(std::string{ aCase } + " (recovered)", *read.Publication);
				expect(read.Status == cycle_publish_status::recovered, "recovery reads what the cancelled write left");
				if (read.Publication->Journal.Phase != core::publication_phase::linked) {
					live.reauthorize(cycle, *read.Publication);
				}
			} else if (state == core::publication_effect_state::not_sent) {
				live.reauthorize(cycle, *stopped.Publication);
			}
			auto finished = publisher.publish(cycle.Subject, plan.Id.Id, { });
			if (finished.Status == cycle_publish_status::unauthorized) {
				// Linked by recovery or by the first run: nothing more to send.
				expect(finished.Publication->Journal.Phase == core::publication_phase::linked, "already linked");
			}
			report(std::string{ aCase } + " (finished)", *finished.Publication);
			const auto after = session.observe_branch(target, branch(aCase), { });
			expect(finished.Publication->Journal.Phase == core::publication_phase::linked && pulls(branch(aCase)).size() == 1U && after && *after && **after == plan.Plan.Commit,
				   "one branch at the planned commit and one pull request, whatever the cancellation did");
		};
		cancelled("cancelled-branch", live_fault::cancel_branch, core::publication_effect_kind::branch);
		cancelled("cancelled-pull", live_fault::cancel_pull, core::publication_effect_kind::pull_request);

		std::cout << "LIVE commit " << hex(ready.Plan.Commit) << " parent " << hex(ready.Plan.Parent) << " author offset +05:30, committer offset -07:00\n";
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QCoreApplication application{ aArgc, aArgv };
	const auto       arguments = parse(aArgc, aArgv);
	run(arguments);
	std::cout << "review-cycle-live-publication:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle live publication failure: " << aError.what() << '\n';
	return bpr::tests::failing_exit();
} catch (...) {
	return bpr::tests::failing_exit();
}
