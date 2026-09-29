#include "bpr/application/review_tour_host.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include <algorithm>
#include <chrono>
#include <deque>
#include <utility>
namespace bpr::application {
	struct review_tour_host::implementation : std::enable_shared_from_this<implementation>
	{
		core::review_tour                        Seed;
		std::vector<core::tour_import_candidate> Imports;
		std::unique_ptr<review_tour_store>       Store;
		effect_executor&                         Effects;
		decision_executor&                       Decision;
		publication                              Publish;
		tour_presentation                        State;
		std::deque<tour_history_entry>           Undo;
		std::deque<tour_history_entry>           Redo;
		tour_presentation_origin                 Origin = tour_presentation_origin::desktop;
		std::vector<tour_request_receipt>        Receipts;
		std::vector<tour_assist_ticket>          Tickets;
		std::vector<tour_assist_proposal>        Proposals;
		std::uint64_t                            AssistSequence = 0;
		std::string                              LabelOverride;
		std::uint64_t                            JournalGeneration = 0, SavedJournalGeneration = 0;
		bool                                     DeferringSave = false;
		std::uint64_t                            Request       = 0;
		bool                                     Active        = false;
		bool                                     Started       = false;
		bool                                     Closing       = false;
		implementation(core::review_tour aSeed, std::unique_ptr<review_tour_store> aStore, effect_executor& aEffects, decision_executor& aDecision, publication aPublish)
			: Seed{ std::move(aSeed) }
			, Store{ std::move(aStore) }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Publish{ std::move(aPublish) }
		{ }
		std::vector<core::tour_code_destination> Sources;
		void                                     notify()
		{
			State.CanUndo   = !Undo.empty();
			State.CanRedo   = !Redo.empty();
			State.UndoLabel = Undo.empty() ? std::string{ } : Undo.back().Label;
			State.RedoLabel = Redo.empty() ? std::string{ } : Redo.back().Label;
			State.Destinations.clear();
			if (State.Draft) {
				for (const auto& stop : State.Draft->Stops) {
					auto source = stop.Destination;
					if (stop.Original) {
						source = core::destination_of(*stop.Original);
					}
					core::tour_destination_observation observation;
					if (source) {
						observation.Source          = *source;
						observation.SourceAvailable = source->Blob.has_value() || std::ranges::any_of(Sources, [&](const auto& aKnown) {auto requested=*source;requested.Blob=aKnown.Blob;return requested==aKnown; });
						if (source->Commit != State.Draft->Head || source->Side == core::diff_side::left) {
							observation.Subject = core::tour_subject::historical;
						}
					}
					observation.Body = core::classify_review_body(stop.Body);
					// This preparation host has no qualified pending-write observation.
					// Existing standalone success, source availability and a web link cannot supply one.
					State.Destinations.push_back({ .Stop = stop.Id, .Plan = core::plan_tour_destination(observation) });
				}
			}

			if (Closing || !Publish) {
				return;
			}
			const auto snapshot = State;
			try {
				Publish(snapshot);
			} catch (...) { /* A projection cannot undo durable state. */
			}
		}
		void start()
		{
			if (Started || Closing) {
				return;
			}
			Started            = true;
			Active             = true;
			State.Loading      = true;
			const auto request = ++Request;
			const auto self    = shared_from_this();
			try {
				Effects.post([self, request] {
					auto observed = tour_store_result<std::optional<tour_stored_state>>{ std::unexpected(tour_store_error{ }) };
					try {
						observed = self->Store->load_state(self->Seed.Owner, self->Seed.Id, { });
					} catch (...) { }
					if (observed && *observed && (!application::valid(**observed) || (**observed).Draft.Owner != self->Seed.Owner || (**observed).Draft.Id != self->Seed.Id)) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					self->Decision.post([self, request, result = std::move(observed)]() mutable {
						if (!self->Active || request != self->Request) {
							return;
						}
						self->Active        = false;
						self->State.Loading = false;
						if (result && *result) {
							self->State.Draft         = std::make_shared<core::review_tour>(std::move(result->value().Draft));
							self->State.SavedRevision = self->State.Draft->Revision;
							self->Undo                = std::move(result->value().Undo);
							self->Redo                = std::move(result->value().Redo);
							self->Receipts            = std::move(result->value().Receipts);
							self->Tickets             = std::move(result->value().Tickets);
							self->Proposals           = std::move(result->value().Proposals);
							self->AssistSequence      = result->value().AssistSequence;
							self->State.Resume        = result->value().Resume;
							self->State.Selected      = self->State.Resume.Stop;
						} else {
							self->State.Draft = std::make_shared<core::review_tour>(self->Seed);
							if (!result) {
								self->State.Failure = result.error().Failure;
							}
						}
						if (!self->Imports.empty()) {
							self->refresh_bound_tour();
						}
						self->notify();
					});
				});
			} catch (...) {
				Active        = false;
				State.Loading = false;
				State.Failure = tour_store_failure::unavailable;
				State.Draft   = std::make_shared<core::review_tour>(Seed);
			}
			notify();
		}
		void observe_imports(std::vector<core::tour_import_candidate> aCandidates)
		{
			if (Closing || aCandidates.size() > 64) {
				return;
			}
			Imports = std::move(aCandidates);
			State.ImportChoices.clear();
			for (const auto& item : Imports) {
				State.ImportChoices.push_back(item.Label);
			}
			refresh_bound_tour();
			notify();
		}
		void adopt_observation(core::review_tour aCandidate)
		{
			if (!State.Draft) {
				return;
			}
			const auto& draft    = *State.Draft;
			const auto  accepted = core::apply(draft, { .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = draft.Revision, .Kind = core::tour_command_kind::restore, .Restore = std::move(aCandidate) });
			if (accepted.Changed) {
				Undo.clear();
				Redo.clear();
				State.Draft = std::make_shared<core::review_tour>(accepted.Accepted);
				pump();
			}
		}
		void refresh_bound_tour()
		{
			if (State.Loading || !State.Draft || !State.Draft->Remote) {
				return;
			}
			const auto& draft   = *State.Draft;
			const auto  matches = std::ranges::count_if(Imports, [&](const auto& aCandidate) { return aCandidate.Draft.Remote && aCandidate.Draft.Remote->Entity == draft.Remote->Entity; });
			if (matches == 1) {
				const auto found = std::ranges::find_if(Imports, [&](const auto& aCandidate) { return aCandidate.Draft.Remote && aCandidate.Draft.Remote->Entity == draft.Remote->Entity; });
				adopt_observation(core::reconcile_tour(draft, found->Draft));
			} else {
				auto retained         = draft;
				retained.RemoteStale  = true;
				retained.ImportNotice = "The bound website index is unavailable or needs a list choice. Retained text is stale; absence is not deletion.";
				adopt_observation(std::move(retained));
			}
		}
		void import_selected(std::size_t aIndex)
		{
			if (Closing || State.Loading || !State.Draft || aIndex >= Imports.size()) {
				return;
			}
			const auto& draft = *State.Draft;
			if (draft.Remote && Imports[aIndex].Draft.Remote && draft.Remote->Entity == Imports[aIndex].Draft.Remote->Entity) {
				adopt_observation(core::reconcile_tour(draft, Imports[aIndex].Draft));
				notify();
				return;
			}
			if (!draft.Problem.empty() || !draft.Solution.empty() || !draft.Stops.empty() || draft.Remote) {
				State.ImportNotice = "A private draft already exists. Its text is preserved; importing cannot replace it.";
				notify();
				return;
			}
			const auto& candidate = Imports[aIndex].Draft;
			if (candidate.Owner != Seed.Owner || candidate.Head != Seed.Head || candidate.Base != Seed.Base) {
				return;
			}
			dispatch({ .Draft = draft.Id, .Owner = draft.Owner, .ExpectedRevision = draft.Revision, .Kind = core::tour_command_kind::restore, .Restore = candidate });
		}

