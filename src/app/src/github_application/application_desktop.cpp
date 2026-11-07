#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_data_reset.hpp"
#include "bpr/app/release_maintenance_controls.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/release_update_recovery.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_command.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/tour_owner_replies.hpp"
#include "github_application/window_composition.hpp"
#include "review_cycle_desktop.hpp"
#include "tour_assist.hpp"
#include "tour_cli_types.hpp"
#include "tour_owner_service.hpp"

#include <QPlainTextEdit>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/appearance_preferences_application_host.hpp"
	#include "bpr/application/workbench_preferences_host.hpp"
	#include "bpr/qt/settings_window.hpp"
#endif
#if defined(BPR_LOCAL_AUTOMATION)
	#include "bpr/qt/event_loop_executor.hpp"
	#include "cycle_commands.hpp"
	#include "cycle_owner_service.hpp"
	#include <future>
	#include <stop_token>
	#include <thread>
#endif

namespace bpr::app {
	auto github_application::desktop_registry_root() const -> std::filesystem::path { return Profile->RegistryRoot; }

	auto github_application::desktop_update_recovery_root() const -> std::filesystem::path { return Profile->ApplicationDataRoot; }

	auto github_application::desktop_update_ready() const noexcept -> bool
	{
		return (!Profile->ReviewRestoration || Profile->ReviewRestoration->durable())
			&& (!Profile->AppearancePreferences || Profile->AppearancePreferences->durable())
			&& (!Profile->WorkbenchPreferences || (!Profile->WorkbenchPreferences->is_busy() && Profile->WorkbenchPreferences->is_quiescent() && !Profile->WorkbenchPreferences->delivery_failed()));
	}

