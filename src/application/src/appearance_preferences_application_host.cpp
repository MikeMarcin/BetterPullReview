#include "bpr/application/appearance_preferences_application_host.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		enum class appearance_request_domain : std::uint8_t {
			settings_load,
			settings_store,
			theme_import,
			theme_export,
		};

		struct theme_import_attempt
		{
			std::optional<core::appearance_palette>    Palette;
			std::vector<core::theme_import_rejection>  ThemeRejections;
			std::vector<appearance_contrast_rejection> ContrastRejections;
		};

		struct staged_settings_load
		{
			appearance_request_context                                     Context;
			appearance_settings_result<std::optional<appearance_settings>> Result;
		};

		struct staged_settings_store
		{
			appearance_request_context       Context;
			appearance_settings_result<void> Result;
		};

		struct staged_theme_import
		{
			appearance_request_context                  Context;
			theme_document_result<theme_import_attempt> Result;
		};

		struct deferred_appearance_restore
		{
		};

		struct deferred_appearance_select
		{
			core::appearance_mode Mode = core::appearance_mode::system;
		};

		struct deferred_appearance_observe
		{
			core::appearance_observation Observation;
			bool                         Initialize = false;
		};

		struct deferred_appearance_palette
		{
			core::appearance_palette Palette{ };
			core::appearance_variant Variant{ };
		};

		/// \brief One scoped reset, carrying the variant a palette reset acts on.
		///
		/// \details Distinct from `reset_appearance_intent` because the intent is
		/// the vocabulary a settings surface speaks and carries no variant: the
		/// resolved theme is the composition's to supply, never presentation's to
		/// decide.
		struct deferred_appearance_reset
		{
			appearance_reset_scope   Scope{ };
			core::appearance_variant Variant{ };
		};

		struct deferred_appearance_typography
		{
			core::typography_settings Typography;
		};

		struct deferred_appearance_zoom
		{
			core::code_zoom_step Step = core::code_zoom_step::actual_size;
		};

		/// [[BPR-1714]]. One whole known-namespace list.
		struct deferred_known_namespaces
		{
			core::known_namespaces Namespaces;
		};

		/// [[BPR-1807]]. What people's avatars show.
		struct deferred_avatar_style
		{
			core::identity_avatar_style Style = core::identity_avatar_style::github;
		};

		struct deferred_appearance_theme_import
		{
			std::string Path;
		};

		struct deferred_appearance_theme_export
		{
			std::string              Path;
			core::appearance_palette Palette;
			core::appearance_variant Variant = core::appearance_variant::light;
		};

		struct staged_theme_export
		{
			appearance_request_context  Context;
			theme_document_result<void> Result;
		};

		struct deferred_appearance_cancel
		{
		};

		struct deferred_appearance_shutdown
		{
		};

		using deferred_appearance_ingress = std::variant<core::dif_preferences, core::motion_observation, cancel_appearance_palette_preview_intent, deferred_appearance_reset, deferred_appearance_theme_export, staged_theme_export, deferred_appearance_restore,
														 deferred_appearance_select,
														 deferred_appearance_observe,
														 deferred_appearance_palette,
														 deferred_appearance_typography,
														 deferred_appearance_zoom,
														 deferred_known_namespaces,
														 deferred_avatar_style,
														 deferred_appearance_theme_import,
														 deferred_appearance_cancel,
														 deferred_appearance_shutdown,
														 staged_settings_load,
														 staged_settings_store,
														 staged_theme_import>;

		struct queued_preferences_ingress
		{
			deferred_appearance_ingress Value;
			appearance_consumer_id      Consumer;
		};

		constexpr auto all_appearance_variants = std::array{
			core::appearance_variant::light,
			core::appearance_variant::dark,
			core::appearance_variant::high_contrast_light,
			core::appearance_variant::high_contrast_dark,
		};

		[[nodiscard]] auto contrast_rejections(
			const core::appearance_palette& aPalette)
			-> std::vector<appearance_contrast_rejection>
		{
			auto rejections = std::vector<appearance_contrast_rejection>{ };
			for (const auto variant : all_appearance_variants) {
				for (auto finding : core::failing_contrast(aPalette, variant)) {
					rejections.push_back(appearance_contrast_rejection{
						.Variant = variant,
						.Finding = std::move(finding),
					});
				}
			}
			return rejections;
		}

		/// \brief Checks that every built-in palette assigns every role.
		///
		/// \details [[BPR-1479]]. These fill the roles an older theme omits, so a
		/// built-in that left one unassigned would hand the import the very hole
		/// it is meant to fill.
		[[nodiscard]] auto validated_builtins(appearance_builtin_palettes aBuiltins)
			-> appearance_builtin_palettes
		{
			for (const auto& palette : aBuiltins) {
				if (std::ranges::find(palette, core::unassigned_appearance_color) != palette.end()) {
					throw std::invalid_argument{ "Invalid built-in appearance palettes" };
				}
			}
			return aBuiltins;
		}

		[[nodiscard]] auto validated_limits(const appearance_host_limits aLimits)
			-> appearance_host_limits
		{
			if (!valid(aLimits)) {
				throw std::invalid_argument{ "Invalid appearance host limits" };
			}
			return aLimits;
		}

		/// \brief Checks the composed base appearance against the core's own rules.
		///
		/// \details A host that started from an illegal base would have to either
		/// publish it or silently correct it, and both would defeat the rule that
		/// only the core decides legality. Refusing at construction keeps that rule
		/// with one check instead of one per operation.
		[[nodiscard]] auto validated_settings(appearance_settings aSettings)
			-> appearance_settings
		{
			if (!core::valid(aSettings.Dif)) {
				throw std::invalid_argument{ "Invalid Dif preferences" };
			}
			if (!known(aSettings.Mode)) {
				throw std::invalid_argument{ "Invalid appearance mode" };
			}
			if (!core::validate_typography(aSettings.Typography).empty()) {
				throw std::invalid_argument{ "Invalid appearance typography" };
			}
			if (!core::validate_known_namespaces(aSettings.Namespaces).empty()) {
				throw std::invalid_argument{ "Invalid known namespaces" };
			}
			if (!core::valid(aSettings.Avatars)) {
				throw std::invalid_argument{ "Invalid avatar style" };
			}
			return aSettings;
		}

	}
	struct appearance_preferences_application_host::implementation
	{
		struct consumer_state
		{
			appearance_consumer_id                      Id;
			std::shared_ptr<appearance_projection_port> Port;
			bool                                        Active                      = true;
			bool                                        PersistenceFailurePublished = false;
			core::appearance_observation                Observation;
			core::motion_observation                    MotionObservation = core::motion_observation::unknown;
			std::optional<core::dif_resolution>         PublishedDif;
			appearance_epoch                            Epoch;
			std::optional<core::appearance_resolution>  PublishedResolution;
			/// \brief The per-variant customization table this consumer has seen.
			std::optional<appearance_palette_customisations> PublishedPalettes;
			std::optional<core::typography_settings>         PublishedTypography;
			std::optional<core::known_namespaces>            PublishedNamespaces;
			std::optional<core::identity_avatar_style>       PublishedAvatars;
		};
		[[nodiscard]] auto find_consumer(appearance_consumer_id aId) const -> std::shared_ptr<consumer_state>
		{
			const auto found = std::ranges::find(Consumers, aId, [](const auto& aValue) { return aValue->Id; });
			return found == Consumers.end() ? nullptr : *found;
		}
		void publish_to_consumer(const appearance_projection& aProjection)
		{
			const auto consumer = find_consumer(CurrentConsumer);
			if (consumer && consumer->Active) {
				consumer->Port->publish(aProjection);
			}
		}
		void fail_consumer(const appearance_failure_publication& aFailure)
		{
			const auto consumer = find_consumer(CurrentConsumer);
			if (consumer && consumer->Active) {
				consumer->Port->fail(aFailure);
			}
		}
		struct completion_gate
		{
			std::atomic<implementation*> Host = nullptr;
		};

		struct outstanding_request
		{
			appearance_request_context    Context;
			appearance_consumer_id        Consumer;
			appearance_request_domain     Domain     = appearance_request_domain::settings_load;
			appearance_terminal_rejection Retirement = appearance_terminal_rejection::none;
			std::stop_source              Stop;
		};

		struct claimed_request
		{
			appearance_terminal_rejection Rejection = appearance_terminal_rejection::unknown_request;
		};

		implementation(appearance_settings                aInitialSettings,
					   const appearance_builtin_palettes& aBuiltins,
					   appearance_settings_port&          aSettings,
					   theme_document_port&               aThemeDocuments,
					   effect_executor&                   aEffects,
					   decision_executor&                 aDecisionLane,
					   const core::application_session_id aSession,
					   const appearance_host_limits       aLimits)
			: Settings{ validated_settings(std::move(aInitialSettings)) }
			, Defaults{ Settings }
			, Builtins{ validated_builtins(aBuiltins) }
			, SettingsPort{ aSettings }
			, ThemeDocuments{ aThemeDocuments }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
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

		// --- Ingress serialization ---------------------------------------------

		void enqueue(deferred_appearance_ingress aValue, appearance_consumer_id aConsumer = { })
		{
			auto aIngress = queued_preferences_ingress{ std::move(aValue), aConsumer };
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

		void process(queued_preferences_ingress aIngress)
		{
			CurrentConsumer = aIngress.Consumer;
			std::visit(
				[this](auto&& aTyped) {
					using ingress_type = std::decay_t<decltype(aTyped)>;
					if constexpr (!std::is_same_v<ingress_type, staged_settings_load>
								  && !std::is_same_v<ingress_type, staged_settings_store>
								  && !std::is_same_v<ingress_type, staged_theme_import>
								  && !std::is_same_v<ingress_type, staged_theme_export>
								  && !std::is_same_v<ingress_type, deferred_appearance_cancel>
								  && !std::is_same_v<ingress_type, deferred_appearance_shutdown>) {
						if (!find_consumer(CurrentConsumer)) {
							return;
						}
					}
					if constexpr (std::is_same_v<ingress_type, core::dif_preferences>) {
						if (!Closing) {
							if (!core::valid(aTyped)) {
								publish_failure(local_context(), appearance_host_failure::invalid_dif_preferences);
							} else {
								auto next = Settings;
								next.Dif  = aTyped;
								adopt(std::move(next));
							}
						}
					} else if constexpr (std::is_same_v<ingress_type, core::motion_observation>) {
						const auto consumer = find_consumer(CurrentConsumer);
						if (!Closing && consumer && consumer->MotionObservation != aTyped) {
							consumer->MotionObservation = aTyped;
							publish_consumer_changes(consumer, local_context(), { });
						}
					} else if constexpr (std::is_same_v<ingress_type, cancel_appearance_palette_preview_intent>) {
						retire(appearance_request_domain::theme_import, appearance_terminal_rejection::cancelled);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_reset>) {
						reset_now(aTyped.Scope, aTyped.Variant);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_theme_export>) {
						if (!Closing) {
							submit_theme_export(std::move(aTyped.Path), aTyped.Palette, aTyped.Variant);
						}
					} else if constexpr (std::is_same_v<ingress_type, staged_theme_export>) {
						finish_theme_export(aTyped.Context, aTyped.Result);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_restore>) {
						restore_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_select>) {
						select_now(aTyped.Mode);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_observe>) {
						if (aTyped.Initialize) {
							const auto consumer = find_consumer(CurrentConsumer);
							if (consumer) {
								publish_consumer_changes(consumer, local_context(), { });
							}
						} else {
							observe_now(aTyped.Observation);
						}
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_palette>) {
						apply_palette_now(aTyped.Palette, aTyped.Variant);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_typography>) {
						apply_typography_now(std::move(aTyped.Typography));
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_zoom>) {
						auto typography             = Settings.Typography;
						typography.Code.ZoomPercent = core::stepped_code_zoom(typography.Code, aTyped.Step);
						apply_typography_now(std::move(typography));
					} else if constexpr (std::is_same_v<ingress_type, deferred_known_namespaces>) {
						apply_known_namespaces_now(std::move(aTyped.Namespaces));
					} else if constexpr (std::is_same_v<ingress_type, deferred_avatar_style>) {
						apply_avatar_style_now(aTyped.Style);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_theme_import>) {
						import_theme_now(std::move(aTyped.Path));
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_cancel>) {
						cancel_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_shutdown>) {
						shutdown_now();
					} else if constexpr (std::is_same_v<ingress_type, staged_settings_load>) {
						finish_settings_load(aTyped.Context, std::move(aTyped.Result));
					} else if constexpr (std::is_same_v<ingress_type, staged_settings_store>) {
						finish_settings_store(aTyped.Context, aTyped.Result);
					} else if constexpr (std::is_same_v<ingress_type, staged_theme_import>) {
						finish_theme_import(aTyped.Context, std::move(aTyped.Result));
					}
				},
				std::move(aIngress.Value));
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
				const auto found    = std::ranges::find(host->Outstanding, aTerminal.Context.Request,
														[](const outstanding_request& aRecord) { return aRecord.Context.Request; });
				const auto consumer = found == host->Outstanding.end() ? appearance_consumer_id{ } : found->Consumer;
				host->enqueue(std::move(aTerminal), consumer);
			}
		}

		// --- Request identity ---------------------------------------------------

		[[nodiscard]] auto next_context() noexcept -> appearance_request_context
		{
			const auto request = core::request_id{ NextRequest };
			++NextRequest;
			return {
				.Request = request,
				.Session = Session,
			};
		}

		[[nodiscard]] auto local_context() const noexcept
			-> appearance_request_context
		{
			return { .Session = Session };
		}

		void retire(const appearance_request_domain     aDomain,
					const appearance_terminal_rejection aRejection) noexcept
		{
			for (auto& record : Outstanding) {
				if ((aDomain == appearance_request_domain::theme_import || aDomain == appearance_request_domain::theme_export)
					&& record.Consumer != CurrentConsumer) {
					continue;
				}
				if (record.Domain != aDomain
					|| record.Retirement != appearance_terminal_rejection::none) {
					continue;
				}
				record.Retirement = aRejection;
				record.Stop.request_stop();
			}
		}

		void retire_all(const appearance_terminal_rejection aRejection) noexcept
		{
			for (auto& record : Outstanding) {
				if (record.Retirement == appearance_terminal_rejection::none) {
					record.Retirement = aRejection;
				}
				record.Stop.request_stop();
			}
		}

		/// \brief Claims one terminal against its outstanding request record.
		///
		/// \details The record is erased on first delivery, so a duplicate finds
		/// nothing and is refused. A record retired by supersession or cancellation
		/// keeps its retirement reason and refuses the terminal it was still owed.
		/// Retirement is per domain; transient requests additionally belong to their
		/// initiating attachment.
		[[nodiscard]] auto claim(const appearance_request_context& aContext,
								 const appearance_request_domain   aDomain)
			-> claimed_request
		{
			const auto found = std::ranges::find_if(
				Outstanding, [&aContext](const outstanding_request& aRecord) {
					return aRecord.Context.Request == aContext.Request;
				});
			if (found == Outstanding.end()) {
				return { .Rejection = appearance_terminal_rejection::unknown_request };
			}
			auto claimed = claimed_request{
				.Rejection = appearance_terminal_rejection::none,
			};
			const auto domain     = found->Domain;
			const auto context    = found->Context;
			const auto retirement = found->Retirement;
			Outstanding.erase(found);
			if (domain != aDomain || context != aContext) {
				claimed.Rejection = appearance_terminal_rejection::inexact_terminal;
				return claimed;
			}
			if (retirement != appearance_terminal_rejection::none) {
				claimed.Rejection = retirement;
				return claimed;
			}
			if (Closing) {
				claimed.Rejection = appearance_terminal_rejection::cancelled;
			}
			return claimed;
		}

		void refuse(const appearance_terminal_rejection aRejection) noexcept
		{
			++RejectedTerminals;
			LastRejection       = aRejection;
			const auto consumer = find_consumer(CurrentConsumer);
			if (consumer && consumer->Active) {
				deliver_projection([&consumer, aRejection] { consumer->Port->rejected(aRejection); });
			}
		}

		// --- Effect submission --------------------------------------------------

		[[nodiscard]] auto reserve(const appearance_request_domain aDomain)
			-> outstanding_request*
		{
			if (Outstanding.size() >= Limits.OutstandingRequests) {
				return nullptr;
			}
			Outstanding.push_back(outstanding_request{
				.Context  = next_context(),
				.Consumer = CurrentConsumer,
				.Domain   = aDomain,
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

		void submit_settings_load()
		{
			auto* const record = reserve(appearance_request_domain::settings_load);
			if (record == nullptr) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			const auto  context = record->Context;
			const auto  stop    = record->Stop.get_token();
			const auto  gate    = std::weak_ptr<completion_gate>{ Gate };
			auto* const port    = &SettingsPort;
			auto* const lane    = &DecisionLane;
			try {
				Effects.post([gate, lane, port, context, stop] {
					auto staged = staged_settings_load{
						.Context = context,
						.Result  = perform_settings_load(*port, stop),
					};
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context, appearance_host_failure::settings_unavailable);
			}
		}

		[[nodiscard]] static auto perform_settings_load(
			appearance_settings_port& aPort, std::stop_token aStop)
			-> appearance_settings_result<std::optional<appearance_settings>>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ appearance_settings_error{
					.Failure = appearance_settings_failure::cancelled } };
			}
			try {
				return aPort.load(std::move(aStop));
			} catch (...) {
				return std::unexpected{ appearance_settings_error{
					.Failure = appearance_settings_failure::unavailable } };
			}
		}

		/// \brief Persists the durable appearance, coalescing with a pending write.
		void submit_settings_store()
		{
			retire(appearance_request_domain::settings_store,
				   appearance_terminal_rejection::superseded);
			auto* const record = reserve(appearance_request_domain::settings_store);
			if (record == nullptr) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			const auto  context  = record->Context;
			const auto  stop     = record->Stop.get_token();
			const auto  settings = Settings;
			const auto  gate     = std::weak_ptr<completion_gate>{ Gate };
			auto* const port     = &SettingsPort;
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
				publish_failure(context, appearance_host_failure::settings_unavailable);
			}
		}

		[[nodiscard]] static auto perform_settings_store(
			appearance_settings_port&  aPort,
			const appearance_settings& aSettings,
			std::stop_token            aStop) -> appearance_settings_result<void>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ appearance_settings_error{
					.Failure = appearance_settings_failure::cancelled } };
			}
			try {
				return aPort.store(aSettings, std::move(aStop));
			} catch (...) {
				return std::unexpected{ appearance_settings_error{
					.Failure = appearance_settings_failure::unavailable } };
			}
		}

		/// \brief Writes the palette as a theme whose base is the variant it customises.
		void submit_theme_export(std::string aPath, const core::appearance_palette& aPalette, const core::appearance_variant aVariant)
		{
			if (!known(aVariant)) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			retire(appearance_request_domain::theme_export, appearance_terminal_rejection::superseded);
			auto* const record = reserve(appearance_request_domain::theme_export);
			if (record == nullptr) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			const auto  context = record->Context;
			const auto  stop    = record->Stop.get_token();
			const auto  gate    = std::weak_ptr<completion_gate>{ Gate };
			auto* const port    = &ThemeDocuments;
			auto* const lane    = &DecisionLane;
			try {
				Effects.post([gate, lane, port, context, path = std::move(aPath), palette = aPalette, aVariant, stop] {
					auto staged = staged_theme_export{
						.Context = context,
						.Result  = perform_theme_export(*port, path, palette, aVariant, stop),
					};
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context, appearance_host_failure::theme_export_unavailable);
			}
		}

		/// \brief Writes one theme document; a stop or any throw yields the default `unavailable` error.
		[[nodiscard]] static auto perform_theme_export(
			theme_document_port&            aPort,
			const std::string&              aPath,
			const core::appearance_palette& aPalette,
			const core::appearance_variant  aBase,
			std::stop_token                 aStop) -> theme_document_result<void>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ theme_document_error{ } };
			}
			try {
				return aPort.write(aPath, core::export_theme(aPalette, aBase), std::move(aStop));
			} catch (...) {
				return std::unexpected{ theme_document_error{ } };
			}
		}

		void finish_theme_export(const appearance_request_context& aContext, theme_document_result<void> aResult)
		{
			const auto claimed = claim(aContext, appearance_request_domain::theme_export);
			if (claimed.Rejection != appearance_terminal_rejection::none) {
				refuse(claimed.Rejection);
				return;
			}
			if (!aResult) {
				publish_failure(aContext, appearance_host_failure::theme_export_unavailable);
				return;
			}
			const auto projection = appearance_projection{ .Context = aContext, .ThemeExported = true };
			deliver_projection([this, &projection] { publish_to_consumer(projection); });
		}

		/// \brief Resets one durable group, and for a palette exactly one variant.
		///
		/// \details RFC-BPR-0034: Reset all resets the theme on screen. The other
		/// three variants' customizations are not part of that gesture and survive
		/// it, which is the whole difference from the single-palette record this
		/// replaced — there, resetting was necessarily resetting the only one.
		void reset_now(
			const appearance_reset_scope   aScope,
			const core::appearance_variant aVariant)
		{
			if (Closing) {
				return;
			}
			auto next            = Settings;
			auto acknowledgement = std::optional<palette_adoption_publication>{ };
			switch (aScope) {
			case appearance_reset_scope::dif:              next.Dif = Defaults.Dif; break;
			case appearance_reset_scope::mode:             next.Mode = Defaults.Mode; break;
			case appearance_reset_scope::interface_font:   next.Typography.Ui = Defaults.Typography.Ui; break;
			case appearance_reset_scope::code_font:        next.Typography.Code = Defaults.Typography.Code; break;
			case appearance_reset_scope::known_namespaces: next.Namespaces = Defaults.Namespaces; break;
			case appearance_reset_scope::palette:
				if (!known(aVariant)) {
					publish_failure(local_context(), appearance_host_failure::host_failure);
					return;
				}
				retire(appearance_request_domain::theme_import, appearance_terminal_rejection::superseded);
				customisation_of(next, aVariant) = customisation_of(Defaults, aVariant);
				acknowledgement                  = palette_adoption_publication{
					.Palette = customisation_of(next, aVariant).Palette,
					.Variant = aVariant,
				};
				break;
			default: return;
			}
			adopt(std::move(next), acknowledgement);
		}

		void submit_theme_import(std::string aPath)
		{
			retire(appearance_request_domain::theme_import,
				   appearance_terminal_rejection::superseded);
			auto* const record = reserve(appearance_request_domain::theme_import);
			if (record == nullptr) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			const auto  context = record->Context;
			const auto  stop    = record->Stop.get_token();
			const auto  gate    = std::weak_ptr<completion_gate>{ Gate };
			auto* const port    = &ThemeDocuments;
			auto* const lane    = &DecisionLane;
			try {
				Effects.post([gate, lane, port, context, path = std::move(aPath), builtins = Builtins, stop] {
					auto staged = staged_theme_import{
						.Context = context,
						.Result  = perform_theme_import(*port, path, builtins, stop),
					};
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context,
								appearance_host_failure::theme_document_unavailable);
			}
		}

		[[nodiscard]] static auto perform_theme_import(
			theme_document_port&               aPort,
			const std::string&                 aPath,
			const appearance_builtin_palettes& aBuiltins,
			std::stop_token                    aStop)
			-> theme_document_result<theme_import_attempt>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ theme_document_error{
					.Failure = theme_document_failure::cancelled } };
			}
			try {
				auto document = aPort.read(
					aPath, core::theme_document_byte_limit + 1U, aStop);
				if (!document.has_value()) {
					return std::unexpected{ document.error() };
				}
				if (aStop.stop_requested()) {
					return std::unexpected{ theme_document_error{
						.Failure = theme_document_failure::cancelled } };
				}
				auto parsed  = core::import_theme(*document);
				auto attempt = theme_import_attempt{
					.ThemeRejections = std::move(parsed.Rejections),
				};
				if (parsed.Palette.has_value() && parsed.Base.has_value()) {
					// [[BPR-1479]]. Roles the document predates take its own base's
					// built-in colours here, before the check, so the palette that
					// is checked is the one previewed, kept and exported again.
					auto palette = core::with_base_defaults(
						*parsed.Palette, aBuiltins[variant_index(*parsed.Base)]);
					attempt.ContrastRejections = contrast_rejections(palette);
					if (attempt.ContrastRejections.empty()) {
						attempt.Palette = std::move(palette);
					}
				}
				return attempt;
			} catch (...) {
				return std::unexpected{ theme_document_error{
					.Failure = theme_document_failure::unavailable } };
			}
		}

		void restore_now()
		{
			if (Closing) {
				return;
			}
			retire(appearance_request_domain::settings_load,
				   appearance_terminal_rejection::superseded);
			submit_settings_load();
		}

		/// \brief Adopts a complete settings record and persists it.
		///
		/// \details Adoption is whole-record and only ever reached with a record the
		/// core accepted. A record equal to the retained one is not a change and is
		/// not persisted. Palette adoption still publishes its typed acknowledgement
		/// so presentation can finish an equal-preview commit without inventing a
		/// write or treating an unrelated invalidation as that acknowledgement.
		void adopt(
			appearance_settings                         aSettings,
			std::optional<palette_adoption_publication> aPaletteAdoption = std::nullopt)
		{
			if (Settings == aSettings) {
				if (aPaletteAdoption.has_value()) {
					publish_changes(local_context(), std::move(aPaletteAdoption));
				}
				return;
			}
			// A reviewer gesture is newer than a startup read that has not reached
			// the decision lane. Letting that older terminal adopt afterward would
			// split the visible session from the record this gesture just stored.
			retire(appearance_request_domain::settings_load,
				   appearance_terminal_rejection::superseded);
			Settings = std::move(aSettings);
			submit_settings_store();
			publish_changes(local_context(), std::move(aPaletteAdoption));
		}

		void select_now(const core::appearance_mode aMode)
		{
			if (Closing) {
				return;
			}
			if (!known(aMode)) {
				publish_failure(local_context(),
								appearance_host_failure::unknown_appearance_mode);
				return;
			}
			auto next = Settings;
			next.Mode = aMode;
			adopt(std::move(next));
		}

		/// \brief Adopts the reported operating-system appearance observation.
		///
		/// \details An observation is not a setting: it is never persisted, and it
		/// changes the published resolution only for the modes the core says consume
		/// it. An explicit mode therefore observes without publishing.
		void observe_now(const core::appearance_observation& aObservation)
		{
			const auto consumer = find_consumer(CurrentConsumer);
			if (Closing || !consumer || consumer->Observation == aObservation) {
				return;
			}
			consumer->Observation = aObservation;
			publish_consumer_changes(consumer, local_context(), { });
		}

		/// \brief Commits one total palette as exactly one variant's customization.
		///
		/// \details A palette customises exactly one variant, and the one it
		/// customises is the theme that was on screen when the reviewer chose the
		/// colours. That variant arrives from the composition rather than being
		/// derived here from the current observation: by the time this runs the
		/// desktop may already have flipped, and deriving it then would file the
		/// colours under a theme the reviewer was not looking at. Every other
		/// variant's customization is untouched, so committing Dark cannot forget
		/// Light.
		void apply_palette_now(
			const core::appearance_palette& aPalette,
			const core::appearance_variant  aVariant)
		{
			if (Closing) {
				return;
			}
			if (!known(aVariant)) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			const auto rejections = contrast_rejections(aPalette);
			if (!rejections.empty()) {
				publish_contrast_rejection(local_context(), rejections);
				return;
			}
			auto next                        = Settings;
			customisation_of(next, aVariant) = appearance_palette_customisation{
				.Palette = aPalette,
				.Stored  = true,
			};
			adopt(
				std::move(next),
				palette_adoption_publication{ .Palette = aPalette, .Variant = aVariant });
		}

		void import_theme_now(std::string aPath)
		{
			if (Closing) {
				return;
			}
			submit_theme_import(std::move(aPath));
		}

		/// \brief Adopts typography the core accepts and refuses what it rejects.
		///
		/// \details Every rejection the core reported travels to presentation. The
		/// host neither keeps the legal half of an illegal record nor substitutes
		/// the nearest legal value, because a clamped value would be reported to the
		/// user as the one they chose.
		void apply_typography_now(core::typography_settings aTypography)
		{
			if (Closing) {
				return;
			}
			LastRejections = core::validate_typography(aTypography);
			if (!LastRejections.empty()) {
				publish_rejection(local_context(), LastRejections);
				return;
			}
			auto next       = Settings;
			next.Typography = std::move(aTypography);
			adopt(std::move(next));
		}

		/// \brief [[BPR-1714]]. Adopts a legal list and refuses one with any
		/// refused entry, naming each.
		/// \details Kept as written, in the reviewer's order: the list a field shows
		/// is the list they made. Classification reads its canonical form.
		void apply_known_namespaces_now(core::known_namespaces aNamespaces)
		{
			if (Closing) {
				return;
			}
			auto rejections = core::validate_known_namespaces(aNamespaces);
			if (!rejections.empty()) {
				publish_namespace_rejection(local_context(), std::move(rejections));
				return;
			}
			auto next       = Settings;
			next.Namespaces = std::move(aNamespaces);
			adopt(std::move(next));
		}

		/// \brief [[BPR-1807]]. Adopts an avatar style from the vocabulary.
		void apply_avatar_style_now(const core::identity_avatar_style aStyle)
		{
			if (Closing) {
				return;
			}
			if (!core::valid(aStyle)) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			auto next    = Settings;
			next.Avatars = aStyle;
			adopt(std::move(next));
		}

		void cancel_now()
		{
			if (Closing) {
				return;
			}
			retire_all(appearance_terminal_rejection::cancelled);
		}

		void shutdown_now()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			retire_all(appearance_terminal_rejection::cancelled);
		}

		// --- Terminals -------------------------------------------------------------

		/// \brief Adopts a durable record only when the core accepts it.
		///
		/// \details An absent record is not a failure and not a defaulted record: the
		/// composed settings stand and nothing is published. A present record is
		/// validated before adoption and refused whole with the core's own typed
		/// reasons when it fails. Restoring never writes the record back, so a
		/// refused record stays on disk for a later version to read rather than being
		/// overwritten with a correction the user never made.
		void finish_settings_load(
			const appearance_request_context&                              aContext,
			appearance_settings_result<std::optional<appearance_settings>> aResult)
		{
			const auto claimed = claim(aContext,
									   appearance_request_domain::settings_load);
			if (claimed.Rejection != appearance_terminal_rejection::none) {
				refuse(claimed.Rejection);
				return;
			}
			if (!aResult.has_value()) {
				publish_failure(aContext,
								appearance_host_failure::settings_unavailable);
				return;
			}
			if (!aResult->has_value()) {
				return;
			}
			auto loaded = std::move(**aResult);
			// The store preserves whether the palette is customized. Reset
			// records intentionally retain an empty override set across restart.
			if (!core::valid(loaded.Dif)) {
				publish_failure(aContext, appearance_host_failure::invalid_dif_preferences);
				return;
			}
			if (!known(loaded.Mode)) {
				publish_failure(aContext,
								appearance_host_failure::unknown_appearance_mode);
				return;
			}
			LastRejections = core::validate_typography(loaded.Typography);
			if (!LastRejections.empty()) {
				publish_rejection(aContext, LastRejections);
				return;
			}
			if (auto rejections = core::validate_known_namespaces(loaded.Namespaces); !rejections.empty()) {
				publish_namespace_rejection(aContext, std::move(rejections));
				return;
			}
			if (!core::valid(loaded.Avatars)) {
				publish_failure(aContext, appearance_host_failure::host_failure);
				return;
			}
			if (Settings == loaded) {
				return;
			}
			Settings = std::move(loaded);
			publish_changes(aContext);
		}

		void finish_settings_store(const appearance_request_context&       aContext,
								   const appearance_settings_result<void>& aResult)
		{
			const auto claimed = claim(aContext,
									   appearance_request_domain::settings_store);
			if (claimed.Rejection != appearance_terminal_rejection::none) {
				refuse(claimed.Rejection);
				return;
			}
			if (!aResult.has_value()) {
				publish_failure(aContext,
								appearance_host_failure::settings_unavailable);
				return;
			}
			++PersistedSettings;
		}

		void finish_theme_import(
			const appearance_request_context&           aContext,
			theme_document_result<theme_import_attempt> aResult)
		{
			const auto claimed = claim(aContext,
									   appearance_request_domain::theme_import);
			if (claimed.Rejection != appearance_terminal_rejection::none) {
				refuse(claimed.Rejection);
				return;
			}
			if (!aResult.has_value()) {
				publish_failure(aContext,
								appearance_host_failure::theme_document_unavailable);
				return;
			}
			if (!aResult->ThemeRejections.empty()) {
				publish_theme_rejection(aContext, aResult->ThemeRejections);
				return;
			}
			if (!aResult->ContrastRejections.empty()) {
				publish_contrast_rejection(
					aContext, aResult->ContrastRejections);
				return;
			}
			if (!aResult->Palette.has_value()) {
				publish_failure(aContext, appearance_host_failure::host_failure);
				return;
			}
			publish_theme_preview(aContext, *aResult->Palette);
		}

		// --- Publication -----------------------------------------------------------

		/// \brief Publishes exactly the presentation domains whose meaning changed.
		///
		/// \details The retained published snapshot decides what presentation has
		/// already seen, so a bounded change never republishes an unrelated domain.
		/// The epoch advances only when something is actually published, which is
		/// what lets presentation treat an unchanged epoch as permission to keep its
		/// derived state.
		///
		/// This is the whole of what an appearance change does. It engages no review
		/// state and touches no cache entry.
		void publish_changes(const appearance_request_context&           aContext,
							 std::optional<palette_adoption_publication> aPaletteAdoption = { })
		{
			const auto consumers = Consumers;
			for (const auto& consumer : consumers) {
				if (consumer->Active) {
					publish_consumer_changes(consumer, aContext,
											 consumer->Id == CurrentConsumer ? aPaletteAdoption : std::nullopt);
				}
			}
		}
		void publish_consumer_changes(const std::shared_ptr<consumer_state>&      aConsumer,
									  const appearance_request_context&           aContext,
									  std::optional<palette_adoption_publication> aPaletteAdoption)
		{
			if (Closing) {
				return;
			}
			const auto resolution   = core::resolve_appearance(Settings.Mode,
															   aConsumer->Observation);
			auto       invalidation = appearance_presentation_invalidation{ };
			const auto dif          = core::resolve_dif(Settings.Dif, aConsumer->MotionObservation);
			if (aConsumer->PublishedDif != dif) {
				invalidation.Dif = dif;
			}
			if (aConsumer->PublishedResolution != resolution) {
				invalidation.Resolution = resolution;
			}
			// The whole table, compared whole. A commit moves one variant and a
			// durable restore moves all four; publishing per variant would make
			// the bounded case the only one a consumer could act on.
			if (aConsumer->PublishedPalettes != Settings.Palettes) {
				invalidation.Palettes = Settings.Palettes;
			}
			if (aConsumer->PublishedTypography != Settings.Typography) {
				invalidation.Typography = Settings.Typography;
			}
			if (aConsumer->PublishedNamespaces != Settings.Namespaces) {
				invalidation.Namespaces = Settings.Namespaces;
			}
			if (aConsumer->PublishedAvatars != Settings.Avatars) {
				invalidation.Avatars = Settings.Avatars;
			}
			publish_persistence_failure(aConsumer, aContext);
			if (!aConsumer->Active || Closing) {
				return;
			}
			if (empty(invalidation) && !aPaletteAdoption.has_value()) {
				return;
			}
			auto presentation
				= std::optional<appearance_presentation_invalidation>{ };
			if (!empty(invalidation)) {
				aConsumer->Epoch               = appearance_epoch{ aConsumer->Epoch.value() + 1U };
				invalidation.Epoch             = aConsumer->Epoch;
				aConsumer->PublishedResolution = resolution;
				aConsumer->PublishedPalettes   = Settings.Palettes;
				aConsumer->PublishedTypography = Settings.Typography;
				aConsumer->PublishedNamespaces = Settings.Namespaces;
				aConsumer->PublishedAvatars    = Settings.Avatars;
				aConsumer->PublishedDif        = dif;
				presentation                   = std::move(invalidation);
			}
			const auto projection = appearance_projection{
				.Context         = aContext,
				.Presentation    = std::move(presentation),
				.PaletteAdoption = std::move(aPaletteAdoption),
			};
			deliver_projection(
				[&aConsumer, &projection] { aConsumer->Port->publish(projection); });
		}

		void publish_theme_preview(
			const appearance_request_context& aContext,
			const core::appearance_palette&   aPalette)
		{
			if (Closing) {
				return;
			}
			const auto projection = appearance_projection{
				.Context      = aContext,
				.ThemePreview = theme_preview_publication{ .Palette = aPalette },
			};
			deliver_projection(
				[this, &projection] { publish_to_consumer(projection); });
		}

		void publish_persistence_failure(const std::shared_ptr<consumer_state>& aConsumer, const appearance_request_context& aContext, const bool aRepeat = false)
		{
			if (PersistenceUnavailable && aConsumer->Active && (aRepeat || !aConsumer->PersistenceFailurePublished)) {
				aConsumer->PersistenceFailurePublished = true;
				deliver_projection([&] { aConsumer->Port->fail(appearance_failure_publication{
											 .Context = aContext, .Failure = appearance_host_failure::settings_unavailable }); });
			}
		}

		void publish_failure(const appearance_request_context& aContext,
							 const appearance_host_failure     aFailure)
		{
			if (Closing) {
				return;
			}
			if (aFailure == appearance_host_failure::settings_unavailable) {
				PersistenceUnavailable = true;
				const auto consumers   = Consumers;
				for (const auto& consumer : consumers) {
					publish_persistence_failure(consumer, aContext, true);
				}
				return;
			}
			deliver_projection([this, &aContext, aFailure] {
				fail_consumer(appearance_failure_publication{
					.Context = aContext,
					.Failure = aFailure,
				});
			});
		}

		void publish_rejection(
			const appearance_request_context&              aContext,
			const std::vector<core::typography_rejection>& aRejections)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, &aRejections] {
				fail_consumer(appearance_failure_publication{
					.Context    = aContext,
					.Failure    = appearance_host_failure::settings_rejected,
					.Rejections = aRejections,
				});
			});
		}

		void publish_namespace_rejection(
			const appearance_request_context&            aContext,
			std::vector<core::known_namespace_rejection> aRejections)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, &aRejections] {
				fail_consumer(appearance_failure_publication{
					.Context             = aContext,
					.Failure             = appearance_host_failure::invalid_known_namespaces,
					.NamespaceRejections = std::move(aRejections),
				});
			});
		}

		void publish_theme_rejection(
			const appearance_request_context&                aContext,
			const std::vector<core::theme_import_rejection>& aRejections)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, &aRejections] {
				fail_consumer(appearance_failure_publication{
					.Context         = aContext,
					.Failure         = appearance_host_failure::theme_import_rejected,
					.ThemeRejections = aRejections,
				});
			});
		}

		void publish_contrast_rejection(
			const appearance_request_context&                 aContext,
			const std::vector<appearance_contrast_rejection>& aRejections)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, &aRejections] {
				fail_consumer(appearance_failure_publication{
					.Context            = aContext,
					.Failure            = appearance_host_failure::palette_contrast_rejected,
					.ContrastRejections = aRejections,
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

		appearance_settings               Settings;
		const appearance_settings         Defaults;
		const appearance_builtin_palettes Builtins;
		appearance_settings_port&         SettingsPort;
		theme_document_port&              ThemeDocuments;
		effect_executor&                  Effects;
		decision_executor&                DecisionLane;
		core::application_session_id      Session;
		appearance_host_limits            Limits;
		std::shared_ptr<completion_gate>  Gate;

		core::appearance_observation                 EmptyObservation;
		std::vector<std::shared_ptr<consumer_state>> Consumers;
		appearance_consumer_id                       CurrentConsumer;
		std::uint64_t                                NextConsumer = 1U;
		std::vector<core::typography_rejection>      LastRejections;

		std::vector<outstanding_request>       Outstanding;
		std::deque<queued_preferences_ingress> DeferredIngress;
		std::uint64_t                          NextRequest = 1U;

		std::size_t                   PersistedSettings        = 0U;
		std::size_t                   RejectedTerminals        = 0U;
		appearance_terminal_rejection LastRejection            = appearance_terminal_rejection::none;
		bool                          Closing                  = false;
		bool                          PersistenceUnavailable   = false;
		bool                          TurnActive               = false;
		bool                          DeferredIngressScheduled = false;
		bool                          ProjectionDeliveryFailed = false;
	};

	appearance_preferences_application_host::appearance_preferences_application_host(appearance_settings aInitialSettings, const appearance_builtin_palettes& aBuiltins,
																					 appearance_settings_port& aSettings, theme_document_port& aThemeDocuments, effect_executor& aEffects,
																					 decision_executor& aDecisionLane, core::application_session_id aSession, appearance_host_limits aLimits)
		: Implementation{ std::make_unique<implementation>(std::move(aInitialSettings), aBuiltins, aSettings, aThemeDocuments, aEffects, aDecisionLane, aSession, aLimits) }
	{ }
	appearance_preferences_application_host::~appearance_preferences_application_host() { Implementation->shutdown_now(); }
	auto appearance_preferences_application_host::attach(std::shared_ptr<appearance_projection_port> aPort, bool aPublishInitial)
		-> std::expected<appearance_consumer_id, appearance_attachment_failure>
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return std::unexpected{ appearance_attachment_failure::closing };
		}
		if (!aPort) {
			return std::unexpected{ appearance_attachment_failure::invalid_consumer };
		}
		if (host.Consumers.size() >= core::maximum_bpr_windows_per_profile) {
			return std::unexpected{ appearance_attachment_failure::capacity };
		}
		if (host.NextConsumer == 0U) {
			return std::unexpected{ appearance_attachment_failure::identity_exhausted };
		}
		const auto id       = appearance_consumer_id{ host.NextConsumer++ };
		auto       consumer = std::make_shared<implementation::consumer_state>();
		consumer->Id        = id;
		consumer->Port      = std::move(aPort);
		host.Consumers.push_back(consumer);
		if (aPublishInitial) {
			host.enqueue(deferred_appearance_observe{ .Observation = consumer->Observation, .Initialize = true }, id);
		}
		return id;
	}
	void appearance_preferences_application_host::detach(appearance_consumer_id aConsumer)
	{
		auto&      host     = *Implementation;
		const auto consumer = host.find_consumer(aConsumer);
		if (!consumer) {
			return;
		}
		consumer->Active = false;
		std::erase(host.Consumers, consumer);
		for (auto& record : host.Outstanding) {
			if (record.Consumer == aConsumer && (record.Domain == appearance_request_domain::theme_import || record.Domain == appearance_request_domain::theme_export)) {
				record.Retirement = appearance_terminal_rejection::cancelled;
				record.Stop.request_stop();
			}
		}
	}

	void appearance_preferences_application_host::apply(appearance_consumer_id aConsumer, core::dif_preferences aPreferences)
	{
		Implementation->enqueue(aPreferences, aConsumer);
	}
	void appearance_preferences_application_host::observe_motion(appearance_consumer_id aConsumer, core::motion_observation aObservation)
	{
		Implementation->enqueue(aObservation, aConsumer);
	}
	auto appearance_preferences_application_host::dif_resolution(appearance_consumer_id aConsumer) const noexcept -> core::dif_resolution
	{
		const auto consumer = Implementation->find_consumer(aConsumer);
		return core::resolve_dif(settings().Dif, consumer ? consumer->MotionObservation : core::motion_observation::unknown);
	}
	void appearance_preferences_application_host::restore(appearance_consumer_id aConsumer)
	{
		Implementation->enqueue(deferred_appearance_restore{ }, aConsumer);
	}

	void appearance_preferences_application_host::select(appearance_consumer_id aConsumer, const core::appearance_mode aMode)
	{
		Implementation->enqueue(deferred_appearance_select{ .Mode = aMode }, aConsumer);
	}

	void appearance_preferences_application_host::observe(appearance_consumer_id             aConsumer,
														  const core::appearance_observation aObservation)
	{
		Implementation->enqueue(
			deferred_appearance_observe{ .Observation = aObservation }, aConsumer);
	}

	void appearance_preferences_application_host::apply(appearance_consumer_id aConsumer, core::appearance_palette aPalette, const core::appearance_variant aVariant)
	{
		Implementation->enqueue(
			deferred_appearance_palette{ .Palette = aPalette, .Variant = aVariant }, aConsumer);
	}

	void appearance_preferences_application_host::apply(appearance_consumer_id aConsumer, core::typography_settings aTypography)
	{
		Implementation->enqueue(
			deferred_appearance_typography{ .Typography = std::move(aTypography) }, aConsumer);
	}

	void appearance_preferences_application_host::step_code_zoom(appearance_consumer_id aConsumer, const core::code_zoom_step aStep)
	{
		Implementation->enqueue(deferred_appearance_zoom{ .Step = aStep }, aConsumer);
	}

	void appearance_preferences_application_host::apply(appearance_consumer_id aConsumer, core::known_namespaces aNamespaces)
	{
		Implementation->enqueue(deferred_known_namespaces{ .Namespaces = std::move(aNamespaces) }, aConsumer);
	}

	void appearance_preferences_application_host::apply(appearance_consumer_id aConsumer, const core::identity_avatar_style aStyle)
	{
		Implementation->enqueue(deferred_avatar_style{ .Style = aStyle }, aConsumer);
	}

	void appearance_preferences_application_host::import_theme_file(appearance_consumer_id aConsumer, std::string aPath)
	{
		Implementation->enqueue(
			deferred_appearance_theme_import{ .Path = std::move(aPath) }, aConsumer);
	}

	void appearance_preferences_application_host::cancel_theme_import(appearance_consumer_id aConsumer)
	{
		Implementation->enqueue(cancel_appearance_palette_preview_intent{ }, aConsumer);
	}

	void appearance_preferences_application_host::export_theme_file(appearance_consumer_id aConsumer, std::string aPath, core::appearance_palette aPalette, const core::appearance_variant aVariant)
	{
		Implementation->enqueue(deferred_appearance_theme_export{ .Path = std::move(aPath), .Palette = aPalette, .Variant = aVariant }, aConsumer);
	}

	void appearance_preferences_application_host::reset(appearance_consumer_id aConsumer, const appearance_reset_scope aScope, const core::appearance_variant aVariant)
	{
		Implementation->enqueue(deferred_appearance_reset{ .Scope = aScope, .Variant = aVariant }, aConsumer);
	}

	void appearance_preferences_application_host::cancel()
	{
		Implementation->enqueue(deferred_appearance_cancel{ });
	}

	void appearance_preferences_application_host::shutdown()
	{
		Implementation->enqueue(deferred_appearance_shutdown{ });
	}

	auto appearance_preferences_application_host::settings() const noexcept
		-> const appearance_settings&
	{
		return Implementation->Settings;
	}

	auto appearance_preferences_application_host::last_rejections() const noexcept
		-> std::span<const core::typography_rejection>
	{
		return Implementation->LastRejections;
	}

	auto appearance_preferences_application_host::persisted_settings() const noexcept
		-> std::size_t
	{
		return Implementation->PersistedSettings;
	}

	auto appearance_preferences_application_host::outstanding_requests() const noexcept
		-> std::size_t
	{
		return Implementation->Outstanding.size();
	}

	auto appearance_preferences_application_host::rejected_terminals() const noexcept
		-> std::size_t
	{
		return Implementation->RejectedTerminals;
	}

	auto appearance_preferences_application_host::last_rejection() const noexcept
		-> appearance_terminal_rejection
	{
		return Implementation->LastRejection;
	}

	auto appearance_preferences_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto appearance_preferences_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Outstanding.empty()
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->TurnActive;
	}

	auto appearance_preferences_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
	auto appearance_preferences_application_host::resolution(appearance_consumer_id aConsumer) const noexcept -> core::appearance_resolution
	{
		return core::resolve_appearance(settings().Mode, observation(aConsumer));
	}
	auto appearance_preferences_application_host::observation(appearance_consumer_id aConsumer) const noexcept -> const core::appearance_observation&
	{
		const auto consumer = Implementation->find_consumer(aConsumer);
		return consumer ? consumer->Observation : Implementation->EmptyObservation;
	}
	auto appearance_preferences_application_host::epoch(appearance_consumer_id aConsumer) const noexcept -> appearance_epoch
	{
		const auto consumer = Implementation->find_consumer(aConsumer);
		return consumer ? consumer->Epoch : appearance_epoch{ };
	}
	auto appearance_preferences_application_host::attached_consumers() const noexcept -> std::size_t { return Implementation->Consumers.size(); }
	auto appearance_preferences_application_host::durable() const noexcept -> bool
	{
		const auto& state = *Implementation;
		return state.Outstanding.empty() && state.DeferredIngress.empty() && !state.DeferredIngressScheduled
			&& !state.TurnActive && !state.PersistenceUnavailable && !state.ProjectionDeliveryFailed;
	}
}