		void pump()
		{
			if (DeferringSave || Active || !State.Draft || State.Failure || (State.SavedRevision == State.Draft->Revision && SavedJournalGeneration == JournalGeneration)) {
				return;
			}
			if (State.Resume.Stop.value() && std::ranges::find(State.Draft->Stops, State.Resume.Stop, &core::tour_stop::Id) == State.Draft->Stops.end()) {
				State.Resume.Stop = { };
			}
			Active                = true;
			State.Saving          = true;
			const auto request    = ++Request;
			const auto self       = shared_from_this();
			const auto candidate  = State.Draft;
			const auto expected   = State.SavedRevision;
			const auto generation = JournalGeneration;
			const auto journal    = std::make_shared<const tour_stored_state>(tour_stored_state{ *candidate, Undo, Redo, State.Resume, Receipts, Tickets, Proposals, AssistSequence });
			try {
				Effects.post([self, request, candidate, expected, journal, generation] {
					auto observed = tour_store_result<core::review_tour>{ std::unexpected(tour_store_error{ }) };
					try {
						observed = self->Store->save_state(*journal, expected, { });
					} catch (...) { }
					if (observed && *observed != *candidate) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					if (!observed && observed.error().Current && (core::validate(*observed.error().Current) != core::tour_refusal::none || observed.error().Current->Owner != candidate->Owner || observed.error().Current->Id != candidate->Id)) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					self->Decision.post([self, request, result = std::move(observed), generation]() mutable {
						if (!self->Active || request != self->Request) {
							return;
						}
						self->Active       = false;
						self->State.Saving = false;
						if (result) {
							self->State.SavedRevision    = result->Revision;
							self->SavedJournalGeneration = generation;
						} else {
							self->State.Failure = result.error().Failure;
							if (result.error().Current) {
								self->State.Conflict = std::make_shared<core::review_tour>(std::move(*result.error().Current));
							}
						}
						self->pump();
						self->notify();
					});
				});
			} catch (...) {
				Active        = false;
				State.Saving  = false;
				State.Failure = tour_store_failure::unavailable;
			}
		}
		void retain(std::deque<tour_history_entry>& aHistory, core::tour_stop_id aSelected, std::string aLabel)
		{
			aHistory.push_back({ *State.Draft, aSelected, LabelOverride.empty() ? std::move(aLabel) : LabelOverride, Origin });
			auto bytes = std::size_t{ 0 };
			for (const auto& item : aHistory) {
				bytes += core::retained_tour_bytes(item.Draft);
			}
			while (aHistory.size() > maximum_tour_history_entries || bytes > maximum_tour_history_bytes) {
				bytes -= core::retained_tour_bytes(aHistory.front().Draft);
				aHistory.pop_front();
			}
		}
		static auto label(core::tour_command_kind aKind) -> std::string
		{
			switch (aKind) {
			case core::tour_command_kind::problem:   return "Edit Problem";
			case core::tour_command_kind::solution:  return "Edit Solution";
			case core::tour_command_kind::edit_stop: return "Edit stop";
			case core::tour_command_kind::insert:    return "Add stop";
			case core::tour_command_kind::move:      return "Move stop";
			case core::tour_command_kind::remove:    return "Remove stop";
			case core::tour_command_kind::complete:  return "Ready stop";
			default:                                 return "Edit tour";
			}
		}

