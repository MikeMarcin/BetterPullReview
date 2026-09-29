#include "tour_runtime.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_tour_import.hpp"
#include "bpr/core/sha256.hpp"
#include "review_submission_execution_authority.hpp"
#include "tour_cancellation.hpp"
#include "tour_decisions.hpp"
#include "tour_observation.hpp"
#include "tour_owner_service.hpp"
#include <QCoreApplication>
#include <QThread>
#include <algorithm>
#include <future>
namespace bpr::app {
	namespace {
		auto failure(std::string aCode, std::string aMessage, int aExit = 7) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		template <typename T>
		auto store_failure(const application::tour_store_result<T>& aResult)
		{
			return failure(aResult.error().Failure == application::tour_store_failure::conflict ? "revision_conflict" : "storage_unavailable", "Private tour storage refused the operation; retained work was not reset.", aResult.error().Failure == application::tour_store_failure::conflict ? 4 : 7);
		}
	}
	namespace {
		class submission_runner final : public application::review_submission_projection_port
		{
		public:
			explicit submission_runner(const tour_publication_context& aContext)
				: Host{ aContext.Factory, aContext.Binding, aContext.Session, core::request_generation{ 1 }, Effects, Decisions, *this }
			{ }
			~submission_runner() override
			{
				Host.shutdown();
				drain();
				Effects.close_and_drain();
			}
			void publish(const application::review_submission_host_terminal& aTerminal) override { Terminal = aTerminal; }
			auto discover() -> application::staged_review_discovery
			{
				Terminal.reset();
				if (!Host.discover_tours()) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				drain();
				if (Terminal) {
					if (const auto* value = std::get_if<application::staged_review_discovery>(&Terminal->Outcome)) {
						return *value;
					}
				}
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			auto execute(const application::staged_review_plan& aPlan, application::review_submission_execution_grant aGrant = { }) -> application::staged_publication_result
			{
				Terminal.reset();
				const auto request = aGrant ? Host.publish_tour(aPlan, std::move(aGrant)) : Host.reconcile_tour(aPlan);
				if (!request) {
					return { };
				}
				drain();
				if (Terminal) {
					if (const auto* value = std::get_if<application::staged_publication_result>(&Terminal->Outcome)) {
						return *value;
					}
				}
				return { };
			}

		private:
			void drain()
			{
				while (!Host.is_quiescent()) {
					if (tour_cancelled()) {
						Host.shutdown();
					}
					Decisions.drain();
					QCoreApplication::processEvents();
					if (!Host.is_quiescent()) {
						QThread::msleep(1);
					}
				}
				Decisions.drain();
			}
			worker_executor                                             Effects;
			terminal_decisions                                          Decisions;
			std::optional<application::review_submission_host_terminal> Terminal;
			application::review_submission_application_host             Host;
		};
		auto publication_status(application::staged_publication_state aState) -> std::string
		{
			using state = application::staged_publication_state;
			switch (aState) {
			case state::ready:                 return "Prepared locally; preview and explicitly resume the captured plan.";
			case state::staged:                return "Staged on GitHub; author-only pending review. Preview and explicitly resume.";
			case state::published:             return "Published; confirmed identities and navigation retained.";
			case state::outcome_unknown:       return "Outcome unknown; recover before any further write. No ambiguous send is repeated.";
			case state::conflict:              return "Publication conflict; actor, head, body or pending review changed. No further writes.";
			case state::partial:               return "Partially updated; confirmed visible comments retained. Preview before resuming.";
			case state::description_attention: return "Tour published; separate description update needs attention.";
			case state::unavailable:           return "Publication unavailable; private draft retained.";
			}
			return "Publication unavailable.";
		}
		auto resumable(application::staged_publication_state aState) -> bool
		{
			using state = application::staged_publication_state;
			return aState == state::ready || aState == state::staged || aState == state::partial || aState == state::description_attention;
		}
	}