	void github_application::desktop_update_freeze(bool aFreeze)
	{
		Profile->UpdateShutdown = aFreeze;
		if (Profile->Settings) {
			Profile->Settings->setEnabled(!aFreeze);
		}
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (session->Window) {
				session->Window->setEnabled(!aFreeze);
			}
		}
	}

	auto github_application::desktop_update_drafts() const -> std::vector<application::release_update_saved_draft>
	{
		std::vector<application::release_update_saved_draft> result;
		for (const auto& owner : Profile->DraftOwners) {
			const auto& authority = owner.Host->authority();
			for (const auto& saved : owner.Host->recovery_snapshot()) {
				const auto& draft = saved.Draft;
				const auto  scope = std::string{ authority.ProfileAuthority.Hostname.value() } + " · account " + std::to_string(authority.Account.value())
					+ " · repository " + std::to_string(authority.Target.Repository.value()) + " · pull request " + std::to_string(authority.Target.PullRequest.value())
					+ " · head " + std::string{ core::canonical_hex(authority.Target.ExpectedHead).view() }
					+ " · " + (draft.Anchor.Side == core::diff_side::right ? "right" : "left")
					+ (draft.StartLine ? " lines " + std::to_string(*draft.StartLine) + "-" + std::to_string(draft.Anchor.Line) : " line " + std::to_string(draft.Anchor.Line));
				result.push_back({ "inline:" + std::to_string(owner.Scope.value()) + ":" + std::to_string(draft.Id.value()), scope, draft.Body,
								   draft.State == core::review_draft_state::publishing });
			}
		}
		for (const auto& [context, session] : Sessions) {
			if (!session->Window) {
				continue;
			}
			const auto identity = "window:" + std::to_string(context.Window.value());
			if (const auto* editor = session->Window->review_editor(); editor && editor->body_editor() && !editor->body_editor()->toPlainText().isEmpty()) {
				result.push_back({ identity + ":editor", "Unsubmitted inline editor text (including any conflicting edit)", editor->body_editor()->toPlainText().toStdString(),
								   session->PublishingDraft.has_value() });
			}
			if (const auto* editor = session->Window->review_submission_editor_view(); editor && editor->shown_draft() && editor->body_editor()) {
				const auto& draft = *editor->shown_draft();
				const auto  body  = editor->body_editor()->toPlainText().toStdString();
				if (!body.empty()) {
					result.push_back({ identity + ":review", "Whole review · account " + std::to_string(draft.ActingAccount.value()) + " · pull request " + std::to_string(draft.Target.PullRequest.value()) + " · head " + std::string{ core::canonical_hex(draft.Target.ExpectedHead).view() },
									   body, draft.State != core::review_submission_state::drafting });
				}
			}
		}
		return result;
	}

	auto github_application::desktop_release_account() -> std::optional<release_update_account> { return Profile->release_account(); }

	auto github_application::desktop_diagnostic_log() const -> std::shared_ptr<application::diagnostic_log_port> { return Profile->DiagnosticLog; }

	auto github_application::desktop_activation_server() const -> std::shared_ptr<application::profile_activation_server> { return Profile->ActivationServer; }

	auto github_application::desktop_tour_owner_server() const -> std::shared_ptr<application::tour_owner_server> { return Profile->TourOwnerServer; }

	void github_application::desktop_tour_owner_route(application::tour_channel_bytes aRequest, std::function<void(application::tour_channel_bytes)> aRespond)
	{
		auto& profile = *Profile;
		if (Profile->Closing || !profile.TourOwnerService) {
			aRespond(tour_owner_error("owner_unavailable"));
			return;
		}
		const auto request = adapters::sqlite::decode_tour_request(aRequest);
		if (!request) {
			aRespond(tour_owner_error("invalid_request"));
			return;
		}
		if (request->Operation == application::tour_owner_operation::cycle) {
			desktop_cycle_owner_route(std::move(aRequest), request->Cycle, std::move(aRespond));
			return;
		}
		// The draft is read or written where its author is: an open window's host is the one committed
		// history, so a forwarded command never runs beside it on a second copy read from the store.
		const auto found = [&]() -> implementation::live_tour_view {
			implementation::live_tour_view result;
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				const auto seen = session->live_tour(request->Draft);
				if (seen.Host && (!result.Host || (seen.Current && !result.Current))) {
					result = seen;
				}
			}
			return result;
		}();
		const auto serve = [service = profile.TourOwnerService, &effects = profile.WorkbenchEffects](application::tour_channel_bytes aRead, std::function<void(application::tour_channel_bytes)> aReply) {
			effects.post([service, read = std::move(aRead), reply = std::move(aReply)] { reply(service->serve(read)); });
		};
		if (request->Operation == application::tour_owner_operation::source) {
			// [[BPR-1227]]. Source is read where the pull request is open, by that window's own provider
			// composition: no second process reads the provider or opens a profile store, and nothing
			// is written. A draft no window shows is refused by the service, provider-free.
			if (request->Source.LastLine - request->Source.FirstLine >= 200) {
				aRespond(tour_owner_error("source_too_large"));
				return;
			}
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				if (session->read_tour_source(request->Draft, request->Source, aRespond)) {
					return;
				}
			}
			serve(std::move(aRequest), std::move(aRespond));
			return;
		}
		if (!found.Host) {
			serve(std::move(aRequest), std::move(aRespond));
			return;
		}
		if (found.Composing) {
			// Preedit text is not committed text. It is neither flushed nor read past.
			aRespond(tour_owner_error("editor_busy"));
			return;
		}
		const auto  accepted    = profile.WorkbenchPreferences ? profile.WorkbenchPreferences->accepted() : std::optional<application::workbench_profile_preferences>{ };
		const auto* preferences = accepted ? &*accepted : nullptr;
		const auto  isMutation  = request->Operation == application::tour_owner_operation::mutate;
		const bool  assisting   = isMutation && (request->Edit.Operation == application::tour_service_operation::assist_complete || request->Edit.Operation == application::tour_service_operation::outline_use);
		std::string outcomeJson;
		auto        read = application::tour_owner_request{ .Operation = isMutation ? application::tour_owner_operation::request_status : (request->Operation == application::tour_owner_operation::assist_issue || request->Operation == application::tour_owner_operation::assist_cancel) ? application::tour_owner_operation::load
																																																																							: request->Operation,
															.Draft     = request->Draft };
		if (isMutation) {
			auto edit = request->Edit;
			if (edit.RequestId.empty()) {
				aRespond(tour_owner_error("request_id_required"));
				return;
			}
			const auto draft = found.Host->presentation().Draft;
			if (!draft) {
				aRespond(tour_owner_error("owner_unavailable"));
				return;
			}
			edit.Command.Owner = draft->Owner;
			edit.Command.Draft = draft->Id;
			if (assisting) {
				// The receiver's own clock and hook revision; the caller supplied neither.
				if (!preferences) {
					aRespond(tour_owner_error("owner_busy"));
					return;
				}
				edit.Assist.Now                 = tour_assist_now();
				edit.Assist.CurrentHookRevision = preferences->WritingHooks.Revision;
			}
			const auto result = found.Host->execute(edit);
			if (result.Status == application::tour_service_status::busy) {
				aRespond(tour_owner_error("owner_busy"));
				return;
			}
			if (result.Status == application::tour_service_status::refused) {
				if (assisting && !result.Reason.empty()) {
					aRespond(tour_owner_error(assist_refusal_code(result.Reason), result.Detail));
					return;
				}
				aRespond(tour_owner_error(result.Refusal == core::tour_refusal::conflict ? "revision_conflict" : "command_refused"));
				return;
			}
			if (assisting) {
				outcomeJson = tour_assist_outcome_json(result, *found.Host);
			}
			read.Edit.RequestId = edit.RequestId;
		} else if (request->Operation == application::tour_owner_operation::assist_issue) {
			if (!preferences) {
				aRespond(tour_owner_error("owner_busy"));
				return;
			}
			const auto&       q = request->Assist;
			tour_assist_query query{ .Hook = q.Hook, .Stop = q.Stop, .Range = q.HasRange ? std::optional{ std::pair{ q.Begin, q.End } } : std::nullopt, .Quote = q.HasQuote ? std::optional{ q.Quote } : std::nullopt, .Session = q.Session };
			auto              issued = tour_assist_issue(*found.Host, preferences->WritingHooks, query, { }, tour_assist_now());
			if (!issued) {
				aRespond(tour_owner_error(issued.error().Code, issued.error().Message));
				return;
			}
			outcomeJson = issued->ContextJson;
		} else if (request->Operation == application::tour_owner_operation::assist_cancel) {
			found.Host->assist_cancel();
		} else {
			read.Edit.RequestId = request->Edit.RequestId;
		}
		profile.WaitingTourRequests.push_back({ request->Draft, std::move(outcomeJson), adapters::sqlite::encode_tour_request(read), std::move(aRespond), std::chrono::steady_clock::now() + std::chrono::seconds{ 5 } });
		desktop_tour_owner_tick();
	}

	void github_application::desktop_cycle_owner_route(application::tour_channel_bytes aRequest, application::cycle_owner_request aCycle, std::function<void(application::tour_channel_bytes)> aRespond)
	{
#if defined(BPR_LOCAL_AUTOMATION)
		auto& profile = *Profile;
		if (Profile->Closing || !profile.CycleOwnerService) {
			aRespond(tour_owner_error("owner_unavailable"));
			return;
		}
		const auto liveness  = std::weak_ptr<const int>{ Liveness };
		auto&      decisions = profile.DecisionLane;
		const auto stop      = profile.CycleOwnerStop.get_token();
		// Before the command reads or changes a round draft a window may hold, the
		// core decides from what the windows hold; the windows answer on their lane.
		auto guard = [this, liveness, &decisions, stop](const core::cycle_command aCommand, const core::review_cycle_id aTarget) -> cycle_result<void> {
			const auto ask = [&](const bool aSave) -> std::optional<core::cycle_draft_editor> {
				auto promise = std::make_shared<std::promise<core::cycle_draft_editor>>();
				auto future  = promise->get_future();
				decisions.post([this, liveness, promise, aTarget, aSave] {
					promise->set_value(liveness.expired() ? core::cycle_draft_editor::settled : cycle_draft_editors(aTarget, aSave));
				});
				if (future.wait_for(std::chrono::seconds{ 1 }) != std::future_status::ready) {
					return std::nullopt;
				}
				return future.get();
			};
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
			auto       saved    = false;
			while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) {
				const auto editor = ask(!saved);
				if (!editor) {
					continue;
				}
				const auto admission = core::admit_owner_command(aCommand, *editor);
				if (admission == core::cycle_owner_admission::serve) {
					return { };
				}
				if (admission == core::cycle_owner_admission::busy) {
					return std::unexpected(cycle_failure{ "draft_busy", "The desktop is composing text in this round's draft (an input method is open). Finish or cancel it, then retry; nothing was changed.", { }, { } });
				}
				// The window saves what it holds; this command then reads what was saved.
				saved = true;
				std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
			}
			return std::unexpected(cycle_failure{ "draft_busy", "The desktop is still saving this round's draft. Retry shortly; nothing was changed.", { }, { } });
		};
		profile.CycleOwnerEffects.post([service = profile.CycleOwnerService, payload = std::move(aRequest), cycle = std::move(aCycle), admit = std::move(guard), respond = std::move(aRespond), stop, this, liveness, &decisions] {
			auto answer = service->serve(payload, cycle, admit, [stop] { return stop.stop_requested(); });
			respond(std::move(answer.Bytes));
			if (answer.Changed) {
				decisions.post([this, liveness, changed = *answer.Changed] {
					if (!liveness.expired()) {
						cycle_written(changed);
					}
				});
			}
		});
