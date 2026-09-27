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
}
