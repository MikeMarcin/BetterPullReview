#include "bpr/application/review_tour_host.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include <algorithm>
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
		struct history_entry
		{
			core::review_tour  Draft;
			core::tour_stop_id Selected;
		};
		std::deque<history_entry> Undo;
		std::deque<history_entry> Redo;
		std::uint64_t             Request = 0;
		bool                      Active  = false;
		bool                      Started = false;
		bool                      Closing = false;
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
			State.CanUndo = !Undo.empty();
			State.CanRedo = !Redo.empty();
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
					auto observed = tour_store_result<std::optional<core::review_tour>>{ std::unexpected(tour_store_error{ }) };
					try {
						observed = self->Store->load(self->Seed.Owner, self->Seed.Id, { });
					} catch (...) { }
					if (observed && *observed && (core::validate(**observed) != core::tour_refusal::none || (**observed).Owner != self->Seed.Owner || (**observed).Id != self->Seed.Id)) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					self->Decision.post([self, request, result = std::move(observed)]() mutable {
						if (!self->Active || request != self->Request) {
							return;
						}
						self->Active        = false;
						self->State.Loading = false;
						if (result && *result) {
							self->State.Draft         = std::make_shared<core::review_tour>(std::move(**result));
							self->State.SavedRevision = self->State.Draft->Revision;
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
			if (Active || !State.Draft || State.Failure || State.SavedRevision == State.Draft->Revision) {
				return;
			}
			Active               = true;
			State.Saving         = true;
			const auto request   = ++Request;
			const auto self      = shared_from_this();
			const auto candidate = State.Draft;
			const auto expected  = State.SavedRevision;
			try {
				Effects.post([self, request, candidate, expected] {
					auto observed = tour_store_result<core::review_tour>{ std::unexpected(tour_store_error{ }) };
					try {
						observed = self->Store->save(*candidate, expected, { });
					} catch (...) { }
					if (observed && *observed != *candidate) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					if (!observed && observed.error().Current && (core::validate(*observed.error().Current) != core::tour_refusal::none || observed.error().Current->Owner != candidate->Owner || observed.error().Current->Id != candidate->Id)) {
						observed = std::unexpected(tour_store_error{ tour_store_failure::corrupt, { } });
					}
					self->Decision.post([self, request, result = std::move(observed)]() mutable {
						if (!self->Active || request != self->Request) {
							return;
						}
						self->Active       = false;
						self->State.Saving = false;
						if (result) {
							self->State.SavedRevision = result->Revision;
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
		void retain(std::deque<history_entry>& aHistory, core::tour_stop_id aSelected)
		{
			aHistory.push_back({ *State.Draft, aSelected });
			auto bytes = std::size_t{ 0 };
			for (const auto& item : aHistory) {
				bytes += core::retained_tour_bytes(item.Draft);
			}
			while (aHistory.size() > 16 || bytes > 8U * 1024U * 1024U) {
				bytes -= core::retained_tour_bytes(aHistory.front().Draft);
				aHistory.pop_front();
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
				const bool text = aCommand.Kind == core::tour_command_kind::problem || aCommand.Kind == core::tour_command_kind::solution || aCommand.Kind == core::tour_command_kind::edit_stop;
				if (text || aCommand.Kind == core::tour_command_kind::observe_head) {
					Undo.clear();
					Redo.clear();
				} else if (aCommand.Kind != core::tour_command_kind::visit) {
					retain(Undo, aCommand.Stop.value() ? aCommand.Stop : State.Selected);
					++State.SelectionRevision;
					Redo.clear();
				}
				State.Draft    = std::make_shared<core::review_tour>(outcome.Accepted);
				State.Selected = outcome.Selected;
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
				retain(to, State.Selected);
				State.Selected = from.back().Selected;
				++State.SelectionRevision;
				from.pop_back();
				State.Draft = std::make_shared<core::review_tour>(std::move(outcome.Accepted));
				pump();
			}
			notify();
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
		const auto& command  = aRequest.Command;
		const auto  revision = state.Draft->Revision;
		if (command.Owner != state.Draft->Owner || command.Draft != state.Draft->Id) {
			return { tour_service_status::refused, core::tour_refusal::identity, revision };
		}
		if (command.ExpectedRevision != revision) {
			return { tour_service_status::refused, core::tour_refusal::conflict, revision };
		}
		if (state.Failure) {
			return { tour_service_status::storage_failure, core::tour_refusal::none, revision };
		}
		self.State.Refusal = core::tour_refusal::none;
		switch (aRequest.Operation) {
		case tour_service_operation::edit: self.dispatch(command); break;
		case tour_service_operation::undo: self.restore(false); break;
		case tour_service_operation::redo: self.restore(true); break;
		case tour_service_operation::import_selected:
			if (aRequest.ImportIndex >= self.Imports.size()) {
				return { tour_service_status::refused, core::tour_refusal::invalid_command, revision };
			}
			self.import_selected(aRequest.ImportIndex);
			break;
		}
		return { state.Refusal != core::tour_refusal::none ? tour_service_status::refused : (state.Draft->Revision == revision ? tour_service_status::unchanged : tour_service_status::accepted), state.Refusal, state.Draft->Revision };
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