#else
		// [[BPR-1179]]. No command channel is bound without local automation, so
		// nothing reaches here; a forwarded request is still answered, never served.
		static_cast<void>(aRequest);
		static_cast<void>(aCycle);
		aRespond(tour_owner_error("owner_unavailable"));
#endif
	}

	auto github_application::cycle_draft_editors(const core::review_cycle_id aCycle, const bool aSave) -> core::cycle_draft_editor
	{
		auto held = core::cycle_draft_editor::settled;
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (session->Closing || !session->CycleDesktop) {
				continue;
			}
			const auto editor = session->CycleDesktop->draft_editor(aCycle);
			if (editor == core::cycle_draft_editor::unsaved && aSave) {
				session->CycleDesktop->save_draft_editor(aCycle);
			}
			held = std::max(held, editor);
		}
		return held;
	}

	void github_application::cycle_written(const core::review_cycle_id aCycle)
	{
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (!session->Closing && session->CycleDesktop) {
				session->CycleDesktop->external_change(aCycle);
			}
		}
	}

	void github_application::desktop_tour_owner_tick()
	{
		auto& profile = *Profile;
		for (auto waiting = profile.WaitingTourRequests.begin(); waiting != profile.WaitingTourRequests.end();) {
			std::optional<std::string> failure;
			bool                       ready = true;
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				const auto seen = session->live_tour(waiting->Token);
				if (!seen.Host) {
					continue;
				}
				const auto shown = seen.Host->presentation();
				if (shown.Failure) {
					failure = "storage_unavailable";
				} else if (shown.Conflict) {
					failure = "revision_conflict";
				} else if (!shown.Draft || shown.Loading) {
					ready = false;
				} else {
					// A draft nobody has touched is never persisted, so there is nothing to wait for.
					const bool untouched = shown.SavedRevision == 0U && shown.Draft->Revision == 1U && shown.Draft->Problem.empty() && shown.Draft->Solution.empty() && shown.Draft->Stops.empty() && !shown.Draft->ProblemBaseline && !shown.Draft->SolutionBaseline;
					if (!untouched && (shown.Saving || shown.SavedRevision != shown.Draft->Revision || !seen.Host->is_quiescent())) {
						ready = false;
					}
				}
			}
			if (!failure && !ready && !Profile->Closing && std::chrono::steady_clock::now() < waiting->Deadline) {
				++waiting;
				continue;
			}
			// A command that was applied but is not yet durable is uncertain to its caller, who inspects
			// the exact request ID rather than replaying it.
			auto entry = std::move(*waiting);
			waiting    = profile.WaitingTourRequests.erase(waiting);
			if (failure) {
				entry.Respond(tour_owner_error(*failure));
			} else if (!ready) {
				entry.Respond(tour_owner_error(Profile->Closing ? "owner_unavailable" : "save_pending"));
			} else {
				profile.WorkbenchEffects.post([service = profile.TourOwnerService, read = std::move(entry.Read), respond = std::move(entry.Respond), json = std::move(entry.Json)] {
					auto answer = service->serve(read);
					if (!json.empty()) {
						// The durable answer, carrying what the live host said: context or the outcome of a completion.
						if (auto decoded = adapters::sqlite::decode_tour_response(answer); decoded && decoded->Error.empty()) {
							decoded->Json = json;
							answer        = adapters::sqlite::encode_tour_response(std::move(*decoded));
						}
					}
					respond(std::move(answer));
				});
			}
		}
	}

	auto github_application::desktop_scratch() const -> std::shared_ptr<void> { return Profile->StartupScratch; }

	void github_application::set_desktop_quit_handler(std::function<void(int)> aHandler) { Profile->DesktopQuit = std::move(aHandler); }

	void github_application::set_maintenance_retirement(release_maintenance_retirement aRetire) { Profile->MaintenanceRetirement = std::move(aRetire); }

	auto github_application::take_profile_data_reset_report() -> std::optional<profile_data_reset_report> { return std::exchange(Profile->DataResetReport, std::nullopt); }

	auto github_application::take_maintenance() -> std::unique_ptr<release_maintenance_controls>
	{
		if (!Profile->Maintenance || !Profile->Settings) {
			return { };
		}
		std::function<void()> unmount;
		if (Profile->SettingsOwner) {
			const auto found = Profile->Observers.find(*Profile->SettingsOwner);
			if (found != Profile->Observers.end()) {
				unmount = found->second.UnmountSettings;
			}
		}
		Profile->Maintenance->retain_settings(std::move(Profile->Settings), Profile->Admission.Label, std::move(unmount));
		Profile->SettingsOwner.reset();
		return std::move(Profile->Maintenance);
	}

	void github_application::set_profile_label(std::string aLabel)
	{
		Profile->Admission.Label = std::move(aLabel);
		const auto label         = QString::fromStdString(Profile->Admission.Label);
		if (Profile->Settings) {
			Profile->Settings->set_profile_label(label);
		}
		for (const auto& [context, session] : Sessions) {
			if (!session->Closing) {
				session->Window->set_profile_label(label);
			}
		}
	}
}