		void dispatch(const core::tour_command& aCommand)
		{
			if (Closing || State.Loading || !State.Draft) {
				return;
			}
			const auto outcome = core::apply(*State.Draft, aCommand);
			State.Refusal      = outcome.Refusal;
			if (outcome.Changed) {
				// Another presentation's edit is not a gesture of this one's author: the stop this
				// author is on stays selected unless the edit removed it.
				const bool foreign = Origin != tour_presentation_origin::desktop && aCommand.Kind != core::tour_command_kind::observe_head;
				const bool kept    = foreign && (!State.Selected.value() || std::ranges::find(outcome.Accepted.Stops, State.Selected, &core::tour_stop::Id) != outcome.Accepted.Stops.end());
				const bool text    = aCommand.Kind == core::tour_command_kind::problem || aCommand.Kind == core::tour_command_kind::solution || aCommand.Kind == core::tour_command_kind::edit_stop;
				if (aCommand.Kind == core::tour_command_kind::observe_head) {
					Undo.clear();
					Redo.clear();
				} else if (aCommand.Kind != core::tour_command_kind::visit) {
					retain(Undo, aCommand.Stop.value() ? aCommand.Stop : State.Selected, label(aCommand.Kind));
					if (!text && !kept) {
						++State.SelectionRevision;
					}
					Redo.clear();
				}
				State.Draft = std::make_shared<core::review_tour>(outcome.Accepted);
				if (!kept) {
					State.Selected = outcome.Selected;
				}
				State.Resume = { aCommand.Kind == core::tour_command_kind::problem ? tour_section::problem : aCommand.Kind == core::tour_command_kind::solution ? tour_section::solution
																																								: tour_section::stops,
								 outcome.Selected };
				pump();
			}
			notify();
		}
		void restore(bool aRedo)
		{
			auto& from = aRedo ? Redo : Undo;
			auto& to   = aRedo ? Undo : Redo;
			if (Closing || !State.Draft || from.empty()) {
				return;
			}
			core::tour_command command{ .Draft = State.Draft->Id, .Owner = State.Draft->Owner, .ExpectedRevision = State.Draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = from.back().Draft };
			auto               outcome = core::apply(*State.Draft, command);
			State.Refusal              = outcome.Refusal;
			if (outcome.Changed) {
				retain(to, State.Selected, from.back().Label);
				const bool foreign = Origin != tour_presentation_origin::desktop;
				const bool kept    = foreign && (!State.Selected.value() || std::ranges::find(outcome.Accepted.Stops, State.Selected, &core::tour_stop::Id) != outcome.Accepted.Stops.end());
				if (!kept) {
					State.Selected = from.back().Selected;
					++State.SelectionRevision;
				}
				from.pop_back();
				State.Draft       = std::make_shared<core::review_tour>(std::move(outcome.Accepted));
				State.Resume.Stop = State.Selected;
				pump();
			}
			notify();
		}