	struct tour_runtime::implementation
	{
		worker_executor                                     Effects;
		application::manual_executor                        Decisions;
		std::filesystem::path                               Root;
		core::bpr_profile_id                                Profile;
		std::shared_ptr<application::profile_runtime_lease> Lease;
		std::unique_ptr<application::review_tour_store>     Store;
		tour_reader_factory                                 Readers;
		std::optional<tour_cli_error>                       Failure;
		bool                                                Forward = false;
		template <typename TFunction>
		auto effect(TFunction aFunction)
		{
			using result_type = decltype(aFunction());
			auto task         = std::make_shared<std::packaged_task<result_type()>>(std::move(aFunction));
			auto result       = task->get_future();
			Effects.post([task] { (*task)(); });
			return result.get();
		}
		implementation(std::filesystem::path aBase, std::string aProfile, tour_reader_factory aReaders)
			: Readers{ std::move(aReaders) }
		{
			if (!Readers) {
				Readers = make_tour_remote_reader;
			}
			if (!effect([&] { return adapters::sqlite::create_storage_root_owner_private(aBase); })) {
				Failure = tour_cli_error{ "profile_unavailable", "The application root is unavailable or is not owner-private.", 7 };
				return;
			}
			auto registry = adapters::sqlite::make_profile_registry_store(aBase);
			auto loaded   = effect([&] { return registry->load({ }); });
			if (!loaded) {
				Failure = tour_cli_error{ "profile_unavailable", "The private profile registry could not be opened.", 7 };
				return;
			}
			const auto matches = [&](const auto& aEntry) { return aProfile.empty() ? aEntry.Profile.Id == core::default_bpr_profile_id : aProfile == std::to_string(aEntry.Profile.Id.value()) || aProfile == aEntry.Profile.Label; };
			if (std::ranges::count_if(loaded->Profiles, matches) > 1) {
				Failure = tour_cli_error{ "profile_ambiguous", "The profile label is ambiguous; select its numeric ID.", 2 };
				return;
			}
			const auto found = std::ranges::find_if(loaded->Profiles, matches);
			if (found == loaded->Profiles.end()) {
				Failure = tour_cli_error{ "profile_unknown", "Select an existing profile by its exact name or ID.", 2 };
				return;
			}
			Profile        = found->Profile.Id;
			auto authority = application::profile_storage_authority::select(*loaded, Profile);
			if (!authority) {
				Failure = tour_cli_error{ "profile_unavailable", "The profile root authority is invalid.", 7 };
				return;
			}
			auto root = effect([&] { return adapters::sqlite::resolve_profile_storage_root(aBase, *authority); });
			if (!root) {
				Failure = tour_cli_error{ "profile_unavailable", "The profile root is unavailable or is not owner-private.", 7 };
				return;
			}
			Root       = *root;
			auto lease = effect([&] { return adapters::sqlite::acquire_profile_runtime_lease(Root); });
			if (!lease) {
				if (lease.error() == application::profile_runtime_lease_failure::busy) {
					Forward = true;
				} else {
					Failure = tour_cli_error{ "profile_unavailable", "The profile lease is unavailable.", 7 };
				}
				return;
			}
			Lease = std::move(*lease);
			Store = adapters::sqlite::make_review_tour_store(Root, Profile);
		}
		static auto owner_exit(std::string_view aCode) -> int
		{
			if (aCode == "storage_unavailable" || aCode == "owner_unavailable") {
				return 7;
			}
			for (const auto invalid : { "command_refused", "target_ambiguous", "invalid_request", "request_id_required", "invalid_result", "invalid_range", "ambiguous_quote", "quote_not_found", "insufficient_context", "context_too_large", "unknown_stop", "invalid_indices", "source_unavailable", "empty_outline", "invalid_input", "read_only" }) {
				if (aCode == invalid) {
					return 2;
				}
			}
			return 4;
		}
		/// The owner's answer as a CLI result; `aMessage` is its own bounded line when it gave one.
		auto owner_failure(const application::tour_owner_response& aResponse) -> tour_cli_error
		{
			const auto& code = aResponse.Error;
			if (code == "editor_busy") {
				return { code, "The desktop editor holds uncommitted input-method text. Finish or cancel the composition, then retry; nothing was changed.", 4 };
			}
			if (code == "owner_busy") {
				return { code, "The owner is loading or closing that draft. Nothing was changed; retry shortly.", 4 };
			}
			if (code == "save_pending") {
				return { "outcome_unknown", "The owner has not confirmed a durable save. Inspect the exact request ID before any retry; no replay was attempted.", 6 };
			}
			return { code, aResponse.Json.empty() ? "The owner refused this command. Current text is retained; inspect the exact draft and request." : aResponse.Json, owner_exit(code) };
		}
		auto exchange(const application::tour_owner_request& aRequest) -> tour_cli_result<application::tour_owner_response>
		{
			const auto response = effect([&] { return adapters::sqlite::send_tour_owner(Root, Profile, adapters::sqlite::encode_tour_request(aRequest)); });
			if (!response) {
				if (response.error() == application::tour_channel_failure::indeterminate) {
					return failure("outcome_unknown", "Owner delivery is uncertain. Inspect the exact request ID before any retry; no replay was attempted.", 6);
				}
				return failure("owner_protocol_unsupported", "The profile is owned but its tour channel is unavailable. Update/reopen that owner; no second writer was opened.", 4);
			}
			auto decoded = adapters::sqlite::decode_tour_response(*response);
			if (!decoded) {
				return failure("outcome_unknown", "The owner response is invalid. Inspect the request before retrying.", 6);
			}
			if (!decoded->Error.empty()) {
				return std::unexpected(owner_failure(*decoded));
			}
			return std::move(*decoded);
		}
		auto forward(application::tour_owner_request aRequest) -> tour_cli_result<tour_local_snapshot>
		{
			const auto decoded = exchange(aRequest);
			if (!decoded) {
				return std::unexpected(decoded.error());
			}
			if (!decoded->Draft || !decoded->Target || decoded->Target->Owner.Profile != Profile || decoded->Draft->Owner != decoded->Target->Owner || draft_token(*decoded->Target) != aRequest.Draft) {
				return failure("identity_changed", "The owner returned another draft.", 4);
			}
			tour_local_snapshot snapshot{ *decoded->Target, *decoded->Draft, decoded->UndoLabel, decoded->RedoLabel, "not_refreshed", 0, decoded->Resume, decoded->Receipt };
			snapshot.AssistJson = decoded->Json;
			return snapshot;
		}

