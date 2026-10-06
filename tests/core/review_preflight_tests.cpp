// [[BPR-1085]]. The submitting reviewer's own feedback before a native review
// decision (RFC-BPR-0048 R14, R15, R42): the four groups kept apart, other
// reviewers never attributed to the submitter, uncertainty never read as "no
// blockers", label guidance never shown as an explicit blocker, and the exact
// changes that invalidate a prepared decision. Conversations are built as real
// posts, classified by the annotation set and admitted by the shared
// population the label counters use.

#include "bpr/core/review_preflight.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "review preflight failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::string_view me    = "U_me";
	constexpr std::string_view other = "U_other";

	auto review_key() -> cached_review_key
	{
		cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = host_profile_id{ 1U };
		key.Host         = git_host_id{ 1U };
		key.Account      = github_account_id{ 4U };
		key.Locator      = { "octo", "repo" };
		key.Number       = pull_request_number{ 7U };
		return key;
	}

	/// One conversation: its posts (root first), who wrote the root, and its
	/// resolution.
	struct conversation
	{
		std::string                                      Id;
		std::vector<std::pair<std::string, std::string>> Posts;
		std::optional<std::string>                       Author;
		feedback_resolution                              Resolution = feedback_resolution::unresolved;
	};

	struct world
	{
		feedback_annotation_set                  Annotations;
		feedback_population                      Population;
		std::vector<review_preflight_authorship> Authors;
	};

	auto build(const std::vector<conversation>& aConversations, feedback_collection aCollection = feedback_collection::complete) -> world
	{
		auto observations = std::vector<feedback_post_observation>{ };
		auto facts        = std::vector<feedback_conversation_facts>{ };
		auto result       = world{ };
		for (const auto& entry : aConversations) {
			for (std::size_t index = 0U; index < entry.Posts.size(); ++index) {
				observations.push_back({ .Conversation = entry.Id,
										 .Id           = entry.Posts[index].first,
										 .ReplyTo      = index == 0U ? std::nullopt : std::optional<std::string_view>{ entry.Posts.front().first },
										 .Body         = entry.Posts[index].second });
			}
			facts.push_back({ .Id = entry.Id, .Resolution = entry.Resolution, .Anchor = { .Status = feedback_anchor_status::file, .Path = "src/a.cpp" } });
			result.Authors.push_back({ .Conversation = entry.Id, .RootAuthor = entry.Author });
		}
		auto refresh = refresh_feedback_annotations({ }, review_key(), observations);
		expect(refresh.Refusal == feedback_refresh_refusal::none, "annotation refresh accepted");
		auto adopted = adopt_feedback_refresh({ }, std::move(refresh));
		expect(adopted.has_value(), "annotation refresh adopted");
		result.Annotations = std::move(*adopted);
		auto population    = build_feedback_population(result.Annotations, facts, aCollection);
		expect(population.Refusal == feedback_population_refusal::none, "population admitted");
		result.Population = std::move(population.Population);
		return result;
	}

	auto project(const world& aWorld, std::span<const review_preflight_draft> aDrafts = { }, std::string_view aSubmitter = me) -> review_preflight
	{
		auto preflight = project_review_preflight(aSubmitter, aWorld.Annotations, aWorld.Population, aWorld.Authors, aDrafts);
		expect(preflight.Refusal == review_preflight_refusal::none, "preflight accepted");
		return preflight;
	}

	auto blockers(const review_preflight& aPreflight) -> std::vector<std::string>
	{
		auto ids = std::vector<std::string>{ };
		std::ranges::transform(aPreflight.Blockers, std::back_inserter(ids), &review_preflight_blocker::Conversation);
		return ids;
	}

	auto guidance(const review_preflight& aPreflight, feedback_label aLabel) -> std::optional<review_preflight_guidance>
	{
		const auto found = std::ranges::find(aPreflight.Guidance, aLabel, &review_preflight_guidance::Label);
		return found == aPreflight.Guidance.end() ? std::nullopt : std::optional{ *found };
	}

	auto mine(std::string aId, std::string aBody, feedback_resolution aResolution = feedback_resolution::unresolved) -> conversation
	{
		return { .Id = aId, .Posts = { { aId + "-root", std::move(aBody) } }, .Author = std::string{ me }, .Resolution = aResolution };
	}
	auto theirs(std::string aId, std::string aBody, feedback_resolution aResolution = feedback_resolution::unresolved) -> conversation
	{
		return { .Id = aId, .Posts = { { aId + "-root", std::move(aBody) } }, .Author = std::string{ other }, .Resolution = aResolution };
	}

	auto sample() -> std::vector<conversation>
	{
		auto reply = mine("T11", "A plain root that only a reply labels.");
		reply.Posts.emplace_back("T11-reply", "issue (blocking): only the reply says blocking.");
		return {
			mine("T01", "issue (blocking): The cache is never invalidated."),
			mine("T02", "issue (blocking): Resolved already.", feedback_resolution::resolved),
			mine("T03", "issue (blocking): GitHub did not say whether this is resolved.", feedback_resolution::unknown),
			mine("T04", "todo: Add the clamp."),
			mine("T05", "chore: Run the formatter.", feedback_resolution::not_applicable),
			mine("T06", "todo (non-blocking): Later is fine."),
			theirs("T07", "issue (blocking): Another reviewer's blocker."),
			theirs("T08", "issue (blocking: malformed, by someone else."),
			{ .Id = "T09", .Posts = { { "T09-root", "issue (blocking): Written by a deleted account." } }, .Author = std::nullopt },
			mine("T10", "issue (blocking: malformed header of mine."),
			reply,
			mine("T12", "security (blocking): A custom label declares no expectation (grammar revision 1)."),
			mine("T13", "note (blocking): An explicit decoration wins over guidance."),
			mine("T14", "issue: No decoration, and issue gives no guidance."),
			theirs("T15", "todo: Their todo is not my guidance."),
		};
	}

	auto submission_draft(std::string aBody, review_disposition aDisposition = review_disposition::comment) -> review_submission_draft
	{
		return { .Id            = review_submission_draft_id{ 1U },
				 .Target        = { .Host = git_host_id{ 1U }, .Repository = repository_id{ 2U }, .PullRequest = pull_request_id{ 3U }, .ExpectedHead = parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01").value() },
				 .ActingAccount = github_account_id{ 4U },
				 .Body          = std::move(aBody),
				 .Disposition   = aDisposition };
	}

	auto changes(const review_prepared_decision& aWas, const std::optional<review_prepared_decision>& aNow) -> std::vector<review_preflight_change>
	{
		expect(aNow.has_value(), "a drafting review prepares a decision");
		return review_decision_changes(aWas, *aNow);
	}
}