		struct assist_outcome
		{
			std::string               Reason, Detail;
			std::vector<writing_note> Notes;
			std::uint64_t             Proposal = 0;
		};
		static auto assist_field(const core::review_tour& aDraft, core::writing_hook aHook, core::tour_stop_id aStop) -> const std::string*
		{
			if (aHook == core::writing_hook::problem) {
				return &aDraft.Problem;
			}
			if (aHook == core::writing_hook::solution) {
				return &aDraft.Solution;
			}
			if (aHook == core::writing_hook::comment) {
				const auto stop = std::ranges::find(aDraft.Stops, aStop, &core::tour_stop::Id);
				return stop == aDraft.Stops.end() ? nullptr : &stop->Body;
			}
			return nullptr;
		}
		void prune_assist(std::int64_t aNow)
		{
			std::erase_if(Tickets, [&](const auto& aTicket) { return aTicket.Expires <= aNow; });
			std::erase_if(Proposals, [&](const auto& aProposal) { return aProposal.Expires <= aNow; });
		}
		auto refused_assist(assist_outcome aOutcome, std::string aReason, std::string aDetail, core::tour_refusal aRefusal = core::tour_refusal::conflict) -> assist_outcome
		{
			aOutcome.Reason = std::move(aReason);
			aOutcome.Detail = std::move(aDetail);
			State.Refusal   = aRefusal;
			return aOutcome;
		}
		/// A ticket is spent by any completion that names it, whether or not the result is usable, so one
		/// request can never yield two edits and a stale answer cannot be replayed later.
		auto complete_assist(const tour_assist_completion& aCompletion) -> assist_outcome
		{
			assist_outcome out;
			const auto     found = std::ranges::find(Tickets, aCompletion.Id, &tour_assist_ticket::Id);
			if (found == Tickets.end()) {
				return refused_assist(std::move(out), "unknown_ticket", "No such pending request: it was completed, superseded, cancelled or expired.");
			}
			auto ticket = std::move(*found);
			Tickets.erase(found);
			++JournalGeneration;
			if (aCompletion.Now >= ticket.Expires) {
				return refused_assist(std::move(out), "expired", "This request expired. Ask for context again.");
			}
			if (aCompletion.CurrentHookRevision != ticket.HookRevision) {
				return refused_assist(std::move(out), "stale", "The hook instructions changed after this request. Ask for context again.");
			}
			if (ticket.DraftRevision != State.Draft->Revision) {
				return refused_assist(std::move(out), "stale", "The draft changed after this request. Read it again; nothing was applied.");
			}
			if (ticket.Hook == core::writing_hook::outline) {
				const auto decoded = decode_writing_result(aCompletion.ResultJson, core::writing_hook::outline, ticket.Sources, false);
				if (!decoded) {
					return refused_assist(std::move(out), "invalid_result", "The result is not a valid outline. Nothing was proposed.", core::tour_refusal::invalid_command);
				}
				std::vector<core::tour_outline_entry> entries;
				for (const auto& stop : decoded->Stops) {
					const auto source = std::ranges::find(ticket.Sources, stop.Anchor, &writing_source::Id);
					if (source == ticket.Sources.end()) {
						return refused_assist(std::move(out), "source_unavailable", "An outline anchor is not a supplied source. Nothing was proposed.", core::tour_refusal::invalid_command);
					}
					core::tour_outline_entry entry{ .Title = stop.Title, .Source = source->Source, .Suggestion = { stop.Anchor, stop.Reason, stop.Question } };
					for (const auto& reference : stop.Related) {
						const auto related = std::ranges::find(ticket.Sources, reference, &writing_source::Id);
						if (related == ticket.Sources.end() || !related->Source) {
							return refused_assist(std::move(out), "source_unavailable", "A related destination is unavailable. Nothing was proposed.", core::tour_refusal::invalid_command);
						}
						entry.Related.push_back(*related->Source);
					}
					entries.push_back(std::move(entry));
				}
				if (entries.empty()) {
					return refused_assist(std::move(out), "empty_outline", "The outline has no stops. Nothing was proposed.", core::tour_refusal::invalid_command);
				}
				if (Proposals.size() >= maximum_tour_assist_proposals) {
					Proposals.erase(Proposals.begin());
				}
				Proposals.push_back({ ++AssistSequence, State.Draft->Revision, std::move(entries), aCompletion.Now + std::chrono::duration_cast<std::chrono::seconds>(assist_lifetime).count() });
				out.Proposal = Proposals.back().Id;
				return out;
			}
			const auto* raw = assist_field(*State.Draft, ticket.Hook, ticket.Stop);
			if (!raw || ticket.End > raw->size() || core::writing_digest(*raw) != ticket.RawDigest) {
				return refused_assist(std::move(out), "stale", "The field changed after this request. Read it again; nothing was applied.");
			}
			const auto decoded = decode_writing_result(aCompletion.ResultJson, ticket.Hook, ticket.Sources, ticket.Scope == core::writing_scope::empty_field);
			if (!decoded) {
				return refused_assist(std::move(out), "invalid_result", "The result is invalid or unsupported. Your text is unchanged.", core::tour_refusal::invalid_command);
			}
			if (decoded->Insufficient) {
				out.Reason = "insufficient_context";
				out.Detail = decoded->Reason;
				return out;
			}
			const auto text = core::splice_writing_result(*raw, ticket.Begin, ticket.End, decoded->Replacement);
			if (!text) {
				if (text.error() == core::writing_refusal::unchanged) {
					out.Reason = "unchanged";
					out.Detail = "The result made no change.";
					return out;
				}
				return refused_assist(std::move(out), "invalid_result", "The result cannot be applied to the captured text. Your text is unchanged.", core::tour_refusal::invalid_command);
			}
			core::tour_command command{ .Draft = State.Draft->Id, .Owner = State.Draft->Owner, .ExpectedRevision = State.Draft->Revision };
			if (ticket.Hook == core::writing_hook::comment) {
				const auto stop    = std::ranges::find(State.Draft->Stops, ticket.Stop, &core::tour_stop::Id);
				command.Kind       = core::tour_command_kind::edit_stop;
				command.Stop       = ticket.Stop;
				command.Value      = *stop;
				command.Value.Body = *text;
			} else {
				command.Kind = ticket.Hook == core::writing_hook::problem ? core::tour_command_kind::problem : core::tour_command_kind::solution;
				command.Text = *text;
			}
			LabelOverride = ticket.Hook == core::writing_hook::problem ? "Rewrite Problem" : ticket.Hook == core::writing_hook::solution ? "Rewrite Solution"
																																		 : "Rewrite comment";
			dispatch(command);
			LabelOverride.clear();
			out.Notes = decoded->Notes;
			return out;
		}
		auto use_outline(const tour_assist_completion& aCompletion) -> assist_outcome
		{
			assist_outcome out;
			const auto     found = std::ranges::find(Proposals, aCompletion.Id, &tour_assist_proposal::Id);
			if (found == Proposals.end()) {
				return refused_assist(std::move(out), "unknown_proposal", "No such outline proposal: it was used, cancelled or expired.");
			}
			auto proposal = *found;
			if (aCompletion.Now >= proposal.Expires || proposal.DraftRevision != State.Draft->Revision) {
				Proposals.erase(found);
				++JournalGeneration;
				return refused_assist(std::move(out), aCompletion.Now >= proposal.Expires ? "expired" : "stale", "The proposal is no longer current. Ask for a new outline; nothing was inserted.");
			}
			std::vector<std::size_t> seen;
			core::tour_command       command{ .Draft = State.Draft->Id, .Owner = State.Draft->Owner, .ExpectedRevision = State.Draft->Revision, .Kind = core::tour_command_kind::insert_outline, .Position = aCompletion.Position };
			for (const auto index : aCompletion.Indices) {
				if (index >= proposal.Entries.size() || std::ranges::find(seen, index) != seen.end()) {
					return refused_assist(std::move(out), "invalid_indices", "Choose distinct suggestion numbers from the proposal.", core::tour_refusal::invalid_command);
				}
				seen.push_back(index);
				command.Outline.push_back(proposal.Entries[index]);
			}
			if (command.Outline.empty() || aCompletion.Position > State.Draft->Stops.size()) {
				return refused_assist(std::move(out), "invalid_indices", "Choose at least one suggestion and a valid position.", core::tour_refusal::invalid_command);
			}
			LabelOverride = "Insert outline stops";
			dispatch(command);
			LabelOverride.clear();
			if (State.Refusal == core::tour_refusal::none) {
				Proposals.erase(std::ranges::find(Proposals, aCompletion.Id, &tour_assist_proposal::Id));
				++JournalGeneration;
			}
			return out;
		}
	};
	review_tour_host::review_tour_host(core::review_tour aSeed, std::unique_ptr<review_tour_store> aStore, effect_executor& aEffects, decision_executor& aDecision, publication aPublish)
		: Implementation{ std::make_shared<implementation>(std::move(aSeed), std::move(aStore), aEffects, aDecision, std::move(aPublish)) }
	{ }
	review_tour_host::~review_tour_host() { shutdown(); }
	void review_tour_host::resolve_remote_conflict(std::uint64_t aRevision, bool aUseRemote)
	{
		auto& state = *Implementation;
		if (state.Closing || !state.State.Draft || state.State.Draft->Revision != aRevision) {
			return;
		}
		if (auto chosen = core::resolve_tour_conflict(*state.State.Draft, 0, aUseRemote)) {
			state.adopt_observation(std::move(*chosen));
			state.notify();
		}
	}
	void review_tour_host::observe_comparison(const core::diff_semantic_index& aIndex, const core::tour_code_destination& aLeft, const core::tour_code_destination& aRight)
	{
		auto& self = *Implementation;
		if (self.Closing || self.State.Loading || !self.State.Draft) {
			return;
		}
		self.adopt_observation(core::map_tour_revision(*self.State.Draft, aIndex, aLeft, aRight));
		self.notify();
	}

