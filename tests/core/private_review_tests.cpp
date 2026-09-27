#include "private_review_fixture.hpp"
#include <type_traits>

namespace {
	using namespace private_review_fixture;
	void replay()
	{
		const auto first = created();
		auto       next  = input(3);
		auto       reply = append_private_turn(first.Thread, 1, next, &first.Revision);
		expect(reply && reply->Revision.ReplyTo == first.Revision.Identity, "reply retains exact revision");
		next       = input(5);
		next.Actor = private_agent_actor{ agent_run_id{ 1 } };
		private_run_authorization run{ binding().Scope, agent_run_id{ 1 }, private_vertex_order{ 4 }, binding(), private_revision_completion::complete, private_digest(next.Body) };
		const auto                assistant = append_private_turn(reply->Thread, 2, next, &reply->Revision, &run);
		expect(assistant && eligible_private_revision(assistant->Revision), "complete run creates one eligible agent revision");
		const auto original = assistant->Revision;
		next                = input(6);
		next.Turn           = original.Identity.Turn;
		next.Body           = "reviewer correction";
		const auto edited   = edit_private_revision(assistant->Thread, 3, original, original.Identity.Revision, next);
		expect(edited && std::holds_alternative<private_reviewer_actor>(edited->Revision.Actor), "editing transfers no authorship");
		expect(edited->Revision.Predecessor == original.Identity && edited->Revision.EditDerivation == private_derivation_kind::edited_from, "edit preserves exact predecessor");
		expect(original == assistant->Revision && original.Body == "reviewer text", "original bytes actor and digest untouched");
		expect(!edit_private_revision(assistant->Thread, 2, original, original.Identity.Revision, next), "old header CAS refuses");
		expect(!edit_private_revision(assistant->Thread, 3, original, private_review_turn_revision_id{ 99 }, next), "old tip CAS refuses");
		auto changed             = binding();
		changed.Revision.HeadOid = changed.Revision.BaseOid;
		expect(observe_private_subject(first.Thread.Binding, changed, true) == private_subject_availability::stale, "head changes stale without reanchor");
		expect(observe_private_subject(first.Thread.Binding, changed, false) == private_subject_availability::unplaceable, "unavailable exact subject stays unplaceable");
		expect(first.Thread == created().Thread, "replay is deterministic and observation does not mutate");
		auto archived = set_private_lifecycle(edited->Thread, 4, private_thread_lifecycle::archived);
		expect(archived && !append_private_turn(*archived, 5, input(7)), "archive blocks append without deletion");
		const auto reopened = set_private_lifecycle(*archived, 5, private_thread_lifecycle::open);
		expect(reopened && append_private_turn(*reopened, 6, input(7)), "explicit reopen allows continued private conversation");
	}
	void subjects()
	{
		const auto  semantic  = index();
		const auto& alignment = semantic.diff().Alignment;
		auto        bound     = binding();
		bound.Comparison      = alignment.Digest;
		const std::array<private_thread_subject, 7> subjects{
			private_file_subject{ alignment.Key.File, alignment.Digest }, source_line_ref{ alignment.Digest, diff_side::right, 1 },
			source_boundary_ref{ alignment.Digest, diff_side::left, 3 }, source_span_ref{ alignment.Digest, diff_side::right, { 0, 3 } },
			private_group_subject{ alignment.Digest, alignment.Groups.front().Id }, private_segment_subject{ alignment.Digest, alignment.Segments.front().Id },
			private_pr_subject{ }
		};
		for (const auto& subject : subjects) {
			expect(create_private_thread(private_review_thread_id{ 1 }, { bound, subject }, input(), &semantic).has_value(), "all exact subject families admitted through index");
		}
		expect(!valid_private_subject({ bound, source_line_ref{ alignment.Digest, diff_side::right, 9 } }, &semantic), "out of range never snaps to nearest line");
		expect(!valid_private_subject({ bound, source_line_ref{ alignment.Digest, diff_side::right, 1 } }), "no unindexed semantic bypass");
		expect(!valid_private_subject({ bound, private_move_subject{ alignment.Digest, opaque<move_relation_id>(99) } }, &semantic), "missing move refused");
		auto other       = bound;
		other.Comparison = opaque<file_comparison_digest>(99);
		expect(!valid_private_subject({ other, subjects[1] }, &semantic), "different comparison refuses");
		other                  = bound;
		other.LeftAnchor.Epoch = epoch_ordinal{ 99 };
		expect(!valid_private_subject({ other, subjects[1] }, &semantic), "epoch binding cannot be relabeled");
		expect(!valid_private_subject({ bound, std::monostate{ } }, &semantic), "null is not PR-wide");
		expect(valid_private_subject({ bound, review_conversation_id{ 4 } }), "typed shared conversation subject");
		expect(valid_private_subject({ bound, private_finding_reference{ agent_run_id{ 2 }, 0, private_digest("finding") } }), "zero-based finding ordinal and exact run identity");
	}
	void completion_and_bounds()
	{
		const auto first = created();
		for (auto completion : { private_revision_completion::incomplete, private_revision_completion::failed, private_revision_completion::cancelled }) {
			auto next        = input(3);
			next.Actor       = private_agent_actor{ agent_run_id{ 1 } };
			next.Completion  = completion;
			auto       run   = private_run_authorization{ binding().Scope, agent_run_id{ 1 }, private_vertex_order{ 2 }, binding(), completion, private_digest(next.Body) };
			const auto agent = append_private_turn(first.Thread, 1, next, nullptr, &run);
			expect(agent && !eligible_private_revision(agent->Revision), "partial failed cancelled output stays ineligible");
			auto adoptedInput = input(4);
			adoptedInput.Turn = next.Turn;
			expect(!edit_private_revision(agent->Thread, 2, agent->Revision, next.Revision, adoptedInput), "implicit partial adoption refuses");
			auto adopted = edit_private_revision(agent->Thread, 2, agent->Revision, next.Revision, adoptedInput, true);
			expect(adopted && eligible_private_revision(adopted->Revision) && adopted->Revision.EditDerivation == private_derivation_kind::adopted_from_incomplete_run, "explicit adoption retains incomplete-run origin");
			run.Run = agent_run_id{ 2 };
			expect(!append_private_turn(first.Thread, 1, next, nullptr, &run), "another run cannot supply actor");
		}
		auto next = input(2);
		next.Body.assign(maximum_private_reviewer_body_bytes, 'x');
		expect(append_private_turn(first.Thread, 1, next).has_value(), "reviewer byte ceiling inclusive");
		next.Body.push_back('x');
		expect(!append_private_turn(first.Thread, 1, next), "oversize refuses whole");
		for (const auto id : { std::uint64_t{ 0 }, maximum_private_identity + 1, std::numeric_limits<std::uint64_t>::max() }) {
			expect(!create_private_thread(private_review_thread_id{ id }, { binding(), private_pr_subject{ } }, input()), "invalid durable ID refused");
		}
		auto legacyInput  = input();
		legacyInput.Actor = private_legacy_unknown_actor{ };
		legacyInput.Body  = std::string{ "a\0b\r\n", 5 };
		auto legacy       = migrate_private_thread(private_review_thread_id{ 1 }, { binding().Scope, { } }, legacyInput);
		expect(legacy && legacy->Revision.Body == legacyInput.Body && std::holds_alternative<private_legacy_unknown_actor>(legacy->Revision.Actor), "legacy bytes preserved without inventing author");
		expect(observe_private_subject(legacy->Thread.Binding, binding(), true) == private_subject_availability::unplaceable, "legacy unknown binding never borrows current revision");
		expect(!create_private_thread(private_review_thread_id{ 1 }, { binding(), private_pr_subject{ } }, legacyInput), "new text cannot claim legacy actor");
		auto invalid = first.Revision;
		invalid.Body += "tamper";
		expect(!valid_private_revision(invalid), "body digest checked");
		auto foreign                       = first.Revision;
		foreign.Identity.Scope.Incarnation = opaque<private_ledger_incarnation>(9);
		expect(!append_private_turn(first.Thread, 1, input(2), &foreign), "recreated ledger source refused");
	}
	void lifecycle_properties()
	{
		for (auto from : { private_thread_lifecycle::open, private_thread_lifecycle::resolved_locally, private_thread_lifecycle::archived }) {
			for (auto to : { private_thread_lifecycle::open, private_thread_lifecycle::resolved_locally, private_thread_lifecycle::archived }) {
				auto thread         = created().Thread;
				thread.Lifecycle    = from;
				const auto original = thread;
				const auto result   = set_private_lifecycle(thread, 1, to);
				expect(result && result->Lifecycle == to && result->Version == (from == to ? 1U : 2U), "complete 3x3 local lifecycle transition table");
				expect(thread == original && result->Binding == thread.Binding, "every lifecycle transition preserves source binding");
			}
		}
		auto first = created();
		for (std::uint64_t ordinal = 2; ordinal < 100; ++ordinal) {
			auto next   = input(ordinal);
			next.Turn   = first.Revision.Identity.Turn;
			auto edited = edit_private_revision(first.Thread, first.Thread.Version, first.Revision, first.Revision.Identity.Revision, next);
			expect(edited && edited->Thread.LastOrder == edited->Revision.Identity.Order && edited->Revision.Identity.Order > first.Revision.Identity.Order, "replayed successors strictly ordered");
			expect(!edit_private_revision(first.Thread, first.Thread.Version, first.Revision, first.Revision.Identity.Revision, input(1)), "backwards edge refused");
			first = *edited;
		}
	}
	void terminals()
	{
		static_assert(!std::is_same_v<private_review_thread_id, private_review_turn_id>);
		static_assert(sizeof(private_review_state) < 2048);
		auto state   = private_review_state{ .Scope = binding().Scope };
		auto context = private_request_context{ state.Scope, application_session_id{ 1 }, { state.Scope.Profile, bpr_window_id{ 1 }, bpr_window_generation{ 1 } }, github_identity_generation{ 1 }, request_id{ 1 }, { } };
		auto first   = begin_private_request(state, private_request_lane::transcript, context);
		auto second  = begin_private_request(state, private_request_lane::transcript, context);
		expect(first && second && !finish_private_request(state, private_request_lane::transcript, *first), "superseded terminal rejected");
		auto wrong              = *second;
		wrong.Window.Generation = bpr_window_generation{ 2 };
		expect(!finish_private_request(state, private_request_lane::transcript, wrong), "foreign window terminal rejected");
		expect(finish_private_request(state, private_request_lane::transcript, *second) && !finish_private_request(state, private_request_lane::transcript, *second), "matching terminal consumed exactly once");
		state.Generations[1] = maximum_private_identity;
		expect(!begin_private_request(state, private_request_lane::transcript, context), "generation cannot wrap");
	}
}
int main()
{
	replay();
	subjects();
	completion_and_bounds();
	lifecycle_properties();
	terminals();
	std::cout << "PRIVATE_REVIEW_OK\n";
}
