#include "tour_runtime.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_tour_import.hpp"
#include "bpr/core/sha256.hpp"
#include "review_submission_execution_authority.hpp"
#include "tour_cancellation.hpp"
#include "tour_decisions.hpp"
#include "tour_observation.hpp"
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
				Failure = tour_cli_error{ "owner_busy", "This profile has another owner. Close it or use a compatible owner channel.", 4 };
				return;
			}
			Lease = std::move(*lease);
			Store = adapters::sqlite::make_review_tour_store(Root, Profile);
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
			return tour_local_snapshot{ aTarget, state.Draft, state.Undo.empty() ? std::string{ } : state.Undo.back().Label, state.Redo.empty() ? std::string{ } : state.Redo.back().Label };
		}
	};
	tour_runtime::tour_runtime(std::filesystem::path aBase, std::string aProfile, tour_reader_factory aReaders)
		: Implementation{ std::make_unique<implementation>(std::move(aBase), std::move(aProfile), std::move(aReaders)) }
	{ }
	tour_runtime::~tour_runtime() { Implementation->Effects.close_and_drain(); }
	auto tour_runtime::admission() const -> tour_cli_result<core::bpr_profile_id> { return Implementation->Failure ? tour_cli_result<core::bpr_profile_id>{ std::unexpected(*Implementation->Failure) } : tour_cli_result<core::bpr_profile_id>{ Implementation->Profile }; }
	auto tour_runtime::draft_token(const application::tour_saved_target& aTarget) -> std::string
	{
		std::string key = std::to_string(aTarget.Owner.Profile.value()) + ":" + std::to_string(aTarget.Owner.Host.value()) + ":" + std::to_string(aTarget.Owner.Account.value()) + ":" + std::to_string(aTarget.Owner.Repository.value()) + ":" + std::to_string(aTarget.Owner.PullRequest.value()) + ":" + std::to_string(aTarget.Draft.value());
		for (auto byte : aTarget.Owner.HostAuthority.bytes()) {
			key.push_back(static_cast<char>(byte));
		}
		const auto     digest = core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(key.data()), key.size() });
		constexpr char hex[]  = "0123456789abcdef";
		std::string    result;
		for (auto byte : digest) {
			result.push_back(hex[byte >> 4U]);
			result.push_back(hex[byte & 15U]);
		}
		return result;
	}
	auto tour_runtime::connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>
	{
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
		auto loaded = load(aDraft, true);
		if (!loaded) {
			return loaded;
		}
		auto& self             = *Implementation;
		aRequest.Command.Owner = loaded->Draft.Owner;
		aRequest.Command.Draft = loaded->Draft.Id;
		application::manual_executor  effects, decisions;
		application::review_tour_host host{ loaded->Draft, adapters::sqlite::make_review_tour_store(self.Root, self.Profile), effects, decisions, { } };
		host.start();
		self.effect([&] { effects.drain(); });
		decisions.drain();
		const auto result = host.execute(aRequest);
		host.shutdown();
		while (!host.is_quiescent()) {
			self.effect([&] { effects.drain(); });
			decisions.drain();
		}
		if (result.Status == application::tour_service_status::refused) {
			return failure(result.Refusal == core::tour_refusal::conflict ? "revision_conflict" : "command_refused", "The exact local command was refused; read the current draft before editing.", result.Refusal == core::tour_refusal::conflict ? 4 : 2);
		}
		const auto state = host.presentation();
		if (state.Failure) {
			return failure("storage_unavailable", "The local edit was not durably saved.");
		}
		return tour_local_snapshot{ loaded->Target, *state.Draft, state.UndoLabel, state.RedoLabel };
	}
	auto tour_runtime::prepare_publication(std::string_view aDraft, bool aGh, tour_publication_options aOptions, std::string_view aAttempt, bool aRecover) -> tour_cli_result<tour_publication_preview>
	{
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