	void review_tour_host::observe_publication(core::review_tour aPublished)
	{
		auto& state = *Implementation;
		if (state.Closing || !state.State.Draft || !aPublished.Remote || state.State.Draft->Revision != aPublished.Revision) {
			return;
		}
		const auto& before   = *state.State.Draft;
		const auto  accepted = core::apply(before, { .Draft = before.Id, .Owner = before.Owner, .ExpectedRevision = before.Revision, .Kind = core::tour_command_kind::restore, .Restore = std::move(aPublished) });
		if (!accepted.Changed) {
			return;
		}
		state.Undo.clear();
		state.Redo.clear();
		state.State.Draft = std::make_shared<core::review_tour>(accepted.Accepted);
		state.pump();
		state.notify();
	}

	void review_tour_host::observe_imports(std::vector<core::tour_import_candidate> aCandidates) { Implementation->observe_imports(std::move(aCandidates)); }
	void review_tour_host::import_selected(std::size_t aIndex) { Implementation->import_selected(aIndex); }

	void review_tour_host::observe_source(core::tour_code_destination aSource)
	{
		auto& self = *Implementation;
		if (self.Closing || !core::valid(aSource) || !aSource.Blob) {
			return;
		}
		if (std::ranges::find(self.Sources, aSource) == self.Sources.end()) {
			if (self.Sources.size() == core::maximum_tour_stops) {
				self.Sources.erase(self.Sources.begin());
			}
			self.Sources.push_back(std::move(aSource));
		}
		self.notify();
	}

