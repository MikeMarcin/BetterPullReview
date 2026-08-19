#include "bpr/application/appearance_application_host.hpp"

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
			classification,
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

		struct staged_classification
		{
			syntax_classification Classification;
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
		};

		struct deferred_appearance_palette
		{
			core::appearance_palette Palette{ };
		};

		struct deferred_appearance_typography
		{
			core::typography_settings Typography;
		};

		struct deferred_appearance_theme_import
		{
			std::string Path;
		};

		struct deferred_appearance_classify
		{
			token_span_cache_key Key;
		};

		struct deferred_appearance_cancel
		{
		};

		struct deferred_appearance_shutdown
		{
		};

		using deferred_appearance_ingress = std::variant<deferred_appearance_restore,
														 deferred_appearance_select,
														 deferred_appearance_observe,
														 deferred_appearance_palette,
														 deferred_appearance_typography,
														 deferred_appearance_theme_import,
														 deferred_appearance_classify,
														 deferred_appearance_cancel,
														 deferred_appearance_shutdown,
														 staged_settings_load,
														 staged_settings_store,
														 staged_theme_import,
														 staged_classification>;

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
			if (!known(aSettings.Mode)) {
				throw std::invalid_argument{ "Invalid appearance mode" };
			}
			if (!core::validate_typography(aSettings.Typography).empty()) {
				throw std::invalid_argument{ "Invalid appearance typography" };
			}
			return aSettings;
		}

		[[nodiscard]] constexpr auto failure_for_status(
			const syntax_classification_status aStatus) noexcept
			-> appearance_host_failure
		{
			return aStatus == syntax_classification_status::limit_exceeded
				? appearance_host_failure::classification_limit
				: appearance_host_failure::classification_unavailable;
		}
	} // namespace

	struct appearance_application_host::implementation
	{
		struct completion_gate
		{
			std::atomic<implementation*> Host = nullptr;
		};

		/// \brief Retains one classified blob.
		///
		/// \details `Sequence` is the last-use stamp that selects the eviction
		/// victim. It is not part of the identity: only `Key` is, and `Key` carries
		/// no presentation value at all.
		struct cache_entry
		{
			token_span_cache_key Key;
			shared_token_spans   Spans;
			std::uint64_t        Sequence = 0U;
		};

		struct outstanding_request
		{
			appearance_request_context    Context;
			appearance_request_domain     Domain     = appearance_request_domain::settings_load;
			appearance_terminal_rejection Retirement = appearance_terminal_rejection::none;
			token_span_cache_key          Key;
			std::stop_source              Stop;
		};

		/// \brief Describes the outcome of claiming one terminal.
		///
		/// \details The claimed record is erased, so its key travels back with the
		/// verdict. A classification terminal that names a different key than the
		/// request it claims is not the answer the host asked for.
		struct claimed_request
		{
			appearance_terminal_rejection Rejection = appearance_terminal_rejection::unknown_request;
			token_span_cache_key          Key;
		};

		implementation(appearance_settings                aInitialSettings,
					   appearance_settings_port&          aSettings,
					   theme_document_port&               aThemeDocuments,
					   syntax_classification_port&        aClassification,
					   effect_executor&                   aEffects,
					   decision_executor&                 aDecisionLane,
					   appearance_projection_port&        aProjections,
					   const core::application_session_id aSession,
					   const appearance_host_limits       aLimits)
			: Settings{ validated_settings(std::move(aInitialSettings)) }
			, SettingsPort{ aSettings }
			, ThemeDocuments{ aThemeDocuments }
			, Classifier{ aClassification }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
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

		void enqueue(deferred_appearance_ingress aIngress)
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

		void process(deferred_appearance_ingress aIngress)
		{
			std::visit(
				[this](auto&& aTyped) {
					using ingress_type = std::decay_t<decltype(aTyped)>;
					if constexpr (std::is_same_v<ingress_type, deferred_appearance_restore>) {
						restore_now();
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_select>) {
						select_now(aTyped.Mode);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_observe>) {
						observe_now(aTyped.Observation);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_palette>) {
						apply_palette_now(aTyped.Palette);
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_typography>) {
						apply_typography_now(std::move(aTyped.Typography));
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_theme_import>) {
						import_theme_now(std::move(aTyped.Path));
					} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_classify>) {
						classify_now(std::move(aTyped.Key));
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
					} else {
						finish_classification(std::move(aTyped.Classification));
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
		/// Retirement is per record and per domain: a settings gesture never retires
		/// an in-flight classification, because the bytes it was classifying did not
		/// change.
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
				.Key       = std::move(found->Key),
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
			LastRejection = aRejection;
		}

		// --- Effect submission --------------------------------------------------

		[[nodiscard]] auto reserve(const appearance_request_domain aDomain)
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
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
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
				Effects.post([gate, lane, port, context, path = std::move(aPath), stop] {
					auto staged = staged_theme_import{
						.Context = context,
						.Result  = perform_theme_import(*port, path, stop),
					};
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
			} catch (...) {
				abandon(context.Request);
				publish_failure(context,
								appearance_host_failure::theme_document_unavailable);
			}
		}

		[[nodiscard]] static auto perform_theme_import(
			theme_document_port& aPort,
			const std::string&   aPath,
			std::stop_token      aStop)
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
				if (parsed.Palette.has_value()) {
					attempt.ContrastRejections
						= contrast_rejections(*parsed.Palette);
					if (attempt.ContrastRejections.empty()) {
						attempt.Palette = std::move(parsed.Palette);
					}
				}
				return attempt;
			} catch (...) {
				return std::unexpected{ theme_document_error{
					.Failure = theme_document_failure::unavailable } };
			}
		}

		void submit_classification(token_span_cache_key aKey)
		{
			auto* const record = reserve(appearance_request_domain::classification);
			if (record == nullptr) {
				publish_failure(local_context(), appearance_host_failure::host_failure);
				return;
			}
			record->Key        = std::move(aKey);
			const auto request = syntax_classification_request{
				.Context      = record->Context,
				.Key          = record->Key,
				.MaximumSpans = Limits.TokenSpansPerBlob,
			};
			const auto  stop       = record->Stop.get_token();
			const auto  gate       = std::weak_ptr<completion_gate>{ Gate };
			auto* const classifier = &Classifier;
			auto* const lane       = &DecisionLane;
			try {
				Effects.post([gate, lane, classifier, request, stop] {
					auto staged = staged_classification{
						.Classification = perform_classification(*classifier, request, stop),
					};
					lane->post([gate, staged = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(staged));
					});
				});
				++ClassificationSubmissions;
			} catch (...) {
				abandon(request.Context.Request);
				publish_failure(request.Context,
								appearance_host_failure::classification_unavailable);
			}
		}

		[[nodiscard]] static auto perform_classification(
			syntax_classification_port&          aPort,
			const syntax_classification_request& aRequest,
			std::stop_token                      aStop) -> syntax_classification
		{
			auto refusal = syntax_classification{
				.Context = aRequest.Context,
				.Key     = aRequest.Key,
				.Status  = syntax_classification_status::cancelled,
			};
			if (aStop.stop_requested()) {
				return refusal;
			}
			try {
				return aPort.classify(aRequest, std::move(aStop));
			} catch (...) {
				refusal.Status = syntax_classification_status::unavailable;
				return refusal;
			}
		}

		// --- Token-span cache ----------------------------------------------------

		[[nodiscard]] auto find_entry(const token_span_cache_key& aKey) noexcept
			-> cache_entry*
		{
			const auto found = std::ranges::find_if(
				Cache, [&aKey](const cache_entry& aEntry) {
					return aEntry.Key == aKey;
				});
			return found == Cache.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto retained(const token_span_cache_key& aKey) const
			-> shared_token_spans
		{
			const auto found = std::ranges::find_if(
				Cache, [&aKey](const cache_entry& aEntry) {
					return aEntry.Key == aKey;
				});
			return found == Cache.end() ? shared_token_spans{ } : found->Spans;
		}

		/// \brief Retains one classification under its blob-and-definition key.
		///
		/// \details Eviction is least recently used and bounded by the configured
		/// entry count. Nothing about the active appearance participates here: a
		/// theme, palette, mode, or font change reaches no line of this function,
		/// which is why switching them cannot cost a reparse.
		void retain(const token_span_cache_key& aKey, shared_token_spans aSpans)
		{
			++CacheSequence;
			if (auto* const existing = find_entry(aKey); existing != nullptr) {
				existing->Spans    = std::move(aSpans);
				existing->Sequence = CacheSequence;
				return;
			}
			if (Cache.size() >= Limits.TokenSpanCacheEntries) {
				evict_least_recent();
			}
			Cache.push_back(cache_entry{
				.Key      = aKey,
				.Spans    = std::move(aSpans),
				.Sequence = CacheSequence,
			});
		}

		void evict_least_recent() noexcept
		{
			const auto found = std::ranges::min_element(
				Cache, { }, &cache_entry::Sequence);
			if (found == Cache.end()) {
				return;
			}
			Cache.erase(found);
			++CacheEvictions;
		}

		[[nodiscard]] auto classification_in_flight(
			const token_span_cache_key& aKey) const noexcept -> bool
		{
			return std::ranges::any_of(
				Outstanding, [&aKey](const outstanding_request& aRecord) {
					return aRecord.Domain == appearance_request_domain::classification
						&& aRecord.Retirement == appearance_terminal_rejection::none
						&& aRecord.Key == aKey;
				});
		}

		// --- Operations -----------------------------------------------------------

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
			if (Closing || Observation == aObservation) {
				return;
			}
			Observation = aObservation;
			publish_changes(local_context());
		}

		void apply_palette_now(const core::appearance_palette& aPalette)
		{
			if (Closing) {
				return;
			}
			const auto rejections = contrast_rejections(aPalette);
			if (!rejections.empty()) {
				publish_contrast_rejection(local_context(), rejections);
				return;
			}
			auto next          = Settings;
			next.Palette       = aPalette;
			next.PaletteStored = true;
			adopt(
				std::move(next),
				palette_adoption_publication{ .Palette = aPalette });
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

		void classify_now(token_span_cache_key aKey)
		{
			if (Closing) {
				return;
			}
			if (!valid(aKey)) {
				++ClassificationFailures;
				publish_failure(local_context(),
								appearance_host_failure::invalid_classification_key);
				return;
			}
			if (auto* const entry = find_entry(aKey); entry != nullptr) {
				++CacheSequence;
				entry->Sequence = CacheSequence;
				++CacheHits;
				publish_spans(local_context(), aKey, entry->Spans, true);
				return;
			}
			if (classification_in_flight(aKey)) {
				++CoalescedClassifications;
				return;
			}
			submit_classification(std::move(aKey));
		}

		/// \brief Retires outstanding work without disturbing retained state.
		///
		/// \details There is no generation to advance. Staleness lives on the record
		/// that owns it, so cancelling settings work cannot invalidate a cached
		/// classification, and cancelling a classification cannot invalidate the
		/// appearance.
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
			// A record that came off disk carries a palette a reviewer chose,
			// which a composed default does not. BPR-0216 is what happens when
			// nothing can tell the two apart.
			loaded.PaletteStored = true;
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

		void finish_classification(syntax_classification aClassification)
		{
			++ClassificationTerminals;
			const auto claimed = claim(aClassification.Context,
									   appearance_request_domain::classification);
			if (claimed.Rejection != appearance_terminal_rejection::none) {
				++ClassificationFailures;
				refuse(claimed.Rejection);
				return;
			}
			if (claimed.Key != aClassification.Key) {
				++ClassificationFailures;
				publish_failure(aClassification.Context,
								appearance_host_failure::host_failure);
				return;
			}
			if (aClassification.Status != syntax_classification_status::available) {
				++ClassificationFailures;
				publish_failure(aClassification.Context,
								failure_for_status(aClassification.Status));
				return;
			}
			if (aClassification.Spans.size() > Limits.TokenSpansPerBlob) {
				++ClassificationFailures;
				publish_failure(aClassification.Context,
								appearance_host_failure::classification_limit);
				return;
			}
			auto spans = std::make_shared<const token_span_list>(
				std::move(aClassification.Spans));
			retain(aClassification.Key, spans);
			publish_spans(aClassification.Context,
						  aClassification.Key,
						  std::move(spans),
						  false);
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
		void publish_changes(
			const appearance_request_context&           aContext,
			std::optional<palette_adoption_publication> aPaletteAdoption = std::nullopt)
		{
			if (Closing) {
				return;
			}
			const auto resolution   = core::resolve_appearance(Settings.Mode,
															   Observation);
			auto       invalidation = appearance_presentation_invalidation{ };
			if (PublishedResolution != resolution) {
				invalidation.Resolution = resolution;
			}
			if (PublishedPalette != Settings.Palette) {
				invalidation.Palette = Settings.Palette;
			}
			if (PublishedTypography != Settings.Typography) {
				invalidation.Typography = Settings.Typography;
			}
			if (empty(invalidation) && !aPaletteAdoption.has_value()) {
				return;
			}
			auto presentation
				= std::optional<appearance_presentation_invalidation>{ };
			if (!empty(invalidation)) {
				Epoch               = appearance_epoch{ Epoch.value() + 1U };
				invalidation.Epoch  = Epoch;
				PublishedResolution = resolution;
				PublishedPalette    = Settings.Palette;
				PublishedTypography = Settings.Typography;
				presentation        = std::move(invalidation);
			}
			const auto projection = appearance_projection{
				.Context         = aContext,
				.Presentation    = std::move(presentation),
				.PaletteAdoption = std::move(aPaletteAdoption),
			};
			deliver_projection(
				[this, &projection] { Projections.publish(projection); });
		}

		void publish_spans(const appearance_request_context& aContext,
						   const token_span_cache_key&       aKey,
						   shared_token_spans                aSpans,
						   const bool                        aCacheHit)
		{
			if (Closing) {
				return;
			}
			const auto projection = appearance_projection{
				.Context = aContext,
				.Spans   = token_span_publication{
					.Key      = aKey,
					.Spans    = std::move(aSpans),
					.CacheHit = aCacheHit,
				},
			};
			deliver_projection(
				[this, &projection] { Projections.publish(projection); });
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
				[this, &projection] { Projections.publish(projection); });
		}

		void publish_failure(const appearance_request_context& aContext,
							 const appearance_host_failure     aFailure)
		{
			if (Closing) {
				return;
			}
			deliver_projection([this, &aContext, aFailure] {
				Projections.fail(appearance_failure_publication{
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
				Projections.fail(appearance_failure_publication{
					.Context    = aContext,
					.Failure    = appearance_host_failure::settings_rejected,
					.Rejections = aRejections,
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
				Projections.fail(appearance_failure_publication{
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
				Projections.fail(appearance_failure_publication{
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

		appearance_settings              Settings;
		appearance_settings_port&        SettingsPort;
		theme_document_port&             ThemeDocuments;
		syntax_classification_port&      Classifier;
		effect_executor&                 Effects;
		decision_executor&               DecisionLane;
		appearance_projection_port&      Projections;
		core::application_session_id     Session;
		appearance_host_limits           Limits;
		std::shared_ptr<completion_gate> Gate;

		core::appearance_observation            Observation;
		std::vector<core::typography_rejection> LastRejections;
		std::vector<cache_entry>                Cache;

		std::vector<outstanding_request>        Outstanding;
		std::deque<deferred_appearance_ingress> DeferredIngress;
		std::uint64_t                           NextRequest   = 1U;
		std::uint64_t                           CacheSequence = 0U;
		appearance_epoch                        Epoch;

		std::optional<core::appearance_resolution> PublishedResolution;
		std::optional<core::appearance_palette>    PublishedPalette;
		std::optional<core::typography_settings>   PublishedTypography;

		std::size_t                   CacheHits                 = 0U;
		std::size_t                   ClassificationSubmissions = 0U;
		std::size_t                   CoalescedClassifications  = 0U;
		std::size_t                   ClassificationFailures    = 0U;
		std::size_t                   ClassificationTerminals   = 0U;
		std::size_t                   CacheEvictions            = 0U;
		std::size_t                   PersistedSettings         = 0U;
		std::size_t                   RejectedTerminals         = 0U;
		appearance_terminal_rejection LastRejection             = appearance_terminal_rejection::none;
		bool                          Closing                   = false;
		bool                          TurnActive                = false;
		bool                          DeferredIngressScheduled  = false;
		bool                          ProjectionDeliveryFailed  = false;
	};

	appearance_application_host::appearance_application_host(
		appearance_settings                aInitialSettings,
		appearance_settings_port&          aSettings,
		theme_document_port&               aThemeDocuments,
		syntax_classification_port&        aClassification,
		effect_executor&                   aEffects,
		decision_executor&                 aDecisionLane,
		appearance_projection_port&        aProjections,
		const core::application_session_id aSession,
		const appearance_host_limits       aLimits)
		: Implementation{ std::make_unique<implementation>(std::move(aInitialSettings),
														   aSettings,
														   aThemeDocuments,
														   aClassification,
														   aEffects,
														   aDecisionLane,
														   aProjections,
														   aSession,
														   aLimits) }
	{
	}

	appearance_application_host::~appearance_application_host()
	{
		Implementation->shutdown_now();
	}

	void appearance_application_host::restore()
	{
		Implementation->enqueue(deferred_appearance_restore{ });
	}

	void appearance_application_host::select(const core::appearance_mode aMode)
	{
		Implementation->enqueue(deferred_appearance_select{ .Mode = aMode });
	}

	void appearance_application_host::observe(
		const core::appearance_observation aObservation)
	{
		Implementation->enqueue(
			deferred_appearance_observe{ .Observation = aObservation });
	}

	void appearance_application_host::apply(core::appearance_palette aPalette)
	{
		Implementation->enqueue(
			deferred_appearance_palette{ .Palette = aPalette });
	}

	void appearance_application_host::apply(core::typography_settings aTypography)
	{
		Implementation->enqueue(
			deferred_appearance_typography{ .Typography = std::move(aTypography) });
	}

	void appearance_application_host::import_theme_file(std::string aPath)
	{
		Implementation->enqueue(
			deferred_appearance_theme_import{ .Path = std::move(aPath) });
	}

	void appearance_application_host::classify(token_span_cache_key aKey)
	{
		Implementation->enqueue(
			deferred_appearance_classify{ .Key = std::move(aKey) });
	}

	void appearance_application_host::cancel()
	{
		Implementation->enqueue(deferred_appearance_cancel{ });
	}

	void appearance_application_host::shutdown()
	{
		Implementation->enqueue(deferred_appearance_shutdown{ });
	}

	auto appearance_application_host::settings() const noexcept
		-> const appearance_settings&
	{
		return Implementation->Settings;
	}

	auto appearance_application_host::resolution() const noexcept
		-> core::appearance_resolution
	{
		return core::resolve_appearance(Implementation->Settings.Mode,
										Implementation->Observation);
	}

	auto appearance_application_host::observation() const noexcept
		-> const core::appearance_observation&
	{
		return Implementation->Observation;
	}

	auto appearance_application_host::epoch() const noexcept -> appearance_epoch
	{
		return Implementation->Epoch;
	}

	auto appearance_application_host::last_rejections() const noexcept
		-> std::span<const core::typography_rejection>
	{
		return Implementation->LastRejections;
	}

	auto appearance_application_host::cached(const token_span_cache_key& aKey) const
		-> shared_token_spans
	{
		return Implementation->retained(aKey);
	}

	auto appearance_application_host::cached_classifications() const noexcept
		-> std::size_t
	{
		return Implementation->Cache.size();
	}

	auto appearance_application_host::cache_hits() const noexcept -> std::size_t
	{
		return Implementation->CacheHits;
	}

	auto appearance_application_host::classification_submissions() const noexcept
		-> std::size_t
	{
		return Implementation->ClassificationSubmissions;
	}

	auto appearance_application_host::classification_failures() const noexcept
		-> std::size_t
	{
		return Implementation->ClassificationFailures;
	}

	auto appearance_application_host::classification_terminals() const noexcept
		-> std::size_t
	{
		return Implementation->ClassificationTerminals;
	}

	auto appearance_application_host::coalesced_classifications() const noexcept
		-> std::size_t
	{
		return Implementation->CoalescedClassifications;
	}

	auto appearance_application_host::cache_evictions() const noexcept
		-> std::size_t
	{
		return Implementation->CacheEvictions;
	}

	auto appearance_application_host::persisted_settings() const noexcept
		-> std::size_t
	{
		return Implementation->PersistedSettings;
	}

	auto appearance_application_host::outstanding_requests() const noexcept
		-> std::size_t
	{
		return Implementation->Outstanding.size();
	}

	auto appearance_application_host::rejected_terminals() const noexcept
		-> std::size_t
	{
		return Implementation->RejectedTerminals;
	}

	auto appearance_application_host::last_rejection() const noexcept
		-> appearance_terminal_rejection
	{
		return Implementation->LastRejection;
	}

	auto appearance_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto appearance_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Outstanding.empty()
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->TurnActive;
	}

	auto appearance_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
} // namespace bpr::application
