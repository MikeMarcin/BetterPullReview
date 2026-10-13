#include "bpr/application/workbench_application_host.hpp"

#include "header_source_index.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
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

		struct deferred_workbench_actionability_retain
		{
			workbench_pull_request_authority Authority;
			core::workbench_file_id          File;
			bool                             Actionable = true;
		};

		struct deferred_workbench_change_navigation
		{
			workbench_selected_diff_request              Request;
			std::optional<core::workbench_change_cursor> Cursor;
		};

		struct deferred_workbench_resize
		{
			core::workbench_drawer Drawer = core::workbench_drawer::files;
			std::uint32_t          Width  = 0U;
		};

		struct deferred_workbench_behaviour
		{
			review_behaviour_settings Settings;
		};

		struct deferred_workbench_home_view
		{
			home_view_state View;
		};
		struct deferred_workbench_summary_resize
		{
			std::optional<std::uint32_t> Fraction;
		};
		struct deferred_workbench_line_details_resize
		{
			std::optional<std::uint32_t> Height;
		};
		struct deferred_workbench_geometry
		{
			workbench_window_geometry Geometry;
		};

		struct deferred_workbench_preferences
		{
			workbench_profile_preferences Preferences;
		};
		struct deferred_workbench_cancel
		{
		};

		struct deferred_workbench_shutdown
		{
		};
		struct deferred_workbench_orderly_shutdown
		{
		};

		using deferred_workbench_ingress = std::variant<deferred_workbench_restore,
														deferred_workbench_observe,
														deferred_workbench_invalidate,
														deferred_workbench_selected_diff_refresh,
														deferred_workbench_actionability_refresh,
														deferred_workbench_actionability_retain,
														deferred_workbench_change_navigation,
														deferred_workbench_intent,
														deferred_workbench_resize,
														deferred_workbench_behaviour, deferred_workbench_home_view, deferred_workbench_geometry, deferred_workbench_summary_resize, deferred_workbench_line_details_resize,
														deferred_workbench_preferences,
														deferred_workbench_cancel,
														deferred_workbench_shutdown, deferred_workbench_orderly_shutdown,
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
			const std::optional<core::workbench_file_id>&  aSelected,
			const std::optional<std::size_t>               aPosition) noexcept
			-> core::workbench_navigation_cursor
		{
			auto cursor = core::workbench_navigation_cursor{
				.File            = aSelected,
				.ActionableCount = static_cast<std::uint32_t>(aOrder.size()),
			};
			if (!aPosition.has_value()) {
				cursor.PreviousAvailable = !aOrder.empty();
				cursor.NextAvailable     = !aOrder.empty();
				return cursor;
			}
			cursor.Ordinal           = static_cast<std::uint32_t>(*aPosition + 1U);
			cursor.PreviousAvailable = cursor.Ordinal > 1U;
			cursor.NextAvailable     = cursor.Ordinal < cursor.ActionableCount;
			return cursor;
		}

		[[nodiscard]] auto header_source_repository_path(const std::filesystem::path& aPath) -> std::string
		{
			const auto utf8 = aPath.generic_u8string();
			return { utf8.begin(), utf8.end() };
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
					   workbench_selected_diff_projection_port* aSelectedDiffProjections,
					   workbench_preferences_host* aPreferences, const core::bpr_profile_id aProfile)
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
			if (aPreferences) {
				Preferences = aPreferences;
			} else {
				OwnedPreferences = std::make_unique<workbench_preferences_host>(Settings, Effects, DecisionLane, aProfile);
				Preferences      = OwnedPreferences.get();
			}
			const auto gate        = std::weak_ptr<completion_gate>{ Gate };
			PreferenceSubscription = Preferences->subscribe(aProfile, [gate](const workbench_profile_preferences& aAccepted) {
				if (const auto locked = gate.lock()) {
					if (auto* host = locked->Host.load(std::memory_order_acquire)) {
						host->enqueue(deferred_workbench_preferences{ aAccepted });
					}
				}
			});
		}

		~implementation()
		{
			Preferences->unsubscribe(PreferenceSubscription);
			if (OwnedPreferences) {
				OwnedPreferences->shutdown();
			}
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
					} else if constexpr (std::is_same_v<
											 ingress_type,
											 deferred_workbench_actionability_retain>) {
						retain_actionability_now(aTyped.Authority, aTyped.File, aTyped.Actionable);
					} else if constexpr (std::is_same_v<
											 ingress_type,
											 deferred_workbench_change_navigation>) {
						observe_change_navigation_now(
							aTyped.Request, aTyped.Cursor);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_intent>) {
						apply_now(aTyped.Intent);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_resize>) {
						resize_now(aTyped.Drawer, aTyped.Width);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_behaviour>) {
						apply_behaviour_now(aTyped.Settings);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_home_view>) {
						apply_home_view_now(aTyped.View);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_summary_resize>) {
						resize_summary_now(aTyped.Fraction);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_line_details_resize>) {
						resize_line_details_now(aTyped.Height);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_geometry>) {
						observe_geometry_now(aTyped.Geometry);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_preferences>) {
						accept_preferences(aTyped.Preferences);
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_cancel>) {
						cancel_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_shutdown>) {
						shutdown_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_workbench_orderly_shutdown>) {
						shutdown_orderly_now();
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
			flush_settings_store();
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
			// [[BPR-0783]]. An orderly close also admits the settings load that a
			// held Home view is still waiting for: that record is the only thing
			// the view can be merged into, and `shutdown_orderly_now` leaves the
			// load alone for exactly as long as one is held.
			const auto drained = aDomain == workbench_request_domain::settings_store
				|| (aDomain == workbench_request_domain::settings_load && (HeldHome.has_value() || UpdatesLineHeld || HeldSummary.has_value() || HeldLineDetails.has_value()));
			return Closing && !(OrderlyClosing && drained)
				? workbench_terminal_rejection::cancelled
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
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
					});
				});
			} catch (...) {
				abandon(request.Context.Request);
				publish_failure(request.Context, workbench_host_failure::observation_unavailable);
			}
		}

		/// \brief Looks up a file without walking the accepted navigation order.
		[[nodiscard]] auto file_position(const core::workbench_file_id aFile) const noexcept -> std::optional<std::size_t>
		{
			const auto found = std::ranges::lower_bound(FileIndex, aFile, [this](const auto& aEntry, const auto aIdentity) {
				++WorkStatistics.FileLookupComparisons;
				return aEntry < aIdentity; }, [](const auto& aEntry) { return aEntry.first; });
			if (found == FileIndex.end()) {
				return std::nullopt;
			}
			++WorkStatistics.FileLookupComparisons;
			return found->first == aFile ? std::optional{ found->second } : std::nullopt;
		}

		/// \brief Enters a file inserted at an order position into the index.
		/// \details Later positions move up by one; no identity is compared
		/// beyond the binary search for the new entry.
		void index_inserted(const core::workbench_file_id aFile, const std::size_t aPosition)
		{
			for (auto& entry : FileIndex) {
				++WorkStatistics.FilePositionVisits;
				entry.second += entry.second >= aPosition ? 1U : 0U;
			}
			const auto at = std::ranges::lower_bound(FileIndex, std::pair{ aFile, aPosition });
			FileIndex.insert(at, std::pair{ aFile, aPosition });
		}

		/// \brief Removes a file erased from an order position from the index.
		void index_erased(const core::workbench_file_id aFile, const std::size_t aPosition)
		{
			if (const auto at = std::ranges::lower_bound(FileIndex, std::pair{ aFile, aPosition });
				at != FileIndex.end() && *at == std::pair{ aFile, aPosition }) {
				FileIndex.erase(at);
			}
			for (auto& entry : FileIndex) {
				++WorkStatistics.FilePositionVisits;
				entry.second -= entry.second > aPosition ? 1U : 0U;
			}
		}

		void rebuild_file_index()
		{
			FileIndex.clear();
			FileIndex.reserve(Files.size());
			for (std::size_t index = 0U; index < Files.size(); ++index) {
				FileIndex.emplace_back(Files[index].File, index);
			}
			std::ranges::sort(FileIndex);
		}

		[[nodiscard]] auto catalogue_file(const core::workbench_file_id aFile) const noexcept -> const workbench_file_observation_entry*
		{
			const auto position = file_position(aFile);
			if (position) {
				return &Files[*position];
			}
			const auto withheld = Withheld.find(aFile);
			return withheld == Withheld.end() ? nullptr : &withheld->second;
		}

		[[nodiscard]] auto header_source_target() const -> core::header_source_resolution
		{
			if (Closing || !CatalogueAvailable) {
				return { .Status = core::header_source_status::unavailable_catalogue };
			}
			if (!Selected) {
				return { .Status = core::header_source_status::no_selection };
			}
			const auto* selected = catalogue_file(*Selected);
			if (selected == nullptr) {
				return { .Status = core::header_source_status::no_selection };
			}
			return Counterparts.resolve(header_source_repository_path(selected->CurrentPath), WorkStatistics.CounterpartDirectoryLookups);
		}

		[[nodiscard]] auto index_counterparts(const workbench_file_observation& aObservation) -> header_source_index
		{
			auto       index      = header_source_index{ };
			auto       identities = std::set<core::workbench_file_id>{ };
			const auto include    = [&](const workbench_file_observation_entry& aEntry) {
				if (identities.insert(aEntry.File).second) {
					index.insert(aEntry.File, header_source_repository_path(aEntry.CurrentPath));
					++WorkStatistics.CounterpartIndexedRows;
				}
			};
			std::ranges::for_each(aObservation.Actionable, include);
			std::ranges::for_each(aObservation.Withheld, include);
			return index;
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
			const auto position = file_position(*Selected);
			return position ? &Files[*position] : nullptr;
		}

		[[nodiscard]] auto preparation_window(const std::size_t aWindow) const
			-> std::vector<workbench_selected_diff_request>
		{
			if (Closing || SelectedDiffs == nullptr || !Authority.has_value() || Files.empty()) {
				return { };
			}
			auto context = std::optional<workbench_selected_diff_context>{ };
			try {
				context = SelectedDiffs->context(*Authority);
			} catch (...) {
				return { };
			}
			if (!context.has_value()) {
				return { };
			}
			const auto cursor = Selected.has_value() ? file_position(*Selected) : std::nullopt;
			auto       result = std::vector<workbench_selected_diff_request>{ };
			for (const auto position : core::prepared_comparison_order(Files.size(), cursor, aWindow)) {
				const auto& file = Files[position];
				if (file.ContentKind == workbench_file_content_kind::binary) {
					continue;
				}
				// Not a request this host tracks: the identity only has to be a
				// valid one. An open of the file is answered under its own.
				auto identity    = local_context();
				identity.Request = core::request_id{ result.size() + 1U };
				result.push_back(workbench_selected_diff_request{
					.Context            = identity,
					.WorkbenchAuthority = *Authority,
					.Diff               = *context,
					.File               = file,
				});
			}
			return result;
		}

		void submit_selected_diff()
		{
			clear_change_navigation();
			publish_changes(local_context());
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
					local_context(), workbench_host_failure::selected_file_comparison_unavailable);
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
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
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
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
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
		/// \details The core owns the drawer states and the source-pane numbering
		/// while a workbench is active, so the record adopts them then. Before
		/// activation the loaded record is still the authority and must not be
		/// overwritten with defaults.
		void submit_settings_store()
		{
			if (PreferenceSubscription == 0) {
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				return;
			}
			if (State.Navigation.has_value()) {
				Layout.Files         = State.FilesDrawer;
				Layout.Conversations = State.ConversationsDrawer;
				Layout.Repositories  = State.RepositoriesDrawer;
				Layout.LineNumbers   = State.LineNumbers;
				Layout.OnePane       = State.OnePane;
			}
			if (profile_defaults_scoped()) {
				// [[BPR-0783]]. Home belongs to the window that shows it. The
				// profile's defaults record has no window, so the view is dropped
				// here rather than stored as a default every later window would
				// inherit without having chosen it.
				auto defaults        = window_layout(Layout);
				defaults.Home        = { };
				defaults.Geometry    = { };
				defaults.UpdatesLine = core::workbench_updates_line::folded;
				defaults.SummaryFraction.reset();
				defaults.LineDetailsHeight.reset();
				PendingPreferences        = workbench_profile_preferences{ Layout.Behaviour, defaults, Layout.WritingHooks, Layout.ClaudeWrapper, Layout.ClaudeModel };
				PendingPreferenceDefaults = true;
				if (!Preferences->apply_defaults(PendingPreferences->NewWindowDefaults)) {
					PendingPreferences.reset();
					publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				}
				return;
			}
			PendingLayout = Layout;
		}

		void flush_settings_store()
		{
			if (!PendingLayout || (Closing && !OrderlyClosing)
				|| Outstanding.size() >= Limits.OutstandingRequests
				|| std::ranges::any_of(Outstanding, [](const auto& aRecord) { return aRecord.Domain == workbench_request_domain::settings_store; })) {
				return;
			}
			auto* const record = reserve(workbench_request_domain::settings_store);
			if (record == nullptr) {
				publish_failure(local_context(), workbench_host_failure::host_failure);
				return;
			}
			const auto  context  = record->Context;
			const auto  stop     = record->Stop.get_token();
			const auto  settings = std::exchange(PendingLayout, std::nullopt).value();
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
				publish_failure(context, workbench_host_failure::settings_unavailable, true);
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
			if (PreferenceSubscription == 0) {
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				return;
			}
			if (Closing) {
				return;
			}
			retire(workbench_request_domain::settings_load,
				   workbench_terminal_rejection::superseded);
			// [[BPR-0783]]. A fresh attempt at the record clears the earlier
			// refusal, so a Home gesture waits for this load rather than being
			// applied as though nothing could ever be stored.
			LayoutLoadFailed = false;
			if (Settings.profile_scoped() && Settings.window().value() != 0) {
				Preferences->restore();
			}
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
			FileIndex.clear();
			Withheld.clear();
			Counterparts       = header_source_index{ };
			CatalogueAvailable = false;
			Numbered           = false;
			PendingFilesDelta.reset();
			FilesDirty = true;
			Selected.reset();
			SelectedFile.reset();
			const auto output = core::observe_workbench_navigation(State, std::nullopt);
			LastReason        = output.Reason;
			clear_change_navigation();
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
			++WorkStatistics.ActionabilityObservations;
			retire(workbench_request_domain::observation,
				   workbench_terminal_rejection::superseded);
			submit_observation(*Authority);
		}

		/// \brief Applies the retained receipt's actionability to the order in place.
		///
		/// \details [[BPR-0972]]. The port's order is its catalogue less the
		/// withheld files, and only the retained file is ever withheld. So the
		/// new order restores every withheld file but `aFile`, and withholds
		/// `aFile` when it is not actionable: exactly what observing again would
		/// answer. It observes again instead when an observation is in flight,
		/// when a file to restore was never named, or when the catalogue is not
		/// numbered.
		void retain_actionability_now(
			const workbench_pull_request_authority& aAuthority,
			const core::workbench_file_id           aFile,
			const bool                              aActionable)
		{
			if (Closing || !Authority.has_value() || *Authority != aAuthority) {
				return;
			}
			const auto observing = std::ranges::any_of(Outstanding, [](const outstanding_request& aRecord) {
				return aRecord.Domain == workbench_request_domain::observation
					&& aRecord.Retirement == workbench_terminal_rejection::none;
			});
			if (observing || !Numbered) {
				refresh_actionability_now();
				return;
			}
			auto delta = workbench_files_delta{ };
			for (auto withheld = Withheld.begin(); withheld != Withheld.end();) {
				if (withheld->first == aFile && !aActionable) {
					++withheld;
					continue;
				}
				const auto position = std::ranges::lower_bound(Files, withheld->second.Ordinal, { }, &workbench_file_observation_entry::Ordinal);
				const auto at       = static_cast<std::size_t>(std::distance(Files.begin(), position));
				Files.insert(position, std::move(withheld->second));
				Actionable.insert(Actionable.begin() + static_cast<std::ptrdiff_t>(at), withheld->first);
				index_inserted(withheld->first, at);
				for (auto& inserted : delta.Inserted) {
					inserted.first += inserted.first >= at ? 1U : 0U;
				}
				delta.Inserted.emplace_back(at, workbench_file_projection{ .File = Files[at].File, .Path = Files[at].PresentationPath });
				if (PublishedFiles) {
					WorkStatistics.PublishedFileRowsShifted += PublishedFiles->size() - at;
					PublishedFiles->insert(PublishedFiles->begin() + static_cast<std::ptrdiff_t>(at), delta.Inserted.back().second);
				}
				withheld = Withheld.erase(withheld);
			}
			if (!aActionable) {
				if (const auto position = file_position(aFile)) {
					if (PublishedFiles) {
						WorkStatistics.PublishedFileRowsShifted += PublishedFiles->size() - *position - 1U;
						PublishedFiles->erase(PublishedFiles->begin() + static_cast<std::ptrdiff_t>(*position));
					}
					Withheld.emplace(aFile, std::move(Files[*position]));
					Files.erase(Files.begin() + static_cast<std::ptrdiff_t>(*position));
					Actionable.erase(Actionable.begin() + static_cast<std::ptrdiff_t>(*position));
					index_erased(aFile, *position);
					std::erase_if(delta.Inserted, [&aFile](const auto& aInserted) { return aInserted.second.File == aFile; });
					for (auto& inserted : delta.Inserted) {
						inserted.first -= inserted.first > *position ? 1U : 0U;
					}
					delta.Removed.push_back(aFile);
				}
			}
			if (delta.Removed.empty() && delta.Inserted.empty()) {
				return;
			}
			std::ranges::sort(delta.Inserted, { }, &std::pair<std::size_t, workbench_file_projection>::first);
			++WorkStatistics.TargetedActionabilityUpdates;
			WorkStatistics.TargetedActionabilityFiles += delta.Removed.size() + delta.Inserted.size();
			PendingFilesDelta = std::move(delta);
			if (Selected.has_value() && file_position(*Selected).has_value()) {
				SelectedFile = Files[*file_position(*Selected)];
			}
			observe_cursor();
			publish_changes(local_context());
		}

		void observe_change_navigation_now(
			const workbench_selected_diff_request&              aRequest,
			const std::optional<core::workbench_change_cursor>& aCursor)
		{
			if (Closing || !AcceptedSelectedDiff.has_value()
				|| aRequest != *AcceptedSelectedDiff
				|| !Authority.has_value()
				|| aRequest.WorkbenchAuthority != *Authority
				|| aRequest.Context.Session != Session
				|| aRequest.Context.Generation != Generation
				|| !Selected.has_value()
				|| aRequest.File.File != *Selected) {
				return;
			}
			const auto output = core::observe_workbench_change_navigation(State, aCursor);
			LastReason        = output.Reason;
			if (output.Reason == core::workbench_reason_code::none) {
				publish_changes(aRequest.Context);
			}
		}

		void clear_change_navigation()
		{
			AcceptedSelectedDiff.reset();
			const auto output = core::observe_workbench_change_navigation(
				State, std::nullopt);
			LastReason = output.Reason;
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
			FileIndex.clear();
			Withheld.clear();
			Counterparts       = header_source_index{ };
			CatalogueAvailable = false;
			Numbered           = false;
			PendingFilesDelta.reset();
			FilesDirty = true;
			Selected.reset();
			SelectedFile.reset();
			const auto output = core::observe_workbench_navigation(State, std::nullopt);
			LastReason        = output.Reason;
			clear_change_navigation();
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
			if (output.Drawers.has_value() || output.LineNumbers.has_value() || output.OnePane.has_value()) {
				submit_settings_store();
			}
			if (output.UpdatesLine.has_value()) {
				keep_updates_line();
			}
			// [[BPR-0727]]. A refused gesture is published as the event it
			// is, so presentation can explain it where the reviewer looks.
			PendingRefusal = output.Reason == core::workbench_reason_code::none
				? std::nullopt
				: std::optional{ output.Reason };
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
			Selected          = target;
			const auto* entry = catalogue_file(*target);
			SelectedFile      = entry ? std::optional{ *entry } : std::nullopt;
			observe_cursor();
		}

		void observe_cursor()
		{
			const auto output = core::observe_workbench_navigation(
				State, assemble_cursor(Actionable, Selected, Selected ? file_position(*Selected) : std::nullopt));
			LastReason = output.Reason;
		}

		void resize_now(const core::workbench_drawer aDrawer,
						const std::uint32_t          aWidth)
		{
			if (Closing) {
				return;
			}
			auto& retained = [this, aDrawer]() -> std::uint32_t& {
				switch (aDrawer) {
				case core::workbench_drawer::files:
					return Layout.FilesWidth;
				case core::workbench_drawer::conversations:
					return Layout.ConversationsWidth;
				case core::workbench_drawer::repositories:
					return Layout.RepositoriesWidth;
				}
				return Layout.FilesWidth;
			}();
			const auto width = clamped_width(aWidth, Limits);
			if (retained == width) {
				return;
			}
			retained = width;
			submit_settings_store();
			publish_changes(local_context());
		}

		// [[BPR-0661]]. A record outside its vocabulary is refused; the held
		// record is republished so the page reverts to it.
		void apply_behaviour_now(const review_behaviour_settings& aSettings)
		{
			if (PreferenceSubscription == 0) {
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				return;
			}
			if (Closing) {
				return;
			}
			if (!valid(aSettings)) {
				RepublishBehaviour = true;
				publish_changes(local_context());
				return;
			}
			// [[BPR-1088]]. A record equal to the held one is nothing to write,
			// unless this window's earlier edit is still in flight: then it is
			// the reviewer going back, and it must follow that edit.
			if (Layout.Behaviour == aSettings && !(PendingPreferences && !PendingPreferenceDefaults)) {
				return;
			}
			if (!Preferences->apply_behaviour(aSettings)) {
				RepublishBehaviour = true;
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				publish_changes(local_context());
				return;
			}
			auto candidate            = *Preferences->accepted();
			candidate.Behaviour       = aSettings;
			PendingPreferences        = candidate;
			PendingPreferenceDefaults = false;
		}

		/// \brief Whether this host writes the profile's defaults record.
		///
		/// \details [[BPR-0783]]. That record names no window, so it carries no
		/// Home view; a window bound to it still shows one, but nothing durable
		/// keeps it.
		[[nodiscard]] auto profile_defaults_scoped() const -> bool
		{
			return Settings.profile_scoped() && Settings.window().value() == 0;
		}

		/// \brief The Home view this window is showing.
		///
		/// \details [[BPR-0783]]. The held view until this window's own record
		/// arrives, because until then the host has no view of its own and the
		/// declared default is not what the reviewer is looking at.
		[[nodiscard]] auto current_home() const noexcept -> const home_view_state&
		{
			return HeldHome ? *HeldHome : Layout.Home;
		}

		void resize_summary_now(std::optional<std::uint32_t> aFraction)
		{
			if (Closing || profile_defaults_scoped()) {
				return;
			}
			if (aFraction && (*aFraction == 0 || *aFraction >= 10000)) {
				PublishedLayout.reset();
				publish_changes(local_context());
				return;
			}
			const auto current = HeldSummary ? *HeldSummary : Layout.SummaryFraction;
			if ((LayoutLoaded || HeldSummary.has_value()) && current == aFraction) {
				return;
			}
			if (!LayoutLoaded && !LayoutLoadFailed) {
				HeldSummary.emplace(aFraction);
				return;
			}
			Layout.SummaryFraction = aFraction;
			if (LayoutLoaded) {
				submit_settings_store();
			} else {
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
			}
			publish_changes(local_context());
		}

		/// [[BPR-1334]]. The same terms as the summary: this window's explicit
		/// Line details height, or Reset, held until its record loads.
		void resize_line_details_now(std::optional<std::uint32_t> aHeight)
		{
			if (Closing || profile_defaults_scoped()) {
				return;
			}
			if (!valid_line_details_height(aHeight)) {
				PublishedLayout.reset();
				publish_changes(local_context());
				return;
			}
			const auto current = HeldLineDetails ? *HeldLineDetails : Layout.LineDetailsHeight;
			if ((LayoutLoaded || HeldLineDetails.has_value()) && current == aHeight) {
				return;
			}
			if (!LayoutLoaded && !LayoutLoadFailed) {
				HeldLineDetails.emplace(aHeight);
				return;
			}
			Layout.LineDetailsHeight = aHeight;
			if (LayoutLoaded) {
				submit_settings_store();
			} else {
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
			}
			publish_changes(local_context());
		}

		void observe_geometry_now(const workbench_window_geometry& aGeometry)
		{
			if (Closing || !LayoutLoaded || profile_defaults_scoped() || !valid(aGeometry)
				|| aGeometry.Width == 0 || Layout.Geometry == aGeometry) {
				return;
			}
			Layout.Geometry = aGeometry;
			submit_settings_store();
		}

		/// \brief [[BPR-0783]]. Accepts one window's Home view.
		///
		/// \details Presentation arrangement the host holds, validates and
		/// persists beside the window's layout; nothing here interprets it. A
		/// view outside its vocabularies is refused and the held one republished
		/// so Home reverts to it. A view that arrives before this window's record
		/// has loaded is held rather than applied: persisting it then would write
		/// the default drawers and widths this window never chose over the ones
		/// the stored record still holds.
		void apply_home_view_now(const home_view_state& aView)
		{
			if (Closing) {
				return;
			}
			if (!valid(aView)) {
				RepublishHome = true;
				publish_changes(local_context());
				return;
			}
			// An unchanged view publishes nothing, which is also what keeps a
			// window that merely repeats the view it is already showing from
			// writing that view over a stored one it has not read yet.
			if (aView == current_home()) {
				return;
			}
			if (!LayoutLoaded) {
				if (!LayoutLoadFailed) {
					HeldHome = aView;
					return;
				}
				// [[BPR-0783]]. The record never arrived, so there is nothing to
				// merge into and nothing this view could be written over. The
				// arrangement still applies to the window in front of the
				// reviewer, and the reason it will not be kept is said rather
				// than swallowed, as an unavailable behaviour write says it.
				Layout.Home = aView;
				publish_changes(local_context());
				publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				return;
			}
			Layout.Home = aView;
			if (!profile_defaults_scoped()) {
				submit_settings_store();
			}
			publish_changes(local_context());
		}

		/// \brief [[BPR-1130]]. Keeps the reviewer's fold with this window.
		///
		/// \details On the terms Home's view is kept: only in the window's own
		/// record, never in the profile's defaults (every new window starts
		/// folded), and only once that record has loaded. A fold chosen before
		/// then is held and merged into the record when it arrives, so the
		/// drawers and widths the stored record owns are not written over with
		/// the defaults this window has not read yet.
		void keep_updates_line()
		{
			if (profile_defaults_scoped()) {
				return;
			}
			if (!LayoutLoaded) {
				UpdatesLineHeld = !LayoutLoadFailed;
				return;
			}
			Layout.UpdatesLine = State.UpdatesLine;
			submit_settings_store();
		}

		/// \brief [[BPR-1130]]. Restores this window's own fold from the
		/// record just loaded, or merges a fold chosen before it arrived.
		void restore_updates_line()
		{
			if (std::exchange(UpdatesLineHeld, false)) {
				if (Layout.UpdatesLine != State.UpdatesLine) {
					Layout.UpdatesLine = State.UpdatesLine;
					submit_settings_store();
				}
				return;
			}
			static_cast<void>(core::transition(
				State, core::set_workbench_updates_line_intent{ .State = Layout.UpdatesLine }));
		}

		/// \brief Adopts a view applied before this window's record arrived.
		///
		/// \details [[BPR-0783]]. Merged into the record just loaded rather than
		/// written on its own, so the drawers and widths that record still owns
		/// reach the store unchanged even when no review has been opened yet.
		void merge_held_home()
		{
			const auto held = std::exchange(HeldHome, std::nullopt);
			if (!held || *held == Layout.Home) {
				return;
			}
			Layout.Home = *held;
			if (!profile_defaults_scoped()) {
				submit_settings_store();
			}
		}

		void accept_preferences(const workbench_profile_preferences& aPreferences)
		{
			if (Closing) {
				return;
			}
			Layout.Behaviour = aPreferences.Behaviour;
			if (PendingPreferences && !Preferences->is_busy()) {
				if (PendingPreferenceDefaults ? PendingPreferences->NewWindowDefaults == aPreferences.NewWindowDefaults : PendingPreferences->Behaviour == aPreferences.Behaviour) {
					++PersistedLayouts;
				} else {
					RepublishBehaviour = true;
					publish_failure(local_context(), workbench_host_failure::settings_unavailable);
				}
				PendingPreferences.reset();
			}
			publish_changes(local_context());
		}
		void cancel_now()
		{
			if (Closing) {
				return;
			}
			PendingLayout.reset();
			retire_all(workbench_terminal_rejection::cancelled);
			Generation = core::request_generation{ Generation.value() + 1U };
			if (OwnedPreferences) {
				OwnedPreferences->cancel();
			}
			PendingPreferences.reset();
		}

		void shutdown_orderly_now()
		{
			if (Closing) {
				return;
			}
			Closing        = true;
			OrderlyClosing = true;
			Preferences->unsubscribe(PreferenceSubscription);
			if (OwnedPreferences) {
				OwnedPreferences->shutdown();
			}
			retire(workbench_request_domain::observation, workbench_terminal_rejection::cancelled);
			// [[BPR-0783]]. A view applied before this window's record arrived
			// drains with that record rather than being dropped: the load it must
			// merge into is left to finish, so the last Home the reviewer chose is
			// stored beside the loaded layout instead of over it.
			if (!HeldHome && !UpdatesLineHeld && !HeldSummary && !HeldLineDetails) {
				retire(workbench_request_domain::settings_load, workbench_terminal_rejection::cancelled);
			}
			retire(workbench_request_domain::selected_diff, workbench_terminal_rejection::cancelled);
		}

		void shutdown_now()
		{
			if (Closing && !OrderlyClosing) {
				return;
			}
			Closing        = true;
			OrderlyClosing = false;
			PendingLayout.reset();
			Preferences->unsubscribe(PreferenceSubscription);
			if (OwnedPreferences) {
				OwnedPreferences->shutdown();
			}
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
			if (aObservation.Actionable.size() > Limits.ActionableFiles
				|| aObservation.Withheld.size() > Limits.ActionableFiles - aObservation.Actionable.size()) {
				publish_failure(aObservation.Context,
								workbench_host_failure::observation_limit);
				return;
			}
			auto counterparts  = index_counterparts(aObservation);
			Counterparts       = std::move(counterparts);
			CatalogueAvailable = true;
			WorkStatistics.AcceptedObservationRows += aObservation.Actionable.size();
			Files = std::move(aObservation.Actionable);
			Withheld.clear();
			for (auto& withheld : aObservation.Withheld) {
				const auto file = withheld.File;
				Withheld.emplace(file, std::move(withheld));
			}
			// [[BPR-0972]]. A restored file is placed by catalogue position, so
			// the order must be strictly numbered for that to be exact.
			Numbered = numbered_catalogue();
			rebuild_file_index();
			FilesDirty = true;
			if (Selected.has_value()) {
				const auto position = file_position(*Selected);
				if (position) {
					SelectedFile = Files[*position];
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

		[[nodiscard]] auto numbered_catalogue() const -> bool
		{
			if (std::ranges::adjacent_find(Files, std::greater_equal<>{ }, &workbench_file_observation_entry::Ordinal) != Files.end()) {
				return false;
			}
			auto       ordinals   = std::set<std::size_t>{ };
			auto       identities = std::set<core::workbench_file_id>{ };
			const auto accept     = [&](const workbench_file_observation_entry& aEntry) {
				return aEntry.Ordinal.has_value() && ordinals.insert(*aEntry.Ordinal).second && identities.insert(aEntry.File).second;
			};
			return std::ranges::all_of(Files, accept)
				&& std::ranges::all_of(Withheld, [&](const auto& aEntry) { return accept(aEntry.second); });
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
			const auto exactEndpoint = [&aObservation](
										   const std::optional<core::git_blob_oid>&               aBlob,
										   const std::optional<std::filesystem::path>&            aPath,
										   const std::optional<workbench_selected_diff_endpoint>& aEndpoint,
										   const core::diff_side                                  aSide) {
				return aBlob.has_value() == aPath.has_value()
					&& aBlob.has_value() == aEndpoint.has_value()
					&& (!aBlob.has_value()
						|| valid(*aEndpoint, aObservation.Request, aSide, *aBlob, *aPath));
			};
			if ((!aObservation.LeftBlob && !aObservation.RightBlob)
				|| !exactEndpoint(
					aObservation.LeftBlob,
					aObservation.LeftPath,
					aObservation.LeftEndpoint,
					core::diff_side::left)
				|| !exactEndpoint(
					aObservation.RightBlob,
					aObservation.RightPath,
					aObservation.RightEndpoint,
					core::diff_side::right)) {
				aObservation.Status = workbench_selected_diff_status::failure;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			const auto key = selected_diff_key(aObservation);
			if (!key.has_value()) {
				aObservation.Status = workbench_selected_diff_status::failure;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			auto publication = workbench_selected_diff_publication{
				.Request       = aObservation.Request,
				.Key           = *key,
				.LeftPath      = std::move(aObservation.LeftPath),
				.RightPath     = std::move(aObservation.RightPath),
				.LeftEndpoint  = std::move(aObservation.LeftEndpoint),
				.RightEndpoint = std::move(aObservation.RightEndpoint),
			};
			if (!core::valid(publication.Key)) {
				aObservation.Status = workbench_selected_diff_status::failure;
				deliver_projection([this, &aObservation] {
					SelectedDiffProjections->fail(aObservation);
				});
				return;
			}
			AcceptedSelectedDiff = aObservation.Request;
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
					   })
				&& std::ranges::all_of(aObservation.Withheld, [](const workbench_file_observation_entry& aFile) { return valid(aFile); });
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
				LayoutLoadFailed = true;
				// [[BPR-0783]]. A view held for this record now has nowhere to
				// go. It still belongs to the window, and the failure is
				// reported on the same terms as a failed write, so an orderly
				// close cannot swallow the last arrangement the reviewer chose.
				const auto held = HeldHome.has_value() || HeldSummary.has_value() || HeldLineDetails.has_value();
				if (held) {
					if (HeldHome) {
						Layout.Home = *std::exchange(HeldHome, std::nullopt);
					}
					if (HeldSummary) {
						Layout.SummaryFraction = *std::exchange(HeldSummary, std::nullopt);
					}
					if (HeldLineDetails) {
						Layout.LineDetailsHeight = *std::exchange(HeldLineDetails, std::nullopt);
					}
					publish_changes(aContext);
				}
				publish_failure(aContext, workbench_host_failure::settings_unavailable, held);
				return;
			}
			if (PreferenceSubscription != 0 && OwnedPreferences && (!Settings.profile_scoped() || Settings.window().value() == 0)) {
				// [[BPR-0783]]. The defaults the preferences host holds carry no
				// Home, for the reason `submit_settings_store` gives.
				auto defaults        = window_layout(*aResult);
				defaults.Home        = { };
				defaults.Geometry    = { };
				defaults.UpdatesLine = core::workbench_updates_line::folded;
				defaults.SummaryFraction.reset();
				defaults.LineDetailsHeight.reset();
				Preferences->adopt_loaded({ aResult->Behaviour, defaults, aResult->WritingHooks, aResult->ClaudeWrapper, aResult->ClaudeModel });
			}
			Layout = *aResult;
			if (profile_defaults_scoped()) {
				Layout.Home        = { };
				Layout.Geometry    = { };
				Layout.UpdatesLine = core::workbench_updates_line::folded;
				Layout.SummaryFraction.reset();
				Layout.LineDetailsHeight.reset();
			}
			if (PreferenceSubscription != 0 && Preferences->accepted()) {
				Layout.Behaviour = Preferences->accepted()->Behaviour;
			}
			Layout.FilesWidth         = clamped_width(Layout.FilesWidth, Limits);
			Layout.ConversationsWidth = clamped_width(Layout.ConversationsWidth, Limits);
			Layout.RepositoriesWidth  = clamped_width(Layout.RepositoriesWidth, Limits);
			RestorePending            = true;
			apply_restored_drawers();
			// [[BPR-0783]]. After the drawers, so that a view held for this
			// record is merged into the layout just loaded rather than into
			// whatever state the window happened to be in before it arrived.
			LayoutLoaded = true;
			merge_held_home();
			const auto heldGeometry = HeldSummary.has_value() || HeldLineDetails.has_value();
			if (HeldSummary) {
				Layout.SummaryFraction = *std::exchange(HeldSummary, std::nullopt);
			}
			if (HeldLineDetails) {
				Layout.LineDetailsHeight = *std::exchange(HeldLineDetails, std::nullopt);
			}
			if (heldGeometry) {
				submit_settings_store();
			}
			restore_updates_line();
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
								workbench_host_failure::settings_unavailable, true);
				return;
			}
			++PersistedLayouts;
		}

		/// \brief Applies the restored surface state once the workbench is active.
		///
		/// \details A drawer or numbering gesture is only legal while a workbench
		/// is published, so a layout restored before the first observation waits
		/// here rather than being decided outside the core. Restoring never writes
		/// the layout back.
		void apply_restored_drawers()
		{
			if (!RestorePending || !State.Navigation.has_value()) {
				return;
			}
			RestorePending = false;
			set_drawer(core::workbench_drawer::files, Layout.Files);
			set_drawer(core::workbench_drawer::conversations, Layout.Conversations);
			set_drawer(core::workbench_drawer::repositories, Layout.Repositories);
			set_line_numbers(Layout.LineNumbers);
			set_one_pane(Layout.OnePane);
		}

		void set_line_numbers(const core::workbench_line_numbers aState)
		{
			const auto output = core::transition(
				State,
				core::set_workbench_line_numbers_intent{ .State = aState });
			LastReason = output.Reason;
		}

		void set_one_pane(const core::workbench_one_pane aState)
		{
			const auto output = core::transition(
				State,
				core::set_workbench_one_pane_intent{ .State = aState });
			LastReason = output.Reason;
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
				.Repositories  = State.RepositoriesDrawer,
			};
			if (PublishedDrawers != drawers) {
				projection.Drawers = drawers;
			}
			const auto restore = core::project_workbench_drawer_restore(State);
			if (PublishedDrawerRestore != restore) {
				projection.DrawerRestore = restore;
			}
			const auto lineNumbers = core::workbench_line_numbers_delta{
				.State = State.LineNumbers,
			};
			const auto onePane = core::workbench_one_pane_delta{
				.State = State.OnePane,
			};
			if (PublishedOnePane != onePane) {
				projection.OnePane = onePane;
			}
			if (PublishedLineNumbers != lineNumbers) {
				projection.LineNumbers = lineNumbers;
			}
			const auto updatesLine = core::workbench_updates_line_delta{
				.State = State.UpdatesLine,
			};
			if (PublishedUpdatesLine != updatesLine) {
				projection.UpdatesLine = updatesLine;
			}
			const auto commands = core::project_workbench_commands(State);
			if (PublishedCommands != commands) {
				projection.Commands = commands;
			}
			const auto layout = workbench_layout_delta{
				.FilesWidth         = Layout.FilesWidth,
				.ConversationsWidth = Layout.ConversationsWidth,
				.RepositoriesWidth  = Layout.RepositoriesWidth,
				.SummaryFraction    = (LayoutLoaded || LayoutLoadFailed) ? std::optional{ Layout.SummaryFraction } : std::nullopt,
				.LineDetailsHeight  = (LayoutLoaded || LayoutLoadFailed) ? std::optional{ Layout.LineDetailsHeight } : std::nullopt,
			};
			if (PublishedLayout != layout) {
				projection.Layout = layout;
			}
			if (LayoutLoaded && !PublishedGeometry) {
				projection.Geometry = valid(Layout.Geometry) ? Layout.Geometry : workbench_window_geometry{ };
				PublishedGeometry   = true;
			}
			if (PublishedBehaviour != std::optional{ Layout.Behaviour } || RepublishBehaviour) {
				projection.Behaviour = Layout.Behaviour;
			}
			// [[BPR-0783]]. Withheld until this window's record has loaded.
			// Before that the host holds no view of its own, and publishing the
			// declared default would revert an arrangement the reviewer made
			// while the load was still in flight.
			// [[BPR-0783]]. A view adopted with no record to merge into is
			// published as well. That path only exists because the load
			// failed, so `LayoutLoaded` never becomes true, and presentation
			// would otherwise never learn the view the host accepted.
			if (RepublishHome
				|| ((LayoutLoaded || LayoutLoadFailed)
					&& PublishedHome != std::optional{ current_home() })) {
				projection.HomeView = current_home();
			}
			if (PendingFilesDelta.has_value() && !FilesDirty) {
				projection.FilesDelta = std::exchange(PendingFilesDelta, std::nullopt);
			}
			PendingFilesDelta.reset();
			if (FilesDirty) {
				auto files = std::vector<workbench_file_projection>{ };
				if (files.capacity() < Files.size()) {
					++WorkStatistics.FileProjectionCapacityGrowths;
				}
				WorkStatistics.FileProjectionRows += Files.size();
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
				PublishedFiles = std::move(files);
				FilesDirty     = false;
			}
			projection.Refusal = std::exchange(PendingRefusal, std::nullopt);
			if (empty(projection)) {
				return;
			}
			PublishedSelection     = selection;
			PublishedDrawers       = drawers;
			PublishedDrawerRestore = restore;
			PublishedLineNumbers   = lineNumbers;
			PublishedUpdatesLine   = updatesLine;
			PublishedOnePane       = onePane;
			PublishedCommands      = commands;
			PublishedLayout        = layout;
			PublishedBehaviour     = Layout.Behaviour;
			RepublishBehaviour     = false;
			if (projection.HomeView.has_value()) {
				PublishedHome = projection.HomeView;
			}
			RepublishHome = false;
			deliver_projection(
				[this, &projection] { Projections.publish(projection); });
		}

		void publish_failure(const workbench_request_context& aContext,
							 const workbench_host_failure aFailure, const bool aDuringOrderly = false)
		{
			if (Closing && !(OrderlyClosing && aDuringOrderly)) {
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

		core::workbench_interaction_state            State;
		workbench_observation_port&                  Observations;
		workbench_settings_port&                     Settings;
		std::unique_ptr<workbench_preferences_host>  OwnedPreferences;
		workbench_preferences_host*                  Preferences            = nullptr;
		std::uint64_t                                PreferenceSubscription = 0;
		std::optional<workbench_profile_preferences> PendingPreferences;
		bool                                         PendingPreferenceDefaults = false;
		effect_executor&                             Effects;
		decision_executor&                           DecisionLane;
		workbench_projection_port&                   Projections;
		workbench_selected_diff_port*                SelectedDiffs;
		workbench_selected_diff_projection_port*     SelectedDiffProjections;
		core::application_session_id                 Session;
		workbench_host_limits                        Limits;
		std::shared_ptr<completion_gate>             Gate;

		mutable workbench_work_statistics WorkStatistics;
		/// Counterpart lookup includes both visible and withheld catalogue entries.
		header_source_index Counterparts;
		bool                CatalogueAvailable = false;
		/// [[BPR-0972]]. Catalogue files withheld from the order, by file, and
		/// whether the order's catalogue positions are strictly increasing.
		std::map<core::workbench_file_id, workbench_file_observation_entry> Withheld;
		bool                                                                Numbered = false;
		std::optional<workbench_files_delta>                                PendingFilesDelta;
		std::vector<std::pair<core::workbench_file_id, std::size_t>>        FileIndex;
		bool                                                                FilesDirty = true;
		std::vector<core::workbench_file_id>                                Actionable;
		std::vector<workbench_file_observation_entry>                       Files;
		std::optional<core::workbench_file_id>                              Selected;
		std::optional<workbench_file_observation_entry>                     SelectedFile;
		std::optional<workbench_pull_request_authority>                     Authority;
		std::optional<workbench_selected_diff_request>                      AcceptedSelectedDiff;
		workbench_layout_settings                                           Layout;
		std::optional<workbench_layout_settings>                            PendingLayout;
		/// [[BPR-0783]]. A Home view applied before this window's own record
		/// loaded, waiting to be merged into it.
		std::optional<home_view_state>              HeldHome;
		std::optional<std::optional<std::uint32_t>> HeldSummary;
		std::optional<std::optional<std::uint32_t>> HeldLineDetails;
		/// [[BPR-1130]]. A fold chosen before this window's record loaded,
		/// merged into it when it arrives.
		bool UpdatesLineHeld   = false;
		bool PublishedGeometry = false;
		/// [[BPR-0783]]. Whether that record has arrived. Until it has, a
		/// view is held rather than applied, and none is published.
		bool LayoutLoaded = false;
		/// [[BPR-0783]]. Set when this window's record answered with a failure,
		/// so a later Home gesture is applied and reported instead of waiting
		/// for a merge that can never happen.
		bool LayoutLoadFailed = false;

		std::vector<outstanding_request>       Outstanding;
		std::deque<deferred_workbench_ingress> DeferredIngress;
		std::uint64_t                          NextRequest = 1U;
		core::request_generation               Generation{ 1U };

		std::optional<core::workbench_selection_delta>        PublishedSelection;
		std::optional<core::workbench_drawer_delta>           PublishedDrawers;
		std::optional<core::workbench_drawer_restore_delta>   PublishedDrawerRestore;
		std::optional<core::workbench_line_numbers_delta>     PublishedLineNumbers;
		std::optional<core::workbench_updates_line_delta>     PublishedUpdatesLine;
		std::optional<core::workbench_one_pane_delta>         PublishedOnePane;
		std::optional<core::workbench_command_delta>          PublishedCommands;
		std::optional<workbench_layout_delta>                 PublishedLayout;
		std::optional<review_behaviour_settings>              PublishedBehaviour;
		bool                                                  RepublishBehaviour = false;
		std::optional<home_view_state>                        PublishedHome;
		bool                                                  RepublishHome = false;
		std::optional<std::vector<workbench_file_projection>> PublishedFiles;

		std::size_t                                RejectedTerminals = 0U;
		std::size_t                                PersistedLayouts  = 0U;
		workbench_terminal_rejection               LastRejection     = workbench_terminal_rejection::none;
		core::workbench_reason_code                LastReason        = core::workbench_reason_code::none;
		std::optional<core::workbench_reason_code> PendingRefusal;
		bool                                       RestorePending           = false;
		bool                                       Closing                  = false;
		bool                                       OrderlyClosing           = false;
		bool                                       TurnActive               = false;
		bool                                       DeferredIngressScheduled = false;
		bool                                       ProjectionDeliveryFailed = false;
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
		workbench_selected_diff_projection_port* aSelectedDiffProjections,
		workbench_preferences_host* aPreferences, const core::bpr_profile_id aProfile)
		: Implementation{ std::make_unique<implementation>(aInitialState,
														   aObservations,
														   aSettings,
														   aEffects,
														   aDecisionLane,
														   aProjections,
														   aSession,
														   aLimits,
														   aSelectedDiffs,
														   aSelectedDiffProjections, aPreferences, aProfile) }
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

	void workbench_application_host::retain_actionability(
		workbench_pull_request_authority aAuthority,
		const core::workbench_file_id    aFile,
		const bool                       aActionable)
	{
		Implementation->enqueue(deferred_workbench_actionability_retain{
			.Authority  = std::move(aAuthority),
			.File       = aFile,
			.Actionable = aActionable,
		});
	}

	void workbench_application_host::observe_change_navigation(
		workbench_selected_diff_request              aRequest,
		std::optional<core::workbench_change_cursor> aCursor)
	{
		Implementation->enqueue(deferred_workbench_change_navigation{
			.Request = std::move(aRequest),
			.Cursor  = aCursor,
		});
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

	void workbench_application_host::apply_behaviour(review_behaviour_settings aSettings)
	{
		// [[BPR-1088]]. A reviewer's record carries no load observation.
		Implementation->enqueue(deferred_workbench_behaviour{ .Settings = edited(std::move(aSettings)) });
	}

	void workbench_application_host::apply_home_view(home_view_state aState)
	{
		Implementation->enqueue(deferred_workbench_home_view{ .View = aState });
	}
	void workbench_application_host::observe_geometry(workbench_window_geometry aGeometry)
	{
		Implementation->enqueue(deferred_workbench_geometry{ .Geometry = aGeometry });
	}

	void workbench_application_host::resize_summary(std::optional<std::uint32_t> aFraction)
	{
		Implementation->enqueue(deferred_workbench_summary_resize{ .Fraction = aFraction });
	}

	void workbench_application_host::resize_line_details(std::optional<std::uint32_t> aHeight)
	{
		Implementation->enqueue(deferred_workbench_line_details_resize{ .Height = aHeight });
	}

	void workbench_application_host::cancel()
	{
		Implementation->enqueue(deferred_workbench_cancel{ });
	}

	void workbench_application_host::shutdown()
	{
		Implementation->enqueue(deferred_workbench_shutdown{ });
	}

	auto workbench_application_host::work_statistics() const noexcept -> workbench_work_statistics
	{
		return Implementation->WorkStatistics;
	}

	auto workbench_application_host::state() const noexcept
		-> const core::workbench_interaction_state&
	{
		return Implementation->State;
	}

	void workbench_application_host::shutdown_orderly() { Implementation->enqueue(deferred_workbench_orderly_shutdown{ }); }

	auto workbench_application_host::layout() const noexcept
		-> const workbench_layout_settings&
	{
		return Implementation->Layout;
	}

	auto workbench_application_host::file_observation(const core::workbench_file_id aFile) const -> std::optional<workbench_file_observation_entry>
	{
		const auto* entry = Implementation->catalogue_file(aFile);
		return entry ? std::optional{ *entry } : std::nullopt;
	}

	auto workbench_application_host::header_source_target() const -> core::header_source_resolution
	{
		return Implementation->header_source_target();
	}

	auto workbench_application_host::contains_file(const core::workbench_file_id aFile) const noexcept -> bool
	{
		return Implementation->catalogue_file(aFile) != nullptr;
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

	auto workbench_application_host::preparation_window(const std::size_t aWindow) const
		-> std::vector<workbench_selected_diff_request>
	{
		return Implementation->preparation_window(aWindow);
	}

	auto selected_diff_key(const workbench_selected_diff_observation& aObservation)
		-> std::optional<core::file_comparison_key>
	{
		if (aObservation.Status != workbench_selected_diff_status::available
			|| (!aObservation.LeftBlob && !aObservation.RightBlob)) {
			return std::nullopt;
		}
		const auto& diff = aObservation.Request.Diff;
		auto        key  = core::file_comparison_key{
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
		};
		return core::valid(key) ? std::optional{ std::move(key) } : std::nullopt;
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
		return Implementation->Closing && Implementation->Outstanding.empty() && !Implementation->PendingLayout
			&& (!Implementation->OwnedPreferences || Implementation->OwnedPreferences->is_quiescent())
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