	void review_tour_host::start() { Implementation->start(); }
	auto review_tour_host::execute(const tour_service_request& aRequest) -> tour_service_result
	{
		auto&       self  = *Implementation;
		const auto& state = self.State;
		if (self.Closing || state.Loading || !state.Draft) {
			return { tour_service_status::busy };
		}
		if (!aRequest.RequestId.empty()) {
			const auto found = std::ranges::find(self.Receipts, aRequest.RequestId, &tour_request_receipt::Id);
			if (found != self.Receipts.end()) {
				if (found->Digest != aRequest.Digest) {
					return { tour_service_status::refused, core::tour_refusal::identity, state.Draft->Revision };
				}
				tour_service_result replay{ static_cast<tour_service_status>(found->Status), found->Refusal, found->Revision, true };
				replay.Proposal = found->Value;
				return replay;
			}
			if (aRequest.RequestId.size() > 64 || self.Receipts.size() >= maximum_tour_request_receipts) {
				return { tour_service_status::refused, core::tour_refusal::bounds, state.Draft->Revision };
			}
		}
		const auto& command  = aRequest.Command;
		const auto  revision = state.Draft->Revision;
		if (command.Owner != state.Draft->Owner || command.Draft != state.Draft->Id) {
			return { tour_service_status::refused, core::tour_refusal::identity, revision };
		}
		const bool assisting = aRequest.Operation == tour_service_operation::assist_complete || aRequest.Operation == tour_service_operation::outline_use;
		if (!assisting && command.ExpectedRevision != revision) {
			return { tour_service_status::refused, core::tour_refusal::conflict, revision };
		}
		if (state.Failure) {
			return { tour_service_status::storage_failure, core::tour_refusal::none, revision };
		}
		self.State.Refusal = core::tour_refusal::none;
		self.Origin        = aRequest.Origin;
		self.DeferringSave = true;
		implementation::assist_outcome assisted;
		switch (aRequest.Operation) {
		case tour_service_operation::edit: self.dispatch(command); break;
		case tour_service_operation::undo: self.restore(false); break;
		case tour_service_operation::redo: self.restore(true); break;
		case tour_service_operation::import_selected:
			if (aRequest.ImportIndex >= self.Imports.size()) {
				self.DeferringSave = false;
				return { tour_service_status::refused, core::tour_refusal::invalid_command, revision };
			}
			self.import_selected(aRequest.ImportIndex);
			break;
		case tour_service_operation::assist_complete: assisted = self.complete_assist(aRequest.Assist); break;
		case tour_service_operation::outline_use:     assisted = self.use_outline(aRequest.Assist); break;
		}
		// A direct call that follows is this presentation's own gesture, whatever asked before.
		self.Origin = tour_presentation_origin::desktop;
		tour_service_result result{ state.Refusal != core::tour_refusal::none ? tour_service_status::refused : (state.Draft->Revision == revision ? tour_service_status::unchanged : tour_service_status::accepted), state.Refusal, state.Draft->Revision };
		result.Reason   = std::move(assisted.Reason);
		result.Detail   = std::move(assisted.Detail);
		result.Notes    = std::move(assisted.Notes);
		result.Proposal = assisted.Proposal;
		if (!aRequest.RequestId.empty()) {
			self.Receipts.push_back({ aRequest.RequestId, aRequest.Digest, static_cast<std::uint8_t>(result.Status), result.Refusal, result.Revision, assisted.Proposal });
			++self.JournalGeneration;
		}
		self.DeferringSave = false;
		self.pump();
		return result;
	}
	auto review_tour_host::assist_issue(tour_assist_ticket aTicket, std::int64_t aNow) -> std::expected<tour_assist_ticket, std::string>
	{
		auto& self = *Implementation;
		if (self.Closing || self.State.Loading || !self.State.Draft || self.State.Failure) {
			return std::unexpected("busy");
		}
		const auto& draft = *self.State.Draft;
		if (draft.ReadOnly) {
			return std::unexpected("read_only");
		}
		if (aTicket.DraftRevision != draft.Revision) {
			return std::unexpected("stale");
		}
		if (aTicket.Hook == core::writing_hook::outline) {
			aTicket.Begin = aTicket.End = 0;
			aTicket.RawDigest           = { };
			aTicket.Stop                = { };
			aTicket.Scope               = core::writing_scope::field;
		} else {
			const auto* raw = implementation::assist_field(draft, aTicket.Hook, aTicket.Stop);
			if (!raw) {
				return std::unexpected("no_such_field");
			}
			if (core::writing_digest(*raw) != aTicket.RawDigest) {
				return std::unexpected("stale");
			}
			if (aTicket.End > raw->size() || !core::writing_boundary(*raw, aTicket.Begin) || !core::writing_boundary(*raw, aTicket.End)) {
				return std::unexpected("invalid_range");
			}
			const bool whole = aTicket.Begin == 0 && aTicket.End == raw->size();
			const auto scope = core::writing_blank(*raw) ? core::writing_scope::empty_field : whole ? core::writing_scope::field
																									: core::writing_scope::selection;
			if (scope != aTicket.Scope || (scope == core::writing_scope::selection && aTicket.Begin == aTicket.End)) {
				return std::unexpected("invalid_range");
			}
		}
		aTicket.Id      = ++self.AssistSequence;
		aTicket.Expires = aNow + std::chrono::duration_cast<std::chrono::seconds>(assist_lifetime).count();
		if (!valid(aTicket)) {
			--self.AssistSequence;
			return std::unexpected("invalid_ticket");
		}
		self.prune_assist(aNow);
		self.Tickets.clear();
		self.Tickets.push_back(aTicket);
		++self.JournalGeneration;
		self.pump();
		return aTicket;
	}
	auto review_tour_host::assist_ticket(std::uint64_t aId) const -> std::optional<tour_assist_ticket>
	{
		const auto found = std::ranges::find(Implementation->Tickets, aId, &tour_assist_ticket::Id);
		return found == Implementation->Tickets.end() ? std::nullopt : std::optional{ *found };
	}
	auto review_tour_host::assist_proposal(std::uint64_t aId) const -> std::optional<tour_assist_proposal>
	{
		const auto found = std::ranges::find(Implementation->Proposals, aId, &tour_assist_proposal::Id);
		return found == Implementation->Proposals.end() ? std::nullopt : std::optional{ *found };
	}
	void review_tour_host::assist_cancel()
	{
		auto& self = *Implementation;
		if (self.Tickets.empty() && self.Proposals.empty()) {
			return;
		}
		self.Tickets.clear();
		self.Proposals.clear();
		++self.JournalGeneration;
		self.pump();
	}
	auto review_tour_host::request_status(std::string_view aId) const -> std::optional<tour_request_receipt>
	{
		const auto& records = Implementation->Receipts;
		const auto  found   = std::ranges::find(records, aId, &tour_request_receipt::Id);
		return found == records.end() ? std::nullopt : std::optional{ *found };
	}
	void review_tour_host::dispatch(const core::tour_command& aCommand) { Implementation->dispatch(aCommand); }
	void review_tour_host::undo() { Implementation->restore(false); }
	void review_tour_host::redo() { Implementation->restore(true); }
	void review_tour_host::retry_save()
	{
		auto& self = *Implementation;
		if (self.Closing || self.State.Conflict) {
			return;
		}
		self.State.Failure.reset();
		self.pump();
		self.notify();
	}
	void review_tour_host::use_saved_copy()
	{
		auto& self = *Implementation;
		if (self.Closing || self.Active || !self.State.Conflict) {
			return;
		}
		self.State.Refusal       = core::tour_refusal::none;
		self.State.Draft         = self.State.Conflict;
		self.State.SavedRevision = self.State.Draft->Revision;
		self.State.Conflict.reset();
		self.State.Failure.reset();
		self.Undo.clear();
		self.Redo.clear();
		self.notify();
	}
	void review_tour_host::shutdown()
	{
		if (Implementation) {
			Implementation->Closing = true;
			Implementation->Publish = { };
			const auto& state       = Implementation->State;
			const bool  untouched   = state.Draft && state.SavedRevision == 0U && state.Draft->Revision == 1U && state.Draft->Problem.empty() && state.Draft->Solution.empty() && state.Draft->Stops.empty() && !state.Draft->ProblemBaseline && !state.Draft->SolutionBaseline;
			// Opening a PR observes its optional tour; it does not consume a
			// durable draft slot unless the author actually changes something.
			if (!untouched) {
				Implementation->pump();
			}
		}
	}
	auto review_tour_host::presentation() const -> tour_presentation { return Implementation->State; }
	auto review_tour_host::is_quiescent() const noexcept -> bool { return !Implementation->Active; }
}