		auto observe_receipt(const application::tour_saved_target& aTarget, const core::review_tour& aDraft, const application::staged_publication_record& aRecord) -> bool
		{
			const auto published = application::published_tour(aRecord, aDraft);
			// A newer local draft is preserved for the importer; a receipt cannot overwrite it.
			if (!published) {
				return true;
			}
			application::manual_executor  effects, decisions;
			application::review_tour_host host{ aDraft, adapters::sqlite::make_review_tour_store(Root, Profile), effects, decisions, { } };
			host.start();
			effect([&] { effects.drain(); });
			decisions.drain();
			host.observe_publication(*published);
			host.shutdown();
			while (!host.is_quiescent()) {
				effect([&] { effects.drain(); });
				decisions.drain();
			}
			return !host.presentation().Failure && host.presentation().Draft->Owner == aTarget.Owner;
		}

		auto snapshot(const application::tour_saved_target& aTarget) -> tour_cli_result<tour_local_snapshot>
		{
			auto value = effect([&] { return Store->load_state(aTarget.Owner, aTarget.Draft, { }); });
			if (!value) {
				return store_failure(value);
			}
			if (!*value) {
				return tour_local_snapshot{ aTarget, core::review_tour{ .Id = aTarget.Draft, .Owner = aTarget.Owner, .Base = aTarget.Base, .Head = aTarget.Head }, { }, { } };
			}
			const auto& state = value->value();
			return tour_local_snapshot{ aTarget, state.Draft, state.Undo.empty() ? std::string{ } : state.Undo.back().Label, state.Redo.empty() ? std::string{ } : state.Redo.back().Label, "not_refreshed", 0, state.Resume };
		}
	};
	tour_runtime::tour_runtime(std::filesystem::path aBase, std::string aProfile, tour_reader_factory aReaders)
		: Implementation{ std::make_unique<implementation>(std::move(aBase), std::move(aProfile), std::move(aReaders)) }
	{ }
	tour_runtime::~tour_runtime() { Implementation->Effects.close_and_drain(); }
	auto tour_runtime::admission() const -> tour_cli_result<core::bpr_profile_id> { return Implementation->Failure ? tour_cli_result<core::bpr_profile_id>{ std::unexpected(*Implementation->Failure) } : tour_cli_result<core::bpr_profile_id>{ Implementation->Profile }; }
	auto tour_runtime::owner() const -> tour_cli_result<owner_binding>
	{
		const auto& self = *Implementation;
		if (self.Failure || !self.Lease) {
			return failure("owner_unavailable", "This runtime does not own its profile.", 4);
		}
		return owner_binding{ self.Root, self.Profile, self.Lease };
	}
	auto tour_runtime::draft_token(const application::tour_saved_target& aTarget) -> std::string { return tour_draft_token(aTarget); }
	auto tour_runtime::connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>
	{
		if (Implementation->Forward) {
			return failure("interaction_required", "Use the owning desktop for provider observation or publication. Local draft commands remain available through its tour channel.", 3);
		}
		auto& self = *Implementation;
		if (self.Failure) {
			return std::unexpected(*self.Failure);
		}
		auto reader = self.Readers(self.Root, self.Profile);
		return reader->connect(std::move(aHostname), aGh, aInteractive);
	}
	auto tour_runtime::load(std::string_view aDraft, bool aMachine) -> tour_cli_result<tour_local_snapshot>
	{
		auto& self = *Implementation;
		if (self.Failure) {
			return std::unexpected(*self.Failure);
		}
		if (aDraft.empty() && aMachine) {
			return failure("target_ambiguous", "Machine commands require --profile and the draft_id returned by open.", 2);
		}
		if (self.Forward) {
			return self.forward({ .Draft = std::string{ aDraft } });
		}
		auto targets = self.effect([&] { return self.Store->targets({ }); });
		if (!targets) {
			return store_failure(targets);
		}
		std::erase_if(*targets, [&](const auto& aTarget) { return !aDraft.empty() && draft_token(aTarget) != aDraft; });
		if (targets->size() != 1) {
			return failure("target_ambiguous", "Select one exact --draft returned by open; no global last-opened PR is used.", 2);
		}
		return self.snapshot(targets->front());
	}
	auto tour_runtime::open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh, std::string aRoot, bool aOffline) -> tour_cli_result<tour_local_snapshot>
	{
		if (Implementation->Forward) {
			return failure("interaction_required", "Use the owning desktop for provider observation or publication. Local draft commands remain available through its tour channel.", 3);
		}
		auto& self = *Implementation;
		if (self.Failure) {
			return std::unexpected(*self.Failure);
		}
		if (aOffline) {
			auto targets = self.effect([&] { return self.Store->targets({ }); });
			if (!targets) {
				return store_failure(targets);
			}
			std::erase_if(*targets, [&](const auto& aTarget) { return aTarget.Hostname != aHostname || aTarget.Repository != aRepository || aTarget.Number != aNumber; });
			if (targets->size() != 1 || !aRoot.empty()) {
				return failure("target_ambiguous", "Offline open requires one previously observed target. Use its explicit draft_id.", 2);
			}
			return self.snapshot(targets->front());
		}
		auto reader = self.Readers(self.Root, self.Profile);
		auto read   = reader->open(std::move(aHostname), std::move(aRepository), aNumber, aGh);
		if (!read) {
			return std::unexpected(read.error());
		}
		if (read->Target.Owner.Profile != self.Profile || read->Target.Owner.Account != read->Snapshot.Account || read->Target.Owner.Host != read->Snapshot.Host || read->Target.Owner.Repository != read->Snapshot.Repository.Repository || read->Target.Owner.PullRequest != read->Snapshot.PullRequest.PullRequest) {
			return failure("identity_changed", "The accepted read does not match the selected owner.", 4);
		}
		auto snapshot = self.snapshot(read->Target);
		if (!snapshot) {
			return snapshot;
		}
		auto seed    = snapshot->Draft;
		auto imports = observe_published_tours(core::review_tour{ .Id = read->Target.Draft, .Owner = read->Target.Owner, .Base = read->Target.Base, .Head = read->Target.Head }, read->Snapshot, read->Target.Hostname);
		if (imports.empty() && !application::review_tour_observation_complete(read->Snapshot)) {
			return failure("remote_incomplete", "Published collections or bodies are unavailable. No empty tour is inferred; existing private work is retained.", 5);
		}
		std::optional<std::size_t> selected;
		for (std::size_t i = 0; i < imports.size(); ++i) {
			const auto& remote = imports[i].Draft.Remote;
			if (remote && (aRoot == imports[i].Label || aRoot == remote->WebUrl || aRoot == remote->Entity || aRoot == std::to_string(remote->DatabaseId))) {
				if (selected) {
					return failure("target_ambiguous", "This root contains several lists; select a unique candidate label.", 2);
				}
				selected = i;
			}
		}
		if (!aRoot.empty() && !selected) {
			return failure("target_ambiguous", "The requested published root is not among the observed candidates.", 2);
		}
		if (imports.size() > 1 && !selected && !seed.Remote) {
			std::string choices = "Several published tour roots were observed. Select --root with one exact label:";
			for (const auto& item : imports) {
				choices += "\n" + item.Label;
			}
			return failure("target_ambiguous", std::move(choices), 2);
		}
		if (imports.size() == 1 && !selected) {
			selected = 0;
		}
		// This bounded synchronous transaction uses the same host/importer as the desktop.
		application::manual_executor  effects, decisions;
		application::review_tour_host host{ seed, adapters::sqlite::make_review_tour_store(self.Root, self.Profile), effects, decisions, { } };
		host.start();
		self.effect([&] { effects.drain(); });
		decisions.drain();
		if (host.presentation().Draft->Head != read->Target.Head) {
			const auto& current = *host.presentation().Draft;
			host.dispatch({ .Draft = current.Id, .Owner = current.Owner, .ExpectedRevision = current.Revision, .Kind = core::tour_command_kind::observe_head, .Head = read->Target.Head });
		}
		host.observe_imports(std::move(imports));
		if (selected) {
			host.import_selected(*selected);
		}
		host.shutdown();
		while (!host.is_quiescent()) {
			self.effect([&] { effects.drain(); });
			decisions.drain();
		}
		auto state = host.presentation();
		if (state.Failure) {
			return failure("storage_unavailable", "The imported draft could not be saved; existing local work remains retained.");
		}
		auto saved = self.effect([&] { return self.Store->remember_target(read->Target, { }); });
		if (!saved) {
			return store_failure(saved);
		}
		auto result = *state.Draft;
		if (!state.ImportNotice.empty()) {
			result.ImportNotice += state.ImportNotice;
		}
		const auto pending = static_cast<std::size_t>(std::ranges::count(read->Snapshot.Reviews, application::github_review_state::pending, &application::github_review_record::State));
		if (pending) {
			result.ImportNotice += " Pending server reviews are candidates only; absent local WIP and approval are unknown. No automatic adoption, editing, deletion or submission.";
		}
		const auto observation = !application::review_tour_observation_complete(read->Snapshot) ? "partial" : result.RemoteStale ? "stale"
			: result.Remote                                                                                                      ? "complete"
																																 : "no_published_root";
		return tour_local_snapshot{ read->Target, std::move(result), state.UndoLabel, state.RedoLabel, observation, pending };
	}
	auto tour_runtime::source(std::string_view aDraft, const core::tour_code_destination& aDestination, bool aGh) -> tour_cli_result<tour_source_material>
	{
		if (Implementation->Forward) {
			return failure("interaction_required", "Use the owning desktop for provider observation or publication. Local draft commands remain available through its tour channel.", 3);
		}
		if (!core::valid(aDestination)) {
			return failure("invalid_source", "The source destination is invalid.", 2);
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return std::unexpected(loaded.error());
		}
		auto&       self   = *Implementation;
		auto        reader = self.Readers(self.Root, self.Profile);
		const auto& target = loaded->Target;
		auto        read   = reader->open(target.Hostname, target.Repository, target.Number, aGh);
		if (!read) {
			return std::unexpected(read.error());
		}
		if (read->Target.Owner != target.Owner || read->Target.Draft != target.Draft) {
			return failure("identity_changed", "The source read belongs to a different account or target.", 4);
		}
		return reader->source(aDestination);
	}

