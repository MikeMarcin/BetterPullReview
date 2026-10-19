#include "bpr/application/syntax_classification_application_host.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
		enum class appearance_request_domain : std::uint8_t { classification };
		struct staged_classification
		{
			syntax_classification Classification;
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
		using deferred_appearance_ingress = std::variant<staged_classification, deferred_appearance_classify, deferred_appearance_cancel, deferred_appearance_shutdown>;
		[[nodiscard]] auto validated_limits(const appearance_host_limits aLimits)
			-> appearance_host_limits
		{
			if (!valid(aLimits)) {
				throw std::invalid_argument{ "Invalid appearance host limits" };
			}
			return aLimits;
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

	struct syntax_classification_application_host::implementation
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
			appearance_request_domain     Domain     = appearance_request_domain::classification;
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

		implementation(syntax_classification_port& aClassification, effect_executor& aEffects,
					   decision_executor& aDecisionLane, appearance_projection_port& aProjections,
					   const core::application_session_id aSession, const appearance_host_limits aLimits)
			: Classifier{ aClassification }
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
			std::visit([this](auto&& aTyped) {
				using ingress_type = std::decay_t<decltype(aTyped)>;
				if constexpr (std::is_same_v<ingress_type, deferred_appearance_classify>) {
					classify_now(std::move(aTyped.Key));
				} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_cancel>) {
					cancel_now();
				} else if constexpr (std::is_same_v<ingress_type, deferred_appearance_shutdown>) {
					shutdown_now();
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
		/// Retirement is per record; preference gestures never enter this owner.
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
			deliver_projection([this, aRejection] { Projections.rejected(aRejection); });
		}

		// --- Effect submission --------------------------------------------------

		[[nodiscard]] auto reserve(const appearance_request_domain aDomain)
			-> outstanding_request*
		{
			if (Outstanding.size() >= Limits.OutstandingRequests) {
				return nullptr;
			}
			// Identity is taken before the slot is allocated, as it was when the
			// record travelled through a temporary; the record is built in place.
			const auto context = next_context();
			auto&      record  = Outstanding.emplace_back();
			record.Context     = context;
			record.Domain      = aDomain;
			return &record;
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
					lane->post([gate, stagedTerminal = std::move(staged)]() mutable {
						deliver_terminal(gate, std::move(stagedTerminal));
					});
				});
				++ClassificationSubmissions;
				if (WorkObserver) {
					core::insights_work_sample sample;
					sample.Kind = core::insights_work_kind::syntax_miss;
					WorkObserver->record_work(sample);
				}
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
			if (WorkObserver) {
				core::insights_work_sample sample;
				sample.Kind = core::insights_work_kind::syntax_eviction;
				WorkObserver->record_work(sample);
			}
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
				if (WorkObserver) {
					core::insights_work_sample sample;
					sample.Kind = core::insights_work_kind::syntax_hit;
					WorkObserver->record_work(sample);
				}
				publish_spans(local_context(), aKey, entry->Spans, true);
				return;
			}
			if (classification_in_flight(aKey)) {
				++CoalescedClassifications;
				return;
			}
			submit_classification(std::move(aKey));
		}

		/// \brief Retires this window's outstanding work without disturbing its cache.
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

		template <typename Callback>
		void deliver_projection(Callback&& aCallback)
		{
			try {
				std::forward<Callback>(aCallback)();
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		syntax_classification_port&             Classifier;
		effect_executor&                        Effects;
		decision_executor&                      DecisionLane;
		appearance_projection_port&             Projections;
		core::application_session_id            Session;
		appearance_host_limits                  Limits;
		std::shared_ptr<completion_gate>        Gate;
		std::shared_ptr<insights_observer>      WorkObserver;
		std::vector<cache_entry>                Cache;
		std::vector<outstanding_request>        Outstanding;
		std::deque<deferred_appearance_ingress> DeferredIngress;
		std::uint64_t                           NextRequest               = 1U;
		std::uint64_t                           CacheSequence             = 0U;
		std::size_t                             CacheHits                 = 0U;
		std::size_t                             ClassificationSubmissions = 0U;
		std::size_t                             CoalescedClassifications  = 0U;
		std::size_t                             ClassificationFailures    = 0U;
		std::size_t                             ClassificationTerminals   = 0U;
		std::size_t                             CacheEvictions            = 0U;
		std::size_t                             RejectedTerminals         = 0U;
		appearance_terminal_rejection           LastRejection             = appearance_terminal_rejection::none;
		bool                                    Closing                   = false;
		bool                                    TurnActive                = false;
		bool                                    DeferredIngressScheduled  = false;
		bool                                    ProjectionDeliveryFailed  = false;
	};
	void syntax_classification_application_host::observe_work(std::shared_ptr<insights_observer> aObserver) { Implementation->WorkObserver = std::move(aObserver); }

	syntax_classification_application_host::syntax_classification_application_host(
		syntax_classification_port& aClassification, effect_executor& aEffects, decision_executor& aDecisionLane,
		appearance_projection_port& aProjections, core::application_session_id aSession, appearance_host_limits aLimits)
		: Implementation{ std::make_unique<implementation>(aClassification, aEffects, aDecisionLane, aProjections, aSession, aLimits) }
	{ }
	syntax_classification_application_host::~syntax_classification_application_host() { Implementation->shutdown_now(); }

	void syntax_classification_application_host::classify(token_span_cache_key aKey)
	{
		Implementation->enqueue(
			deferred_appearance_classify{ .Key = std::move(aKey) });
	}

	void syntax_classification_application_host::cancel()
	{
		Implementation->enqueue(deferred_appearance_cancel{ });
	}

	void syntax_classification_application_host::shutdown()
	{
		Implementation->enqueue(deferred_appearance_shutdown{ });
	}

	auto syntax_classification_application_host::cached(const token_span_cache_key& aKey) const
		-> shared_token_spans
	{
		return Implementation->retained(aKey);
	}

	auto syntax_classification_application_host::last_rejection() const noexcept
		-> appearance_terminal_rejection
	{
		return Implementation->LastRejection;
	}

	auto syntax_classification_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto syntax_classification_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Outstanding.empty()
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->TurnActive;
	}

	auto syntax_classification_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
	auto syntax_classification_application_host::cached_classifications() const noexcept -> std::size_t { return Implementation->Cache.size(); }
	auto syntax_classification_application_host::cache_hits() const noexcept -> std::size_t { return Implementation->CacheHits; }
	auto syntax_classification_application_host::classification_submissions() const noexcept -> std::size_t { return Implementation->ClassificationSubmissions; }
	auto syntax_classification_application_host::coalesced_classifications() const noexcept -> std::size_t { return Implementation->CoalescedClassifications; }
	auto syntax_classification_application_host::classification_failures() const noexcept -> std::size_t { return Implementation->ClassificationFailures; }
	auto syntax_classification_application_host::classification_terminals() const noexcept -> std::size_t { return Implementation->ClassificationTerminals; }
	auto syntax_classification_application_host::cache_evictions() const noexcept -> std::size_t { return Implementation->CacheEvictions; }
	auto syntax_classification_application_host::outstanding_requests() const noexcept -> std::size_t { return Implementation->Outstanding.size(); }
	auto syntax_classification_application_host::rejected_terminals() const noexcept -> std::size_t { return Implementation->RejectedTerminals; }
}