auto main() -> int
{
	// --- The four groups ---------------------------------------------------------
	const auto sampleWorld = build(sample());
	const auto drafts      = std::vector<review_preflight_draft>{
		{ .Id = "D1", .Body = "issue (blocking): Not sent yet.", .Selected = true },
		{ .Id = "D2", .Body = "todo: Also not sent.", .Selected = true },
		{ .Id = "D3", .Body = "issue (blocking): Not selected for this review.", .Selected = false },
		{ .Id = "D4", .Body = "Plain prose.", .Selected = true },
		{ .Id = "D5", .Body = "issue (blocking: malformed draft.", .Selected = true },
	};
	const auto preflight = project(sampleWorld, drafts);
	expect(blockers(preflight) == std::vector<std::string>{ "T01", "T03", "T13" }, "only my open, explicitly blocking roots are my published blockers");
	expect(preflight.Blockers[0].Label == feedback_label::issue && preflight.Blockers[0].Resolution == feedback_resolution::unresolved, "a blocker keeps its label and state");
	expect(preflight.Blockers[1].Resolution == feedback_resolution::unknown, "an unknown resolution is shown as unknown, never as open or resolved");
	expect(preflight.Blockers[2].Label == feedback_label::note, "an explicit note (blocking) is a blocker by its decoration; a custom label is not");
	expect(preflight.Pending.size() == 1U && preflight.Pending.front().Draft == "D1" && preflight.Pending.front().Label == feedback_label::issue,
		   "only a selected, explicitly blocking draft is pending; an unselected one is not read");
	const auto todo  = guidance(preflight, feedback_label::todo);
	const auto chore = guidance(preflight, feedback_label::chore);
	expect(todo && todo->Published == 1U && todo->Pending == 1U && todo->Guidance == feedback_label_guidance::necessary_change,
		   "an undecorated todo of mine is guidance, published and pending, never a blocker");
	expect(chore && chore->Published == 1U && chore->Pending == 0U && chore->Guidance == feedback_label_guidance::required_before_acceptance,
		   "a chore is guidance, apart from the blockers");
	expect(preflight.Guidance.size() == 2U, "issue, note and decorated todos give no guidance");
	expect(preflight.Others == 1U, "other reviewers' blockers are counted apart and never attributed to the submitter");
	expect(preflight.Uncertain.UnknownAuthor == 1U, "an unknown author's blocker is uncertainty, never mine and never theirs");
	expect(preflight.Uncertain.Unclassified == 2U, "my malformed root and my malformed selected draft are uncertainty");
	expect(preflight.Uncertain.ResolutionUnknown == 1U && preflight.Uncertain.Collection == feedback_collection::complete, "unknown resolution is counted as uncertainty");
	expect(!preflight.Exact && uncertain(preflight) == 4U && open_blocking(preflight) == 4U, "an uncertain preflight is never exact");

	// --- Attribution and coverage -------------------------------------------------
	{
		const auto asOther = project(sampleWorld, drafts, other);
		expect(blockers(asOther) == std::vector<std::string>{ "T07" } && asOther.Others == 3U,
			   "submitting as the other reviewer swaps the groups: their blocker is theirs, mine are others'");
		expect(guidance(asOther, feedback_label::todo)->Published == 1U && !guidance(asOther, feedback_label::chore).has_value(), "guidance follows the submitter too");
	}
	{
		const auto clean = build({ mine("T01", "praise: Nice."), theirs("T02", "issue (blocking): Theirs.") });
		const auto empty = project(clean);
		expect(empty.Blockers.empty() && empty.Pending.empty() && empty.Guidance.empty() && empty.Exact && uncertain(empty) == 0U && empty.Others == 1U,
			   "a complete population with none of my blockers is an exact zero");
		for (const auto collection : { feedback_collection::partial, feedback_collection::stale, feedback_collection::unavailable }) {
			const auto limited = project(build({ mine("T01", "praise: Nice.") }, collection));
			expect(!limited.Exact && limited.Uncertain.Collection == collection, "an incomplete collection is never a clean zero");
		}
		const auto missing = project(build({ }, feedback_collection::unavailable));
		expect(missing.Blockers.empty() && !missing.Exact, "an unread pull request is not \"no blockers\"");
	}

	// --- No verdict from context ---------------------------------------------------
	{
		// Resolving my last blocker or relabelling feedback changes the context
		// only; the preflight has no disposition to give, and a decision
		// prepared before the change is exactly as valid after it.
		const auto draft    = submission_draft("Looks good.", review_disposition::approve);
		const auto selected = std::vector<review_preflight_draft>{ { .Id = "D1", .Body = "praise: Nice.", .Selected = true } };
		const auto before   = project(build({ mine("T01", "issue (blocking): Open.") }), selected);
		const auto prepared = prepare_review_decision(draft, selected).value();
		const auto after    = project(build({ mine("T01", "issue (blocking): Open.", feedback_resolution::resolved), theirs("T02", "issue (blocking): New.") }), selected);
		expect(open_blocking(before) == 1U && open_blocking(after) == 0U && after.Others == 1U, "the last resolution clears my blockers");
		expect(changes(prepared, prepare_review_decision(draft, selected)).empty(), "a resolution or another reviewer's label invalidates no prepared decision");
		expect(prepared.Disposition == review_disposition::approve, "and leaves the explicit decision as chosen");
	}

	// --- Prepared decisions ---------------------------------------------------------
	{
		const auto selection = std::vector<review_preflight_draft>{
			{ .Id = "D1", .Body = "issue (blocking): First.", .Selected = true },
			{ .Id = "D2", .Body = "todo: Second.", .Selected = true },
			{ .Id = "D3", .Body = "note: Not selected.", .Selected = false },
		};
		for (const auto disposition : { review_disposition::comment, review_disposition::approve, review_disposition::request_changes }) {
			const auto draft    = submission_draft("Summary.", disposition);
			const auto prepared = prepare_review_decision(draft, selection).value();
			expect(prepared.Disposition == disposition && prepared.Target == draft.Target && prepared.Account == draft.ActingAccount,
				   "each native decision is prepared with its exact disposition, target and account");
			expect(changes(prepared, prepare_review_decision(draft, selection)).empty(), "an unchanged decision stays valid");

			auto summary = draft;
			summary.Body = "issue (blocking): Summary.";
			expect(changes(prepared, prepare_review_decision(summary, selection)) == std::vector{ review_preflight_change::summary },
				   "a summary or summary label edit invalidates only through the summary");
			auto chosen        = draft;
			chosen.Disposition = disposition == review_disposition::approve ? review_disposition::comment : review_disposition::approve;
			expect(changes(prepared, prepare_review_decision(chosen, selection)) == std::vector{ review_preflight_change::disposition },
				   "another decision is another prepared action");

			auto relabelled    = selection;
			relabelled[1].Body = "todo (blocking): Second.";
			expect(changes(prepared, prepare_review_decision(draft, relabelled)) == std::vector{ review_preflight_change::selection },
				   "a selected draft's label edit invalidates through the selection");
			auto deselected        = selection;
			deselected[0].Selected = false;
			expect(changes(prepared, prepare_review_decision(draft, deselected)) == std::vector{ review_preflight_change::selection },
				   "deselecting a draft invalidates through the selection");
			auto unselected    = selection;
			unselected[2].Body = "issue (blocking): Still not selected.";
			unselected.push_back({ .Id = "D4", .Body = "issue (blocking): New and not selected.", .Selected = false });
			expect(changes(prepared, prepare_review_decision(draft, unselected)).empty(), "an unselected draft's edit leaves the decision valid");
			auto reordered = selection;
			std::ranges::reverse(reordered);
			expect(changes(prepared, prepare_review_decision(draft, reordered)).empty(), "the selection is a set, not an order");

			auto moved                = draft;
			moved.Target.ExpectedHead = parse_git_commit_oid("0123456789abcdef0123456789abcdef01234567").value();
			expect(changes(prepared, prepare_review_decision(moved, selection)) == std::vector{ review_preflight_change::head }, "a new head invalidates");
			auto account          = draft;
			account.ActingAccount = github_account_id{ 5U };
			expect(changes(prepared, prepare_review_decision(account, selection)) == std::vector{ review_preflight_change::account }, "another account invalidates");
			auto elsewhere               = draft;
			elsewhere.Target.PullRequest = pull_request_id{ 9U };
			expect(changes(prepared, prepare_review_decision(elsewhere, selection)) == std::vector{ review_preflight_change::target }, "another pull request invalidates");
			auto renewed = draft;
			renewed.Id   = review_submission_draft_id{ 2U };
			expect(changes(prepared, prepare_review_decision(renewed, selection)) == std::vector{ review_preflight_change::draft }, "another review draft invalidates");

			auto submitting = apply(draft, { .Id = draft.Id, .Kind = review_submission_command_kind::begin_submit }).Draft.value();
			expect(!prepare_review_decision(submitting, selection).has_value(), "a submitting attempt is bound already and prepares nothing new");
		}
	}

	// --- Refusals -----------------------------------------------------------------
	{
		const auto refused = [&](const review_preflight& aPreflight, review_preflight_refusal aRefusal) {
			return aPreflight.Refusal == aRefusal && aPreflight.Blockers.empty() && !aPreflight.Exact;
		};
		expect(refused(project_review_preflight("", sampleWorld.Annotations, sampleWorld.Population, sampleWorld.Authors, { }), review_preflight_refusal::invalid_submitter),
			   "no submitter, no preflight");
		auto stale = sampleWorld.Population;
		++stale.Epoch;
		expect(refused(project_review_preflight(me, sampleWorld.Annotations, stale, sampleWorld.Authors, { }), review_preflight_refusal::mismatched_population),
			   "a population from another annotation epoch is refused");
		auto twice = sampleWorld.Authors;
		twice.push_back({ .Conversation = "T01", .RootAuthor = std::string{ other } });
		expect(refused(project_review_preflight(me, sampleWorld.Annotations, sampleWorld.Population, twice, { }), review_preflight_refusal::invalid_identity),
			   "one conversation with two authors is refused, never guessed");
		const auto repeated = std::vector<review_preflight_draft>{ { .Id = "D1", .Body = "a", .Selected = true }, { .Id = "D1", .Body = "b", .Selected = true } };
		expect(refused(project_review_preflight(me, sampleWorld.Annotations, sampleWorld.Population, sampleWorld.Authors, repeated), review_preflight_refusal::invalid_identity),
			   "one draft with two bodies is refused");
	}

	std::cout << "REVIEW_PREFLIGHT_OK\n";
	return EXIT_SUCCESS;
}
