#include "bpr/application/diff_application_host.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		/// \brief A prepared comparison stopped before it finished, kept so the
		/// next preparation slice continues it. [[BPR-1366]]
		struct paused_preparation
		{
			prepared_diff_request                    Request;
			std::optional<std::vector<std::uint8_t>> Left;
			std::optional<std::vector<std::uint8_t>> Right;
			/// The engine's progress; empty when it stopped before computing.
			core::paused_diff_compute State;
			/// A computation that finished and still needs its semantic index.
			std::shared_ptr<const core::computed_file_diff> Computed;
			/// What it holds, counted inside the prepared share.
			std::uint64_t Bytes = 0U;
		};

		struct staged_diff_completion
		{
			core::diff_request_context                       Context;
			core::diff_options                               Options;
			core::diff_reason_code                           Reason = core::diff_reason_code::host_failure;
			std::shared_ptr<const core::computed_file_diff>  Result;
			std::shared_ptr<const core::diff_semantic_index> Index;
			bool                                             CacheHit = false;
			/// [[BPR-1366]] A prepared comparison that paused instead of
			/// finishing.
			std::shared_ptr<paused_preparation> Paused;
		};

		struct diff_job
		{
			core::compute_file_diff_request Request;
			std::stop_source                Stop;
			/// [[BPR-1344]]. A prepared comparison, not one the reviewer asked for.
			bool Prepared = false;
			/// [[BPR-1366]] Decision lane only: the comparison is no longer
			/// wanted, so a pause is not kept. A stop without it is a yield to
			/// the reviewer.
			bool Superseded = false;
			/// [[BPR-1366]] Progress to continue: a paused preparation, or one
			/// the reviewer adopted. Set before the job is posted and taken by
			/// the worker.
			std::shared_ptr<paused_preparation> Resume;
		};

		/// \brief The engine's stop predicate over a job's own stop token.
		/// [[BPR-1366]]
		class stop_token_check final : public core::diff_compute_stop
		{
		public:
			explicit stop_token_check(std::stop_token aToken) noexcept
				: Token{ std::move(aToken) }
			{
			}

			[[nodiscard]] auto stop_requested(std::uint64_t /*aSteps*/) -> bool override
			{
				return Token.stop_requested();
			}

		private:
			std::stop_token Token;
		};

		struct deferred_diff_request
		{
			core::file_comparison_key Key;
			core::diff_options        Options;
		};

		struct deferred_diff_cancel
		{
		};

		struct deferred_diff_invalidation
		{
			core::pull_request_authority Authority;
		};

		struct deferred_diff_epoch_invalidation
		{
			core::review_epoch_id Epoch;
		};

		struct deferred_diff_shutdown
		{
		};

		struct deferred_diff_preparation
		{
			std::vector<prepared_diff_request> Requests;
		};

		using deferred_diff_ingress = std::variant<deferred_diff_request,
												   deferred_diff_cancel,
												   deferred_diff_invalidation,
												   deferred_diff_epoch_invalidation,
												   deferred_diff_shutdown,
												   deferred_diff_preparation>;

		[[nodiscard]] auto validated_limits(const diff_host_limits aLimits)
			-> diff_host_limits
		{
			if (!valid(aLimits)) {
				throw std::invalid_argument{ "Invalid diff host limits" };
			}
			return aLimits;
		}

		[[nodiscard]] auto validated_compute_limits(
			const core::diff_compute_limits aLimits) -> core::diff_compute_limits
		{
			constexpr auto maximum = core::diff_compute_limits{ };
			if (aLimits.MaximumBytesPerSide == 0U
				|| aLimits.MaximumBytesPerSide > maximum.MaximumBytesPerSide
				|| aLimits.MaximumLinesPerSide == 0U
				|| aLimits.MaximumLinesPerSide > maximum.MaximumLinesPerSide
				|| aLimits.MaximumMatrixCells == 0U
				|| aLimits.MaximumMatrixCells > maximum.MaximumMatrixCells
				|| aLimits.MaximumMoveDetectionWork == 0U
				|| aLimits.MaximumMoveDetectionWork
					> maximum.MaximumMoveDetectionWork
				|| aLimits.MaximumSegments == 0U
				|| aLimits.MaximumSegments > maximum.MaximumSegments) {
				throw std::invalid_argument{ "Invalid diff compute limits" };
			}
			return aLimits;
		}

		[[nodiscard]] constexpr auto reason_for_blob_status(
			const diff_blob_status aStatus,
			const core::diff_side  aSide) noexcept -> core::diff_reason_code
		{
			switch (aStatus) {
			case diff_blob_status::available:
				return core::diff_reason_code::none;
			case diff_blob_status::missing:
				return aSide == core::diff_side::left
					? core::diff_reason_code::missing_left_blob
					: core::diff_reason_code::missing_right_blob;
			case diff_blob_status::binary:
				return core::diff_reason_code::binary_file;
			case diff_blob_status::decode_failure:
				return core::diff_reason_code::decode_failure;
			case diff_blob_status::materialization_limit:
				return core::diff_reason_code::materialization_limit;
			case diff_blob_status::generated_file_unsupported:
				return core::diff_reason_code::generated_file_unsupported;
			case diff_blob_status::cancelled:
				return core::diff_reason_code::cancelled;
			case diff_blob_status::failure:
				return core::diff_reason_code::host_failure;
			}
			return core::diff_reason_code::host_failure;
		}

		[[nodiscard]] auto exact_blob_observation(
			const diff_blob_request&     aRequest,
			const diff_blob_observation& aObservation) noexcept -> bool
		{
			return aObservation.Context == aRequest.Context
				&& aObservation.Side == aRequest.Side
				&& aObservation.Blob == aRequest.Blob
				&& (aObservation.Status == diff_blob_status::available
					|| aObservation.Bytes.empty());
		}

		[[nodiscard]] auto validate_staged(
			const core::compute_file_diff_request& aRequest,
			const core::computed_file_diff&        aResult,
			const core::diff_semantic_index&       aIndex) noexcept -> bool
		{
			const auto digest = core::options_digest(aRequest.Options);
			return core::valid(aRequest.Context)
				&& aRequest.Context.Key.Algorithm
				== core::supported_diff_algorithm_revision
				&& digest.has_value() && *digest == aRequest.Context.Key.Options
				&& aResult.Alignment.Key == aRequest.Context.Key
				&& aResult.Alignment.Options == aRequest.Options
				&& aResult.Alignment.Digest
				== core::comparison_digest(aRequest.Context.Key)
				&& aIndex.matches_result(aResult);
		}

		[[nodiscard]] auto checked_add(std::uint64_t&      aTotal,
									   const std::uint64_t aAmount) noexcept -> bool
		{
			if (aAmount > std::numeric_limits<std::uint64_t>::max() - aTotal) {
				return false;
			}
			aTotal += aAmount;
			return true;
		}

		template <typename Value>
		[[nodiscard]] auto add_vector_bytes(
			std::uint64_t& aTotal, const std::vector<Value>& aValues) noexcept -> bool
		{
			const auto count = static_cast<std::uint64_t>(aValues.capacity());
			if (count > std::numeric_limits<std::uint64_t>::max() / sizeof(Value)) {
				return false;
			}
			return checked_add(aTotal, count * sizeof(Value));
		}

		[[nodiscard]] auto retained_result_bytes(
			const core::computed_file_diff& aResult) noexcept
			-> std::optional<std::uint64_t>
		{
			auto total = static_cast<std::uint64_t>(sizeof(aResult));
			if (!checked_add(total, sizeof(core::file_comparison_key))
				|| !checked_add(
					total,
					sizeof(std::shared_ptr<const core::computed_file_diff>))
				|| !checked_add(total, sizeof(std::uint64_t))
				|| !add_vector_bytes(total, aResult.Alignment.Segments)
				|| !add_vector_bytes(total, aResult.Alignment.Pairs)
				|| !add_vector_bytes(total, aResult.Alignment.Groups)
				|| !add_vector_bytes(total, aResult.Alignment.Moves)
				|| !add_vector_bytes(total, aResult.Intraline)) {
				return std::nullopt;
			}
			const auto addDocument = [&total](
										 const std::optional<
											 core::decoded_text_document>& aDocument) {
				return !aDocument.has_value()
					|| (add_vector_bytes(total, aDocument->Bytes)
						&& add_vector_bytes(total, aDocument->Lines));
			};
			if (!addDocument(aResult.LeftDocument)
				|| !addDocument(aResult.RightDocument)) {
				return std::nullopt;
			}
			return total;
		}

		/// \brief Bytes a paused preparation holds: its copies of both sides,
		/// the engine's progress or the finished result. [[BPR-1366]]
		[[nodiscard]] auto paused_preparation_bytes(const paused_preparation& aPaused) noexcept
			-> std::optional<std::uint64_t>
		{
			auto total = static_cast<std::uint64_t>(sizeof(aPaused));
			if ((aPaused.Left.has_value() && !add_vector_bytes(total, *aPaused.Left))
				|| (aPaused.Right.has_value() && !add_vector_bytes(total, *aPaused.Right))
				|| !checked_add(total, aPaused.State.retained_bytes())) {
				return std::nullopt;
			}
			if (aPaused.Computed) {
				const auto computed = retained_result_bytes(*aPaused.Computed);
				if (!computed || !checked_add(total, *computed)) {
					return std::nullopt;
				}
			}
			return total;
		}
	} // namespace

	struct diff_application_host::implementation
	{
		struct completion_gate
		{
			std::atomic<implementation*> Host = nullptr;
		};

		struct cache_entry
		{
			core::file_comparison_key                        Key;
			std::shared_ptr<const core::computed_file_diff>  Result;
			std::shared_ptr<const core::diff_semantic_index> Index;
			std::uint64_t                                    Bytes = 0U;
			/// [[BPR-1327]]. Computed ahead of the reviewer and not yet opened.
			bool Prepared = false;
		};

		implementation(core::diff_state                aInitialState,
					   diff_blob_port&                 aBlobs,
					   effect_executor&                aEffects,
					   decision_executor&              aDecisionLane,
					   diff_projection_port&           aProjections,
					   const diff_host_limits          aLimits,
					   const core::diff_compute_limits aComputeLimits)
			: State{ std::move(aInitialState) }
			, Blobs{ aBlobs }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, Limits{ validated_limits(aLimits) }
			, ComputeLimits{ validated_compute_limits(aComputeLimits) }
			, Gate{ std::make_shared<completion_gate>() }
		{
			Gate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			Gate->Host.store(nullptr, std::memory_order_release);
		}

		void dispatch(const core::diff_event& aEvent)
		{
			const auto output = core::transition(State, aEvent);
			for (const auto& request : output.Requests) {
				std::visit(
					[this](const auto& aTypedRequest) {
						using request_type = std::decay_t<decltype(aTypedRequest)>;
						if constexpr (std::is_same_v<
										  request_type,
										  core::compute_file_diff_request>) {
							execute(aTypedRequest);
						} else {
							stop(aTypedRequest.Context);
						}
					},
					request);
			}
			if (output.Terminal.has_value()
				&& output.Terminal->Reason != core::diff_reason_code::none
				&& !Closing) {
				publish_failure(output.Terminal->Context,
								output.Terminal->Reason);
			}
		}

		void request(core::file_comparison_key aKey, core::diff_options aOptions)
		{
			enqueue(deferred_diff_request{
				.Key     = std::move(aKey),
				.Options = aOptions,
			});
		}

		void request_now(core::file_comparison_key aKey,
						 const core::diff_options  aOptions)
		{
			if (Closing) {
				return;
			}
			dispatch(core::request_file_diff_intent{
				.Key     = std::move(aKey),
				.Options = aOptions,
			});
		}

		void cancel()
		{
			enqueue(deferred_diff_cancel{ });
		}

		void cancel_now()
		{
			if (!Closing) {
				dispatch(core::cancel_file_diff_intent{ });
			}
		}

		void invalidate(const core::pull_request_authority& aAuthority)
		{
			enqueue(deferred_diff_invalidation{ aAuthority });
		}

		void invalidate_now(const core::pull_request_authority& aAuthority)
		{
			if (Closing) {
				return;
			}
			dispatch(core::invalidate_diff_authority_intent{ aAuthority });
			for (const auto& job : Jobs) {
				if (job->Request.Context.Key.Authority == aAuthority) {
					job->Stop.request_stop();
				}
			}
			discard_preparation([&aAuthority](const core::file_comparison_key& aKey) {
				return aKey.Authority == aAuthority;
			});
			auto current = Cache.begin();
			while (current != Cache.end()) {
				if (current->Key.Authority != aAuthority) {
					++current;
					continue;
				}
				CachedBytes -= current->Bytes;
				current = Cache.erase(current);
			}
			deliver_projection(
				[this, &aAuthority] { Projections.invalidate(aAuthority); });
		}

		void invalidate(const core::review_epoch_id& aEpoch)
		{
			enqueue(deferred_diff_epoch_invalidation{ aEpoch });
		}

		void invalidate_now(const core::review_epoch_id& aEpoch)
		{
			if (Closing) {
				return;
			}
			// A newer pending selection owns the projection even when Current
			// still names a previously published comparison from this authority.
			const auto visible = State.Pending ? State.Pending : State.Current;
			dispatch(core::invalidate_diff_epoch_intent{ aEpoch });
			for (const auto& job : Jobs) {
				if (core::uses_epoch(job->Request.Context.Key, aEpoch)) {
					job->Stop.request_stop();
				}
			}
			discard_preparation([&aEpoch](const core::file_comparison_key& aKey) {
				return core::uses_epoch(aKey, aEpoch);
			});
			std::erase_if(Cache, [this, &aEpoch](const cache_entry& aEntry) {
				if (!core::uses_epoch(aEntry.Key, aEpoch)) {
					return false;
				}
				CachedBytes -= aEntry.Bytes;
				return true;
			});
			if (visible && core::uses_epoch(visible->Key, aEpoch)) {
				deliver_projection([this, &visible] { Projections.invalidate(visible->Key.Authority); });
			}
		}

		void shutdown()
		{
			enqueue(deferred_diff_shutdown{ });
		}

		void shutdown_now()
		{
			if (Closing) {
				return;
			}
			Closing           = true;
			const auto output = core::transition(State, core::cancel_file_diff_intent{ });
			for (const auto& request : output.Requests) {
				if (const auto* cancel = std::get_if<core::cancel_file_diff_request>(&request)) {
					stop(cancel->Context);
				}
			}
			for (const auto& job : Jobs) {
				job->Stop.request_stop();
			}
			discard_preparation([](const core::file_comparison_key&) { return true; });
			Cache.clear();
			CachedBytes = 0U;
		}

		void prepare(std::vector<prepared_diff_request> aRequests)
		{
			enqueue(deferred_diff_preparation{ std::move(aRequests) });
		}

		/// \brief Replaces the preparation queue. [[BPR-1327]]
		void prepare_now(std::vector<prepared_diff_request> aRequests)
		{
			if (Closing) {
				return;
			}
			PreparedQueue.clear();
			const auto capacity = prepared_entry_capacity(Limits);
			for (auto& request : aRequests) {
				if (PreparedQueue.size() >= capacity) {
					break;
				}
				if (!preparable(request)
					|| std::ranges::find(PreparedQueue, request.Key, &prepared_diff_request::Key)
						!= PreparedQueue.end()) {
					continue;
				}
				PreparedQueue.push_back(std::move(request));
			}
			if (PreparedJob) {
				const auto& key    = PreparedJob->Request.Context.Key;
				const auto  wanted = std::ranges::find(PreparedQueue, key, &prepared_diff_request::Key)
					!= PreparedQueue.end();
				if (!wanted && !(Adopted && Adopted->Context.Key == key)) {
					PreparedJob->Superseded = true;
					PreparedJob->Stop.request_stop();
				}
				std::erase_if(PreparedQueue, [&key](const prepared_diff_request& aRequest) {
					return aRequest.Key == key;
				});
			}
			// [[BPR-1366]] A paused comparison the new window still wants is
			// continued ahead of it; any other is discarded.
			std::erase_if(Paused, [this](const std::shared_ptr<paused_preparation>& aPaused) {
				const auto wanted = std::ranges::find(PreparedQueue, aPaused->Request.Key, &prepared_diff_request::Key);
				if (wanted == PreparedQueue.end()) {
					PausedBytes -= aPaused->Bytes;
					return true;
				}
				PreparedQueue.erase(wanted);
				return false;
			});
			pump_preparation();
		}

		[[nodiscard]] auto preparable(const prepared_diff_request& aRequest) const -> bool
		{
			const auto digest = core::options_digest(aRequest.Options);
			return core::valid(aRequest.Key) && digest.has_value() && *digest == aRequest.Key.Options
				&& aRequest.Key.Algorithm == core::supported_diff_algorithm_revision
				&& State.ApplicationSession.value() != 0U
				&& std::ranges::find(Cache, aRequest.Key, &cache_entry::Key) == Cache.end()
				&& std::ranges::none_of(Jobs, [&aRequest](const auto& aJob) {
					   return aJob->Request.Context.Key == aRequest.Key;
				   });
		}

		template <typename Predicate>
		void discard_preparation(Predicate&& aPredicate)
		{
			std::erase_if(PreparedQueue, [&aPredicate](const prepared_diff_request& aRequest) {
				return aPredicate(aRequest.Key);
			});
			discard_paused([&aPredicate](const paused_preparation& aPaused) {
				return aPredicate(aPaused.Request.Key);
			});
			if (PreparedJob && aPredicate(PreparedJob->Request.Context.Key)) {
				PreparedJob->Superseded = true;
				PreparedJob->Stop.request_stop();
			}
		}

		/// [[BPR-1366]] Discards the paused preparations aPredicate names. A
		/// discarded one is computed again from the start if it is wanted
		/// later; it is never a wrong result.
		template <typename Predicate>
		void discard_paused(Predicate&& aPredicate)
		{
			std::erase_if(Paused, [this, &aPredicate](const std::shared_ptr<paused_preparation>& aPaused) {
				if (!aPredicate(*aPaused)) {
					return false;
				}
				PausedBytes -= aPaused->Bytes;
				return true;
			});
		}

		/// [[BPR-1366]] Takes the paused preparation of aKey, if one is kept.
		[[nodiscard]] auto take_paused(const core::file_comparison_key& aKey) -> std::shared_ptr<paused_preparation>
		{
			const auto found = std::ranges::find_if(Paused, [&aKey](const std::shared_ptr<paused_preparation>& aPaused) {
				return aPaused->Request.Key == aKey;
			});
			if (found == Paused.end()) {
				return nullptr;
			}
			auto paused = std::move(*found);
			Paused.erase(found);
			PausedBytes -= paused->Bytes;
			return paused;
		}

		/// \brief Keeps a paused preparation, first in line to continue, while
		/// it fits the prepared share. [[BPR-1366]]
		///
		/// \details At most the prepared entry capacity are kept, and their
		/// bytes count with the prepared comparisons inside the prepared byte
		/// share and the cache's bytes. One that does not fit is discarded.
		void keep_paused(std::shared_ptr<paused_preparation> aPaused)
		{
			const auto bytes = paused_preparation_bytes(*aPaused);
			if (!bytes || !preparable(aPaused->Request) || prepared_entry_capacity(Limits) == 0U) {
				return;
			}
			aPaused->Bytes = *bytes;
			discard_paused([&aPaused](const paused_preparation& aKept) {
				return aKept.Request.Key == aPaused->Request.Key;
			});
			while (Paused.size() >= prepared_entry_capacity(Limits)) {
				PausedBytes -= Paused.back()->Bytes;
				Paused.pop_back();
			}
			const auto share = prepared_byte_capacity(Limits);
			const auto held  = prepared_total_bytes() + PausedBytes;
			if (held > share || *bytes > share - held || CachedBytes + PausedBytes > Limits.CacheBytes
				|| *bytes > Limits.CacheBytes - CachedBytes - PausedBytes) {
				return;
			}
			std::erase_if(PreparedQueue, [&aPaused](const prepared_diff_request& aRequest) {
				return aRequest.Key == aPaused->Request.Key;
			});
			PausedBytes += *bytes;
			Paused.push_front(std::move(aPaused));
		}

		[[nodiscard]] auto prepared_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(Cache, &cache_entry::Prepared));
		}

		[[nodiscard]] auto prepared_total_bytes() const noexcept -> std::uint64_t
		{
			auto total = std::uint64_t{ 0U };
			for (const auto& entry : Cache) {
				total += entry.Prepared ? entry.Bytes : 0U;
			}
			return total;
		}

		/// Whether a prepared comparison could be stored without evicting one
		/// the reviewer opened. A guess before the size is known; storage
		/// decides exactly.
		[[nodiscard]] auto preparation_room() const noexcept -> bool
		{
			const auto prepared = prepared_count();
			return prepared_entry_capacity(Limits) > 0U
				&& Cache.size() - prepared < Limits.CacheEntries
				&& CachedBytes - prepared_total_bytes() < Limits.CacheBytes;
		}

		/// Starts the next prepared comparison when no requested one is
		/// outstanding. At most one is ever computing.
		void pump_preparation()
		{
			while (!Closing && !PreparedJob && !Adopted && (!Paused.empty() || !PreparedQueue.empty()) && Jobs.empty()
				   && PendingDecisionCompletions == 0U) {
				// [[BPR-1366]] A paused comparison continues before new work,
				// so a large file is not starved by later ones.
				if (!Paused.empty()) {
					auto paused = std::move(Paused.front());
					Paused.pop_front();
					PausedBytes -= paused->Bytes;
					if (!preparable(paused->Request) || !preparation_room()) {
						continue;
					}
					auto request = paused->Request;
					start_preparation(std::move(request), std::move(paused));
					continue;
				}
				auto next = std::move(PreparedQueue.front());
				PreparedQueue.pop_front();
				if (!preparable(next) || !preparation_room()) {
					continue;
				}
				start_preparation(std::move(next));
			}
		}

		void start_preparation(prepared_diff_request aRequest, std::shared_ptr<paused_preparation> aResume = nullptr)
		{
			if (PreparedSequence == std::numeric_limits<std::uint64_t>::max()) {
				return;
			}
			auto job = std::shared_ptr<diff_job>{ };
			try {
				job          = std::make_shared<diff_job>();
				job->Request = core::compute_file_diff_request{
					.Context = core::diff_request_context{
						.Request            = core::request_id{ ++PreparedSequence },
						.ApplicationSession = State.ApplicationSession,
						.Key                = std::move(aRequest.Key),
						.Generation         = core::request_generation{ 1U },
					},
					.Options = aRequest.Options,
				};
				job->Prepared = true;
				job->Resume   = std::move(aResume);
			} catch (...) {
				return;
			}
			PreparedJob               = job;
			const auto  gate          = std::weak_ptr<completion_gate>{ Gate };
			auto* const blobs         = &Blobs;
			auto* const lane          = &DecisionLane;
			const auto  computeLimits = ComputeLimits;
			// A prepared comparison never reads a side larger than its share of
			// the cache could hold, so preparing cannot raise the transient
			// footprint past what the prepared share already allows.
			const auto materializeLimit = std::min<std::uint64_t>(
				computeLimits.MaximumBytesPerSide, prepared_byte_capacity(Limits) / 4U);
			try {
				Effects.post([job, gate, blobs, lane, computeLimits, materializeLimit] {
					auto completion = perform(job, *blobs, computeLimits, materializeLimit);
					lane->post([gate, stagedCompletion = std::move(completion)]() mutable {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						auto* const host = locked->Host.load(std::memory_order_acquire);
						if (host != nullptr) {
							host->accept_preparation(std::move(stagedCompletion));
						}
					});
				});
			} catch (...) {
				PreparedJob.reset();
			}
		}

		void accept_preparation(staged_diff_completion aCompletion)
		{
			if (TurnActive) {
				finish_preparation(std::move(aCompletion));
				return;
			}
			run_turn(
				[this, completion = std::move(aCompletion)]() mutable {
					finish_preparation(std::move(completion));
				});
		}

		void finish_preparation(staged_diff_completion aCompletion)
		{
			if (!PreparedJob || PreparedJob->Request.Context != aCompletion.Context) {
				return;
			}
			// [[BPR-1366]] A comparison stopped because it is no longer wanted
			// is discarded. One stopped so the reviewer could go first keeps
			// its progress, or its result when it finished regardless.
			const auto discarded = PreparedJob->Superseded;
			PreparedJob.reset();
			if (!Closing && !discarded && aCompletion.Paused) {
				try {
					keep_paused(std::move(aCompletion.Paused));
				} catch (...) {
				}
			} else if (!Closing && !discarded && aCompletion.Reason == core::diff_reason_code::none
					   && aCompletion.Result && aCompletion.Index
					   && validate_staged(
						   core::compute_file_diff_request{
							   .Context = aCompletion.Context,
							   .Options = aCompletion.Options,
						   },
						   *aCompletion.Result, *aCompletion.Index)) {
				try {
					store_prepared(aCompletion.Result, aCompletion.Index);
				} catch (...) {
				}
			}
			if (Adopted) {
				auto request = std::move(*Adopted);
				Adopted.reset();
				if (!Closing) {
					// The reviewer asked for this comparison while it was being
					// prepared. Answer from the cache when preparation stored
					// it; otherwise compute it as an ordinary request, which
					// continues a paused preparation of it ([[BPR-1366]]).
					if (const auto cached = lookup(request); cached.has_value()) {
						PreparedHits += cached->Prepared ? 1U : 0U;
						post_local(staged_diff_completion{
							.Context  = request.Context,
							.Options  = request.Options,
							.Reason   = core::diff_reason_code::none,
							.Result   = cached->Result,
							.Index    = cached->Index,
							.CacheHit = !cached->Prepared,
						});
					} else {
						execute(request);
					}
				}
			}
			pump_preparation();
		}

		void enqueue(deferred_diff_ingress aIngress)
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
			run_turn(
				[this, ingress = std::move(aIngress)]() mutable {
					process(std::move(ingress));
				});
		}

		void process(deferred_diff_ingress aIngress)
		{
			std::visit(
				[this](auto&& aTypedIngress) {
					using ingress_type = std::decay_t<decltype(aTypedIngress)>;
					if constexpr (std::is_same_v<ingress_type,
												 deferred_diff_request>) {
						request_now(std::move(aTypedIngress.Key),
									aTypedIngress.Options);
					} else if constexpr (std::is_same_v<ingress_type,
														deferred_diff_cancel>) {
						cancel_now();
					} else if constexpr (std::is_same_v<
											 ingress_type,
											 deferred_diff_invalidation>) {
						invalidate_now(aTypedIngress.Authority);
					} else if constexpr (std::is_same_v<ingress_type, deferred_diff_epoch_invalidation>) {
						invalidate_now(aTypedIngress.Epoch);
					} else if constexpr (std::is_same_v<ingress_type, deferred_diff_preparation>) {
						prepare_now(std::move(aTypedIngress.Requests));
					} else {
						shutdown_now();
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
					const auto locked = gate.lock();
					if (!locked) {
						return;
					}
					auto* const host = locked->Host.load(std::memory_order_acquire);
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

		template <typename Callback>
		void deliver_projection(Callback&& aCallback)
		{
			try {
				std::forward<Callback>(aCallback)();
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void accept_completion(staged_diff_completion aCompletion)
		{
			if (TurnActive) {
				finish(std::move(aCompletion));
				return;
			}
			run_turn(
				[this, completion = std::move(aCompletion)]() mutable {
					finish(std::move(completion));
				});
		}

		void execute(const core::compute_file_diff_request& aRequest)
		{
			if (pending_work() >= Limits.OutstandingJobs) {
				finish(staged_diff_completion{
					.Context = aRequest.Context,
					.Options = aRequest.Options,
					.Reason  = core::diff_reason_code::compute_limit,
				});
				return;
			}
			if (const auto cached = lookup(aRequest); cached.has_value()) {
				PreparedHits += cached->Prepared ? 1U : 0U;
				// [[BPR-1327]]. A prepared comparison is the reviewer's first
				// look at the file, not a return to one: it is not presented
				// as cached.
				post_local(staged_diff_completion{
					.Context  = aRequest.Context,
					.Options  = aRequest.Options,
					.Reason   = core::diff_reason_code::none,
					.Result   = cached->Result,
					.Index    = cached->Index,
					.CacheHit = !cached->Prepared,
				});
				return;
			}
			// [[BPR-1327]]. The reviewer opened the file being prepared: wait
			// for that work instead of computing the same comparison twice.
			// Any other preparation yields the worker.
			// [[BPR-1366]] Also when it was stopped to let the reviewer go first:
			// it pauses within milliseconds and the reviewer continues it.
			if (PreparedJob && !Adopted && !PreparedJob->Superseded
				&& PreparedJob->Request.Context.Key == aRequest.Context.Key) {
				Adopted = aRequest;
				return;
			}
			if (PreparedJob) {
				PreparedJob->Stop.request_stop();
			}
			auto job = std::shared_ptr<diff_job>{ };
			try {
				job          = std::make_shared<diff_job>();
				job->Request = aRequest;
				// [[BPR-1366]] The result is the same, so the reviewer may
				// continue a paused preparation of this comparison. Only the
				// reviewer's own cancellation can stop it from here on.
				job->Resume = take_paused(aRequest.Context.Key);
				Jobs.push_back(job);
			} catch (...) {
				finish(staged_diff_completion{
					.Context = aRequest.Context,
					.Options = aRequest.Options,
					.Reason  = core::diff_reason_code::host_failure,
				});
				return;
			}
			const auto  gate          = std::weak_ptr<completion_gate>{ Gate };
			auto* const blobs         = &Blobs;
			auto* const lane          = &DecisionLane;
			const auto  computeLimits = ComputeLimits;
			try {
				Effects.post([job, gate, blobs, lane, computeLimits] {
					auto completion = perform(job, *blobs, computeLimits, computeLimits.MaximumBytesPerSide);
					lane->post([gate, stagedCompletion = std::move(completion)]() mutable {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						auto* const host = locked->Host.load(std::memory_order_acquire);
						if (host != nullptr) {
							host->accept_completion(std::move(stagedCompletion));
						}
					});
				});
			} catch (...) {
				erase_job(aRequest.Context);
				post_local(staged_diff_completion{
					.Context = aRequest.Context,
					.Options = aRequest.Options,
					.Reason  = core::diff_reason_code::host_failure,
				});
			}
		}

		/// The read of one side of a job's comparison.
		[[nodiscard]] static auto blob_request(const diff_job& aJob, const core::diff_side aSide,
											   const std::uint64_t aMaterializeLimit) -> diff_blob_request
		{
			const auto& key = aJob.Request.Context.Key;
			return {
				.Context      = aJob.Request.Context,
				.Side         = aSide,
				.Blob         = aSide == core::diff_side::left ? *key.LeftBlob : *key.RightBlob,
				.MaximumBytes = aMaterializeLimit,
				.Prepared     = aJob.Prepared,
			};
		}

		/// Reads one side of a job's comparison and checks the answer. Returns
		/// the bytes, or sets aCompletion's reason and returns nothing.
		[[nodiscard]] static auto read_side(const diff_job& aJob, const core::diff_side aSide,
											const std::uint64_t                  aMaterializeLimit,
											std::optional<diff_blob_observation> aObservation,
											diff_blob_port& aBlobs, const std::stop_token& aStop,
											staged_diff_completion& aCompletion) -> std::optional<std::vector<std::uint8_t>>
		{
			const auto request     = blob_request(aJob, aSide, aMaterializeLimit);
			auto       observation = aObservation ? std::move(*aObservation) : aBlobs.materialize(request, aStop);
			if (!exact_blob_observation(request, observation)) {
				aCompletion.Reason = core::diff_reason_code::host_failure;
				return std::nullopt;
			}
			aCompletion.Reason = reason_for_blob_status(observation.Status, request.Side);
			if (aCompletion.Reason != core::diff_reason_code::none) {
				return std::nullopt;
			}
			if (std::cmp_greater(observation.Bytes.size(), request.MaximumBytes)) {
				aCompletion.Reason = core::diff_reason_code::materialization_limit;
				return std::nullopt;
			}
			return std::move(observation.Bytes);
		}

		[[nodiscard]] static auto perform(
			const std::shared_ptr<diff_job>& aJob,
			diff_blob_port&                  aBlobs,
			const core::diff_compute_limits& aLimits,
			const std::uint64_t              aMaterializeLimit) -> staged_diff_completion
		{
			auto completion = staged_diff_completion{
				.Context = aJob->Request.Context,
				.Options = aJob->Request.Options,
				.Reason  = core::diff_reason_code::host_failure,
			};
			try {
				const auto stop = aJob->Stop.get_token();
				// [[BPR-1366]] Progress to continue, taken from the job; a
				// prepared job that stops hands it (or what it has added to it)
				// back. A reviewer job never does: its stop is its own cancel.
				auto       resume   = std::move(aJob->Resume);
				const auto prepared = aJob->Prepared;
				const auto pause    = [&completion, prepared](std::shared_ptr<paused_preparation> aPaused) {
					completion.Reason = core::diff_reason_code::cancelled;
					completion.Result.reset();
					completion.Index.reset();
					if (prepared) {
						completion.Paused = std::move(aPaused);
					}
					return completion;
				};
				if (stop.stop_requested()) {
					return pause(std::move(resume));
				}
				const auto& key = aJob->Request.Context.Key;
				if (!resume) {
					auto left  = std::optional<std::vector<std::uint8_t>>{ };
					auto right = std::optional<std::vector<std::uint8_t>>{ };
					// [[BPR-1360]]. A modified file's two sides are asked for
					// together, so a port that can overlap their reads makes the
					// open wait for the longer read, not both. Each answer is then
					// checked in order, left first, exactly as one read would be.
					auto both = std::optional<std::pair<diff_blob_observation, diff_blob_observation>>{ };
					if (key.LeftBlob.has_value() && key.RightBlob.has_value()) {
						both = aBlobs.materialize_both(blob_request(*aJob, core::diff_side::left, aMaterializeLimit),
													   blob_request(*aJob, core::diff_side::right, aMaterializeLimit), stop);
					}
					if (key.LeftBlob.has_value()) {
						left = read_side(*aJob, core::diff_side::left, aMaterializeLimit,
										 both ? std::optional{ std::move(both->first) } : std::nullopt, aBlobs, stop, completion);
						if (!left) {
							return completion;
						}
					}
					if (stop.stop_requested()) {
						completion.Reason = core::diff_reason_code::cancelled;
						return completion;
					}
					if (key.RightBlob.has_value()) {
						right = read_side(*aJob, core::diff_side::right, aMaterializeLimit,
										  both ? std::optional{ std::move(both->second) } : std::nullopt, aBlobs, stop, completion);
						if (!right) {
							return completion;
						}
					}
					resume          = std::make_shared<paused_preparation>();
					resume->Request = prepared_diff_request{ .Key = key, .Options = aJob->Request.Options };
					resume->Left    = std::move(left);
					resume->Right   = std::move(right);
					if (stop.stop_requested()) {
						// [[BPR-1366]] Both sides are read: a prepared job keeps
						// them, so its next slice does not read them again.
						return pause(std::move(resume));
					}
				}
				auto check   = stop_token_check{ stop };
				auto control = core::diff_compute_control{ .Stop = &check };
				if (!resume->Computed) {
					const auto input = core::diff_compute_input{
						.Key        = key,
						.Options    = aJob->Request.Options,
						.LeftBytes  = resume->Left.has_value()
							? std::optional{ std::span<const std::uint8_t>{ *resume->Left } }
							: std::nullopt,
						.RightBytes = resume->Right.has_value()
							? std::optional{ std::span<const std::uint8_t>{ *resume->Right } }
							: std::nullopt,
						.Limits     = aLimits,
					};
					auto slice = core::compute_file_diff(input, control, std::move(resume->State));
					if (!slice.Outcome.has_value()) {
						// [[BPR-1366]] Stopped mid-computation: within the
						// engine's check interval of the request.
						resume->State = std::move(slice.Paused);
						return pause(std::move(resume));
					}
					auto& outcome = *slice.Outcome;
					if (stop.stop_requested() && !prepared) {
						completion.Reason = core::diff_reason_code::cancelled;
						return completion;
					}
					if (!outcome.Result.has_value()) {
						completion.Reason = outcome.Reason == core::diff_reason_code::none
							? core::diff_reason_code::invalid_alignment
							: outcome.Reason;
						return completion;
					}
					resume->Computed = std::make_shared<const core::computed_file_diff>(std::move(*outcome.Result));
					resume->Left.reset();
					resume->Right.reset();
				}
				completion.Result = resume->Computed;
				// [[BPR-1366]] The index validates the whole result; a stop
				// restarts it, keeping the finished computation.
				auto build = core::build_diff_semantic_index(completion.Result, control);
				if (build.Stopped) {
					return pause(std::move(resume));
				}
				auto& index = build.Index;
				if (!index || !validate_staged(aJob->Request, *completion.Result, *index)) {
					completion.Result.reset();
					completion.Reason = core::diff_reason_code::invalid_alignment;
					return completion;
				}
				if (stop.stop_requested() && !prepared) {
					completion.Result.reset();
					completion.Reason = core::diff_reason_code::cancelled;
					return completion;
				}
				completion.Index  = std::make_shared<const core::diff_semantic_index>(std::move(*index));
				completion.Reason = core::diff_reason_code::none;
				return completion;
			} catch (...) {
				completion.Result.reset();
				completion.Index.reset();
				completion.Paused.reset();
				completion.Reason = core::diff_reason_code::host_failure;
				return completion;
			}
		}

		void post_local(staged_diff_completion aCompletion)
		{
			++PendingDecisionCompletions;
			const auto gate = std::weak_ptr<completion_gate>{ Gate };
			DecisionLane.post([gate, completion = std::move(aCompletion)]() mutable {
				const auto locked = gate.lock();
				if (!locked) {
					return;
				}
				auto* const host = locked->Host.load(std::memory_order_acquire);
				if (host != nullptr) {
					--host->PendingDecisionCompletions;
					host->accept_completion(std::move(completion));
				}
			});
		}

		void finish(staged_diff_completion aCompletion)
		{
			finish_requested(std::move(aCompletion));
			pump_preparation();
		}

		void finish_requested(staged_diff_completion aCompletion)
		{
			erase_job(aCompletion.Context);
			if (aCompletion.Reason == core::diff_reason_code::none) {
				if (!aCompletion.Result || !aCompletion.Index
					|| !validate_staged(
						core::compute_file_diff_request{
							.Context = aCompletion.Context,
							.Options = aCompletion.Options,
						},
						*aCompletion.Result, *aCompletion.Index)) {
					aCompletion.Result.reset();
					aCompletion.Index.reset();
					aCompletion.Reason = core::diff_reason_code::invalid_alignment;
				}
			}

			const auto output = aCompletion.Reason == core::diff_reason_code::none
				? core::transition(State,
								   core::file_diff_computed_event{
									   .Context = aCompletion.Context,
								   })
				: core::transition(State,
								   core::file_diff_failed_event{
									   .Context = aCompletion.Context,
									   .Reason  = aCompletion.Reason,
								   });
			if (Closing || !output.Terminal.has_value()) {
				return;
			}
			if (output.Accepted.has_value() && aCompletion.Result) {
				try {
					store(aCompletion.Result, aCompletion.Index);
				} catch (...) {
					Cache.clear();
					CachedBytes = 0U;
				}
				deliver_projection([this, &aCompletion] {
					Projections.publish(diff_publication{
						.Context  = aCompletion.Context,
						.Result   = aCompletion.Result,
						.Index    = aCompletion.Index,
						.CacheHit = aCompletion.CacheHit,
					});
				});
				return;
			}
			publish_failure(output.Terminal->Context, output.Terminal->Reason);
		}

		void publish_failure(const core::diff_request_context& aContext,
							 const core::diff_reason_code      aReason)
		{
			deliver_projection([this, &aContext, aReason] {
				Projections.fail(diff_failure_publication{
					.Context = aContext,
					.Reason  = aReason,
				});
			});
		}

		[[nodiscard]] auto find_job(
			const core::diff_request_context& aContext) noexcept
		{
			return std::ranges::find_if(
				Jobs, [&aContext](const auto& aJob) {
					return aJob->Request.Context == aContext;
				});
		}

		void stop(const core::diff_request_context& aContext)
		{
			const auto found = find_job(aContext);
			if (found != Jobs.end()) {
				(*found)->Stop.request_stop();
			}
			if (Adopted && Adopted->Context == aContext) {
				const auto adopted = std::move(*Adopted);
				Adopted.reset();
				try {
					post_local(staged_diff_completion{
						.Context = adopted.Context,
						.Options = adopted.Options,
						.Reason  = core::diff_reason_code::cancelled,
					});
				} catch (...) {
					ProjectionDeliveryFailed = true;
				}
			}
		}

		[[nodiscard]] auto pending_work() const noexcept -> std::size_t
		{
			return Jobs.size() + PendingDecisionCompletions
				+ DeferredIngress.size() + (Adopted ? 1U : 0U);
		}

		void erase_job(const core::diff_request_context& aContext)
		{
			const auto found = find_job(aContext);
			if (found != Jobs.end()) {
				Jobs.erase(found);
			}
		}

		[[nodiscard]] auto lookup(const core::compute_file_diff_request& aRequest)
			-> std::optional<cache_entry>
		{
			const auto found = std::ranges::find_if(
				Cache, [&aRequest](const cache_entry& aEntry) {
					return aEntry.Key == aRequest.Context.Key;
				});
			if (found == Cache.end()) {
				return { };
			}
			if (!found->Index || !validate_staged(aRequest, *found->Result, *found->Index)) {
				CachedBytes -= found->Bytes;
				Cache.erase(found);
				return { };
			}
			return *found;
		}

		void store(const std::shared_ptr<const core::computed_file_diff>&  aResult,
				   const std::shared_ptr<const core::diff_semantic_index>& aIndex)
		{
			auto       bytes      = retained_result_bytes(*aResult);
			const auto indexBytes = aIndex->retained_bytes();
			if (!bytes || !indexBytes || !checked_add(*bytes, *indexBytes)
				|| !checked_add(*bytes, sizeof(aIndex)) || *bytes > Limits.CacheBytes) {
				return;
			}
			const auto duplicate = std::ranges::find_if(
				Cache, [&aResult](const cache_entry& aEntry) {
					return aEntry.Key == aResult->Alignment.Key;
				});
			if (duplicate != Cache.end()) {
				CachedBytes -= duplicate->Bytes;
				Cache.erase(duplicate);
			}
			// [[BPR-1366]] A paused preparation of this comparison is no longer
			// needed, and paused ones give way first when the reviewer needs
			// room.
			discard_paused([&aResult](const paused_preparation& aPaused) {
				return aPaused.Request.Key == aResult->Alignment.Key;
			});
			while (!Paused.empty() && CachedBytes + PausedBytes > Limits.CacheBytes - *bytes) {
				PausedBytes -= Paused.back()->Bytes;
				Paused.pop_back();
			}
			while (!Cache.empty()
				   && (Cache.size() >= Limits.CacheEntries
					   || CachedBytes > Limits.CacheBytes - *bytes)) {
				// [[BPR-1327]]. A prepared comparison gives way first.
				auto victim = std::ranges::find_if(Cache, &cache_entry::Prepared);
				if (victim == Cache.end()) {
					victim = Cache.begin();
				}
				CachedBytes -= victim->Bytes;
				Cache.erase(victim);
			}
			Cache.push_back(cache_entry{
				.Key    = aResult->Alignment.Key,
				.Result = aResult,
				.Index  = aIndex,
				.Bytes  = *bytes,
			});
			CachedBytes += *bytes;
		}

		/// \brief Stores a prepared comparison in room the reviewer's own
		/// comparisons leave free, within the prepared share. [[BPR-1327]]
		void store_prepared(const std::shared_ptr<const core::computed_file_diff>&  aResult,
							const std::shared_ptr<const core::diff_semantic_index>& aIndex)
		{
			auto       bytes      = retained_result_bytes(*aResult);
			const auto indexBytes = aIndex->retained_bytes();
			const auto byteShare  = prepared_byte_capacity(Limits);
			const auto entryShare = prepared_entry_capacity(Limits);
			if (!bytes || !indexBytes || !checked_add(*bytes, *indexBytes)
				|| !checked_add(*bytes, sizeof(aIndex)) || *bytes > byteShare || entryShare == 0U
				|| std::ranges::find(Cache, aResult->Alignment.Key, &cache_entry::Key) != Cache.end()) {
				return;
			}
			// [[BPR-1366]] Paused preparations count inside the share and give
			// way first.
			const auto overfull = [this, &bytes, byteShare] {
				return prepared_total_bytes() + PausedBytes > byteShare - *bytes
					|| CachedBytes + PausedBytes > Limits.CacheBytes - *bytes;
			};
			while (!Paused.empty() && overfull()) {
				PausedBytes -= Paused.back()->Bytes;
				Paused.pop_back();
			}
			const auto full = [this, &bytes, byteShare, entryShare] {
				return prepared_count() >= entryShare || prepared_total_bytes() > byteShare - *bytes
					|| Cache.size() >= Limits.CacheEntries || CachedBytes > Limits.CacheBytes - *bytes;
			};
			while (full()) {
				const auto victim = std::ranges::find_if(Cache, &cache_entry::Prepared);
				if (victim == Cache.end()) {
					return;
				}
				CachedBytes -= victim->Bytes;
				Cache.erase(victim);
			}
			Cache.push_back(cache_entry{
				.Key      = aResult->Alignment.Key,
				.Result   = aResult,
				.Index    = aIndex,
				.Bytes    = *bytes,
				.Prepared = true,
			});
			CachedBytes += *bytes;
		}

		core::diff_state                       State;
		diff_blob_port&                        Blobs;
		effect_executor&                       Effects;
		decision_executor&                     DecisionLane;
		diff_projection_port&                  Projections;
		diff_host_limits                       Limits;
		core::diff_compute_limits              ComputeLimits;
		std::shared_ptr<completion_gate>       Gate;
		std::vector<std::shared_ptr<diff_job>> Jobs;
		std::vector<cache_entry>               Cache;
		std::uint64_t                          CachedBytes                = 0U;
		std::size_t                            PendingDecisionCompletions = 0U;
		std::deque<deferred_diff_ingress>      DeferredIngress;
		std::deque<prepared_diff_request>      PreparedQueue;
		/// [[BPR-1366]] Paused preparations, the next to continue first.
		std::deque<std::shared_ptr<paused_preparation>> Paused;
		std::uint64_t                                   PausedBytes = 0U;
		std::shared_ptr<diff_job>                       PreparedJob;
		std::optional<core::compute_file_diff_request>  Adopted;
		std::uint64_t                                   PreparedSequence         = 0U;
		std::uint64_t                                   PreparedHits             = 0U;
		bool                                            Closing                  = false;
		bool                                            ProjectionDeliveryFailed = false;
		bool                                            TurnActive               = false;
		bool                                            DeferredIngressScheduled = false;
	};

	diff_application_host::diff_application_host(
		core::diff_state                aInitialState,
		diff_blob_port&                 aBlobs,
		effect_executor&                aEffects,
		decision_executor&              aDecisionLane,
		diff_projection_port&           aProjections,
		const diff_host_limits          aLimits,
		const core::diff_compute_limits aComputeLimits)
		: Implementation{ std::make_unique<implementation>(
			  std::move(aInitialState),
			  aBlobs,
			  aEffects,
			  aDecisionLane,
			  aProjections,
			  aLimits,
			  aComputeLimits) }
	{
	}

	diff_application_host::~diff_application_host()
	{
		Implementation->shutdown_now();
	}

	void diff_application_host::request(core::file_comparison_key aKey,
										core::diff_options        aOptions)
	{
		Implementation->request(std::move(aKey), aOptions);
	}

	void diff_application_host::prepare(std::vector<prepared_diff_request> aRequests)
	{
		Implementation->prepare(std::move(aRequests));
	}

	void diff_application_host::cancel()
	{
		Implementation->cancel();
	}

	void diff_application_host::invalidate(
		const core::pull_request_authority& aAuthority)
	{
		Implementation->invalidate(aAuthority);
	}

	void diff_application_host::invalidate(const core::review_epoch_id& aEpoch)
	{
		Implementation->invalidate(aEpoch);
	}

	void diff_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto diff_application_host::state() const noexcept -> const core::diff_state&
	{
		return Implementation->State;
	}

	auto diff_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto diff_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Jobs.empty()
			&& !Implementation->PreparedJob && !Implementation->Adopted
			&& Implementation->PendingDecisionCompletions == 0U
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->TurnActive;
	}

	auto diff_application_host::outstanding_jobs() const noexcept -> std::size_t
	{
		return Implementation->pending_work();
	}

	auto diff_application_host::cache_entries() const noexcept -> std::size_t
	{
		return Implementation->Cache.size();
	}

	auto diff_application_host::cache_bytes() const noexcept -> std::uint64_t
	{
		return Implementation->CachedBytes;
	}

	auto diff_application_host::prepared_entries() const noexcept -> std::size_t
	{
		return Implementation->prepared_count();
	}

	auto diff_application_host::prepared_bytes() const noexcept -> std::uint64_t
	{
		return Implementation->prepared_total_bytes();
	}

	auto diff_application_host::preparing() const noexcept -> std::size_t
	{
		return Implementation->PreparedQueue.size() + Implementation->Paused.size()
			+ (Implementation->PreparedJob ? 1U : 0U);
	}

	auto diff_application_host::paused_entries() const noexcept -> std::size_t
	{
		return Implementation->Paused.size();
	}

	auto diff_application_host::paused_bytes() const noexcept -> std::uint64_t
	{
		return Implementation->PausedBytes;
	}

	auto diff_application_host::prepared_hits() const noexcept -> std::uint64_t
	{
		return Implementation->PreparedHits;
	}

	auto diff_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
} // namespace bpr::application
