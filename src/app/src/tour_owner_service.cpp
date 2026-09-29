#include "tour_owner_service.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/sha256.hpp"
#include <algorithm>
#include <tuple>
namespace bpr::app {
	namespace {
		auto failure(std::string aCode, std::string aMessage, int aExit = 7) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		template <typename T>
		auto store_failure(const application::tour_store_result<T>& aResult)
		{
			const bool conflict = aResult.error().Failure == application::tour_store_failure::conflict;
			return failure(conflict ? "revision_conflict" : "storage_unavailable", "Private tour storage refused the operation; retained work was not reset.", conflict ? 4 : 7);
		}
		auto snapshot_of(const application::tour_saved_target& aTarget, const std::optional<application::tour_stored_state>& aState) -> tour_local_snapshot
		{
			if (!aState) {
				return tour_local_snapshot{ aTarget, core::review_tour{ .Id = aTarget.Draft, .Owner = aTarget.Owner, .Base = aTarget.Base, .Head = aTarget.Head }, { }, { } };
			}
			return tour_local_snapshot{ aTarget, aState->Draft, aState->Undo.empty() ? std::string{ } : aState->Undo.back().Label, aState->Redo.empty() ? std::string{ } : aState->Redo.back().Label, "not_refreshed", 0, aState->Resume };
		}
		/// The code is the caller's stable key; the message is the owner's one bounded human line.
		auto error_response(std::string aCode, std::string aMessage = { }) -> application::tour_channel_bytes
		{
			return adapters::sqlite::encode_tour_response({ .Error = std::move(aCode), .Json = std::move(aMessage) });
		}
	}
	auto tour_draft_token(const application::tour_saved_target& aTarget) -> std::string
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
	auto load_local_tour(application::review_tour_store& aStore, std::string_view aDraft, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>
	{
		if (aDraft.empty()) {
			return failure("target_ambiguous", "Machine commands require --profile and the draft_id returned by open.", 2);
		}
		std::optional<application::tour_store_result<std::vector<application::tour_saved_target>>> targets;
		aLane([&] { targets = aStore.targets({ }); });
		if (!*targets) {
			return store_failure(*targets);
		}
		std::erase_if(**targets, [&](const auto& aTarget) { return tour_draft_token(aTarget) != aDraft; });
		if ((*targets)->size() != 1) {
			return failure("target_ambiguous", "Select one exact --draft returned by open; no global last-opened PR is used.", 2);
		}
		const auto&                                                                                  target = (*targets)->front();
		std::optional<application::tour_store_result<std::optional<application::tour_stored_state>>> state;
		aLane([&] { state = aStore.load_state(target.Owner, target.Draft, { }); });
		if (!*state) {
			return store_failure(*state);
		}
		return snapshot_of(target, **state);
	}
	namespace {
		/// One short-lived host on the caller's lane: start, run one operation, drain its saves.
		template <typename TOperation>
		auto with_local_host(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, const tour_store_lane& aLane, TOperation aOperation)
		{
			application::manual_executor  effects, decisions;
			application::review_tour_host host{ aLoaded.Draft, adapters::sqlite::make_review_tour_store(aRoot, aProfile), effects, decisions, { } };
			host.start();
			aLane([&] { effects.drain(); });
			decisions.drain();
			auto result = aOperation(host);
			host.shutdown();
			while (!host.is_quiescent()) {
				aLane([&] { effects.drain(); });
				decisions.drain();
			}
			return std::pair{ std::move(result), host.presentation() };
		}
		auto assist_exit(std::string_view aReason) -> int
		{
			return aReason == "stale" || aReason == "expired" || aReason == "unknown_ticket" || aReason == "unknown_proposal" ? 4 : 2;
		}
	}
	auto mutate_local_tour(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, application::tour_service_request aRequest, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>
	{
		aRequest.Command.Owner = aLoaded.Draft.Owner;
		aRequest.Command.Draft = aLoaded.Draft.Id;
		const bool assisting   = aRequest.Operation == application::tour_service_operation::assist_complete || aRequest.Operation == application::tour_service_operation::outline_use;
		if (assisting) {
			// The clock and the hook revision are the receiver's own, never the caller's.
			auto settings = load_tour_hook_settings(aRoot);
			if (!settings) {
				return std::unexpected(settings.error());
			}
			aRequest.Assist.Now                 = tour_assist_now();
			aRequest.Assist.CurrentHookRevision = settings->Revision;
		}
		auto [result, state]                 = with_local_host(aRoot, aProfile, aLoaded, aLane, [&](application::review_tour_host& aHost) {
			auto executed = aHost.execute(aRequest);
			return std::tuple{ executed, assisting ? tour_assist_outcome_json(executed, aHost) : std::string{ }, aHost.request_status(aRequest.RequestId) };
		});
		const auto& [outcome, json, receipt] = result;
		if (outcome.Status == application::tour_service_status::refused) {
			if (assisting && !outcome.Reason.empty()) {
				return failure(outcome.Reason, outcome.Detail, assist_exit(outcome.Reason));
			}
			const bool conflict = outcome.Refusal == core::tour_refusal::conflict;
			return failure(conflict ? "revision_conflict" : "command_refused", "The exact local command was refused; read the current draft before editing.", conflict ? 4 : 2);
		}
		if (state.Failure) {
			return failure("storage_unavailable", "The local edit was not durably saved.");
		}
		tour_local_snapshot snapshot{ aLoaded.Target, *state.Draft, state.UndoLabel, state.RedoLabel, "not_refreshed", 0, state.Resume };
		snapshot.Receipt    = receipt;
		snapshot.AssistJson = json;
		return snapshot;
	}
	auto issue_local_assist(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>
	{
		auto settings = load_tour_hook_settings(aRoot);
		if (!settings) {
			return std::unexpected(settings.error());
		}
		auto [issued, state] = with_local_host(aRoot, aProfile, aLoaded, aLane, [&](application::review_tour_host& aHost) { return tour_assist_issue(aHost, *settings, aQuery, std::move(aExtra), tour_assist_now()); });
		if (!issued) {
			return std::unexpected(issued.error());
		}
		if (state.Failure) {
			return failure("storage_unavailable", "The request was not durably recorded.");
		}
		tour_local_snapshot snapshot{ aLoaded.Target, *state.Draft, state.UndoLabel, state.RedoLabel, "not_refreshed", 0, state.Resume };
		snapshot.AssistJson = issued->ContextJson;
		return snapshot;
	}
	auto cancel_local_assist(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, const tour_local_snapshot& aLoaded, const tour_store_lane& aLane) -> tour_cli_result<tour_local_snapshot>
	{
		auto [done, state] = with_local_host(aRoot, aProfile, aLoaded, aLane, [&](application::review_tour_host& aHost) {
			aHost.assist_cancel();
			return 0;
		});
		if (state.Failure) {
			return failure("storage_unavailable", "The closure was not durably recorded.");
		}
		return tour_local_snapshot{ aLoaded.Target, *state.Draft, state.UndoLabel, state.RedoLabel, "not_refreshed", 0, state.Resume };
	}
	tour_owner_service::tour_owner_service(std::filesystem::path aRoot, core::bpr_profile_id aProfile)
		: Root{ std::move(aRoot) }
		, Profile{ aProfile }
	{ }
	auto tour_owner_service::serve(std::span<const std::uint8_t> aPayload) const -> application::tour_channel_bytes
	{
		try {
			const auto request = adapters::sqlite::decode_tour_request(aPayload);
			if (!request) {
				return error_response("invalid_request");
			}
			const tour_store_lane lane = [](std::function<void()> aTask) { aTask(); };
			if (request->Operation == application::tour_owner_operation::hooks_show) {
				const auto settings = load_tour_hook_settings(Root);
				if (!settings) {
					return error_response(settings.error().Code, settings.error().Message);
				}
				return adapters::sqlite::encode_tour_response({ .Json = tour_hooks_json(*settings) });
			}
			auto store  = adapters::sqlite::make_review_tour_store(Root, Profile);
			auto loaded = load_local_tour(*store, request->Draft, lane);
			if (!loaded) {
				return error_response(loaded.error().Code, loaded.error().Message);
			}
			if (loaded->Target.Owner.Profile != Profile) {
				return error_response("wrong_profile");
			}
			std::optional<application::tour_request_receipt> receipt;
			if (request->Operation == application::tour_owner_operation::mutate) {
				auto edit = request->Edit;
				if (edit.RequestId.empty()) {
					// A forwarded mutation without identity could not be inspected after a lost reply.
					return error_response("request_id_required");
				}
				auto changed = mutate_local_tour(Root, Profile, *loaded, std::move(edit), lane);
				if (!changed) {
					return error_response(changed.error().Code, changed.error().Message);
				}
				loaded  = std::move(changed);
				receipt = loaded->Receipt;
			} else if (request->Operation == application::tour_owner_operation::assist_issue) {
				const auto&       q = request->Assist;
				tour_assist_query query{ .Hook = q.Hook, .Stop = q.Stop, .Range = q.HasRange ? std::optional{ std::pair{ q.Begin, q.End } } : std::nullopt, .Quote = q.HasQuote ? std::optional{ q.Quote } : std::nullopt, .Session = q.Session };
				auto              issued = issue_local_assist(Root, Profile, *loaded, query, { }, lane);
				if (!issued) {
					return error_response(issued.error().Code, issued.error().Message);
				}
				loaded = std::move(issued);
			} else if (request->Operation == application::tour_owner_operation::assist_cancel) {
				auto cancelled = cancel_local_assist(Root, Profile, *loaded, lane);
				if (!cancelled) {
					return error_response(cancelled.error().Code, cancelled.error().Message);
				}
				loaded = std::move(cancelled);
			} else if (request->Operation == application::tour_owner_operation::request_status && !request->Edit.RequestId.empty()) {
				std::optional<application::tour_store_result<std::optional<application::tour_stored_state>>> state;
				lane([&] { state = store->load_state(loaded->Target.Owner, loaded->Target.Draft, { }); });
				if (state && *state && state->value()) {
					const auto& records = state->value()->Receipts;
					if (const auto found = std::ranges::find(records, request->Edit.RequestId, &application::tour_request_receipt::Id); found != records.end()) {
						receipt = *found;
					}
				}
			}
			return adapters::sqlite::encode_tour_response({ .Target = loaded->Target, .Draft = loaded->Draft, .UndoLabel = loaded->UndoLabel, .RedoLabel = loaded->RedoLabel, .Resume = loaded->Resume, .Receipt = std::move(receipt), .Json = loaded->AssistJson });
		} catch (...) {
			return error_response("owner_unavailable");
		}
	}
}
