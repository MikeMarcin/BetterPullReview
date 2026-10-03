#include "bpr/core/review_composition.hpp"
#include "private_review_fixture.hpp"
#include <algorithm>
#include <tuple>

namespace {
	using namespace private_review_fixture;
	struct source_fixture
	{
		diff_semantic_index                     Index   = index();
		private_review_binding                  Binding = binding();
		private_composition_target              Target;
		std::vector<private_source_selection>   Selected;
		std::vector<private_source_observation> Observed;
		source_fixture()
		{
			Binding.Comparison = Index.diff().Alignment.Digest;
			const auto target  = make_composition_target(Binding, { *Binding.Comparison, diff_side::right, 1 }, Index);
			expect(target.has_value(), "real semantic index admits exact right target");
			Target            = *target;
			const auto first  = created().Revision;
			const auto source = source_of(first);
			Selected.push_back({ source, source.Key, false });
			Observed.push_back({ source, source.Key, first.Actor, first.Completion, private_subject_availability::current,
								 Target, private_copy_body{ first.Identity.Digest, first.Body.size() } });
		}
		auto workspace(review_composition_mode aMode = review_composition_mode::reviewer_authored) const -> review_composition_state
		{
			auto result = create_review_composition(review_composition_id{ 1 }, Binding, aMode, Selected, Observed);
			expect(result.has_value(), "create workspace through public API");
			return *result;
		}
	};
	auto run(const source_fixture& aFixture, agent_run_id aRun = agent_run_id{ 1 }) -> agent_run_provenance
	{
		return { .Run = aRun, .Session = agent_session_id{ 9 }, .Order = private_vertex_order{ 10 }, .Profile = agent_profile_id{ 1 }, .ProfileDisplayName = "unconfirmed tool label", .Protocol = agent_protocol::codex_app_server, .EntryPoint = agent_entry_point::workshop_comment, .Purpose = private_run_purpose::composition, .Binding = aFixture.Binding, .Metadata = { }, .SelectedPrivateSources = aFixture.Selected, .InputSelectionDigest = *private_selection_digest(aFixture.Selected), .OutputDigest = private_digest("proposal"), .Outcome = private_run_outcome::succeeded, .Completion = private_revision_completion::complete };
	}
	void metadata_matrix()
	{
		auto facts = agent_runtime_metadata{ };
		expect(inspect_runtime_metadata(facts)->empty(), "all unknown facts accepted without inference");
		facts.Requested.Model           = "requested";
		facts.Configured.Model          = "configured";
		facts.ContentSelfReported.Model = "claimed";
		facts.Requested.Effort          = "high";
		expect(inspect_runtime_metadata(facts)->size() == 3, "requested configured self-report pairwise conflicts kept");
		auto report = agent_reported_facts{ .Basis = "turn/model", .Settings = { "reported", "low" }, .ToolVersion = "1.2", .RemoteSession = "session", .RemoteThread = "thread", .RemoteTurn = "turn", .RemoteRun = "run" };
		auto full   = append_runtime_report(facts, report);
		expect(full && full->Requested == facts.Requested && full->Configured == facts.Configured && full->ContentSelfReported == facts.ContentSelfReported, "reported facts never rewrite other categories");
		expect(inspect_runtime_metadata(*full)->size() == 7, "full conflicting metadata retains location-qualified conflicts");
		auto repeated = append_runtime_report(*full, report);
		expect(repeated && repeated->ProtocolReported.size() == 1, "identical report coalesces");
		report.Settings.Model = "different";
		report.RemoteTurn     = "other turn";
		auto differing        = append_runtime_report(*full, report);
		expect(differing && differing->ProtocolReported.size() == 2 && inspect_runtime_metadata(*differing)->size() > 7, "conflicting reports never overwrite");
		auto partial = agent_runtime_metadata{ };
		partial.ProtocolReported.push_back({ .Basis = "init", .Settings = { "only model", { } } });
		expect(inspect_runtime_metadata(partial) && !partial.Requested.Model && !partial.ProtocolReported[0].Settings.Effort, "missing values remain absent");
		for (const auto& invalid : { std::string{ "bad\nvalue" }, std::string{ "bad\0value", 9 }, std::string{ "\xC0\x80", 2 }, std::string(257, 'a') }) {
			auto malformed            = facts;
			malformed.Requested.Model = invalid;
			expect(!inspect_runtime_metadata(malformed), "control invalid UTF-8 and oversize refused");
		}
		auto bounded = agent_runtime_metadata{ };
		for (std::size_t i = 0; i < maximum_runtime_reports; ++i) {
			report        = { .Basis = std::to_string(i), .Settings = { "model", { } } };
			auto appended = append_runtime_report(bounded, report);
			expect(appended.has_value(), "bounded distinct report admitted");
			bounded = *appended;
		}
		report.Basis = "overflow";
		expect(!append_runtime_report(bounded, report), "report count bound refuses whole");
		auto oversized = agent_runtime_metadata{ };
		for (int i = 0; i < 16; ++i) {
			oversized.ProtocolReported.push_back({ .Basis = "init", .RemoteThread = std::string(1024, 'x'), .RemoteTurn = std::string(1024, 'y') });
		}
		expect(!inspect_runtime_metadata(oversized), "aggregate metadata bytes bounded");
	}
	void source_admission()
	{
		source_fixture f;
		auto           admitted = admit_composition_sources(f.Binding.Scope, f.Selected, f.Observed);
		expect(admitted.has_value(), "complete exact source admitted");
		expect(admit_private_run_sources(f.Binding, { }, { }).has_value(), "ordinary agent run may select no private sources");
		auto runObserved            = f.Observed;
		runObserved.front().Binding = f.Binding;
		expect(admit_private_run_sources(f.Binding, f.Selected, runObserved).has_value(), "same revision is eligible for explicit invocation");
		runObserved.front().Binding->Revision.RightEpoch = epoch_ordinal{ 99 };
		expect(!admit_private_run_sources(f.Binding, f.Selected, runObserved), "stale source cannot borrow current invocation binding");

		auto changed             = f.Observed;
		changed[0].Source.Digest = private_digest("changed");
		expect(admit_composition_sources(f.Binding.Scope, f.Selected, changed).error() == private_review_refusal::digest_mismatch, "immutable digest mismatch refuses");
		changed                                                   = f.Observed;
		std::get<private_turn_source>(changed[0].Latest).Revision = private_review_turn_revision_id{ 2 };
		expect(admit_composition_sources(f.Binding.Scope, f.Selected, changed).error() == private_review_refusal::source_changed, "tip changed since selection refuses");
		auto selected              = f.Selected;
		selected[0].ExpectedLatest = changed[0].Latest;
		expect(!admit_composition_sources(f.Binding.Scope, selected, changed), "implicit history refuses");
		selected[0].Historical = true;
		expect(admit_composition_sources(f.Binding.Scope, selected, changed).has_value(), "explicit historical revision accepted with exact tip");
		changed[0].Availability = private_subject_availability::stale;
		expect(admit_composition_sources(f.Binding.Scope, selected, changed).has_value(), "known old head remains eligible locally");
		auto foreign        = f.Binding.Scope;
		foreign.Incarnation = opaque<private_ledger_incarnation>(9);
		expect(!admit_composition_sources(foreign, f.Selected, f.Observed), "ledger replacement/cross review refuses");
		expect(!admit_composition_sources(f.Binding.Scope, f.Selected, { }), "missing source refuses");
		selected = f.Selected;
		selected.push_back(selected[0]);
		changed = f.Observed;
		changed.push_back(changed[0]);
		expect(admit_composition_sources(f.Binding.Scope, selected, changed).error() == private_review_refusal::duplicate_source, "duplicate source refused before composition");
		for (auto completion : { private_revision_completion::incomplete, private_revision_completion::failed, private_revision_completion::cancelled }) {
			changed               = f.Observed;
			changed[0].Completion = completion;
			expect(!admit_composition_sources(f.Binding.Scope, f.Selected, changed), "incomplete sources cannot compose implicitly");
		}
		selected.assign(257, f.Selected[0]);
		expect(!private_selection_digest(selected), "source count whole-operation bound");
		auto second           = f.Selected[0];
		second.Source.Key     = private_turn_source{ private_review_thread_id{ 1 }, private_review_turn_id{ 2 }, private_review_turn_revision_id{ 2 } };
		second.ExpectedLatest = second.Source.Key;
		second.Source.Order   = private_vertex_order{ 2 };
		selected              = { f.Selected[0], second };
		const auto ordered    = private_selection_digest(selected);
		std::ranges::reverse(selected);
		expect(ordered == private_selection_digest(selected), "canonical source digest independent of presentation order");
	}
	/// [[BPR-1245]]. A comment-workshop press asks about exactly the one
	/// reviewer revision that recorded the draft at the press.
	void workshop_sources()
	{
		source_fixture f;
		expect(admit_workshop_run_sources(f.Selected, f.Observed).has_value(), "one complete current reviewer revision is the workshop's source");
		expect(admit_workshop_run_sources({ }, { }).error() == private_review_refusal::invalid_provenance, "a press with no recorded revision is refused");
		auto second           = f.Selected[0];
		second.Source.Key     = private_turn_source{ private_review_thread_id{ 1 }, private_review_turn_id{ 2 }, private_review_turn_revision_id{ 2 } };
		second.ExpectedLatest = second.Source.Key;
		auto observedTwo      = f.Observed;
		observedTwo.push_back(f.Observed[0]);
		observedTwo.back().Source = second.Source;
		observedTwo.back().Latest = second.Source.Key;
		auto two                  = std::vector{ f.Selected[0], second };
		expect(!admit_workshop_run_sources(two, observedTwo), "a second source is refused");
		auto agent          = f.Observed;
		agent.front().Actor = private_agent_actor{ agent_run_id{ 1 } };
		expect(!admit_workshop_run_sources(f.Selected, agent), "an agent's turn is not the reviewer's draft");
		auto incomplete               = f.Observed;
		incomplete.front().Completion = private_revision_completion::incomplete;
		expect(!admit_workshop_run_sources(f.Selected, incomplete), "an incomplete revision is refused");
		auto historical               = f.Selected;
		historical.front().Historical = true;
		expect(!admit_workshop_run_sources(historical, f.Observed), "an explicitly historical revision is refused");
		auto moved                                                   = f.Observed;
		std::get<private_turn_source>(moved.front().Latest).Revision = private_review_turn_revision_id{ 2 };
		expect(!admit_workshop_run_sources(f.Selected, moved), "a revision the thread has moved past is refused");
		auto finding                   = f.Selected;
		finding.front().Source.Key     = private_finding_source{ agent_run_id{ 1 }, 0 };
		finding.front().ExpectedLatest = finding.front().Source.Key;
		auto findingObserved           = f.Observed;
		findingObserved.front().Source = finding.front().Source;
		findingObserved.front().Latest = finding.front().Source.Key;
		expect(!admit_workshop_run_sources(finding, findingObserved), "a finding is not a recorded draft");
	}
	void lineage()
	{
		source_fixture                        f;
		auto                                  destination = private_source_reference{ f.Binding.Scope, private_composition_source{ review_composition_id{ 1 }, review_composition_revision_id{ 1 } }, private_vertex_order{ 5 }, private_digest("new"), 3 };
		std::vector<private_derivation_edge>  edges{ { private_derivation_kind::composed_from, f.Selected[0].Source } };
		std::vector<private_source_reference> existing{ edges[0].Source };
		expect(admit_private_derivation(destination, false, edges, existing).has_value(), "new vertex points to exact older source");
		expect(!admit_private_derivation(destination, true, edges, existing), "later attachment cannot rewrite immutable lineage");
		expect(!admit_private_derivation(destination, false, edges, { }), "missing prior source refuses");
		destination.Order = private_vertex_order{ 1 };
		expect(!admit_private_derivation(destination, false, edges, existing), "equal/backwards ordinal refuses cycle");
		destination.Order             = private_vertex_order{ 5 };
		destination.Scope.PullRequest = pull_request_id{ 9 };
		expect(admit_private_derivation(destination, false, edges, existing).error() == private_review_refusal::wrong_scope, "cross review edge rejected");
		destination.Scope = f.Binding.Scope;
		edges[0].Source   = destination;
		existing[0]       = destination;
		expect(!admit_private_derivation(destination, false, edges, existing), "self edge rejected even if caller claims existence");
		for (std::uint64_t n = 2; n < 80; ++n) {
			auto prior        = f.Selected[0].Source;
			prior.Order       = private_vertex_order{ n - 1 };
			destination.Order = private_vertex_order{ n };
			edges[0].Source   = prior;
			existing[0]       = prior;
			expect(admit_private_derivation(destination, false, edges, existing).has_value(), "all increasing DAG steps admitted");
			destination.Order = private_vertex_order{ n - 1 };
			expect(!admit_private_derivation(destination, false, edges, existing), "order invariant rules out every reversed edge");
		}
	}
	void modes_and_local_draft()
	{
		static_assert(sizeof(review_composition_state) < 2048);
		source_fixture f;
		for (auto mode : { review_composition_mode::reviewer_authored, review_composition_mode::exact_copy, review_composition_mode::agent_assisted }) {
			const auto initial  = f.workspace(mode);
			const auto original = f.Observed;
			const auto agent    = run(f);
			const auto body     = mode == review_composition_mode::exact_copy ? "reviewer text" : "proposal";
			auto       proposal = propose_composition_revision(initial, review_composition_revision_id{ 1 }, private_vertex_order{ 11 }, body, f.Selected, f.Observed,
															   mode == review_composition_mode::agent_assisted ? &agent : nullptr);
			expect(proposal.has_value(), "all three explicit composition modes produce local candidates");
			expect(proposal->State.Lifecycle == review_composition_lifecycle::editing, "agent or copy completion cannot accept/publish");
			expect(std::holds_alternative<private_agent_actor>(proposal->Revision.Actor) == (mode == review_composition_mode::agent_assisted), "candidate actor from explicit mode");
			auto draft    = revisioned_review_draft{ .Draft = { review_draft_id{ 1 }, f.Target.Line, "old", review_draft_state::drafting, { } }, .Revision = 1, .Scope = review_draft_scope_id{ 1 } };
			auto accepted = accept_composition_as_draft(proposal->State, proposal->Revision, f.Selected, f.Observed, f.Index, draft);
			expect(accepted && accepted->Body == body && accepted->ExpectedDraftRevision == 1, "accept emits exact body and local draft CAS only");
			expect(draft.Draft.State == review_draft_state::drafting && draft.Draft.Body == "old", "pure proposal does not mutate existing draft");
			auto linked = apply(draft, revisioned_review_draft_command{ .Command = { draft.Draft.Id, review_draft_command_kind::edit, accepted->Body }, .ExpectedRevision = 1, .Scope = draft.Scope, .Composition = review_composition_link{ f.Binding.Scope, initial.Id, review_composition_revision_id{ 1 }, proposal->Revision.Identity.Digest } });
			expect(linked.Changed && linked.Accepted.Composition && !linked.Accepted.Draft.shared(), "existing core draft policy accepts local correlation without publication");
			const std::array published{ composition_publication_observation{ f.Binding.Scope, proposal->State.SourceSetDigest, proposal->Revision.Identity.Digest, f.Target } };
			expect(duplicate_composition_publication(proposal->State, proposal->Revision, published), "exact source/body/target duplicate is advisory");
			expect(accept_composition_as_draft(proposal->State, proposal->Revision, f.Selected, f.Observed, f.Index).has_value(), "duplicate advisory never blocks intentional local acceptance");
			const auto acknowledged = acknowledge_composition_draft(proposal->State, proposal->Revision.Identity);
			expect(acknowledged && acknowledged->Lifecycle == review_composition_lifecycle::accepted_as_draft, "only exact durable acknowledgement accepts");
			expect(!propose_composition_revision(*acknowledged, review_composition_revision_id{ 2 }, private_vertex_order{ 12 }, "later", f.Selected, f.Observed), "accepted composition has no further transition through candidate path");
			auto edited = propose_composition_revision(proposal->State, review_composition_revision_id{ 2 }, private_vertex_order{ 12 }, "reviewer revision", f.Selected, f.Observed, nullptr, &proposal->Revision);
			expect(edited && std::holds_alternative<private_reviewer_actor>(edited->Revision.Actor) && edited->Revision.Derivation[0].Kind == private_derivation_kind::edited_from, "editing every mode produces immutable reviewer successor");
			expect(proposal->Revision.Body == body && f.Observed[0].Source == original[0].Source, "sources and prior candidates remain intact");
			auto wrong             = f.Observed;
			wrong[0].Source.Digest = private_digest("mutated");
			expect(!accept_composition_as_draft(proposal->State, proposal->Revision, f.Selected, wrong, f.Index), "dispatch/acceptance reload catches source changes");
		}
		auto copy = f.workspace(review_composition_mode::exact_copy);
		expect(!propose_composition_revision(copy, review_composition_revision_id{ 1 }, private_vertex_order{ 20 }, "almost reviewer text", f.Selected, f.Observed), "exact copy cannot edit bytes silently");
		auto agentState = f.workspace(review_composition_mode::agent_assisted);
		expect(!propose_composition_revision(agentState, review_composition_revision_id{ 1 }, private_vertex_order{ 20 }, "proposal", f.Selected, f.Observed), "agent mode requires exact run provenance");
		auto agent                     = run(f);
		agent.Binding.Revision.HeadOid = agent.Binding.Revision.BaseOid;
		expect(!propose_composition_revision(agentState, review_composition_revision_id{ 1 }, private_vertex_order{ 20 }, "proposal", f.Selected, f.Observed, &agent), "wrong run binding refuses");
		const auto abandoned = abandon_review_composition(copy);
		expect(abandoned && !propose_composition_revision(*abandoned, review_composition_revision_id{ 1 }, private_vertex_order{ 20 }, "reviewer text", f.Selected, f.Observed), "abandoned is terminal");
		expect(!propose_composition_revision(f.workspace(), review_composition_revision_id{ 1 }, private_vertex_order{ 20 }, std::string(16385, 'x'), f.Selected, f.Observed), "candidate ceiling refuses whole");
	}
	void partitions()
	{
		source_fixture f;
		auto           second    = f.Observed[0];
		second.Source.Key        = private_turn_source{ private_review_thread_id{ 1 }, private_review_turn_id{ 2 }, private_review_turn_revision_id{ 2 } };
		second.Source.Order      = private_vertex_order{ 2 };
		second.Latest            = second.Source.Key;
		second.Target->Line.Line = 2;
		f.Observed.push_back(second);
		f.Selected.push_back({ second.Source, second.Latest, false });
		auto partitions = partition_composition_targets(f.Observed);
		expect(partitions && partitions->Assigned.size() == 2 && partitions->Unassigned.empty(), "exact different lines split into separate partitions");
		auto ambiguous = f.workspace();
		expect(!ambiguous.Target, "mixed targets are never guessed");
		auto proposal = propose_composition_revision(ambiguous, review_composition_revision_id{ 1 }, private_vertex_order{ 10 }, "combined", f.Selected, f.Observed);
		expect(proposal && accept_composition_as_draft(proposal->State, proposal->Revision, f.Selected, f.Observed, f.Index).error() == private_review_refusal::ambiguous_target, "missing explicit target prevents draft intent");
		auto explicitTarget = create_review_composition(review_composition_id{ 2 }, f.Binding, review_composition_mode::reviewer_authored, f.Selected, f.Observed, f.Target);
		expect(explicitTarget && explicitTarget->Target == f.Target, "explicit chosen target is preserved");
		f.Observed[1].Target.reset();
		partitions = partition_composition_targets(f.Observed);
		expect(partitions && partitions->Assigned[0].Sources == std::vector<std::size_t>{ 0 } && partitions->Unassigned == std::vector<std::size_t>{ 1 }, "targetless sources remain visible not dropped");
		expect(!f.workspace().Target, "one target plus unassigned still requires choice");
		expect(!make_composition_target(f.Binding, { *f.Binding.Comparison, diff_side::left, 1 }, f.Index), "left subject grants no root publication target");
	}
	void runs_and_findings()
	{
		source_fixture f;
		auto           first  = run(f);
		auto           second = run(f, agent_run_id{ 2 });
		expect(valid_run_provenance(first) && valid_run_provenance(second) && first.Session == second.Session && first.Run != second.Run, "one session retains independent per-invocation runs");
		expect(!first.Metadata.Requested.Model && first.Metadata.ProtocolReported.empty(), "profile name never supplies a model");
		auto oversized = first;
		oversized.SelectedPrivateSources.clear();
		for (std::uint64_t ordinal = 1; ordinal <= 3; ++ordinal) {
			auto selected             = f.Selected[0];
			selected.Source.Key       = private_turn_source{ private_review_thread_id{ 1 }, private_review_turn_id{ ordinal }, private_review_turn_revision_id{ ordinal } };
			selected.ExpectedLatest   = selected.Source.Key;
			selected.Source.BodyBytes = maximum_private_assistant_body_bytes;
			oversized.SelectedPrivateSources.push_back(selected);
		}
		oversized.InputSelectionDigest = *private_selection_digest(oversized.SelectedPrivateSources);
		expect(!valid_run_provenance(oversized), "storage-eligible sources can exceed private invocation byte admission");
		oversized.SelectedPrivateSources.pop_back();
		oversized.InputSelectionDigest = *private_selection_digest(oversized.SelectedPrivateSources);
		expect(valid_run_provenance(oversized), "two MiB private selection boundary is inclusive");
		first.Purpose    = private_run_purpose::conversation;
		first.EntryPoint = agent_entry_point::ask_about_selection;
		auto auth        = authorize_private_run(first);
		expect(auth.has_value(), "qualified conversation terminal authorizes exact actor");
		auto initial = created();
		auto body    = input(12);
		body.Actor   = private_agent_actor{ first.Run };
		body.Body    = "proposal";
		// This run names an exact file comparison, so creation retains that same binding.
		initial = *create_private_thread(private_review_thread_id{ 1 }, { first.Binding, private_pr_subject{ } }, input());
		expect(append_private_turn(initial.Thread, 1, body, nullptr, &*auth).has_value(), "provenance composes with thread terminal admission");
		body.Body = "uncorrelated body";
		expect(!append_private_turn(initial.Thread, 1, body, nullptr, &*auth), "matching run cannot claim different assistant bytes");
		first.Outcome = private_run_outcome::protocol_failure;
		expect(!valid_run_provenance(first), "protocol failure cannot masquerade as complete");
		first                            = run(f);
		first.Purpose                    = private_run_purpose::findings;
		first.EntryPoint                 = agent_entry_point::run_ai_review;
		second                           = first;
		second.Run                       = agent_run_id{ 2 };
		const auto&            alignment = f.Index.diff().Alignment;
		auto                   finding   = agent_finding{ .Provider = first.Profile, .Path = "file.cpp", .Lines = { 1, 2 }, .Anchor = { .Line = f.Target.Line }, .Claim = "claim", .Evidence = "evidence", .Consequence = "consequence", .SuggestedComment = "public comment", .RequiredContext = "context", .AnalyzedBaseOid = first.Binding.Revision.BaseOid, .AnalyzedHeadOid = first.Binding.Revision.HeadOid };
		agent_finding_snapshot snapshot{ first.Binding.Revision, { { "file.cpp", alignment.Digest, 3, 3, { }, { }, { } } } };
		auto                   retained  = retain_agent_finding(first, 0, private_vertex_order{ 20 }, finding, snapshot);
		auto                   retained2 = retain_agent_finding(second, 0, private_vertex_order{ 21 }, finding, snapshot);
		expect(retained && retained2 && retained->Verdict.anchored() && retained->Source.Key != retained2->Source.Key, "same-session same-ordinal findings remain distinct by run");
		expect(retained->Finding == finding && retained->Verdict.AnalyzedHeadOid == finding.AnalyzedHeadOid, "immutable finding snapshot retains every structured fact");
		finding.Consequence = "different";
		auto changed        = retain_agent_finding(first, 0, private_vertex_order{ 22 }, finding, snapshot);
		expect(changed && changed->Source.Digest != retained->Source.Digest, "finding digest binds full structured content");
		finding.AnalyzedHeadOid = finding.AnalyzedBaseOid;
		auto stale              = retain_agent_finding(first, 1, private_vertex_order{ 23 }, finding, snapshot);
		expect(stale && stale->Verdict.State == agent_finding_anchor_state::stale, "retained stale finding does not claim anchored");
		auto selected = std::vector<private_source_selection>{ { retained->Source, retained->Source.Key, false } };
		auto observed = std::vector<private_source_observation>{ { retained->Source, retained->Source.Key, private_agent_actor{ first.Run }, private_revision_completion::complete,
																   private_subject_availability::current, f.Target, private_copy_body{ private_digest("public comment"), 14 } } };
		expect(!admit_composition_sources(f.Binding.Scope, selected, observed), "finding source cannot omit its validation verdict");
		observed[0].FindingVerdict = retained->Verdict;
		auto state                 = create_review_composition(review_composition_id{ 3 }, f.Binding, review_composition_mode::exact_copy, selected, observed);
		expect(state.has_value(), "validated finding workspace admitted");
		auto copy = propose_composition_revision(*state, review_composition_revision_id{ 3 }, private_vertex_order{ 24 }, "public comment", selected, observed);
		expect(copy && std::get<private_agent_actor>(copy->Revision.Actor).Run == first.Run, "finding suggested comment copies exact bytes and author with finding lineage");
	}
	void draft_link_integrity()
	{
		source_fixture f;
		auto           draft   = revisioned_review_draft{ .Draft = { review_draft_id{ 1 }, f.Target.Line, "old", review_draft_state::drafting, { } }, .Revision = 1, .Scope = review_draft_scope_id{ 1 }, .Composition = review_composition_link{ f.Binding.Scope, review_composition_id{ 1 }, review_composition_revision_id{ 1 }, private_digest("old") } };
		auto           command = revisioned_review_draft_command{ .Command = { draft.Draft.Id, review_draft_command_kind::edit, "new" }, .ExpectedRevision = 1, .Scope = draft.Scope };
		expect(apply(draft, command).Refusal == revisioned_draft_refusal::invalid_command, "linked draft cannot lose provenance on edit");
		command.Composition = review_composition_link{ f.Binding.Scope, review_composition_id{ 1 }, review_composition_revision_id{ 2 }, private_digest("new") };
		auto edited         = apply(draft, command);
		expect(edited.Changed && edited.Accepted.Composition == command.Composition, "same draft authority appends exact candidate correlation");
		auto publish  = revisioned_review_draft_command{ .Command = { draft.Draft.Id, review_draft_command_kind::begin_publish, { } }, .ExpectedRevision = 2, .Scope = draft.Scope };
		auto separate = apply(edited.Accepted, publish);
		expect(separate.Changed && separate.Accepted.Composition == edited.Accepted.Composition, "separate existing publish command preserves exact local linkage");
		auto staleLink                  = command;
		staleLink.ExpectedRevision      = edited.Accepted.Revision;
		staleLink.Composition->Revision = review_composition_revision_id{ 1 };
		expect(apply(edited.Accepted, staleLink).Refusal == revisioned_draft_refusal::invalid_command, "draft cannot move composition revision backwards");
		command.Composition->BodyDigest = private_digest("wrong");
		expect(apply(draft, command).Refusal != revisioned_draft_refusal::none, "mismatched body correlation refused");
		command.Composition.reset();
		draft.Composition->BodyDigest = private_digest("corrupt");
		expect(apply(draft, command).Refusal == revisioned_draft_refusal::invalid_identity, "corrupt retained link refused before transition");
	}
}
int main()
{
	metadata_matrix();
	source_admission();
	workshop_sources();
	lineage();
	modes_and_local_draft();
	partitions();
	runs_and_findings();
	draft_link_integrity();
	std::cout << "REVIEW_COMPOSITION_OK\n";
}
