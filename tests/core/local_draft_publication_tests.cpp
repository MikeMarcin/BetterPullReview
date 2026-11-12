/// \file
/// \brief RFC-BPR-0111 ([[BPR-2029]], [[BPR-2030]]). Creating a draft pull
/// request from a local draft without any network: the hosted branch decision
/// and the named push, the exact plan and its digest, authorization bound to
/// the digest and account, the journal through created, refused, not sent and
/// uncertain answers, read-first recovery, a restart, explicit association of
/// an existing pull request, drift, follow-ups, the stored form and a plan
/// stored while it still bound evidence.

#include "bpr/core/hex.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_publication.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/sha256.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename T>
	auto ok(T aResult, const std::string_view aMessage)
	{
		expect(aResult.has_value(), aMessage);
		return std::move(*aResult);
	}

	auto commit(const char aDigit) -> core::git_commit_oid { return *core::parse_git_commit_oid(std::string(40U, aDigit)); }
	auto entry_oid(const char aDigit) -> core::git_entry_oid { return *core::parse_git_entry_oid(std::string(40U, aDigit)); }

	auto candidate(const char aHead, const bool aWorking) -> core::local_draft_candidate
	{
		return { .Head      = commit(aHead),
				 .TargetTip = commit('b'),
				 .Base      = commit('c'),
				 .Included  = aWorking ? std::vector<std::string>{ "src/search.cpp" } : std::vector<std::string>{ },
				 .Changes   = { { .Path   = "src/search.cpp",
								  .Change = core::draft_change::modified,
								  .Left   = core::draft_side{ .Mode = 0100644, .Object = entry_oid('2') },
								  .Right  = core::draft_side{ .Mode = 0100644, .Object = entry_oid('3') } } } };
	}

	auto make_draft(const bool aWorking = false) -> core::local_draft
	{
		auto draft = ok(core::create_local_draft({ }, { std::string(32U, '1') },
												 { .Location = "/work/my search", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } },
												 "feature/search", { .Target = { .Remote = "origin", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" } },
												 { }, candidate('a', aWorking), 100),
						"a draft is created");
		return ok(core::edit_description(draft, draft.Version, "Cancel searches promptly", "## Why\n\nSearches kept running.\n", 101), "the description saves");
	}

	auto observed(const std::optional<core::git_commit_oid>& aHead) -> core::local_publication_observation
	{
		return { .Host = "github.com", .Account = core::github_account_id{ 7U }, .Login = "author", .BranchObserved = true, .BranchHead = aHead, .TargetTip = commit('b') };
	}

	auto pull(const core::git_commit_oid& aHead) -> core::local_publication_link
	{
		return { .Number     = 42U,
				 .Url        = "https://github.com/acme/engine/pull/42",
				 .NodeId     = "PR_x",
				 .Owner      = "acme",
				 .Repository = "engine",
				 .HeadBranch = "feature/search",
				 .Head       = aHead,
				 .BaseBranch = "main",
				 .Author     = "author",
				 .Draft      = true };
	}

	auto planned(const core::local_draft& aDraft, const bool aTour = false) -> core::local_draft
	{
		auto plan = ok(core::make_local_publication(aDraft, observed(commit('a')), aTour), "a plan from the pushed branch");
		return ok(core::record_local_publication(aDraft, aDraft.Version, std::move(plan), 110), "the plan is recorded");
	}

	void test_hosted_branch_and_named_push()
	{
		const auto  draft = make_draft();
		const auto& c     = *core::latest_candidate(draft);
		expect(core::decide_hosted_branch(c, true, commit('a')) == core::hosted_branch_state::matches, "the pushed candidate matches");
		expect(core::decide_hosted_branch(c, true, std::nullopt) == core::hosted_branch_state::missing, "no branch is missing");
		expect(core::decide_hosted_branch(c, true, commit('d')) == core::hosted_branch_state::differs, "another head differs");
		expect(core::decide_hosted_branch(c, false, std::nullopt) == core::hosted_branch_state::unobserved, "an unread branch is not guessed");
		const auto dirty = make_draft(true);
		expect(core::decide_hosted_branch(*core::latest_candidate(dirty), true, commit('a')) == core::hosted_branch_state::uncommitted,
			   "a candidate with working files is never the pushed branch");
		const auto push = core::name_push(draft, c);
		expect(push.Repository == "https://github.com/acme/engine.git", "the push names the hosted repository");
		expect(push.Command == "git -C '/work/my search' push https://github.com/acme/engine.git " + std::string(40U, 'a') + ":refs/heads/feature/search",
			   "the exact candidate commit to the draft's branch, in the worktree, never origin");
		expect(push.Command.find("origin") == std::string::npos && push.Command.find("--force") == std::string::npos && push.Command.find(" -f") == std::string::npos,
			   "no bare origin and no force");
		expect(core::make_local_publication(draft, observed(std::nullopt), false).error() == core::local_publication_refusal::branch_missing, "missing refuses with the push");
		expect(core::make_local_publication(draft, observed(commit('d')), false).error() == core::local_publication_refusal::branch_differs, "differs refuses");
		expect(core::make_local_publication(dirty, observed(commit('a')), false).error() == core::local_publication_refusal::uncommitted, "dirty refuses");
		auto other = observed(commit('a'));
		other.Host = "ghe.example.com";
		expect(core::make_local_publication(draft, other, false).error() == core::local_publication_refusal::no_account, "another host's account cannot publish here");
		auto existing     = observed(commit('a'));
		existing.Existing = 9U;
		expect(core::make_local_publication(draft, existing, false).error() == core::local_publication_refusal::pull_request_exists, "an existing pull request is never duplicated");
	}

	void test_plan_binds_content()
	{
		auto draft = make_draft();
		draft      = ok(core::add_note(draft, draft.Version, "Private thought.", "", core::note_origin::author, 105), "a note");
		auto plan  = ok(core::make_local_publication(draft, observed(commit('a')), false), "the plan");
		expect(plan.Title == "Cancel searches promptly" && plan.Body == "## Why\n\nSearches kept running.\n" && plan.Draft, "title and body exactly, as a draft");
		expect(plan.PrivateNotes == 1U && plan.PrivateConversations == 0U, "the note stays on this computer");
		expect(plan.Destination.Owner == "acme" && plan.Destination.BaseBranch == "main" && plan.Destination.HeadBranch == "feature/search" && plan.Head == commit('a'),
			   "destination and head are the draft's");
		const auto digest = core::local_publication_digest(plan);
		auto       body   = plan;
		body.Body += "x";
		expect(core::local_publication_digest(body) != digest, "the body changes the digest");
		auto account                = plan;
		account.Destination.Account = core::github_account_id{ 8U };
		expect(core::local_publication_digest(account) != digest, "the account changes the digest");
		const auto preview = core::render_local_publication_preview(plan);
		expect(preview.find("as author") != std::string::npos && preview.find("acme/engine main from feature/search") != std::string::npos
				   && preview.find("Stays on this computer: 1 note, 0 agent conversations") != std::string::npos && preview.find("Private thought") == std::string::npos
				   && preview.find("evidence") == std::string::npos,
			   "the preview says where, as whom and what stays local, without private text");
		auto moved      = observed(commit('a'));
		moved.TargetTip = commit('e');
		const auto base = ok(core::make_local_publication(draft, moved, false), "a moved target still plans");
		expect(core::render_local_publication_preview(base).find("The target moved") != std::string::npos, "base movement is said");
	}

	void test_authorize_and_create()
	{
		auto draft  = planned(make_draft());
		auto digest = draft.Publication->Journal.Digest;
		expect(core::authorize_local_publication(draft, draft.Version, "0000", core::github_account_id{ 7U }, 111).error() == core::local_publication_refusal::stale_plan,
			   "another digest authorizes nothing");
		expect(core::authorize_local_publication(draft, draft.Version, digest, core::github_account_id{ 8U }, 111).error() == core::local_publication_refusal::authority_changed,
			   "another account authorizes nothing");
		expect(core::begin_local_creation(draft, draft.Version, 1U, core::github_account_id{ 7U }, 111).error() == core::local_publication_refusal::unauthorized,
			   "nothing is sent unconfirmed");
		draft                 = ok(core::authorize_local_publication(draft, draft.Version, digest, core::github_account_id{ 7U }, 111), "confirmed");
		const auto generation = draft.Publication->Journal.Generation;
		auto       refreshed  = ok(core::add_candidate(draft, draft.Version, candidate('d', false), 112), "a successor candidate");
		expect(core::authorize_local_publication(refreshed, refreshed.Version, digest, core::github_account_id{ 7U }, 113).error() == core::local_publication_refusal::stale_plan,
			   "a new candidate makes the plan stale");
		draft = ok(core::begin_local_creation(draft, draft.Version, generation, core::github_account_id{ 7U }, 112), "the creation is recorded first");
		expect(draft.Publication->Journal.Phase == core::local_publication_phase::creating && draft.Publication->Journal.Create == core::local_step_state::sending, "sending");
		expect(core::record_local_publication(draft, draft.Version, draft.Publication->Plan, 113).error() == core::local_publication_refusal::unresolved,
			   "no new plan while one is in flight");
		auto foreign       = pull(commit('a'));
		foreign.HeadBranch = "other";
		expect(core::complete_local_creation(draft, draft.Version, core::local_creation_outcome::created, foreign, { }, 113).error() == core::local_publication_refusal::invalid,
			   "an answer for another branch is not this creation");
		draft = ok(core::complete_local_creation(draft, draft.Version, core::local_creation_outcome::created, pull(commit('a')), { }, 113), "created");
		expect(draft.Publication->Journal.Phase == core::local_publication_phase::linked && draft.Association && draft.Association->Number == 42U && !draft.Publication->Journal.Link->Drifted,
			   "the draft is associated with exactly the created pull request");
		expect(draft.Publication->Journal.FollowUps.size() == 1U && draft.Publication->Journal.FollowUps.front().State == core::local_step_state::sent, "the association step is sent");
		expect(core::make_local_publication(draft, observed(commit('a')), false).error() == core::local_publication_refusal::linked, "a linked draft creates no second pull request");
		expect(core::drafts_in_preparation(std::vector{ draft }).empty(), "it leaves In preparation for the pull request's row");
		const auto decoded = core::decode_local_draft(core::encode_local_draft(draft));
		expect(decoded && *decoded == draft, "the journal round-trips in the draft's record");
	}

	void test_lost_answer_and_restart()
	{
		auto draft = planned(make_draft());
		draft      = ok(core::authorize_local_publication(draft, draft.Version, draft.Publication->Journal.Digest, core::github_account_id{ 7U }, 111), "confirmed");
		draft      = ok(core::begin_local_creation(draft, draft.Version, draft.Publication->Journal.Generation, core::github_account_id{ 7U }, 112), "sending");
		auto lost  = ok(core::complete_local_creation(draft, draft.Version, core::local_creation_outcome::uncertain, std::nullopt, "GitHub did not answer.", 113), "the answer was lost");
		expect(lost.Publication->Journal.Phase == core::local_publication_phase::uncertain && lost.Publication->Journal.Generation == 0U, "uncertain, no authorization left");
		expect(core::authorize_local_publication(lost, lost.Version, lost.Publication->Journal.Digest, core::github_account_id{ 7U }, 114).error() == core::local_publication_refusal::unresolved,
			   "no new confirmation before a read: never a blind retry");
		auto found = ok(core::observe_local_creation(lost, lost.Version, pull(commit('a')), 114), "a read finds it");
		expect(found.Publication->Journal.Phase == core::local_publication_phase::linked && found.Association, "found: linked, not created again");
		auto absent = ok(core::observe_local_creation(lost, lost.Version, std::nullopt, 114), "a read finds nothing");
		expect(absent.Publication->Journal.Phase == core::local_publication_phase::prepared && absent.Publication->Journal.Create == core::local_step_state::not_sent,
			   "nothing found: prepared again, a new confirmation needed");
		auto other   = pull(commit('a'));
		other.Author = "someone";
		expect(core::observe_local_creation(lost, lost.Version, other, 114).error() == core::local_publication_refusal::invalid, "another author's pull request is not ours");
		auto restarted = core::restart_local_publication(draft);
		expect(restarted.Publication->Journal.Phase == core::local_publication_phase::uncertain, "a restart mid-creation is uncertain");
		auto refused = ok(core::complete_local_creation(draft, draft.Version, core::local_creation_outcome::refused, std::nullopt, "Validation failed.", 113), "refused");
		expect(refused.Publication->Journal.Phase == core::local_publication_phase::refused && refused.Publication->Journal.Create == core::local_step_state::failed && !refused.Association,
			   "a refusal fails the step and associates nothing");
		auto unsent = ok(core::complete_local_creation(draft, draft.Version, core::local_creation_outcome::not_sent, std::nullopt, "Offline.", 113), "not sent");
		expect(unsent.Publication->Journal.Phase == core::local_publication_phase::prepared, "not sent: nothing happened remotely");
		auto authorized = planned(make_draft());
		authorized      = ok(core::authorize_local_publication(authorized, authorized.Version, authorized.Publication->Journal.Digest, core::github_account_id{ 7U }, 111), "confirmed");
		expect(core::restart_local_publication(authorized).Publication->Journal.Generation == 0U, "no authorization survives a restart");
		auto cancelled = ok(core::cancel_local_publication(authorized, authorized.Version, 112), "cancel before sending");
		expect(cancelled.Publication->Journal.Phase == core::local_publication_phase::cancelled, "cancelled");
		expect(core::cancel_local_publication(draft, draft.Version, 113).error() == core::local_publication_refusal::unresolved, "a creation in flight is not pretended away");
	}

	void test_existing_drift_and_follow_ups()
	{
		auto draft   = planned(make_draft(), true);
		auto adopted = ok(core::adopt_local_pull_request(draft, draft.Version, pull(commit('d')), 111), "the author associates an existing pull request");
		expect(adopted.Publication->Journal.Link->Adopted && adopted.Publication->Journal.Link->Drifted && adopted.Publication->Journal.Create == core::local_step_state::not_sent,
			   "associated, not created, and its other head is drift");
		expect(core::tour_follow_up_blocked(adopted) == core::tour_follow_up_block::not_selected, "no stops: the tour follow-up was not planned");

		auto toured = make_draft();
		toured      = ok(core::add_local_stop(toured, toured.Version, "Cancel", "Stops the worker.", { .Path = "src/search.cpp", .Text = std::string{ "a\nb\nc\n" } }, core::diff_side::right, 1U, 2U, 102),
						 "a stop");
		auto plan   = ok(core::make_local_publication(toured, observed(commit('a')), true), "the plan with the tour");
		expect(plan.TourFollowUp && plan.Unaccepted.size() == 1U, "the unaccepted stop is listed and does not block creation (X20)");
		toured = ok(core::record_local_publication(toured, toured.Version, plan, 103), "recorded");
		toured = ok(core::authorize_local_publication(toured, toured.Version, toured.Publication->Journal.Digest, core::github_account_id{ 7U }, 104), "confirmed");
		toured = ok(core::begin_local_creation(toured, toured.Version, toured.Publication->Journal.Generation, core::github_account_id{ 7U }, 105), "sending");
		toured = ok(core::complete_local_creation(toured, toured.Version, core::local_creation_outcome::created, pull(commit('a')), { }, 106), "created");
		expect(toured.Publication->Journal.FollowUps.size() == 2U && toured.Publication->Journal.FollowUps.back().State == core::local_step_state::waiting, "the tour waits");
		expect(core::tour_follow_up_blocked(toured) == core::tour_follow_up_block::unaccepted, "the tour cannot publish with an unaccepted stop");
		toured = ok(core::accept_local_stop(toured, toured.Version, toured.Preparation.Tour.Stops.front().Id, 107), "accepted");
		expect(!core::tour_follow_up_blocked(toured), "accepted: the tour may publish");
		toured = ok(core::record_follow_up(toured, toured.Version, core::local_follow_up::tour, core::local_step_state::failed, "GitHub refused the comment.", 108), "failed");
		expect(toured.Publication->Journal.Phase == core::local_publication_phase::linked && toured.Association, "a failed follow-up leaves the pull request associated");
		expect(core::record_local_publication(toured, toured.Version, plan, 109).error() == core::local_publication_refusal::linked, "resuming a follow-up never recreates");
		const auto decoded = core::decode_local_draft(core::encode_local_draft(toured));
		expect(decoded && *decoded == toured, "follow-ups round-trip");
		auto corrupt = core::encode_local_draft(toured);
		corrupt.replace(corrupt.find("\"phase\":\"linked\""), 16U, "\"phase\":\"prepared\"");
		expect(!core::decode_local_draft(corrupt), "a linked journal without its phase is refused");
	}

	/// A plan stored before RFC-BPR-0109 was deferred also named evidence and
	/// its digest covered it: the stored form is still read, its digest is
	/// checked against the form it was made in and carried to the same plan
	/// without evidence. A tampered one is refused.
	void test_plan_stored_with_evidence()
	{
		const auto draft   = planned(make_draft());
		const auto current = core::encode_local_draft_publication(*draft.Publication);
		const auto open    = std::string_view{ "{\"plan\":" };
		const auto close   = current.find(",\"digest\":\"");
		expect(current.starts_with(open) && close != std::string::npos, "the stored form leads with the plan");
		auto       plan = current.substr(open.size(), close - open.size());
		const auto at   = plan.find(",\"private_notes\":");
		expect(at != std::string::npos, "the plan names its private notes");
		plan.insert(at, ",\"evidence\":[3],\"stale\":[],\"private_records\":1");
		const auto bytes  = std::span{ reinterpret_cast<const std::uint8_t*>(plan.data()), plan.size() };
		auto       legacy = std::string{ open } + plan + current.substr(close, 11U) + core::lower_hex(core::sha256(bytes)) + current.substr(close + 11U + 64U);
		const auto read   = core::decode_local_draft_publication(legacy);
		expect(read && *read == *draft.Publication, "the earlier plan reads as the same plan, with the digest it has now");
		expect(core::encode_local_draft_publication(*read).find("evidence") == std::string::npos, "and is written without evidence");
		auto tampered = legacy;
		tampered.replace(tampered.find("\"evidence\":[3]"), 15U, "\"evidence\":[4]");
		expect(!core::decode_local_draft_publication(tampered), "a digest that does not match the earlier form is refused");
	}
}

auto main() -> int
{
	test_hosted_branch_and_named_push();
	test_plan_binds_content();
	test_authorize_and_create();
	test_lost_answer_and_restart();
	test_existing_drift_and_follow_ups();
	test_plan_stored_with_evidence();
	std::cout << "local-draft-publication-core:pass\n";
	return EXIT_SUCCESS;
}
