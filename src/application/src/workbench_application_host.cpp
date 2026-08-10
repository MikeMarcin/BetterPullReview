#include "bpr/application/workbench_application_host.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		enum class workbench_request_domain : std::uint8_t {
			observation,
			settings_load,
			settings_store,
			selected_diff,
		};

		struct staged_observation
		{
			workbench_file_observation Observation;
		};

		struct staged_settings_load
		{
			workbench_request_context                            Context;
			workbench_settings_result<workbench_layout_settings> Result;
		};

		struct staged_settings_store
		{
			workbench_request_context       Context;
			workbench_settings_result<void> Result;
		};

		struct staged_selected_diff
		{
			workbench_selected_diff_observation Observation;
		};

		struct deferred_workbench_restore
		{
		};

		struct deferred_workbench_observe
		{
			workbench_pull_request_authority Authority;
		};

		struct deferred_workbench_invalidate
		{
			workbench_pull_request_authority Authority;
		};

		struct deferred_workbench_intent
		{
			core::workbench_interaction_intent Intent;
		};

		struct deferred_workbench_selected_diff_refresh
		{
		};

		struct deferred_workbench_actionability_refresh
		{
		};

		struct deferred_workbench_resize
		{
			core::workbench_drawer Drawer = core::workbench_drawer::files;
			std::uint32_t          Width  = 0U;
		};

		struct deferred_workbench_cancel
		{
		};

		struct deferred_workbench_shutdown
		{
		};

		using deferred_workbench_ingress = std::variant<deferred_workbench_restore,
														deferred_workbench_observe,
														deferred_workbench_invalidate,
														deferred_workbench_selected_diff_refresh,
														deferred_workbench_actionability_refresh,
														deferred_workbench_intent,
														deferred_workbench_resize,
														deferred_workbench_cancel,
														deferred_workbench_shutdown,
														staged_observation,
														staged_settings_load,
														staged_settings_store,
														staged_selected_diff>;

		[[nodiscard]] auto validated_limits(const workbench_host_limits aLimits)
			-> workbench_host_limits
		{
			if (!valid(aLimits)) {
				throw std::invalid_argument{ "Invalid workbench host limits" };
			}
			return aLimits;
		}

		[[nodiscard]] constexpr auto clamped_width(
			const std::uint32_t         aWidth,
			const workbench_host_limits aLimits) noexcept -> std::uint32_t
		{
			return std::clamp(aWidth, minimum_workbench_drawer_width, aLimits.DrawerWidth);
		}

		/// \brief Assembles the bounded cursor facts for one retained selection.
		///
		/// \details This is the whole of what the core learns about the workbench
		/// file set. A resident selection reports its 1-based ordinal and the
		/// neighbour facts that follow from it. A selection absent from the order,
		/// including no selection at all, reports no ordinal, and navigation may
		/// enter a nonempty order from either end.
		[[nodiscard]] auto assemble_cursor(
			const std::span<const core::workbench_file_id> aOrder,
			const std::optional<core::workbench_file_id>&  aSelected) noexcept
			-> core::workbench_navigation_cursor
		{
			auto cursor = core::workbench_navigation_cursor{
				.File            = aSelected,
				.ActionableCount = static_cast<std::uint32_t>(aOrder.size()),
			};
			const auto found = aSelected.has_value()
				? std::ranges::find(aOrder, *aSelected)
				: aOrder.end();
			if (found == aOrder.end()) {
				cursor.PreviousAvailable = !aOrder.empty();
				cursor.NextAvailable     = !aOrder.empty();
				return cursor;
			}
			cursor.Ordinal = static_cast<std::uint32_t>(
				std::ranges::distance(aOrder.begin(), found) + 1);
			cursor.PreviousAvailable = cursor.Ordinal > 1U;
			cursor.NextAvailable     = cursor.Ordinal < cursor.ActionableCount;
			return cursor;
		}

		/// \brief Resolves one core navigation decision against the retained order.
		///
		/// \details The core names the entry; only the host holds the order that
		/// answers with an identity. A relative target that carries no ordinal came
		/// from a selection that left the filtered actionable order, so the host
		/// enters that order from the matching end: it retains no position for a
		/// file the order no longer contains.
		[[nodiscard]] auto resolve_decision(
			const std::span<const core::workbench_file_id> aOrder,
			const core::workbench_navigation_decision&     aDecision) noexcept
			-> std::optional<core::workbench_file_id>
		{
			switch (aDecision.Target) {
			case core::workbench_navigation_target::exact_file:
				return aDecision.File;
			case core::workbench_navigation_target::first_actionable:
			case core::workbench_navigation_target::next_actionable:
				if (!aDecision.Ordinal.has_value()) {
					return aOrder.empty()
						? std::nullopt
						: std::optional{ aOrder.front() };
				}
				break;
			case core::workbench_navigation_target::last_actionable:
			case core::workbench_navigation_target::previous_actionable:
				if (!aDecision.Ordinal.has_value()) {
					return aOrder.empty()
						? std::nullopt
						: std::optional{ aOrder.back() };
				}
				break;
			}
			const auto ordinal = *aDecision.Ordinal;
			if (ordinal == 0U || ordinal > aOrder.size()) {
				return std::nullopt;
			}
			return aOrder[ordinal - 1U];
		}

		[[nodiscard]] constexpr auto failure_for_status(
			const workbench_observation_status aStatus) noexcept
			-> workbench_host_failure
		{
			return aStatus == workbench_observation_status::limit_exceeded
				? workbench_host_failure::observation_limit
				: workbench_host_failure::observation_unavailable;
		}

	} // namespace

	auto valid(const workbench_file_observation_entry& aEntry) noexcept -> bool
	{
		try {
			const auto validPath = [](const std::filesystem::path& aPath) {
				const auto text = aPath.generic_u8string();
				return !text.empty()
					&& text.size() <= core::maximum_identified_path_bytes
					&& text.find(u8'\0') == decltype(text)::npos;
			};
			const auto renamedOrCopied
				= aEntry.Change == workbench_file_change_kind::renamed
				|| aEntry.Change == workbench_file_change_kind::copied;
			return aEntry.File.valid() && !aEntry.PresentationPath.empty()
				&& aEntry.PresentationPath.size() <= core::maximum_identified_path_bytes
				&& aEntry.PresentationPath.find('\0') == std::string::npos
				&& validPath(aEntry.CurrentPath)
				&& (!aEntry.PreviousPath || validPath(*aEntry.PreviousPath))
				&& aEntry.PreviousPath.has_value() == renamedOrCopied
				&& aEntry.Change != workbench_file_change_kind::uninitialized
				&& (aEntry.ContentKind == workbench_file_content_kind::unknown
					|| aEntry.ContentKind == workbench_file_content_kind::text
					|| aEntry.ContentKind == workbench_file_content_kind::binary);
		} catch (...) {
			return false;
		}
	}

	struct workbench_application_host::implementation
	{
		struct completion_gate
		{
			std::atomic<implementation*> Host = nullptr;
		};

		struct outstanding_request
		{
			workbench_request_context    Context;
			workbench_request_domain     Domain     = workbench_request_domain::observation;
			workbench_terminal_rejection Retirement = workbench_terminal_rejection::none;
			std::stop_source             Stop;
		};

		implementation(core::workbench_interaction_state        aInitialState,
					   workbench_observation_port&              aObservations,
					   workbench_settings_port&                 aSettings,
					   effect_executor&                         aEffects,
					   decision_executor&                       aDecisionLane,
					   workbench_projection_port&               aProjections,
					   const core::application_session_id       aSession,
					   const workbench_host_limits              aLimits,
					   workbench_selected_diff_port*            aSelectedDiffs,
					   workbench_selected_diff_projection_port* aSelectedDiffProjections)
			: State{ aInitialState }
			, Observations{ aObservations }
			, Settings{ aSettings }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, SelectedDiffs{ aSelectedDiffs }
			, SelectedDiffProjections{ aSelectedDiffProjections }
			, Session{ aSession }
			, Limits{ validated_limits(aLimits) }
			, Gate{ std::make_shared<completion_gate>() }
		{
			Gate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			Gate->Host.store(nullptr, std::memory_order_release);
		}

		implementation(const implementation&)                    = delete;
		auto operator=(const implementation&) -> implementation& = delete;
		implementation(implementation&&)                         = delete;
		auto operator=(implementation&&) -> implementation&      = delete;

		// --- Ingress serialization -------------------------------------------

		void enqueue(deferred_workbench_ingress aIngress)
		{
			if (TurnActive) {
				DeferredIngress.push_back(std::move(aIngress));
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.push_back(std::move(aIngress));
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, ingress = std::move(aIngress)]() mutable {
				process(std::move(ingress));
			});
		}

		void process(deferred_workbench_ingress aIngress)
		{
			std::visit(
				[this](auto&& aTyped) {
					using ingress_type = std::decay_t<decltype(aTyped)>;
					if constexpr (std::is_same_v<ingress_type, deferred_workbench_restore>) {
						restore_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_observe>) {
						observe_now(aTyped.Authority);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_invalidate>) {
						invalidate_now(aTyped.Authority);
					} else if constexpr (std::is_same_v<
											 ingress_type,
											 deferred_workbench_selected_diff_refresh>) {
						refresh_selected_diff_now();
					} else if constexpr (std::is_same_v<
											 ingress_type,
											 deferred_workbench_actionability_refresh>) {
						refresh_actionability_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_intent>) {
						apply_now(aTyped.Intent);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_resize>) {
						resize_now(aTyped.Drawer, aTyped.Width);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_cancel>) {
						cancel_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_shutdown>) {
						shutdown_now();
					} else if constexpr (std::is_same_v<ingress_type, staged_observation>) {
						finish_observation(std::move(aTyped.Observation));
					} else if constexpr (std::is_same_v<ingress_type, staged_settings_load>) {
						finish_settings_load(aTyped.Context, std::move(aTyped.Result));
					} else if constexpr (std::is_same_v<ingress_type, staged_settings_store>) {
						finish_settings_store(aTyped.Context, aTyped.Result);
					} else {
						finish_selected_diff(std::move(aTyped.Observation));
					}
				},
				std::move(aIngress));
		}

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto count = DeferredIngress.size();
				for (std::size_t index = 0U; index < count; ++index) {
					auto ingress = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					process(std::move(ingress));
				}
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept
		{
			if (DeferredIngress.empty() || DeferredIngressScheduled) {
				return;
			}
			DeferredIngressScheduled = true;
			const auto gate          = std::weak_ptr<completion_gate>{ Gate };
			try {
				DecisionLane.post([gate] {
					auto* const host = locked_host(gate);
					if (host == nullptr) {
						return;
					}
					host->DeferredIngressScheduled = false;
					host->run_turn([] { });
				});
			} catch (...) {
				DeferredIngressScheduled = false;
				DeferredIngress.clear();
				ProjectionDeliveryFailed = true;
			}
		}

		[[nodiscard]] static auto locked_host(
			const std::weak_ptr<completion_gate>& aGate) noexcept -> implementation*
		{
			const auto locked = aGate.lock();
			if (!locked) {
				return nullptr;
			}
			return locked->Host.load(std::memory_order_acquire);
		}

		template <typename Terminal>
		static void deliver_terminal(const std::weak_ptr<completion_gate>& aGate,
									 Terminal                              aTerminal)
		{
			auto* const host = locked_host(aGate);
			if (host != nullptr) {
				host->enqueue(std::move(aTerminal));
			}
		}

		// --- Request identity -------------------------------------------------

		[[nodiscard]] auto next_context() noexcept -> workbench_request_context
		{
			const auto request = core::request_id{ NextRequest };
			++NextRequest;
			return {
				.Request    = request,
				.Session    = Session,
				.Generation = Generation,
			};
		}

		[[nodiscard]] auto local_context() const noexcept
			-> workbench_request_context
		{
			return {
				.Session    = Session,
				.Generation = Generation,
			};
		}

		void retire(const workbench_request_domain     aDomain,
					const workbench_terminal_rejection aRejection) noexcept
		{
			for (auto& record : Outstanding) {
				if (record.Domain != aDomain
					|| record.Retirement != workbench_terminal_rejection::none) {
					continue;
				}
				record.Retirement = aRejection;
				record.Stop.request_stop();
			}
		}

		void retire_all(const workbench_terminal_rejection aRejection) noexcept
		{
			for (auto& record : Outstanding) {
				if (record.Retirement == workbench_terminal_rejection::none) {
					record.Retirement = aRejection;
				}
				record.Stop.request_stop();
			}
		}

		/// \brief Claims one terminal against its outstanding request record.
		///
		/// \details The record is erased on first delivery, so a duplicate finds
		/// nothing and is refused. A record retired by supersession or cancellation
		/// keeps its retirement reason and refuses the terminal it was still owed,
		/// which is what makes a stale or reordered terminal unable to publish.
		[[nodiscard]] auto claim(const workbench_request_context& aContext,
								 const workbench_request_domain   aDomain) noexcept
			-> workbench_terminal_rejection
		{
			const auto found = std::ranges::find_if(
				Outstanding, [&aContext](const outstanding_request& aRecord) {
					return aRecord.Context.Request == aContext.Request;
				});
			if (found == Outstanding.end()) {
				return workbench_terminal_rejection::unknown_request;
			}
			const auto domain     = found->Domain;
			const auto context    = found->Context;
			const auto retirement = found->Retirement;
			Outstanding.erase(found);
			if (domain != aDomain || context != aContext) {
				return workbench_terminal_rejection::inexact_terminal;
			}
			if (retirement != workbench_terminal_rejection::none) {
				return retirement;
			}
			return Closing ? workbench_terminal_rejection::cancelled
						   : workbench_terminal_rejection::none;
		}

		void refuse(const workbench_terminal_rejection aRejection) noexcept
		{
			++RejectedTerminals;
			LastRejection = aRejection;
		}

		// --- Effect submission ------------------------------------------------

		[[nodiscard]] auto reserve(const workbench_request_domain aDomain)
			-> outstanding_request*
		{
			if (Outstanding.size() >= Limits.OutstandingRequests) {
				return nullptr;
			}
			Outstanding.push_back(outstanding_request{
				.Context = next_context(),
				.Domain  = aDomain,
			});
			return &Outstanding.back();
		}

		void abandon(const core::request_id aRequest) noexcept
		{
			const auto found = std::ranges::find_if(
				Outstanding, [aRequest](const outstanding_request& aRecord) {
					return aRecord.Context.Request == aRequest;
				});
			if (found != Outstanding.end()) {
				Outstanding.erase(found);
			}
		}

		void submit_observation(const workbench_pull_request_authority& aAuthority)
		{
			auto* const record = reserve(workbench_request_domain::observation);
			if (record == nullptr) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto request = workbench_observation_request{
				.Context      = record->Context,
				.Authority    = aAuthority,
				.MaximumFiles = Limits.ActionableFiles,
			};
			const auto  stop         = record->Stop.get_token();
			const auto  gate         = std::weak_ptr<completion_gate>{ Gate };
			auto* const observations = &Observations;
			auto* const lane         = &DecisionLane;
			try {
				Effects.post([gate, lane, observations, request, stop] {
					auto staged = staged_observation{
						.Observation = perform_observation(*observations, request, stop),
					};
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
			} catch (...) {
				abandon(request.Context.Request);
				publish_failure(request.Context, workbench_host_failure::observation_unavailable);
			}
		}

		[[nodiscard]] auto selected_file() const noexcept
			-> const workbench_file_observation_entry*
		{
			if (!Selected.has_value()) {
				return nullptr;
			}
			if (SelectedFile.has_value() && SelectedFile->File == *Selected) {
				return &*SelectedFile;
			}
			const auto found = std::ranges::find(
				Files, *Selected, &workbench_file_observation_entry::File);
			return found == Files.end() ? nullptr : &*found;
		}

		void submit_selected_diff()
		{
			if (SelectedDiffs == nullptr || SelectedDiffProjections == nullptr
				|| !Authority.has_value() || selected_file() == nullptr) {
				return;
			}
			retire(workbench_request_domain::selected_diff,
				   workbench_terminal_rejection::superseded);
			auto context = std::optional<workbench_selected_diff_context>{ };
			try {
				context = SelectedDiffs->context(*Authority);
			} catch (...) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			if (!context.has_value()) {
				publish_failure(
					local_context(), workbench_host_failure::selected_file_stale);
				return;
			}
			auto* const record = reserve(workbench_request_domain::selected_diff);
			if (record == nullptr) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto request = workbench_selected_diff_request{
				.Context            = record->Context,
				.WorkbenchAuthority = *Authority,
				.Diff               = *context,
				.File               = *selected_file(),
			};
			deliver_projection(
				[this, &request] { SelectedDiffProjections->begin(request); });
			const auto  stop     = record->Stop.get_token();
			const auto  gate     = std::weak_ptr<completion_gate>{ Gate };
			auto* const resolver = SelectedDiffs;
			auto* const lane     = &DecisionLane;
			try {
				Effects.post([gate, lane, resolver, request, stop] {
					auto staged = staged_selected_diff{
						.Observation = perform_selected_diff(*resolver, request, stop),
					};
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
			} catch (...) {
				abandon(request.Context.Request);
				auto failure   = workbench_selected_diff_observation{ .Request = request };
				failure.Status = workbench_selected_diff_status::failure;
				deliver_projection(
					[this, &failure] { SelectedDiffProjections->fail(failure); });
			}
		}

		[[nodiscard]] static auto perform_selected_diff(
			workbench_selected_diff_port&          aPort,
			const workbench_selected_diff_request& aRequest,
			std::stop_token                        aStop)
			-> workbench_selected_diff_observation
		{
			auto refusal = workbench_selected_diff_observation{
				.Request = aRequest,
				.Status  = workbench_selected_diff_status::cancelled,
			};
			if (aStop.stop_requested()) {
				return refusal;
			}
			try {
				return aPort.resolve(aRequest, std::move(aStop));
			} catch (...) {
				refusal.Status = workbench_selected_diff_status::failure;
				return refusal;
			}
		}

		[[nodiscard]] static auto perform_observation(
			workbench_observation_port&          aPort,
			const workbench_observation_request& aRequest,
			std::stop_token                      aStop) -> workbench_file_observation
		{
			auto refusal = workbench_file_observation{
				.Context   = aRequest.Context,
				.Authority = aRequest.Authority,
				.Status    = workbench_observation_status::cancelled,
			};
			if (aStop.stop_requested()) {
				return refusal;
			}
			try {
				return aPort.assemble(aRequest, std::move(aStop));
			} catch (...) {
				refusal.Status = workbench_observation_status::unavailable;
				return refusal;
			}
		}

		void submit_settings_load()
		{
			auto* const record = reserve(workbench_request_domain::settings_load);
			if (record == nullptr) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto  context  = record->Context;
			const auto  stop     = record->Stop.get_token();
			const auto  gate     = std::weak_ptr<completion_gate>{ Gate };
			auto* const settings = &Settings;
			auto* const lane     = &DecisionLane;
			try {
				Effects.post([gate, lane, settings, context, stop] {
					auto staged = staged_settings_load{
						.Context = context,
						.Result  = perform_settings_load(*settings, stop),
					};
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context, workbench_host_failure::settings_unavailable);
			}
		}

		[[nodiscard]] static auto perform_settings_load(
			workbench_settings_port& aPort, std::stop_token aStop)
			-> workbench_settings_result<workbench_layout_settings>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ workbench_settings_error{
					.Failure = workbench_settings_failure::cancelled } };
			}
			try {
				return aPort.load(std::move(aStop));
			} catch (...) {
				return std::unexpected{ workbench_settings_error{
					.Failure = workbench_settings_failure::unavailable } };
			}
		}

		/// \brief Persists the durable layout, coalescing with any pending write.
		///
		/// \details The core owns the drawer states while a workbench is active, so
		/// the record adopts them then. Before activation the loaded record is
		/// still the authority and must not be overwritten with defaults.
		void submit_settings_store()
		{
			if (State.Navigation.has_value()) {
				Layout.Files         = State.FilesDrawer;
				Layout.Conversations = State.ConversationsDrawer;
			}
			retire(workbench_request_domain::settings_store,
				   workbench_terminal_rejection::superseded);
			auto* const record = reserve(workbench_request_domain::settings_store);
			if (record == nullptr) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto  context  = record->Context;
			const auto  stop     = record->Stop.get_token();
			const auto  settings = Layout;
			const auto  gate     = std::weak_ptr<completion_gate>{ Gate };
			auto* const port     = &Settings;
			auto* const lane     = &DecisionLane;
			try {
				Effects.post([gate, lane, port, context, settings, stop] {
					auto staged = staged_settings_store{
						.Context = context,
						.Result  = perform_settings_store(*port, settings, stop),
					};
					lane->post([gate, staged]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context, workbench_host_failure::settings_unavailable);
			}
		}

		[[nodiscard]] static auto perform_settings_store(
			workbench_settings_port&         aPort,
			const workbench_layout_settings& aSettings,
			std::stop_token                  aStop)
			-> workbench_settings_result<void>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ workbench_settings_error{
					.Failure = workbench_settings_failure::cancelled } };
			}
			try {
				return aPort.store(aSettings, std::move(aStop));
			} catch (...) {
				return std::unexpected{ workbench_settings_error{
					.Failure = workbench_settings_failure::unavailable } };
			}
		}

		// --- Operations --------------------------------------------------------

		void restore_now()
		{
			if (Closing) {
				return;
			}
			retire(workbench_request_domain::settings_load,
				   workbench_terminal_rejection::superseded);
			submit_settings_load();
		}

		void observe_now(const workbench_pull_request_authority& aAuthority)
		{
			if (Closing) {
				return;
			}
			if (!valid(aAuthority)) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto hadAuthorityBoundState = !Files.empty() || Selected.has_value()
				|| State.Navigation.has_value();
			retire(workbench_request_domain::observation,
				   workbench_terminal_rejection::superseded);
			retire(workbench_request_domain::selected_diff,
				   workbench_terminal_rejection::superseded);
			Generation = core::request_generation{ Generation.value() + 1U };
			Authority  = aAuthority;
			Actionable.clear();
			Files.clear();
			Selected.reset();
			SelectedFile.reset();
			const auto output = core::observe_workbench_navigation(State, std::nullopt);
			LastReason        = output.Reason;
			if (hadAuthorityBoundState) {
				publish_changes(local_context());
			}
			submit_observation(aAuthority);
		}

		void refresh_selected_diff_now()
		{
			if (!Closing) {
				submit_selected_diff();
			}
		}

		void refresh_actionability_now()
		{
			if (Closing || !Authority.has_value()) {
				return;
			}
			retire(workbench_request_domain::observation,
				   workbench_terminal_rejection::superseded);
			submit_observation(*Authority);
		}

		void invalidate_now(const workbench_pull_request_authority& aAuthority)
		{
			if (Closing || !Authority.has_value() || *Authority != aAuthority) {
				return;
			}
			retire(workbench_request_domain::observation,
				   workbench_terminal_rejection::superseded);
			retire(workbench_request_domain::selected_diff,
				   workbench_terminal_rejection::superseded);
			Generation = core::request_generation{ Generation.value() + 1U };
			Authority.reset();
			Actionable.clear();
			Files.clear();
			Selected.reset();
			SelectedFile.reset();
			const auto output = core::observe_workbench_navigation(State, std::nullopt);
			LastReason        = output.Reason;
			publish_changes(local_context());
		}

		void apply_now(const core::workbench_interaction_intent& aIntent)
		{
			if (Closing) {
				return;
			}
			const auto output = core::transition(State, aIntent);
			LastReason        = output.Reason;
			if (output.Decision.has_value()) {
				execute_decision(*output.Decision);
			}
			if (output.Drawers.has_value()) {
				submit_settings_store();
			}
			publish_changes(local_context());
			if (output.Decision.has_value()) {
				submit_selected_diff();
			}
		}

		void execute_decision(const core::workbench_navigation_decision& aDecision)
		{
			const auto target = resolve_decision(Actionable, aDecision);
			if (!target.has_value()) {
				LastReason = core::workbench_reason_code::no_actionable_files;
				return;
			}
			Selected         = target;
			const auto found = std::ranges::find(
				Files, *target, &workbench_file_observation_entry::File);
			SelectedFile = found == Files.end()
				? std::nullopt
				: std::optional<workbench_file_observation_entry>{ *found };
			observe_cursor();
		}

		void observe_cursor()
		{
			const auto output = core::observe_workbench_navigation(
				State, assemble_cursor(Actionable, Selected));
			LastReason = output.Reason;
		}

		void resize_now(const core::workbench_drawer aDrawer,
						const std::uint32_t          aWidth)
		{
			if (Closing) {
				return;
			}
			auto&      retained = aDrawer == core::workbench_drawer::files
				? Layout.FilesWidth
				: Layout.ConversationsWidth;
			const auto width    = clamped_width(aWidth, Limits);
			if (retained == width) {
				return;
			}
			retained = width;
			submit_settings_store();
			publish_changes(local_context());
		}

		void cancel_now()
		{
			if (Closing) {
				return;
			}
			retire_all(workbench_terminal_rejection::cancelled);
			Generation = core::request_generation{ Generation.value() + 1U };
		}

		void shutdown_now()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			retire_all(workbench_terminal_rejection::cancelled);
		}

		// --- Terminals ---------------------------------------------------------

		void finish_observation(workbench_file_observation aObservation)
		{
			const auto rejection = claim(aObservation.Context,
										 workbench_request_domain::observation);
			if (rejection != workbench_terminal_rejection::none) {
				refuse(rejection);
				return;
			}
			if (!exact_observation(aObservation)) {
				publish_failure(aObservation.Context,
								workbench_host_failure::host_failure);
				return;
			}
			if (aObservation.Status != workbench_observation_status::available) {
				publish_failure(aObservation.Context,
								failure_for_status(aObservation.Status));
				return;
			}
			if (aObservation.Actionable.size() > Limits.ActionableFiles) {
				publish_failure(aObservation.Context,
								workbench_host_failure::observation_limit);
				return;
			}
			Files = std::move(aObservation.Actionable);
			if (Selected.has_value()) {
				const auto found = std::ranges::find(
					Files, *Selected, &workbench_file_observation_entry::File);
				if (found != Files.end()) {
					SelectedFile = *found;
				} else if (!SelectedFile.has_value()
						   || SelectedFile->File != *Selected) {
					SelectedFile.reset();
				}
			} else {
				SelectedFile.reset();
			}
			Actionable.clear();
			Actionable.reserve(Files.size());
			std::ranges::transform(
				Files, std::back_inserter(Actionable),
				&workbench_file_observation_entry::File);
			observe_cursor();
			apply_restored_drawers();
			publish_changes(aObservation.Context);
		}

		void finish_selected_diff(workbench_selected_diff_observation aObservation)
		{
			const auto rejection = claim(
				aObservation.Request.Context,
				workbench_request_domain::selected_diff);
			if (rejection != workbench_terminal_rejection::none) {
				refuse(rejection);
				return;
			}
			if (SelectedDiffs == nullptr || SelectedDiffProjections == nullptr) {
				return;
			}
			const auto exactCurrent = [&]() {
				if (!Authority.has_value()
					|| aObservation.Request.WorkbenchAuthority != *Authority
					|| aObservation.Request.Context.Generation != Generation
					|| !Selected.has_value()
					|| aObservation.Request.File.File != *Selected
					|| selected_file() == nullptr
					|| *selected_file() != aObservation.Request.File) {
					return false;
				}
				try {
					return SelectedDiffs->context(*Authority)
						== std::optional{ aObservation.Request.Diff };
				} catch (...) {
					return false;
				}
			};
			if (!exactCurrent()) {
				aObservation.Status = workbench_selected_diff_status::stale;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			if (aObservation.Status != workbench_selected_diff_status::available) {
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			if (!aObservation.LeftBlob && !aObservation.RightBlob) {
				aObservation.Status = workbench_selected_diff_status::failure;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			const auto& diff        = aObservation.Request.Diff;
			auto        publication = workbench_selected_diff_publication{
				.Request = aObservation.Request,
				.Key     = core::file_comparison_key{
					.Authority   = diff.Authority,
					.Repository  = diff.Authority.Repository,
					.PullRequest = diff.Authority.PullRequest,
					.LeftAnchor  = diff.Comparison.Left.Identity,
					.RightAnchor = diff.Comparison.Right.Identity,
					.LeftBlob    = aObservation.LeftBlob,
					.RightBlob   = aObservation.RightBlob,
					.File        = aObservation.Request.File.File,
					.Options     = diff.OptionsDigest,
					.Algorithm   = diff.Algorithm,
				},
			};
			if (!core::valid(publication.Key)) {
				aObservation.Status = workbench_selected_diff_status::failure;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			deliver_projection([this, &publication] {
				SelectedDiffProjections->publish(publication);
			});
		}

		[[nodiscard]] auto exact_observation(
			const workbench_file_observation& aObservation) const noexcept -> bool
		{
			return Authority.has_value() && aObservation.Authority == *Authority
				&& std::ranges::all_of(
					   aObservation.Actionable,
					   [](const workbench_file_observation_entry& aFile) {
						   return valid(aFile);
					   });
		}

		void finish_settings_load(
			const workbench_request_context&                            aContext,
			const workbench_settings_result<workbench_layout_settings>& aResult)
		{
			const auto rejection = claim(aContext,
										 workbench_request_domain::settings_load);
			if (rejection != workbench_terminal_rejection::none) {
				refuse(rejection);
				return;
			}
			if (!aResult.has_value()) {
				publish_failure(aContext,
								workbench_host_failure::settings_unavailable);
				return;
			}
			Layout                    = *aResult;
			Layout.FilesWidth         = clamped_width(Layout.FilesWidth, Limits);
			Layout.ConversationsWidth = clamped_width(Layout.ConversationsWidth, Limits);
			RestorePending            = true;
			apply_restored_drawers();
			publish_changes(aContext);
		}

		void finish_settings_store(const workbench_request_context&       aContext,
								   const workbench_settings_result<void>& aResult)
		{
			const auto rejection = claim(aContext,
										 workbench_request_domain::settings_store);
			if (rejection != workbench_terminal_rejection::none) {
				refuse(rejection);
				return;
			}
			if (!aResult.has_value()) {
				publish_failure(aContext,
								workbench_host_failure::settings_unavailable);
				return;
			}
			++PersistedLayouts;
		}

		/// \brief Applies restored drawer states once the workbench is active.
		///
		/// \details A drawer gesture is only legal while a workbench is published,
		/// so a layout restored before the first observation waits here rather than
		/// being decided outside the core. Restoring never writes the layout back.
		void apply_restored_drawers()
		{
			if (!RestorePending || !State.Navigation.has_value()) {
				return;
			}
			RestorePending = false;
			set_drawer(core::workbench_drawer::files, Layout.Files);
			set_drawer(core::workbench_drawer::conversations, Layout.Conversations);
		}

		void set_drawer(const core::workbench_drawer       aDrawer,
						const core::workbench_drawer_state aState)
		{
			const auto output = core::transition(
				State,
				core::set_workbench_drawer_intent{
					.Drawer = aDrawer,
					.State  = aState,
				});
			LastReason = output.Reason;
		}

		// --- Publication -------------------------------------------------------

		/// \brief Publishes exactly the projection domains whose meaning changed.
		///
		/// \details The retained published snapshot, not the core transition
		/// output, decides what presentation has already seen. That keeps a bounded
		/// change from republishing an unrelated domain, and keeps a gesture that
		/// ends where it started from publishing at all.
		void publish_changes(const workbench_request_context& aContext)
		{
			if (Closing) {
				return;
			}
			auto       projection = workbench_projection{ .Context = aContext };
			const auto selection  = core::workbench_selection_delta{
				.Cursor = State.Navigation,
			};
			if (PublishedSelection != selection) {
				projection.Selection = selection;
			}
			const auto drawers = core::workbench_drawer_delta{
				.Files         = State.FilesDrawer,
				.Conversations = State.ConversationsDrawer,
			};
			if (PublishedDrawers != drawers) {
				projection.Drawers = drawers;
			}
			const auto commands = core::project_workbench_commands(State);
			if (PublishedCommands != commands) {
				projection.Commands = commands;
			}
			const auto layout = workbench_layout_delta{
				.FilesWidth         = Layout.FilesWidth,
				.ConversationsWidth = Layout.ConversationsWidth,
			};
			if (PublishedLayout != layout) {
				projection.Layout = layout;
			}
			auto files = std::vector<workbench_file_projection>{ };
			files.reserve(Files.size());
			std::ranges::transform(
				Files,
				std::back_inserter(files),
				[](const workbench_file_observation_entry& aFile) {
					return workbench_file_projection{
						.File = aFile.File,
						.Path = aFile.PresentationPath,
					};
				});
			if (PublishedFiles != files) {
				projection.Files = files;
			}
			if (empty(projection)) {
				return;
			}
			PublishedSelection = selection;
			PublishedDrawers   = drawers;
			PublishedCommands  = commands;
			PublishedLayout    = layout;
			PublishedFiles     = std::move(files);
			deliver_projection(
				[this, &projection] { Projections.publish(projection); });
		}

		void publish_failure(const workbench_request_context& aContext,
							 const workbench_host_failure     aFailure)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, aFailure] {
				Projections.fail(workbench_failure_publication{
					.Context = aContext,
					.Failure = aFailure,
				});
			});
		}

		template <typename Callback>
		void deliver_projection(Callback&& aCallback)
		{
			try {
				std::forward<Callback>(aCallback)();
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		core::workbench_interaction_state        State;
		workbench_observation_port&              Observations;
		workbench_settings_port&                 Settings;
		effect_executor&                         Effects;
		decision_executor&                       DecisionLane;
		workbench_projection_port&               Projections;
		workbench_selected_diff_port*            SelectedDiffs;
		workbench_selected_diff_projection_port* SelectedDiffProjections;
		core::application_session_id             Session;
		workbench_host_limits                    Limits;
		std::shared_ptr<completion_gate>         Gate;

		std::vector<core::workbench_file_id>            Actionable;
		std::vector<workbench_file_observation_entry>   Files;
		std::optional<core::workbench_file_id>          Selected;
		std::optional<workbench_file_observation_entry> SelectedFile;
		std::optional<workbench_pull_request_authority> Authority;
		workbench_layout_settings                       Layout;

		std::vector<outstanding_request>       Outstanding;
		std::deque<deferred_workbench_ingress> DeferredIngress;
		std::uint64_t                          NextRequest = 1U;
		core::request_generation               Generation{ 1U };

		std::optional<core::workbench_selection_delta>        PublishedSelection;
		std::optional<core::workbench_drawer_delta>           PublishedDrawers;
		std::optional<core::workbench_command_delta>          PublishedCommands;
		std::optional<workbench_layout_delta>                 PublishedLayout;
		std::optional<std::vector<workbench_file_projection>> PublishedFiles;

		std::size_t                  RejectedTerminals        = 0U;
		std::size_t                  PersistedLayouts         = 0U;
		workbench_terminal_rejection LastRejection            = workbench_terminal_rejection::none;
		core::workbench_reason_code  LastReason               = core::workbench_reason_code::none;
		bool                         RestorePending           = false;
		bool                         Closing                  = false;
		bool                         TurnActive               = false;
		bool                         DeferredIngressScheduled = false;
		bool                         ProjectionDeliveryFailed = false;
	};

	workbench_application_host::workbench_application_host(
		core::workbench_interaction_state        aInitialState,
		workbench_observation_port&              aObservations,
		workbench_settings_port&                 aSettings,
		effect_executor&                         aEffects,
		decision_executor&                       aDecisionLane,
		workbench_projection_port&               aProjections,
		const core::application_session_id       aSession,
		const workbench_host_limits              aLimits,
		workbench_selected_diff_port*            aSelectedDiffs,
		workbench_selected_diff_projection_port* aSelectedDiffProjections)
		: Implementation{ std::make_unique<implementation>(aInitialState,
														   aObservations,
														   aSettings,
														   aEffects,
														   aDecisionLane,
														   aProjections,
														   aSession,
														   aLimits,
														   aSelectedDiffs,
														   aSelectedDiffProjections) }
	{
	}

	workbench_application_host::~workbench_application_host()
	{
		Implementation->shutdown_now();
	}

	void workbench_application_host::restore()
	{
		Implementation->enqueue(deferred_workbench_restore{ });
	}

	void workbench_application_host::observe(workbench_pull_request_authority aAuthority)
	{
		Implementation->enqueue(deferred_workbench_observe{ std::move(aAuthority) });
	}

	void workbench_application_host::invalidate(workbench_pull_request_authority aAuthority)
	{
		Implementation->enqueue(deferred_workbench_invalidate{ std::move(aAuthority) });
	}

	void workbench_application_host::refresh_selected_diff()
	{
		Implementation->enqueue(deferred_workbench_selected_diff_refresh{ });
	}

	void workbench_application_host::refresh_actionability()
	{
		Implementation->enqueue(deferred_workbench_actionability_refresh{ });
	}

	void workbench_application_host::apply(core::workbench_interaction_intent aIntent)
	{
		Implementation->enqueue(deferred_workbench_intent{ aIntent });
	}

	void workbench_application_host::resize(const core::workbench_drawer aDrawer,
											const std::uint32_t          aWidth)
	{
		Implementation->enqueue(deferred_workbench_resize{
			.Drawer = aDrawer,
			.Width  = aWidth,
		});
	}

	void workbench_application_host::cancel()
	{
		Implementation->enqueue(deferred_workbench_cancel{ });
	}

	void workbench_application_host::shutdown()
	{
		Implementation->enqueue(deferred_workbench_shutdown{ });
	}

	auto workbench_application_host::state() const noexcept
		-> const core::workbench_interaction_state&
	{
		return Implementation->State;
	}

	auto workbench_application_host::layout() const noexcept
		-> const workbench_layout_settings&
	{
		return Implementation->Layout;
	}

	auto workbench_application_host::actionable() const noexcept
		-> std::span<const core::workbench_file_id>
	{
		return Implementation->Actionable;
	}

	auto workbench_application_host::selected() const noexcept
		-> const std::optional<core::workbench_file_id>&
	{
		return Implementation->Selected;
	}

	auto workbench_application_host::generation() const noexcept
		-> core::request_generation
	{
		return Implementation->Generation;
	}

	auto workbench_application_host::outstanding_requests() const noexcept
		-> std::size_t
	{
		return Implementation->Outstanding.size();
	}

	auto workbench_application_host::rejected_terminals() const noexcept
		-> std::size_t
	{
		return Implementation->RejectedTerminals;
	}

	auto workbench_application_host::last_rejection() const noexcept
		-> workbench_terminal_rejection
	{
		return Implementation->LastRejection;
	}

	auto workbench_application_host::last_reason() const noexcept
		-> core::workbench_reason_code
	{
		return Implementation->LastReason;
	}

	auto workbench_application_host::persisted_layouts() const noexcept
		-> std::size_t
	{
		return Implementation->PersistedLayouts;
	}

	auto workbench_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto workbench_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Outstanding.empty()
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->TurnActive;
	}

	auto workbench_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
} // namespace bpr::application
