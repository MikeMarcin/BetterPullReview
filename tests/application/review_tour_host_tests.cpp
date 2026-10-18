#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_tour_host.hpp"
#include <stdexcept>
namespace {
	using namespace bpr;
	using tests::expect;
	struct store_state
	{
		std::optional<core::review_tour> Draft;
		bool                             Fail  = false;
		bool                             Throw = false;
		unsigned                         Saves = 0;
	};
	class store final : public application::review_tour_store
	{
	public:
		std::shared_ptr<store_state> State;
		explicit store(std::shared_ptr<store_state> aState)
			: State{ std::move(aState) }
		{ }
		auto load(const core::tour_owner&, core::tour_draft_id, std::stop_token) -> application::tour_store_result<std::optional<core::review_tour>> override { return State->Draft; }
		auto save(const core::review_tour& aDraft, std::uint64_t aExpected, std::stop_token) -> application::tour_store_result<core::review_tour> override
		{
			++State->Saves;
			if (State->Throw) {
				throw std::runtime_error{ "fixture" };
			}
			if (State->Fail) {
				return std::unexpected(application::tour_store_error{ });
			}
			if ((State->Draft ? State->Draft->Revision : 0) != aExpected) {
				return std::unexpected(application::tour_store_error{ application::tour_store_failure::conflict, State->Draft });
			}
			State->Draft = aDraft;
			return aDraft;
		}
	};
}
int main()
{
	auto                          shared = std::make_shared<store_state>();
	application::manual_executor  effects, decisions;
	unsigned                      publications = 0;
	std::function<void()>         feedback;
	application::review_tour_host host{ tests::review_tour_fixture(), std::make_unique<store>(shared), effects, decisions, [&](const auto&) {++publications;if(feedback){auto action=std::move(feedback);feedback={};action();} } };
	host.start();
	expect(host.presentation().Loading && !host.presentation().Draft, "load asynchronous");
	effects.drain();
	decisions.drain();
	feedback = [&] { host.dispatch(tests::tour_edit(*host.presentation().Draft, "reentrant final")); };
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "first"));
	expect(shared->Saves == 0 && effects.pending() == 1, "edits enqueue effects only");
	effects.drain();
	decisions.drain();
	effects.drain();
	decisions.drain();
	expect(shared->Saves == 2 && shared->Draft->Problem == "reentrant final", "coalesced reentrant final text saved");
	const auto selectedBefore = host.presentation().Draft->Stops.front().Id;
	const auto beforeInsert   = *host.presentation().Draft;
	host.dispatch({ .Draft = beforeInsert.Id, .Owner = beforeInsert.Owner, .ExpectedRevision = beforeInsert.Revision, .Kind = core::tour_command_kind::insert, .Stop = selectedBefore, .Position = 1 });
	const auto inserted                   = host.presentation().Selected;
	const auto insertionSelectionRevision = host.presentation().SelectionRevision;
	expect(inserted != selectedBefore && host.presentation().Draft->Stops[1].Id == inserted, "insert projects newly allocated selection");
	host.undo();
	expect(host.presentation().Draft->Stops == beforeInsert.Stops && host.presentation().Selected == selectedBefore && host.presentation().SelectionRevision > insertionSelectionRevision, "undo restores prior current stop and selection revision");
	host.redo();
	expect(host.presentation().Selected == inserted && host.presentation().Draft->Stops[1].Id == inserted && host.presentation().SelectionRevision > insertionSelectionRevision + 1, "redo reselects original inserted identity");
	host.undo();
	effects.drain();
	decisions.drain();
	effects.drain();
	decisions.drain();
	shared->Fail = true;
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "unsaved"));
	effects.drain();
	decisions.drain();
	expect(host.presentation().Failure && host.presentation().SavedRevision < host.presentation().Draft->Revision, "save failure visible and never durable success");
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "still editable"));
	expect(effects.pending() == 0, "failed save is not a retry spin");
	shared->Fail = false;
	host.retry_save();
	effects.drain();
	decisions.drain();
	expect(shared->Draft->Problem == "still editable", "explicit retry retains last edits");
	++shared->Draft->Revision;
	shared->Draft->Problem = "other window";
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "my conflict"));
	effects.drain();
	decisions.drain();
	host.dispatch(tests::tour_edit(*host.presentation().Draft, std::string(core::maximum_tour_field_bytes + 1U, 'x')));
	expect(host.presentation().Conflict && host.presentation().Refusal == core::tour_refusal::bounds, "conflicted editor also retains a refused oversized candidate");
	host.use_saved_copy();
	expect(!host.presentation().Conflict && host.presentation().Refusal == core::tour_refusal::none && host.presentation().Draft->Problem == "other window", "explicit discard adopts the saved copy and clears the old refusal");
	shared->Throw = true;
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "exception"));
	effects.drain();
	decisions.drain();
	expect(host.is_quiescent() && host.presentation().Failure, "throw produces terminal failure");
	shared->Throw = false;
	host.retry_save();
	const auto before = publications;
	host.shutdown();
	effects.drain();
	decisions.drain();
	expect(host.is_quiescent() && publications == before && shared->Draft->Problem == "exception", "shutdown drains without late projection");
	host.dispatch(tests::tour_edit(*host.presentation().Draft, "after shutdown"));
	expect(shared->Draft->Problem == "exception", "closed host refuses edits");
	{
		auto                          firstStore  = std::make_shared<store_state>();
		auto                          secondStore = std::make_shared<store_state>();
		application::review_tour_host desktop{ tests::review_tour_fixture(), std::make_unique<store>(firstStore), effects, decisions, { } };
		application::review_tour_host terminal{ tests::review_tour_fixture(), std::make_unique<store>(secondStore), effects, decisions, { } };
		desktop.start();
		terminal.start();
		effects.drain();
		decisions.drain();
		const auto command = tests::tour_edit(*desktop.presentation().Draft, "same literal author edit");
		desktop.dispatch(command);
		const auto result = terminal.execute({ .Command = command });
		expect(result.Status == application::tour_service_status::accepted && result.Revision == command.ExpectedRevision + 1, "shared facade reports local admission, not publication");
		effects.drain();
		decisions.drain();
		expect(firstStore->Draft == secondStore->Draft && terminal.presentation().SavedRevision == result.Revision, "desktop and terminal semantic commands persist identical parent tour bytes");
		const auto saves = secondStore->Saves;
		expect(terminal.execute({ .Command = command }).Refusal == core::tour_refusal::conflict, "stale terminal command refuses");
		auto wrong          = tests::tour_edit(*terminal.presentation().Draft, "wrong owner");
		wrong.Owner.Account = core::github_account_id{ wrong.Owner.Account.value() + 1 };
		expect(terminal.execute({ .Command = wrong }).Refusal == core::tour_refusal::identity && secondStore->Saves == saves, "another client cannot redirect the exact target");
		desktop.shutdown();
		terminal.shutdown();
		effects.drain();
		decisions.drain();
	}
	{
		auto empty = tests::review_tour_fixture();
		empty.Problem.clear();
		empty.Solution.clear();
		empty.Stops.clear();
		empty.ProblemBaseline.reset();
		empty.SolutionBaseline.reset();
		empty.Revision                          = 1;
		auto                          untouched = std::make_shared<store_state>();
		application::review_tour_host observer{ empty, std::make_unique<store>(untouched), effects, decisions, { } };
		observer.start();
		effects.drain();
		decisions.drain();
		observer.shutdown();
		effects.drain();
		decisions.drain();
		expect(observer.is_quiescent() && untouched->Saves == 0, "opening a PR without authoring does not consume a durable tour slot");
	}
	{
		// [[BPR-1062]]. Another presentation's structural edit does not move this author's selection.
		auto                          shared2 = std::make_shared<store_state>();
		application::review_tour_host desktop{ tests::review_tour_fixture(), std::make_unique<store>(shared2), effects, decisions, { } };
		desktop.start();
		effects.drain();
		decisions.drain();
		const auto first = desktop.presentation().Draft->Stops.front().Id;
		const auto seed  = *desktop.presentation().Draft;
		expect(desktop.execute({ .Command = { .Draft = seed.Id, .Owner = seed.Owner, .ExpectedRevision = seed.Revision, .Kind = core::tour_command_kind::insert, .Stop = first, .Position = 1 } }).Status == application::tour_service_status::accepted, "the author adds a second stop");
		const auto second = desktop.presentation().Selected;
		expect(second != first, "the author's own gesture selects the stop it added");
		desktop.dispatch({ .Draft = seed.Id, .Owner = seed.Owner, .ExpectedRevision = desktop.presentation().Draft->Revision, .Kind = core::tour_command_kind::visit, .Stop = first });
		const auto                        chosen    = desktop.presentation().Selected;
		const auto                        selection = desktop.presentation().SelectionRevision;
		const auto                        draft     = *desktop.presentation().Draft;
		application::tour_service_request move{ .Command = { .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = draft.Revision, .Kind = core::tour_command_kind::move, .Stop = second, .Position = 0, .Selection = { second } }, .Origin = application::tour_presentation_origin::command_client };
		expect(desktop.execute(move).Status == application::tour_service_status::accepted && desktop.presentation().Draft->Stops.front().Id == second, "a command client's reorder applies");
		expect(desktop.presentation().Selected == chosen && desktop.presentation().SelectionRevision == selection, "the author stays on the stop it was on, and no selection change is announced");
		auto removal = application::tour_service_request{ .Command = { .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = desktop.presentation().Draft->Revision, .Kind = core::tour_command_kind::remove, .Stop = chosen, .Selection = { chosen } }, .Origin = application::tour_presentation_origin::command_client };
		expect(desktop.execute(removal).Status == application::tour_service_status::accepted && desktop.presentation().Selected != chosen, "removing the author's stop moves the selection off it");
		desktop.shutdown();
		effects.drain();
		decisions.drain();
	}
	{
		auto                          shared3 = std::make_shared<store_state>();
		application::review_tour_host deduplicating{ tests::review_tour_fixture(), std::make_unique<store>(shared3), effects, decisions, { } };
		deduplicating.start();
		effects.drain();
		decisions.drain();
		auto request = [&](std::string aText, std::string aId, std::uint8_t aDigest) {
			application::tour_service_request result{ .Command = tests::tour_edit(*deduplicating.presentation().Draft, std::move(aText)), .RequestId = std::move(aId) };
			result.Digest.fill(aDigest);
			return result;
		};
		const auto first               = deduplicating.execute(request("deduplicated", "same-id", 1));
		auto       again               = request("deduplicated", "same-id", 1);
		again.Command.ExpectedRevision = first.Revision - 1U;
		const auto replay              = deduplicating.execute(again);
		expect(first.Status == application::tour_service_status::accepted && replay.Replayed && replay.Revision == first.Revision && deduplicating.presentation().Draft->Revision == first.Revision, "the same ID and payload replays the stored outcome, never a second edit or a stale-revision refusal");
		expect(deduplicating.execute(request("another", "same-id", 2)).Refusal == core::tour_refusal::identity, "the same ID with another payload is refused");
		expect(deduplicating.execute(request("deduplicated", "fresh-id", 3)).Status == application::tour_service_status::unchanged, "a new ID with unchanged content is a recorded no-op");
		deduplicating.shutdown();
		effects.drain();
		decisions.drain();
	}
	{
		// [[BPR-1064]]. One ticket, one exactly bounded edit, and every way an answer can be stale.
		auto                          shared4 = std::make_shared<store_state>();
		application::review_tour_host assisted{ tests::review_tour_fixture(), std::make_unique<store>(shared4), effects, decisions, { } };
		assisted.start();
		effects.drain();
		decisions.drain();
		const std::int64_t now   = 1000;
		const auto         drain = [&] {
			effects.drain();
			decisions.drain();
		};
		const auto request = [&](std::string aId, std::uint8_t aDigest, application::tour_assist_completion aCompletion) {
			const auto                        draft = assisted.presentation().Draft;
			application::tour_service_request result{ .Command = { .Draft = draft->Id, .Owner = draft->Owner }, .Operation = application::tour_service_operation::assist_complete, .Origin = application::tour_presentation_origin::command_client, .RequestId = std::move(aId), .Assist = std::move(aCompletion) };
			result.Digest.fill(aDigest);
			return result;
		};
		const auto rewrite = [](std::string aText) { return "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"rewrite\",\"status\":\"ok\",\"replacement_text\":\"" + aText + "\",\"notes\":[]}"; };
		const auto ticket  = [&](core::writing_hook aHook, std::size_t aBegin, std::size_t aEnd, core::writing_scope aScope, std::vector<application::writing_source> aSources = { }) {
			const auto                      draft = assisted.presentation().Draft;
			application::tour_assist_ticket t{ .Hook = aHook, .Scope = aScope, .Begin = aBegin, .End = aEnd, .DraftRevision = draft->Revision, .HookRevision = 7, .Sources = std::move(aSources), .Session = "trace" };
			t.RawDigest = core::writing_digest(aHook == core::writing_hook::solution ? draft->Solution : draft->Problem);
			return t;
		};
		const std::string raw = "A caf\xc3\xa9 \xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb\r\nsecond caf\xc3\xa9 line, second caf\xc3\xa9.\n";
		expect(assisted.execute({ .Command = tests::tour_edit(*assisted.presentation().Draft, raw) }).Status == application::tour_service_status::accepted, "the author's own text");
		drain();
		// A selection: only those bytes change, as one undoable transaction that undo restores exactly.
		const auto begin  = raw.find("line");
		const auto issued = assisted.assist_issue(ticket(core::writing_hook::problem, begin, begin + 4, core::writing_scope::selection), now);
		expect(issued.has_value() && issued->Id != 0 && issued->Expires == now + 1800, "a ticket is issued for the exact selection");
		const auto revisionBefore = assisted.presentation().Draft->Revision;
		const auto done           = assisted.execute(request("assist-1", 1, { .Id = issued->Id, .ResultJson = rewrite("row"), .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(done.Status == application::tour_service_status::accepted && done.Revision == revisionBefore + 1, "a valid result applies at once, with no acceptance step");
		expect(assisted.presentation().Draft->Problem == raw.substr(0, begin) + "row" + raw.substr(begin + 4) && assisted.presentation().UndoLabel == "Rewrite Problem", "only the selected bytes changed, including CRLF and emoji outside them");
		assisted.undo();
		expect(assisted.presentation().Draft->Problem == raw && assisted.presentation().Draft->Revision == revisionBefore + 2, "undo restores the exact bytes with a new revision");
		drain();
		expect(assisted.execute(request("assist-1", 1, { .Id = issued->Id, .ResultJson = rewrite("row"), .Now = now + 1, .CurrentHookRevision = 7 })).Replayed, "a lost answer is replayed from the record, not applied again");
		expect(assisted.execute(request("assist-2", 2, { .Id = issued->Id, .ResultJson = rewrite("row"), .Now = now + 1, .CurrentHookRevision = 7 })).Reason == "unknown_ticket", "a spent ticket cannot be used twice");
		// Whole field, then a repeated word is the caller's job to disambiguate; here the field scope covers it all.
		auto whole = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		expect(whole.has_value(), "a whole-field ticket");
		auto noop = assisted.execute(request("assist-3", 3, { .Id = whole->Id, .ResultJson = rewrite("A caf\xc3\xa9 \xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb\\r\\nsecond caf\xc3\xa9 line, second caf\xc3\xa9.\\n"), .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(noop.Status == application::tour_service_status::unchanged && noop.Reason == "unchanged", "an identical result is a no-op that adds no history");
		// Stale in every way an editor can move under an outstanding request.
		auto stale = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		expect(assisted.execute({ .Command = tests::tour_edit(*assisted.presentation().Draft, raw + "more") }).Status == application::tour_service_status::accepted, "the author types while a request is outstanding");
		expect(assisted.execute({ .Command = tests::tour_edit(*assisted.presentation().Draft, raw) }).Status == application::tour_service_status::accepted, "and types back to identical bytes");
		auto old = assisted.execute(request("assist-4", 4, { .Id = stale->Id, .ResultJson = rewrite("late"), .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(old.Status == application::tour_service_status::refused && old.Reason == "stale" && assisted.presentation().Draft->Problem == raw, "an answer to old bytes is refused even after the text returns to the same bytes");
		auto moved = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		expect(assisted.execute(request("assist-5", 5, { .Id = moved->Id, .ResultJson = rewrite("hook"), .Now = now + 1, .CurrentHookRevision = 8 })).Reason == "stale", "changed hook instructions refuse an answer to the old ones");
		auto slow = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		expect(assisted.execute(request("assist-6", 6, { .Id = slow->Id, .ResultJson = rewrite("late"), .Now = now + 1801, .CurrentHookRevision = 7 })).Reason == "expired", "an old request expires");
		auto first  = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		auto second = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		expect(assisted.execute(request("assist-7", 7, { .Id = first->Id, .ResultJson = rewrite("old"), .Now = now + 1, .CurrentHookRevision = 7 })).Reason == "unknown_ticket" && assisted.assist_ticket(second->Id).has_value(), "a newer request supersedes the older one");
		auto bad = assisted.execute(request("assist-8", 8, { .Id = second->Id, .ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"rewrite\",\"status\":\"ok\",\"replacement_text\":\"x\",\"notes\":[],\"target\":\"stop:9\"}", .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(bad.Status == application::tour_service_status::refused && bad.Reason == "invalid_result" && !assisted.assist_ticket(second->Id).has_value() && assisted.presentation().Draft->Problem == raw, "a result that names a target or adds fields is refused and spends the request");
		auto insufficient = assisted.assist_issue(ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field), now);
		auto more         = assisted.execute(request("assist-9", 9, { .Id = insufficient->Id, .ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"rewrite\",\"status\":\"insufficient_context\",\"reason\":\"Need the failing case.\"}", .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(more.Reason == "insufficient_context" && more.Detail == "Need the failing case." && assisted.presentation().Draft->Problem == raw, "insufficient context is reported and changes nothing");
		expect(!assisted.assist_issue(ticket(core::writing_hook::problem, 3, 3, core::writing_scope::selection), now) && !assisted.assist_issue(ticket(core::writing_hook::problem, raw.find("\r\n") + 1, raw.find("\r\n") + 1 + 3, core::writing_scope::selection), now), "an empty selection or one inside a CRLF pair is not a ticket");
		auto changed = ticket(core::writing_hook::problem, 0, raw.size(), core::writing_scope::field);
		changed.DraftRevision += 1;
		expect(assisted.assist_issue(changed, now).error() == "stale", "a ticket for a revision the draft has not reached is refused");
		// An outline is a body-free proposal; choosing from it inserts suggested stops, once.
		application::writing_source anchor{ "selected-source", "code", assisted.presentation().Draft->Stops.front().Original };
		auto                        outline     = assisted.assist_issue(ticket(core::writing_hook::outline, 0, 0, core::writing_scope::field, { anchor }), now);
		const std::string           outlineJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"outline\",\"stops\":[{\"title\":\"Boundary\",\"anchor_ref\":\"selected-source\",\"related_refs\":[],\"reason\":\"Start here\",\"author_question\":\"Why?\"},{\"title\":\"Limits\",\"anchor_ref\":\"selected-source\",\"related_refs\":[],\"reason\":\"Then this\",\"author_question\":\"\"}]}";
		auto                        proposed    = assisted.execute(request("assist-10", 10, { .Id = outline->Id, .ResultJson = outlineJson, .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(proposed.Status == application::tour_service_status::unchanged && proposed.Proposal != 0 && assisted.assist_proposal(proposed.Proposal)->Entries.size() == 2, "an outline result is a proposal, not an edit");
		const auto                        stopsBefore = assisted.presentation().Draft->Stops.size();
		application::tour_service_request use{ .Command = { .Draft = assisted.presentation().Draft->Id, .Owner = assisted.presentation().Draft->Owner }, .Operation = application::tour_service_operation::outline_use, .Origin = application::tour_presentation_origin::command_client, .RequestId = "use-1", .Assist = { .Id = proposed.Proposal, .Now = now + 2, .Indices = { 1 }, .Position = stopsBefore } };
		use.Digest.fill(11);
		auto insertion = assisted.execute(use);
		expect(insertion.Status == application::tour_service_status::accepted && assisted.presentation().Draft->Stops.size() == stopsBefore + 1, "the chosen suggestion is inserted");
		const auto& added = assisted.presentation().Draft->Stops.back();
		expect(added.Title == "Limits" && added.Body.empty() && added.Preparation == core::tour_preparation::suggested && added.Suggestion && !added.Suggestion->Visited, "it is body-free, suggested and unvisited");
		auto again      = use;
		again.RequestId = "use-2";
		again.Digest.fill(12);
		expect(assisted.execute(again).Reason == "unknown_proposal", "a proposal is used once");
		auto badOutline = assisted.assist_issue(ticket(core::writing_hook::outline, 0, 0, core::writing_scope::field, { anchor }), now);
		auto withBody   = assisted.execute(request("assist-11", 13, { .Id = badOutline->Id, .ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"outline\",\"stops\":[{\"title\":\"T\",\"anchor_ref\":\"selected-source\",\"related_refs\":[],\"reason\":\"r\",\"author_question\":\"\",\"body\":\"written\",\"ready\":true}]}", .Now = now + 1, .CurrentHookRevision = 7 }));
		expect(withBody.Status == application::tour_service_status::refused && withBody.Reason == "invalid_result", "an outline that carries a comment body or a ready flag is refused");
		assisted.shutdown();
		drain();
	}
}