	auto tour_runtime::mutate(std::string_view aDraft, application::tour_service_request aRequest) -> tour_cli_result<tour_local_snapshot>
	{
		// Identity is fixed before the target is bound so a standalone and a forwarded retry agree.
		aRequest.Digest = adapters::sqlite::tour_request_digest(aRequest);
		if (Implementation->Forward) {
			return Implementation->forward({ application::tour_owner_operation::mutate, std::string{ aDraft }, std::move(aRequest) });
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return loaded;
		}
		auto& self = *Implementation;
		return mutate_local_tour(self.Root, self.Profile, *loaded, std::move(aRequest), [&](std::function<void()> aTask) { self.effect([&] { aTask(); return 0; }); });
	}
	auto tour_runtime::request_status(std::string_view aDraft, std::string_view aRequest) -> tour_cli_result<tour_local_snapshot>
	{
		if (Implementation->Forward) {
			application::tour_service_request edit;
			edit.RequestId = std::string{ aRequest };
			return Implementation->forward({ application::tour_owner_operation::request_status, std::string{ aDraft }, std::move(edit) });
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return loaded;
		}
		auto& self  = *Implementation;
		auto  state = self.effect([&] { return self.Store->load_state(loaded->Target.Owner, loaded->Target.Draft, { }); });
		if (!state) {
			return store_failure(state);
		}
		if (state->has_value()) {
			const auto& records = state->value().Receipts;
			if (const auto found = std::ranges::find(records, aRequest, &application::tour_request_receipt::Id); found != records.end()) {
				loaded->Receipt = *found;
			}
		}
		return loaded;
	}
	auto tour_runtime::assist_issue(std::string_view aDraft, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra) -> tour_cli_result<tour_local_snapshot>
	{
		auto& self = *Implementation;
		if (self.Forward) {
			if (!aExtra.empty()) {
				return failure("interaction_required", "Source evidence is read through the provider, which a running desktop owner does not yet forward. Ask without --sources, or close the desktop.", 3);
			}
			application::tour_owner_request request{ .Operation = application::tour_owner_operation::assist_issue, .Draft = std::string{ aDraft } };
			request.Assist = { .Hook = aQuery.Hook, .Stop = aQuery.Stop, .HasRange = aQuery.Range.has_value(), .HasQuote = aQuery.Quote.has_value(), .Begin = aQuery.Range ? aQuery.Range->first : 0U, .End = aQuery.Range ? aQuery.Range->second : 0U, .Quote = aQuery.Quote.value_or(std::string{ }), .Session = aQuery.Session };
			return self.forward(std::move(request));
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return loaded;
		}
		return issue_local_assist(self.Root, self.Profile, *loaded, aQuery, std::move(aExtra), [&](std::function<void()> aTask) { self.effect([&] { aTask(); return 0; }); });
	}
	auto tour_runtime::assist_cancel(std::string_view aDraft) -> tour_cli_result<tour_local_snapshot>
	{
		auto& self = *Implementation;
		if (self.Forward) {
			return self.forward({ .Operation = application::tour_owner_operation::assist_cancel, .Draft = std::string{ aDraft } });
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return loaded;
		}
		return cancel_local_assist(self.Root, self.Profile, *loaded, [&](std::function<void()> aTask) { self.effect([&] { aTask(); return 0; }); });
	}
	auto tour_runtime::hooks_show() -> tour_cli_result<std::string>
	{
		auto& self = *Implementation;
		if (self.Failure) {
			return std::unexpected(*self.Failure);
		}
		if (self.Forward) {
			const auto decoded = self.exchange({ .Operation = application::tour_owner_operation::hooks_show });
			if (!decoded) {
				return std::unexpected(decoded.error());
			}
			return decoded->Json;
		}
		const auto settings = self.effect([&] { return load_tour_hook_settings(self.Root); });
		if (!settings) {
			return std::unexpected(settings.error());
		}
		return tour_hooks_json(*settings);
	}
	auto tour_runtime::hooks_store(core::writing_hook aHook, std::string aInstructions) -> tour_cli_result<std::string>
	{
		auto& self = *Implementation;
		if (self.Failure) {
			return std::unexpected(*self.Failure);
		}
		if (self.Forward) {
			return failure("interaction_required", "A running desktop owns these settings. Change the hook in its Agents & context settings, or close it first.", 3);
		}
		const auto settings = self.effect([&] { return store_tour_hook_override(self.Root, aHook, std::move(aInstructions)); });
		if (!settings) {
			return std::unexpected(settings.error());
		}
		return tour_hooks_json(*settings);
	}
	auto tour_runtime::prepare_publication(std::string_view aDraft, bool aGh, tour_publication_options aOptions, std::string_view aAttempt, bool aRecover) -> tour_cli_result<tour_publication_preview>
	{
		if (Implementation->Forward) {
			return failure("interaction_required", "Use the owning desktop for provider observation or publication. Local draft commands remain available through its tour channel.", 3);
		}
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return std::unexpected(loaded.error());
		}
		auto&       self   = *Implementation;
		auto        reader = self.Readers(self.Root, self.Profile);
		const auto& target = loaded->Target;
		auto        read   = reader->open(target.Hostname, target.Repository, target.Number, aGh);
		if (!read) {
			return std::unexpected(read.error());
		}
		if (read->Target.Owner != target.Owner || read->Target.Draft != target.Draft) {
			return failure("identity_changed", "The current account or PR differs from the saved draft.", 4);
		}
		if (!application::review_tour_observation_complete(read->Snapshot)) {
			return failure("remote_incomplete", "Publication requires a complete current observation. Missing collections or bodies cannot authorize an empty update.", 5);
		}
		auto context = reader->publication();
		if (!context) {
			return std::unexpected(context.error());
		}
		tour_publication_preview preview{ .Snapshot = *loaded, .Options = aOptions };
		preview.PendingServerReviews = static_cast<std::size_t>(std::ranges::count(read->Snapshot.Reviews, application::github_review_state::pending, &application::github_review_record::State));
		submission_runner runner{ *context };
		auto              pending = runner.discover();
		if (!pending) {
			return failure("recovery_unavailable", "The existing publication journal could not be read; no new attempt is allowed.", 7);
		}
		std::erase_if(*pending, [&](const auto& aPlan) { return aPlan.Owner != target.Owner || aPlan.Submission.Proposal.Draft.value() != target.Draft.value(); });
		if (!aAttempt.empty() && !pending->empty()) {
			std::erase_if(*pending, [&](const auto& aPlan) { return aPlan.Submission.IdempotencyKey != aAttempt; });
			if (pending->empty()) {
				return failure("attempt_unknown", "Another journaled attempt is unresolved. Recover its exact attempt ID before preparing new writes.", 4);
			}
		}
		if (pending->size() > 1) {
			return failure("target_ambiguous", "Several journaled attempts exist; select --attempt with an exact attempt ID.", 2);
		}
		if (pending->empty() && !aAttempt.empty()) {
			auto saved = context->Attempt ? self.effect([&] { return context->Attempt(aAttempt); }) : tour_cli_result<std::optional<application::staged_review_plan>>{ std::nullopt };
			if (!saved) {
				return std::unexpected(saved.error());
			}
			if (!*saved || !application::valid(**saved) || (**saved).Owner != target.Owner || (**saved).Submission.Proposal.Draft.value() != target.Draft.value()) {
				return failure("attempt_unknown", "No matching plan exists in this private journal. Remote pending data cannot recover lost authorization.", 4);
			}
			pending->push_back(std::move(**saved));
		}
		if (!pending->empty()) {
			preview.Plan   = pending->front();
			preview.Result = runner.execute(*preview.Plan);
			preview.Resume = true;
			preview.Status = publication_status(preview.Result.State);
		} else {
			const auto prepared = prepare_tour_plan(loaded->Draft, context->Binding, read->Snapshot.PullRequest.Body, aOptions, context->Qualified);
			preview.Plan        = prepared.Plan;
			preview.Status      = prepared.Status;
			if (read->Target.Head != loaded->Draft.ObservedHead.value_or(loaded->Draft.Head)) {
				preview.Plan.reset();
				preview.Status = "The PR head changed. Reopen this exact draft and review its source attention before publication.";
			}
			if (preview.Plan && (aAttempt.empty() || preview.Plan->Submission.IdempotencyKey == aAttempt)) {
				// A completed journal may outlive a crash before local attribution.
				auto recovered = runner.execute(*preview.Plan);
				if (recovered.Record) {
					preview.Result = std::move(recovered);
					preview.Resume = true;
					preview.Status = publication_status(preview.Result.State);
				}
			} else if (!aAttempt.empty()) {
				return failure("attempt_unknown", "This attempt is absent from the admitted local journal. Remote pending material is not authorization.", 4);
			}
			if (!preview.Result.Record) {
				preview.Result.State = preview.Plan ? application::staged_publication_state::ready : application::staged_publication_state::unavailable;
				if (aRecover) {
					preview.Plan.reset();
					preview.Result.State = application::staged_publication_state::unavailable;
					preview.Status       = "No matching local publication receipt. Published content can be reopened from GitHub; private WIP and authorization are not recoverable from pending reviews.";
				}
				if (preview.PendingServerReviews) {
					preview.Plan.reset();
					preview.Result.State = application::staged_publication_state::unavailable;
					preview.Status       = "Pending server review candidates exist without this attempt's receipt. No automatic adoption, edit, deletion or submission is permitted.";
				}
			}
		}
		if (preview.Result.Record && preview.Result.Record->Published) {
			if (!self.observe_receipt(target, loaded->Draft, *preview.Result.Record)) {
				return failure("receipt_save_failed", "GitHub publication was observed but local attribution could not be saved. Recover again; do not republish.", 6);
			}
			auto refreshed = self.snapshot(target);
			if (!refreshed) {
				return std::unexpected(refreshed.error());
			}
			preview.Snapshot = *refreshed;
			if (preview.Result.Record->Plan.Generation != loaded->Draft.Revision) {
				preview.Status += " Newer local text is retained; reopen to reconcile website changes.";
			}
		}
		return preview;
	}

	auto tour_runtime::publish(const tour_publication_preview& aPreview, bool aGh) -> tour_cli_result<tour_publication_preview>
	{
		if (Implementation->Forward) {
			return failure("interaction_required", "Use the owning desktop for provider observation or publication. Local draft commands remain available through its tour channel.", 3);
		}
		if (!aPreview.Plan) {
			return failure("publication_unavailable", "There is no exact qualified plan to authorize.", 5);
		}
		const auto token   = draft_token(aPreview.Snapshot.Target);
		auto       current = prepare_publication(token, aGh, aPreview.Options, aPreview.Resume ? aPreview.Plan->Submission.IdempotencyKey : std::string_view{ });
		if (!current) {
			return current;
		}
		if (current->Snapshot.Draft.Revision != aPreview.Snapshot.Draft.Revision || current->Plan != aPreview.Plan) {
			return failure("preview_changed", "The draft or publication plan changed during confirmation. Preview and confirm again.", 4);
		}
		if (current->Result.State == application::staged_publication_state::published) {
			return current;
		}
		if (!resumable(current->Result.State)) {
			return failure("recovery_required", current->Status, 6);
		}
		auto&       self   = *Implementation;
		auto        reader = self.Readers(self.Root, self.Profile);
		const auto& target = current->Snapshot.Target;
		auto        read   = reader->open(target.Hostname, target.Repository, target.Number, aGh);
		if (!read) {
			return std::unexpected(read.error());
		}
		if (!application::review_tour_observation_complete(read->Snapshot)) {
			return failure("remote_incomplete", "Publication requires a complete current observation. Missing collections or bodies cannot authorize an empty update.", 5);
		}
		auto context = reader->publication();
		if (!context) {
			return std::unexpected(context.error());
		}
		if (read->Target.Owner != target.Owner || !context->Qualified || context->Binding != *current->Plan->Submission.LiveBinding) {
			return failure("authority_changed", "The exact account, head or qualified transport changed. No writes are authorized.", 4);
		}
		application::review_submission_execution_grant grant;
#if defined(BPR_BUILD_TESTING)
		grant = application::make_review_submission_testing_grant(context->Session, core::request_generation{ 1 }, *current->Plan);
#else
		grant = application::detail::review_submission_user_authority_issuer::issue(context->Session, core::request_generation{ 1 }, *current->Plan);
#endif
		if (!grant) {
			return failure("interaction_required", "Exact-plan publication authority is unavailable.", 3);
		}
		submission_runner runner{ *context };
		current->Result = runner.execute(*current->Plan, std::move(grant));
		current->Status = publication_status(current->Result.State);
		if (current->Result.Record && current->Result.Record->Published) {
			if (!self.observe_receipt(target, current->Snapshot.Draft, *current->Result.Record)) {
				return failure("receipt_save_failed", "Publication was observed; recover its saved journal before another action.", 6);
			}
			auto refreshed = self.snapshot(target);
			if (!refreshed) {
				return std::unexpected(refreshed.error());
			}
			current->Snapshot = *refreshed;
		}
		return current;
	}

}
