#include "bpr/application/agent_realignment.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		std::array<std::uint8_t, 32U> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto comparison_key(
		const core::diff_options& aOptions  = { },
		const std::uint8_t        aFileSeed = 31U,
		const bool                aHasLeft  = true,
		const bool                aHasRight = true) -> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "test options normalize");
		const auto scope       = opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority   = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return core::file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = core::comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = core::comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = aHasLeft ? std::optional{ blob(21U) } : std::nullopt,
			.RightBlob = aHasRight ? std::optional{ blob(51U) } : std::nullopt,
			.File      = opaque<core::file_identity>(aFileSeed),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	class fake_blobs final : public application::diff_blob_port
	{
	public:
		[[nodiscard]] auto materialize(
			const application::diff_blob_request& aRequest,
			const std::stop_token                 aStop)
			-> application::diff_blob_observation override
		{
			Requests.push_back(aRequest);
			StopObserved.push_back(aStop.stop_requested());
			if (Throw) {
				throw std::runtime_error{ "blob failure" };
			}
			auto observation = application::diff_blob_observation{
				.Context = aRequest.Context,
				.Side    = aRequest.Side,
				.Blob    = aRequest.Blob,
				.Status  = application::diff_blob_status::available,
			};
			if (DeclaredBlobBytes.has_value()
				&& *DeclaredBlobBytes > aRequest.MaximumBytes) {
				RefusedBeforeAllocation = true;
				observation.Status      = application::diff_blob_status::materialization_limit;
				observation.Bytes.clear();
				return observation;
			}
			observation.Bytes = aRequest.Side == core::diff_side::left ? LeftBytes
																	   : RightBytes;
			if (Status.has_value()
				&& (!StatusSide.has_value() || *StatusSide == aRequest.Side)) {
				observation.Status = *Status;
				if (observation.Status != application::diff_blob_status::available) {
					observation.Bytes.clear();
				}
			}
			if (MismatchContext) {
				observation.Context.Generation = core::request_generation{ 999U };
			}
			if (MismatchSide) {
				observation.Side = aRequest.Side == core::diff_side::left
					? core::diff_side::right
					: core::diff_side::left;
			}
			if (MismatchBlob) {
				observation.Blob = blob(99U);
			}
			return observation;
		}

		void reset_behavior()
		{
			Status.reset();
			StatusSide.reset();
			Throw           = false;
			MismatchContext = false;
			MismatchSide    = false;
			MismatchBlob    = false;
			DeclaredBlobBytes.reset();
			RefusedBeforeAllocation = false;
			LeftBytes               = bytes("left\ncommon\n");
			RightBytes              = bytes("right\ncommon\n");
		}

		std::vector<application::diff_blob_request>  Requests;
		std::vector<bool>                            StopObserved;
		std::vector<std::uint8_t>                    LeftBytes  = bytes("left\ncommon\n");
		std::vector<std::uint8_t>                    RightBytes = bytes("right\ncommon\n");
		std::optional<application::diff_blob_status> Status;
		std::optional<core::diff_side>               StatusSide;
		bool                                         Throw           = false;
		bool                                         MismatchContext = false;
		bool                                         MismatchSide    = false;
		bool                                         MismatchBlob    = false;
		std::optional<std::uint64_t>                 DeclaredBlobBytes;
		bool                                         RefusedBeforeAllocation = false;
	};

	class recording_projection final : public application::diff_projection_port
	{
	public:
		struct result_record
		{
			core::diff_request_context                       Context;
			core::computed_file_diff                         Result;
			std::shared_ptr<const core::diff_semantic_index> Index;
			bool                                             CacheHit = false;
		};

		void publish(const application::diff_publication& aPublication) override
		{
			if (Throw) {
				throw std::runtime_error{ "projection failure" };
			}
			begin_delivery();
			expect(aPublication.Result != nullptr, "success has immutable result");
			expect(aPublication.Index && aPublication.Index->matches_result(*aPublication.Result),
				   "every host publication has an already validated index owning its exact result");
			Results.push_back(result_record{
				.Context  = aPublication.Context,
				.Result   = *aPublication.Result,
				.Index    = aPublication.Index,
				.CacheHit = aPublication.CacheHit,
			});
			run_callback(OnPublish);
			end_delivery();
		}

		void fail(const application::diff_failure_publication& aFailure) override
		{
			if (Throw) {
				throw std::runtime_error{ "projection failure" };
			}
			begin_delivery();
			Failures.push_back(aFailure);
			run_callback(OnFail);
			end_delivery();
		}

		void invalidate(
			const core::pull_request_authority& aAuthority) override
		{
			if (Throw) {
				throw std::runtime_error{ "projection failure" };
			}
			begin_delivery();
			Invalidated.push_back(aAuthority);
			run_callback(OnInvalidate);
			end_delivery();
		}

		void begin_delivery()
		{
			NestedDelivery = NestedDelivery || Delivering;
			Delivering     = true;
		}

		void end_delivery()
		{
			Delivering = false;
		}

		static void run_callback(std::function<void()>& aCallback)
		{
			auto current = std::move(aCallback);
			aCallback    = { };
			if (current) {
				current();
			}
		}

		std::vector<result_record>                         Results;
		std::vector<application::diff_failure_publication> Failures;
		std::vector<core::pull_request_authority>          Invalidated;
		bool                                               Throw          = false;
		bool                                               Delivering     = false;
		bool                                               NestedDelivery = false;
		std::function<void()>                              OnPublish;
		std::function<void()>                              OnFail;
		std::function<void()>                              OnInvalidate;
	};

	class throwing_effect_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()>) override
		{
			throw std::runtime_error{ "effect submission failure" };
		}
	};

	struct fixture
	{
		explicit fixture(
			const application::diff_host_limits aLimits        = { },
			const core::diff_compute_limits     aComputeLimits = { })
			: Host{ std::make_unique<application::diff_application_host>(
				  core::diff_state{
					  .ApplicationSession = core::application_session_id{ 71U },
				  },
				  Blobs,
				  Effects,
				  Decisions,
				  Projections,
				  aLimits,
				  aComputeLimits) }
		{
		}

		~fixture()
		{
			Host->shutdown();
			Effects.drain();
			Decisions.drain();
		}

		void request(const core::file_comparison_key& aKey,
					 const core::diff_options&        aOptions = { })
		{
			Host->request(aKey, aOptions);
		}

		void run_effect_and_decision()
		{
			expect(Effects.pending() == 1U, "one effect is pending");
			Effects.run_next();
			expect(Decisions.pending() == 1U,
				   "effect posts one serialized terminal");
			Decisions.run_next();
		}

		fake_blobs                                          Blobs;
		application::manual_executor                        Effects;
		application::manual_executor                        Decisions;
		recording_projection                                Projections;
		std::unique_ptr<application::diff_application_host> Host;
	};

	// [[BPR-1327]] ---------------------------------------------------------

	[[nodiscard]] auto prepared(const std::uint8_t aFileSeed) -> application::prepared_diff_request
	{
		return { .Key = comparison_key({ }, aFileSeed), .Options = { } };
	}

	/// Runs the one pending effect and every decision it leads to.
	void settle(fixture& aTest)
	{
		while (aTest.Effects.pending() != 0U || aTest.Decisions.pending() != 0U) {
			if (aTest.Decisions.pending() != 0U) {
				aTest.Decisions.run_next();
			} else {
				aTest.Effects.run_next();
			}
		}
	}

	auto test_a_prepared_comparison_opens_without_reading_or_computing() -> void
	{
		// Control: without preparation, opening a file schedules work on the
		// effect lane and reads both blobs before anything can be shown.
		{
			fixture control;
			control.request(comparison_key({ }, 31U));
			expect(control.Effects.pending() == 1U && control.Projections.Results.empty(),
				   "control: an unprepared open waits for the effect lane");
			control.Effects.run_next();
			expect(control.Blobs.Requests.size() == 2U, "control: an unprepared open reads both blobs");
			control.Decisions.run_next();
		}
		fixture test;
		test.Host->prepare({ prepared(31U), prepared(32U) });
		expect(test.Host->preparing() == 2U && test.Effects.pending() == 1U && test.Decisions.pending() == 0U,
			   "preparation computes one comparison at a time on the effect lane");
		expect(test.Host->state().Pending == std::nullopt && !test.Host->state().Current,
			   "preparation is not a request: the core's selection is untouched");
		settle(test);
		expect(test.Host->prepared_entries() == 2U && test.Host->cache_entries() == 2U
				   && test.Host->prepared_bytes() == test.Host->cache_bytes() && test.Host->preparing() == 0U,
			   "both prepared comparisons are stored in the cache");
		expect(test.Projections.Results.empty() && test.Projections.Failures.empty(),
			   "preparation publishes nothing");
		expect(test.Blobs.Requests.size() == 4U
				   && test.Blobs.Requests.front().MaximumBytes
					   == application::prepared_byte_capacity(application::diff_host_limits{ }) / 4U,
			   "preparation reads no side larger than a quarter of its byte share");
		const auto reads = test.Blobs.Requests.size();
		test.request(comparison_key({ }, 31U));
		expect(test.Effects.pending() == 0U && test.Decisions.pending() == 1U,
			   "opening a prepared file needs no effect, only one decision turn");
		test.Decisions.run_next();
		expect(test.Projections.Results.size() == 1U && test.Projections.Results.back().CacheHit
				   && test.Blobs.Requests.size() == reads,
			   "opening a prepared file publishes from the cache without reading a blob");
		expect(test.Host->prepared_entries() == 1U && test.Host->cache_entries() == 2U,
			   "the opened comparison becomes the reviewer's and leaves the prepared share");
	}

	auto test_preparation_yields_to_the_reviewer() -> void
	{
		fixture test;
		test.Host->prepare({ prepared(31U), prepared(32U) });
		expect(test.Effects.pending() == 1U, "the first preparation is posted");
		test.request(comparison_key({ }, 40U));
		expect(test.Effects.pending() == 2U, "the reviewer's request is posted at once");
		test.Effects.run_next();
		expect(test.Blobs.Requests.empty() && test.Decisions.pending() == 1U,
			   "the overtaken preparation stops before reading a blob");
		test.Decisions.run_next();
		expect(test.Host->prepared_entries() == 0U && test.Effects.pending() == 1U,
			   "nothing is stored from it, and no preparation starts while the reviewer's work is outstanding");
		test.Effects.run_next();
		test.Decisions.run_next();
		expect(test.Projections.Results.size() == 1U && !test.Projections.Results.back().CacheHit,
			   "the reviewer's comparison is published");
		expect(test.Effects.pending() == 1U && test.Host->preparing() == 1U,
			   "preparation resumes with what remains once the reviewer's work is done");
		settle(test);
		expect(test.Host->prepared_entries() == 1U, "the remaining comparison is prepared");
	}

	auto test_opening_the_file_being_prepared_waits_for_it() -> void
	{
		fixture test;
		test.Host->prepare({ prepared(31U) });
		test.request(comparison_key({ }, 31U));
		expect(test.Effects.pending() == 1U && test.Host->outstanding_jobs() == 1U,
			   "the reviewer's request adopts the preparation instead of computing it again");
		settle(test);
		expect(test.Projections.Results.size() == 1U && test.Projections.Results.back().CacheHit
				   && test.Blobs.Requests.size() == 2U,
			   "the adopted comparison is published once, from one computation");
		expect(test.Host->state().Current && test.Host->prepared_entries() == 0U && test.Host->cache_entries() == 1U,
			   "it is the reviewer's comparison now");
		// Superseding the adopted request releases it with a cancellation.
		fixture superseded;
		superseded.Host->prepare({ prepared(31U) });
		superseded.request(comparison_key({ }, 31U));
		superseded.request(comparison_key({ }, 33U));
		settle(superseded);
		expect(superseded.Projections.Results.size() == 1U
				   && superseded.Projections.Results.back().Context.Key == comparison_key({ }, 33U),
			   "a superseded adoption publishes nothing; the newer request is published");
	}

	auto test_preparation_stays_inside_its_share_and_never_evicts_the_reviewer() -> void
	{
		{
			auto threw = false;
			try {
				fixture invalid{ application::diff_host_limits{ .PreparedEntries = application::maximum_diff_cache_entries + 1U } };
			} catch (const std::invalid_argument&) {
				threw = true;
			}
			expect(threw, "an oversized prepared share is refused");
		}
		// Eight entries: a quarter, two, may be prepared.
		const auto limits = application::diff_host_limits{ .OutstandingJobs = 4U, .CacheEntries = 8U, .CacheBytes = 64U * 1024U * 1024U };
		expect(application::prepared_entry_capacity(limits) == 2U, "the prepared share of eight entries is two");
		fixture test{ limits };
		auto    window = std::vector<application::prepared_diff_request>{ };
		for (std::uint8_t seed = 31U; seed < 36U; ++seed) {
			window.push_back(prepared(seed));
		}
		test.Host->prepare(window);
		expect(test.Host->preparing() == 2U, "the queue is bounded by the prepared share");
		settle(test);
		expect(test.Host->prepared_entries() == 2U, "two are prepared");
		// The reviewer fills the cache: prepared entries go first.
		for (std::uint8_t seed = 60U; seed < 68U; ++seed) {
			test.request(comparison_key({ }, seed));
			settle(test);
		}
		expect(test.Host->cache_entries() == 8U && test.Host->prepared_entries() == 0U,
			   "the reviewer's comparisons displaced the prepared ones");
		const auto first = comparison_key({ }, 60U);
		test.Host->prepare({ prepared(36U) });
		settle(test);
		expect(test.Host->prepared_entries() == 0U && test.Blobs.Requests.size() == 2U * 2U + 8U * 2U,
			   "with the cache full of the reviewer's comparisons, nothing is computed for preparation");
		test.request(first);
		expect(test.Effects.pending() == 0U, "the reviewer's oldest comparison is still cached");
		settle(test);

		// A prepared comparison larger than its byte share is not kept.
		fixture tight{ application::diff_host_limits{ .PreparedBytes = 1U } };
		tight.Host->prepare({ prepared(31U) });
		settle(tight);
		expect(tight.Host->prepared_entries() == 0U && tight.Host->cache_entries() == 0U,
			   "a comparison over the byte share is discarded");
		fixture disabled{ application::diff_host_limits{ .PreparedEntries = 0U } };
		disabled.Host->prepare({ prepared(31U) });
		expect(disabled.Host->preparing() == 0U && disabled.Effects.pending() == 0U, "a zero share disables preparation");
	}

	auto test_preparation_is_cancelled_with_its_authority() -> void
	{
		fixture test;
		test.Host->prepare({ prepared(31U), prepared(32U) });
		test.Host->invalidate(comparison_key({ }, 31U).Authority);
		expect(test.Host->preparing() == 1U, "invalidation empties the queue; only the running one remains");
		settle(test);
		expect(test.Host->prepared_entries() == 0U && test.Host->cache_entries() == 0U,
			   "the running preparation finishes unstored");

		fixture replaced;
		replaced.Host->prepare({ prepared(31U), prepared(32U) });
		replaced.Host->prepare({ });
		settle(replaced);
		expect(replaced.Host->prepared_entries() == 0U, "an empty window stops preparation");

		fixture epoch;
		epoch.Host->prepare({ prepared(31U) });
		settle(epoch);
		expect(epoch.Host->prepared_entries() == 1U, "prepared before the epoch is invalidated");
		const auto head = comparison_key({ }, 31U).RightAnchor;
		epoch.Host->invalidate(core::review_epoch_id{ .Authority = head.Authority, .Incarnation = head.Incarnation, .Ordinal = head.Epoch });
		expect(epoch.Host->prepared_entries() == 0U, "an invalidated epoch removes its prepared comparisons");

		fixture closing;
		closing.Host->prepare({ prepared(31U), prepared(32U) });
		closing.Host->shutdown();
		expect(closing.Host->preparing() == 1U && !closing.Host->is_quiescent(),
			   "shutdown stops the queue and waits for the running preparation");
		settle(closing);
		expect(closing.Host->is_quiescent() && closing.Host->cache_entries() == 0U,
			   "shutdown is quiescent once the running preparation returns, and nothing is kept");
	}

	auto test_a_large_window_is_bounded() -> void
	{
		fixture test;
		auto    window = std::vector<application::prepared_diff_request>{ };
		for (std::size_t index = 0U; index < 200U; ++index) {
			window.push_back(prepared(static_cast<std::uint8_t>(index)));
		}
		test.Host->prepare(window);
		const auto share = application::prepared_entry_capacity(application::diff_host_limits{ });
		expect(share == 16U && test.Host->preparing() == share, "a 200-file window queues only the prepared share");
		settle(test);
		expect(test.Host->prepared_entries() == share && test.Blobs.Requests.size() == 2U * share,
			   "exactly the share is computed, two reads each");
	}

	auto test_worker_and_cache_lanes() -> void
	{
		fixture    test;
		const auto key = comparison_key();
		test.request(key);
		expect(test.Effects.pending() == 1U && test.Decisions.pending() == 0U
				   && test.Projections.Results.empty(),
			   "request schedules only the effect lane");
		test.Effects.run_next();
		expect(test.Decisions.pending() == 1U
				   && test.Projections.Results.empty()
				   && !test.Host->state().Current.has_value(),
			   "worker cannot accept or publish off decision lane");
		expect(!test.Blobs.Requests.empty()
				   && test.Blobs.Requests.front().MaximumBytes
					   == core::diff_compute_limits{ }.MaximumBytesPerSide,
			   "blob acquisition receives the exact configured byte ceiling");
		test.Decisions.run_next();
		expect(test.Projections.Results.size() == 1U
				   && !test.Projections.Results.back().CacheHit
				   && test.Host->cache_entries() == 1U
				   && test.Host->cache_bytes() > 0U,
			   "validated accepted result publishes and enters bounded cache");
		const auto calls      = test.Blobs.Requests.size();
		const auto prepared   = test.Projections.Results.back().Index;
		const auto cacheBytes = test.Host->cache_bytes();
		expect(cacheBytes > *prepared->retained_bytes(), "cache budget includes prepared index allocation and its result");

		test.request(key);
		expect(test.Effects.pending() == 0U && test.Decisions.pending() == 1U
				   && test.Projections.Results.size() == 1U,
			   "cache hit still traverses serialized decision lane");
		test.Decisions.run_next();
		expect(test.Projections.Results.size() == 2U
				   && test.Projections.Results.back().CacheHit
				   && test.Blobs.Requests.size() == calls,
			   "exact cache hit performs no blob acquisition");
		expect(test.Projections.Results.back().Index == prepared
				   && test.Host->cache_bytes() == cacheBytes,
			   "cache hit reuses the very same prepared index without duplicate retained allocation");
		fixture limited{ application::diff_host_limits{ .OutstandingJobs = 1U, .CacheEntries = 1U, .CacheBytes = cacheBytes - *prepared->retained_bytes() } };
		limited.request(key);
		limited.run_effect_and_decision();
		expect(limited.Host->cache_entries() == 0U && limited.Projections.Results.size() == 1U,
			   "a budget fitting only the result cannot retain the result plus its prepared index");
	}

	auto complete_and_count_miss(fixture&                         aTest,
								 const core::file_comparison_key& aKey,
								 const core::diff_options&        aOptions,
								 std::size_t&                     aCalls) -> void
	{
		aTest.request(aKey, aOptions);
		expect(aTest.Effects.pending() == 1U, "identity change misses cache");
		aTest.run_effect_and_decision();
		expect(aTest.Blobs.Requests.size() > aCalls,
			   "cache collision requires exact blob materialization");
		aCalls = aTest.Blobs.Requests.size();
	}

	auto test_exact_cache_key_collisions() -> void
	{
		fixture    test;
		const auto base = comparison_key();
		test.request(base);
		test.run_effect_and_decision();
		auto calls = test.Blobs.Requests.size();

		auto authority              = base;
		authority.Authority.Account = core::github_account_id{ 41U };
		complete_and_count_miss(test, authority, { }, calls);

		auto repository                 = base;
		repository.Repository           = core::repository_id{ 102U };
		repository.Authority.Repository = repository.Repository;
		complete_and_count_miss(test, repository, { }, calls);

		auto pullRequest                  = base;
		pullRequest.PullRequest           = core::pull_request_id{ 203U };
		pullRequest.Authority.PullRequest = pullRequest.PullRequest;
		pullRequest.Authority.Number      = core::pull_request_number{ 18U };
		complete_and_count_miss(test, pullRequest, { }, calls);

		auto anchors              = base;
		anchors.RightAnchor.Epoch = core::epoch_ordinal{ 3 };
		complete_and_count_miss(test, anchors, { }, calls);

		auto file = base;
		file.File = opaque<core::file_identity>(32U);
		complete_and_count_miss(test, file, { }, calls);

		auto blobs     = base;
		blobs.LeftBlob = blob(22U);
		complete_and_count_miss(test, blobs, { }, calls);

		auto options       = core::diff_options{ };
		options.Whitespace = core::whitespace_policy::ignore_trailing;
		auto optionKey     = comparison_key(options);
		complete_and_count_miss(test, optionKey, options, calls);

		auto unsupported      = base;
		unsupported.Algorithm = core::diff_algorithm_revision{ 2U };
		test.request(unsupported);
		expect(test.Effects.pending() == 0U
				   && test.Decisions.pending() == 0U
				   && test.Host->state().LastReason
					   == core::diff_reason_code::host_failure,
			   "unsupported algorithm revision cannot cross into cache or worker");
	}

	auto test_reordered_generation_and_stale_completion() -> void
	{
		fixture    test;
		const auto key = comparison_key();
		test.request(key);
		test.request(key);
		expect(test.Effects.pending() == 2U && test.Host->outstanding_jobs() == 2U,
			   "same-key supersession retains bounded in-flight terminals");

		test.Effects.run(1U);
		test.Effects.run(0U);
		expect(test.Decisions.pending() == 2U,
			   "manual effect lane can reverse generation completion order");
		test.Decisions.run(1U);
		expect(test.Projections.Results.empty(),
			   "cancelled older generation cannot publish when delivered first");
		test.Decisions.run(0U);
		expect(test.Projections.Results.size() == 1U
				   && test.Projections.Failures.empty()
				   && test.Host->outstanding_jobs() == 0U,
			   "only exact current generation publishes after reordering");
	}

	auto test_invalid_replacement_supersedes_pending_work() -> void
	{
		for (const auto unsupportedReplacement : { false, true }) {
			fixture    test;
			const auto base = comparison_key();
			test.request(base);
			auto replacement = base;
			auto options     = core::diff_options{ };
			if (unsupportedReplacement) {
				replacement.Algorithm = core::diff_algorithm_revision{ 2U };
			} else {
				options.ContextLines = 9U;
			}
			test.request(replacement, options);
			expect(!test.Host->state().Pending.has_value(),
				   "invalid replacement revokes the older pending context");
			test.run_effect_and_decision();
			expect(test.Projections.Results.empty()
					   && test.Projections.Failures.empty(),
				   "revoked work cannot publish after invalid replacement");
		}
	}

	auto test_reordered_exact_identity_boundaries() -> void
	{
		const auto base    = comparison_key();
		auto       changed = std::vector<
			std::pair<core::file_comparison_key, core::diff_options>>{ };

		auto authority              = base;
		authority.Authority.Account = core::github_account_id{ 41U };
		changed.emplace_back(authority, core::diff_options{ });
		auto repository                 = base;
		repository.Repository           = core::repository_id{ 102U };
		repository.Authority.Repository = repository.Repository;
		changed.emplace_back(repository, core::diff_options{ });
		auto pullRequest                  = base;
		pullRequest.PullRequest           = core::pull_request_id{ 203U };
		pullRequest.Authority.PullRequest = pullRequest.PullRequest;
		pullRequest.Authority.Number      = core::pull_request_number{ 18U };
		changed.emplace_back(pullRequest, core::diff_options{ });
		auto epoch              = base;
		epoch.RightAnchor.Epoch = core::epoch_ordinal{ 3 };
		changed.emplace_back(epoch, core::diff_options{ });
		auto file = base;
		file.File = opaque<core::file_identity>(32U);
		changed.emplace_back(file, core::diff_options{ });
		auto changedBlob      = base;
		changedBlob.RightBlob = blob(52U);
		changed.emplace_back(changedBlob, core::diff_options{ });
		auto options        = core::diff_options{ };
		options.LineEndings = core::line_ending_policy::normalize;
		changed.emplace_back(comparison_key(options), options);

		for (const auto& [newKey, newOptions] : changed) {
			fixture test;
			test.request(base);
			test.Effects.run_next();
			test.request(newKey, newOptions);
			test.Effects.run_next();
			expect(test.Decisions.pending() == 2U,
				   "two exact identity terminals can be staged");
			test.Decisions.run_next();
			expect(test.Projections.Results.empty(),
				   "old authority/repository/PR/epoch/file/blob/options result is stale");
			test.Decisions.run_next();
			expect(test.Projections.Results.size() == 1U
					   && test.Projections.Results.back().Result.Alignment.Key
						   == newKey,
				   "only the complete current identity publishes");
		}

		fixture cacheTest;
		cacheTest.request(base);
		cacheTest.run_effect_and_decision();
		cacheTest.request(base);
		const auto miss = comparison_key({ }, 32U);
		cacheTest.request(miss);
		cacheTest.Effects.run_next();
		expect(cacheTest.Decisions.pending() == 2U,
			   "cache hit and cache miss both stage serialized terminals");
		cacheTest.Decisions.run_next();
		expect(cacheTest.Projections.Results.size() == 1U,
			   "superseded cache hit cannot bypass core stale rejection");
		cacheTest.Decisions.run_next();
		expect(cacheTest.Projections.Results.size() == 2U
				   && !cacheTest.Projections.Results.back().CacheHit
				   && cacheTest.Projections.Results.back().Result.Alignment.Key
					   == miss,
			   "current cache miss publishes after reordered stale hit");
	}

	auto test_cancellation_and_shutdown_stages() -> void
	{
		{
			fixture test;
			test.request(comparison_key());
			test.Host->cancel();
			expect(test.Projections.Failures.size() == 1U
					   && test.Projections.Failures.back().Reason
						   == core::diff_reason_code::cancelled,
				   "explicit cancellation is a typed current terminal");
			test.run_effect_and_decision();
			expect(test.Blobs.Requests.empty()
					   && test.Projections.Failures.size() == 1U,
				   "stop is observed before blob acquisition and late terminal is stale");
		}
		{
			fixture test;
			test.Blobs.Status     = application::diff_blob_status::cancelled;
			test.Blobs.StatusSide = core::diff_side::right;
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Blobs.Requests.size() == 2U
					   && test.Projections.Failures.size() == 1U
					   && test.Projections.Failures.back().Reason
						   == core::diff_reason_code::cancelled,
				   "cancellation between side acquisitions suppresses computation");
		}
		{
			fixture test;
			test.request(comparison_key());
			test.Effects.run_next();
			test.Host->cancel();
			test.Decisions.run_next();
			expect(test.Projections.Results.empty()
					   && test.Projections.Failures.size() == 1U,
				   "cancellation after computation suppresses staged publication");
		}
		{
			fixture test;
			test.request(comparison_key());
			test.Effects.run_next();
			test.Host->shutdown();
			expect(!test.Host->is_quiescent(),
				   "shutdown waits for already posted decision terminal");
			test.Decisions.run_next();
			expect(test.Host->is_quiescent()
					   && test.Projections.Results.empty()
					   && test.Projections.Failures.empty(),
				   "shutdown drains without publishing staged work");
		}
	}

	auto test_job_and_cache_bounds() -> void
	{
		{
			auto threw = false;
			try {
				fixture test{ application::diff_host_limits{
					.OutstandingJobs = 0U,
					.CacheEntries    = 1U,
					.CacheBytes      = 1U,
				} };
			} catch (const std::invalid_argument&) {
				threw = true;
			}
			expect(threw, "invalid configured bounds fail instead of expanding");
		}
		{
			auto threw = false;
			try {
				auto computeLimits = core::diff_compute_limits{ };
				++computeLimits.MaximumBytesPerSide;
				fixture test{ { }, computeLimits };
			} catch (const std::invalid_argument&) {
				threw = true;
			}
			expect(threw,
				   "host rejects compute ceilings above the product maximum");
		}
		for (const auto moveWork : std::array<std::uint64_t, 2U>{
				 0U,
				 core::diff_compute_limits{ }.MaximumMoveDetectionWork + 1U }) {
			auto threw = false;
			try {
				auto computeLimits                     = core::diff_compute_limits{ };
				computeLimits.MaximumMoveDetectionWork = moveWork;
				fixture test{ { }, computeLimits };
			} catch (const std::invalid_argument&) {
				threw = true;
			}
			expect(
				threw,
				"host rejects zero or expanded optional move-work authority");
		}
		{
			fixture test{ application::diff_host_limits{
				.OutstandingJobs = 1U,
				.CacheEntries    = 2U,
				.CacheBytes      = 1024U * 1024U,
			} };
			test.request(comparison_key({ }, 31U));
			test.request(comparison_key({ }, 32U));
			expect(test.Host->outstanding_jobs() == 1U
					   && test.Decisions.pending() == 0U,
				   "outstanding work bound never over-enqueues a terminal");
			expect(test.Projections.Failures.size() == 1U
					   && test.Projections.Failures.back().Reason
						   == core::diff_reason_code::compute_limit,
				   "job exhaustion terminates on the serialized caller lane");
			test.Effects.run_next();
			test.Decisions.run_next();
		}
		{
			fixture    test{ application::diff_host_limits{
				.OutstandingJobs = 2U,
				.CacheEntries    = 1U,
				.CacheBytes      = 1024U * 1024U,
			} };
			const auto first  = comparison_key({ }, 31U);
			const auto second = comparison_key({ }, 32U);
			test.request(first);
			test.run_effect_and_decision();
			test.request(second);
			test.run_effect_and_decision();
			expect(test.Host->cache_entries() == 1U,
				   "cache entry capacity evicts deterministically");
			const auto calls = test.Blobs.Requests.size();
			test.request(first);
			expect(test.Effects.pending() == 1U,
				   "evicted exact key misses rather than path-colliding");
			test.run_effect_and_decision();
			expect(test.Blobs.Requests.size() > calls,
				   "evicted result is rematerialized");
		}
		{
			fixture test{ application::diff_host_limits{
				.OutstandingJobs = 1U,
				.CacheEntries    = 1U,
				.CacheBytes      = 1U,
			} };
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Host->cache_entries() == 0U
					   && test.Host->cache_bytes() == 0U,
				   "oversized cache value is published but never retained");
		}
	}

	auto test_terminal_queue_is_bounded() -> void
	{
		fixture    test{ application::diff_host_limits{
			.OutstandingJobs = 1U,
			.CacheEntries    = 1U,
			.CacheBytes      = 1024U * 1024U,
		} };
		const auto key = comparison_key();
		test.request(key);
		test.run_effect_and_decision();
		for (auto index = 0U; index < 100U; ++index) {
			test.request(key);
			expect(test.Host->outstanding_jobs() <= 1U
					   && test.Decisions.pending() <= 1U,
				   "cache terminals and failures share the outstanding bound");
		}
		test.Decisions.drain();
		expect(test.Host->outstanding_jobs() == 0U,
			   "bounded terminal queue drains exactly");
	}

	auto test_absent_and_empty_sides() -> void
	{
		fixture test;
		test.Blobs.RightBytes.clear();
		const auto key = comparison_key({ }, 31U, false, true);
		test.request(key);
		test.run_effect_and_decision();
		expect(test.Blobs.Requests.size() == 1U
				   && test.Blobs.Requests.front().Side == core::diff_side::right,
			   "absent left side performs no fake blob lookup");
		expect(test.Projections.Results.size() == 1U
				   && !test.Projections.Results.front().Result.LeftDocument
				   && test.Projections.Results.front().Result.RightDocument.has_value()
				   && test.Projections.Results.front()
						  .Result.RightDocument->Bytes.empty(),
			   "absent side and present empty blob remain distinguishable");
	}

	auto expect_failure(application::diff_blob_status aStatus,
						core::diff_reason_code        aReason,
						const std::string_view        aMessage) -> void
	{
		fixture test;
		test.Blobs.Status = aStatus;
		test.request(comparison_key());
		test.run_effect_and_decision();
		expect(test.Projections.Failures.size() == 1U
				   && test.Projections.Failures.back().Reason == aReason
				   && test.Projections.Results.empty(),
			   aMessage);
	}

	auto test_typed_failures_and_exact_echo() -> void
	{
		expect_failure(application::diff_blob_status::missing,
					   core::diff_reason_code::missing_left_blob,
					   "missing expected blob is side-specific");
		{
			fixture test;
			test.Blobs.Status     = application::diff_blob_status::missing;
			test.Blobs.StatusSide = core::diff_side::right;
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.back().Reason
					   == core::diff_reason_code::missing_right_blob,
				   "missing right blob retains right-side reason identity");
		}
		expect_failure(application::diff_blob_status::binary,
					   core::diff_reason_code::binary_file,
					   "blob binary policy is typed");
		expect_failure(application::diff_blob_status::decode_failure,
					   core::diff_reason_code::decode_failure,
					   "decoder failure is typed");
		expect_failure(application::diff_blob_status::materialization_limit,
					   core::diff_reason_code::materialization_limit,
					   "materialization limit is typed");
		expect_failure(application::diff_blob_status::generated_file_unsupported,
					   core::diff_reason_code::generated_file_unsupported,
					   "generated policy is typed");
		expect_failure(application::diff_blob_status::failure,
					   core::diff_reason_code::host_failure,
					   "blob host failure is typed");

		{
			fixture test;
			test.Blobs.Throw = true;
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.back().Reason
					   == core::diff_reason_code::host_failure,
				   "worker exceptions are contained");
		}
		{
			fake_blobs                         blobs;
			throwing_effect_executor           effects;
			application::manual_executor       decisions;
			recording_projection               projections;
			application::diff_application_host host{
				core::diff_state{
					.ApplicationSession = core::application_session_id{ 72U },
				},
				blobs,
				effects,
				decisions,
				projections,
			};
			host.request(comparison_key(), { });
			expect(decisions.pending() == 1U && host.outstanding_jobs() == 1U,
				   "effect submission exception becomes a bounded terminal");
			decisions.run_next();
			expect(projections.Failures.size() == 1U
					   && projections.Failures.back().Reason
						   == core::diff_reason_code::host_failure
					   && host.outstanding_jobs() == 0U,
				   "effect submission exception is contained on decision lane");
			host.shutdown();
		}
		for (const auto mismatch : { 0U, 1U, 2U }) {
			fixture test;
			test.Blobs.MismatchContext = mismatch == 0U;
			test.Blobs.MismatchSide    = mismatch == 1U;
			test.Blobs.MismatchBlob    = mismatch == 2U;
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.back().Reason
					   == core::diff_reason_code::host_failure,
				   "context, side, and blob echoes are exact");
		}
		{
			auto limits                = core::diff_compute_limits{ };
			limits.MaximumBytesPerSide = 4U;
			fixture test{ { }, limits };
			test.Blobs.DeclaredBlobBytes = 5U;
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Blobs.RefusedBeforeAllocation
					   && test.Blobs.Requests.front().MaximumBytes == 4U
					   && test.Projections.Failures.back().Reason
						   == core::diff_reason_code::materialization_limit,
				   "port refuses oversized material before allocating bytes");
		}
		{
			fixture test;
			test.Blobs.LeftBytes = { 0U };
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.empty()
					   && test.Projections.Results.size() == 1U
					   && test.Projections.Results.back()
							   .Result.Alignment.Content
						   == core::diff_content_state::binary
					   && test.Projections.Results.back()
							   .Result.Alignment.Diagnostic.Code
						   == core::diff_reason_code::binary_file
					   && test.Projections.Results.back()
							  .Result.Alignment.Segments.empty(),
				   "validated binary result publishes its honest unavailable semantics");
		}
		{
			fixture test;
			test.Blobs.LeftBytes = { 0xC3U, 0x28U };
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.empty()
					   && test.Projections.Results.size() == 1U
					   && test.Projections.Results.back()
							   .Result.Alignment.Content
						   == core::diff_content_state::unavailable
					   && test.Projections.Results.back()
							   .Result.Alignment.Diagnostic.Code
						   == core::diff_reason_code::decode_failure,
				   "validated decode result publishes its honest unavailable semantics");
		}
		{
			auto limits                = core::diff_compute_limits{ };
			limits.MaximumBytesPerSide = 1U;
			fixture test{ { }, limits };
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.back().Reason
					   == core::diff_reason_code::materialization_limit,
				   "compute byte limit cannot publish partial output");
		}
		{
			// [[BPR-0719]]. A work limit degrades the alignment instead of
			// refusing the file: the host publishes a validated text result
			// marked approximate, never an unavailable one.
			auto limits                    = core::diff_compute_limits{ };
			limits.MaximumMatrixCells      = 1U;
			limits.MaximumPrimaryIndexWork = 0U;
			fixture test{ { }, limits };
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(test.Projections.Failures.empty()
					   && test.Projections.Results.size() == 1U
					   && test.Projections.Results.back()
							   .Result.Alignment.Content
						   == core::diff_content_state::text
					   && test.Projections.Results.back()
							   .Result.Alignment.Diagnostic.Code
						   == core::diff_reason_code::none
					   && test.Projections.Results.back().Result.AlignmentApproximate,
				   "compute work limit publishes a validated approximate alignment, not a refusal");
		}
		{
			fixture test;
			auto    options      = core::diff_options{ };
			options.ContextLines = 9U;
			test.request(comparison_key(), options);
			expect(test.Effects.pending() == 0U
					   && test.Decisions.pending() == 0U
					   && test.Host->state().LastReason
						   == core::diff_reason_code::invalid_alignment,
				   "key/options mismatch is rejected before acquisition");
		}
	}

	auto test_invalidation_and_projection_exception() -> void
	{
		fixture    test;
		const auto key = comparison_key();
		test.request(key);
		test.run_effect_and_decision();
		expect(test.Host->cache_entries() == 1U, "fixture has cached authority");
		test.Host->invalidate(key.Authority);
		expect(test.Host->cache_entries() == 0U
				   && test.Host->cache_bytes() == 0U
				   && !test.Host->state().Current.has_value()
				   && test.Projections.Invalidated == std::vector{ key.Authority },
			   "authority invalidation clears semantic current and exact cache");
		test.request(key);
		expect(test.Effects.pending() == 1U,
			   "invalidated authority cannot reuse derived cache");
		test.run_effect_and_decision();

		test.Projections.Throw = true;
		test.request(comparison_key({ }, 32U));
		test.run_effect_and_decision();
		expect(test.Host->projection_delivery_failed(),
			   "projection exception is contained and recorded");
	}

	auto test_exact_epoch_cache_and_pending_invalidation() -> void
	{
		fixture    test;
		const auto key          = comparison_key();
		const auto epoch        = core::review_epoch_id{ key.LeftAnchor.Authority,
														 key.LeftAnchor.Incarnation, key.LeftAnchor.Epoch };
		auto       other        = key;
		other.LeftAnchor.Epoch  = core::epoch_ordinal{ 3U };
		other.RightAnchor.Epoch = core::epoch_ordinal{ 4U };
		test.request(key);
		test.run_effect_and_decision();
		test.request(other);
		const auto pending = test.Host->state().Pending;
		test.Host->invalidate(epoch);
		expect(test.Host->cache_entries() == 0U && test.Host->cache_bytes() == 0U
				   && !test.Host->state().Current && test.Host->state().Pending == pending
				   && test.Projections.Invalidated.empty(),
			   "pruned current cache is released without invalidating the newer projection");
		test.run_effect_and_decision();
		expect(test.Host->state().Current == pending && test.Host->cache_entries() == 1U,
			   "unrelated pending work completes and enters its cache");
		const auto otherBytes = test.Host->cache_bytes();
		test.request(key);
		test.Effects.run_next();
		test.Host->invalidate(epoch);
		test.Decisions.run_next();
		expect(test.Host->cache_entries() == 1U && test.Host->cache_bytes() == otherBytes
				   && test.Host->state().Current == pending && !test.Host->state().Pending
				   && test.Projections.Results.size() == 2U
				   && test.Projections.Invalidated == std::vector{ key.Authority },
			   "completed but undelivered pruned work cannot publish or reinsert cache");
		test.request(other);
		expect(test.Effects.pending() == 0U && test.Decisions.pending() == 1U,
			   "unrelated epoch cache remains reusable");
		test.Decisions.run_next();
		expect(test.Projections.Results.back().CacheHit, "unrelated cache hit still publishes");
		test.request(key);
		test.Host->invalidate(epoch);
		test.run_effect_and_decision();
		expect(test.Host->outstanding_jobs() == 0U && test.Host->cache_bytes() == otherBytes,
			   "cancelled prune work drains exactly once without retaining cache");
	}

	auto test_projection_reentry_is_deferred() -> void
	{
		fixture    test;
		const auto key                = comparison_key();
		auto       requestWasDeferred = false;
		test.Projections.OnPublish    = [&test, &key, &requestWasDeferred] {
			test.Host->request(key, { });
			requestWasDeferred = !test.Host->state().Pending.has_value();
			test.Host->invalidate(key.Authority);
		};
		test.request(key);
		test.run_effect_and_decision();
		expect(requestWasDeferred && !test.Projections.NestedDelivery
				   && test.Projections.Invalidated == std::vector{ key.Authority }
				   && test.Host->cache_entries() == 0U
				   && test.Decisions.pending() == 1U,
			   "projection feedback waits until the active delivery returns");
		test.Decisions.run_next();
		expect(test.Projections.Results.size() == 1U
				   && test.Projections.Failures.empty(),
			   "deferred invalidation makes the queued cache terminal stale");
	}

	auto test_persistent_projection_feedback_yields_between_turns() -> void
	{
		fixture    test;
		const auto authority = comparison_key().Authority;
		auto       recurring = std::function<void()>{ };
		recurring            = [&test, &authority, &recurring] {
			test.Projections.OnInvalidate = recurring;
			test.Host->invalidate(authority);
		};
		test.Projections.OnPublish = [&test, &authority, &recurring] {
			test.Projections.OnInvalidate = recurring;
			test.Host->invalidate(authority);
		};
		test.request(comparison_key());
		test.run_effect_and_decision();
		expect(test.Projections.Invalidated.size() == 1U
				   && test.Decisions.pending() == 1U
				   && !test.Projections.NestedDelivery,
			   "feedback created during the snapshot yields to another decision turn");
		test.Decisions.run_next();
		expect(test.Projections.Invalidated.size() == 2U
				   && test.Decisions.pending() == 1U,
			   "persistent feedback remains one bounded scheduled turn");
		test.Projections.OnInvalidate = { };
		test.Decisions.run_next();
		expect(test.Projections.Invalidated.size() == 3U
				   && test.Decisions.pending() == 0U,
			   "feedback chain stops without stranded deferred ingress");
	}

	auto test_pending_effect_is_safe_after_host_destruction() -> void
	{
		fake_blobs                   blobs;
		application::manual_executor effects;
		application::manual_executor decisions;
		recording_projection         projections;
		{
			application::diff_application_host host{
				core::diff_state{
					.ApplicationSession = core::application_session_id{ 73U },
				},
				blobs,
				effects,
				decisions,
				projections,
			};
			host.request(comparison_key(), { });
			expect(effects.pending() == 1U,
				   "fixture destroys host with one pending effect");
		}
		effects.drain();
		decisions.drain();
		expect(projections.Results.empty() && projections.Failures.empty(),
			   "expired completion gate cannot target destroyed host state");
	}

	// --- BPR-0532: re-alignment against a kept session's own epoch pair ---

	[[nodiscard]] constexpr auto commit(const std::uint8_t aSeed)
		-> core::git_commit_oid
	{
		std::array<std::uint8_t, 32U> raw{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			raw[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, raw };
	}

	/// The tree lookup the workbench resolves selected files through, scripted.
	class fake_selected_files final
		: public application::workbench_selected_diff_port
	{
	public:
		[[nodiscard]] auto context(
			const application::workbench_pull_request_authority&) const
			-> std::optional<application::workbench_selected_diff_context> override
		{
			return std::nullopt;
		}

		[[nodiscard]] auto resolve(
			const application::workbench_selected_diff_request& aRequest,
			const std::stop_token)
			-> application::workbench_selected_diff_observation override
		{
			Asked.push_back(aRequest);
			return application::workbench_selected_diff_observation{
				.Request   = aRequest,
				.Status    = Status,
				.LeftBlob  = Status == application::workbench_selected_diff_status::available
					? std::optional{ blob(21U) }
					: std::nullopt,
				.RightBlob = Status == application::workbench_selected_diff_status::available
					? std::optional{ blob(51U) }
					: std::nullopt,
			};
		}

		std::vector<application::workbench_selected_diff_request> Asked;
		application::workbench_selected_diff_status               Status
			= application::workbench_selected_diff_status::available;
	};

	[[nodiscard]] auto realignment_request(const std::size_t aFiles = 1U)
		-> application::agent_realignment_request
	{
		const auto key   = comparison_key();
		auto       entry = application::workbench_file_observation_entry{
			.File             = key.File,
			.PresentationPath = "src/kept.txt",
			.CurrentPath      = std::filesystem::path{ "src/kept.txt" },
			.Change           = application::workbench_file_change_kind::modified,
			.ContentKind      = application::workbench_file_content_kind::text,
		};
		auto request = application::agent_realignment_request{
			.Binding = core::agent_revision_binding{
				.Host        = key.Authority.Host,
				.Repository  = key.Repository,
				.PullRequest = key.PullRequest,
				.BaseOid     = commit(7U),
				.HeadOid     = commit(9U),
				.LeftEpoch   = core::epoch_ordinal{ 4 },
				.RightEpoch  = core::epoch_ordinal{ 5 },
			},
			.Context = application::workbench_request_context{
				.Request    = core::request_id{ 3U },
				.Session    = core::application_session_id{ 1U },
				.Generation = core::request_generation{ 2U },
			},
			.WorkbenchAuthority = { },
			.Diff               = application::workbench_selected_diff_context{
				.ApplicationSession = core::application_session_id{ 1U },
				.Authority          = key.Authority,
				.Comparison         = core::comparison_pair{
					.Left = core::comparison_anchor{
						.Identity = key.LeftAnchor,
						.Commit   = commit(1U),
					},
					.Right = core::comparison_anchor{
						.Identity = key.RightAnchor,
						.Commit   = commit(2U),
					},
				},
				.ComparisonRevision = core::comparison_revision{ 6 },
				.Options            = core::diff_options{ },
				.OptionsDigest      = key.Options,
				.Algorithm          = key.Algorithm,
			},
			.Files   = { },
			.Options = core::diff_options{ },
			.Limits  = core::diff_compute_limits{ },
		};
		for (std::size_t index = 0U; index < aFiles; ++index) {
			request.Files.push_back(entry);
		}
		return request;
	}

	void realignment_freezes_the_bound_revisions_own_identities()
	{
		auto       files     = fake_selected_files{ };
		auto       blobs     = fake_blobs{ };
		auto       realigner = application::composed_agent_realignment{ files, blobs };
		const auto request   = realignment_request();
		const auto outcome   = realigner.realign(request, std::stop_token{ });
		expect(
			outcome.Refusal == application::agent_realignment_refusal::none,
			"a retained revision re-aligns");
		expect(outcome.Files.size() == 1U, "one selected file, one frozen file");
		expect(
			outcome.Files.front().Path == "src/kept.txt",
			"the frozen file is the one that was asked about");
		expect(!outcome.Files.front().Segments.empty(), "identities were frozen");

		// The lookup names the session's pair and nothing of the current one.
		expect(files.Asked.size() == 1U, "one lookup per file");
		const auto& asked = files.Asked.front();
		expect(
			asked == application::realignment_lookup(request, request.Files.front()),
			"the lookup is exactly the retargeted request");
		expect(
			asked.Diff.Comparison.Left.Commit == request.Binding.BaseOid
				&& asked.Diff.Comparison.Right.Commit == request.Binding.HeadOid
				&& asked.Diff.Comparison.Left.Identity.Epoch == request.Binding.LeftEpoch
				&& asked.Diff.Comparison.Right.Identity.Epoch
					== request.Binding.RightEpoch,
			"the bound revision's pair is what was resolved");
		expect(
			asked.Diff.Authority == request.Diff.Authority
				&& asked.Diff.OptionsDigest == request.Diff.OptionsDigest
				&& asked.Diff.Algorithm == request.Diff.Algorithm,
			"only the pair is retargeted");

		// The identities are the ones the one alignment computation produces
		// for that pair - not a second algorithm's answer about it.
		const auto left       = blobs.LeftBytes;
		const auto right      = blobs.RightBytes;
		auto       key        = comparison_key();
		key.LeftAnchor.Epoch  = request.Binding.LeftEpoch;
		key.RightAnchor.Epoch = request.Binding.RightEpoch;
		const auto computed   = core::compute_file_diff(core::diff_compute_input{
			.Key        = key,
			.Options    = request.Options,
			.LeftBytes  = std::optional{ std::span<const std::uint8_t>{ left } },
			.RightBytes = std::optional{ std::span<const std::uint8_t>{ right } },
			.Limits     = request.Limits,
		});
		expect(computed.Result.has_value(), "the reference alignment computes");
		expect(
			outcome.Files.front().Comparison == computed.Result->Alignment.Digest,
			"the frozen comparison is that pair's own");
		expect(
			outcome.Files.front().Segments.size()
					== computed.Result->Alignment.Segments.size()
				&& outcome.Files.front().Groups.size()
					== computed.Result->Alignment.Groups.size(),
			"the frozen identities are that pair's own");
	}

	void realignment_refuses_when_the_bound_revisions_objects_are_gone()
	{
		auto files     = fake_selected_files{ };
		auto blobs     = fake_blobs{ };
		auto realigner = application::composed_agent_realignment{ files, blobs };
		files.Status   = application::workbench_selected_diff_status::both_missing;
		auto outcome   = realigner.realign(realignment_request(), std::stop_token{ });
		expect(
			outcome.Refusal
				== application::agent_realignment_refusal::objects_unavailable,
			"a tree that no longer answers is objects_unavailable");
		expect(outcome.Files.empty(), "a refused re-alignment freezes nothing");
		expect(blobs.Requests.empty(), "no blob is read once the tree refused");

		// Resolved, then the object itself is gone from the retained cache.
		files.Status = application::workbench_selected_diff_status::available;
		blobs.Status = application::diff_blob_status::missing;
		outcome      = realigner.realign(realignment_request(), std::stop_token{ });
		expect(
			outcome.Refusal
				== application::agent_realignment_refusal::objects_unavailable,
			"a missing blob is objects_unavailable");
		expect(outcome.Files.empty(), "a refused re-alignment freezes nothing");
		expect(
			std::string_view{ application::stable_name(outcome.Refusal) }
				== "objects_unavailable",
			"the refusal names itself");
	}

	void realignment_refuses_past_its_bound_rather_than_blocking()
	{
		auto       files     = fake_selected_files{ };
		auto       blobs     = fake_blobs{ };
		auto       realigner = application::composed_agent_realignment{ files, blobs };
		const auto over
			= realigner.realign(
				realignment_request(application::maximum_agent_realignment_files + 1U),
				std::stop_token{ });
		expect(
			over.Refusal
				== application::agent_realignment_refusal::file_limit_exceeded,
			"a request over the file bound refuses");
		expect(
			files.Asked.empty() && blobs.Requests.empty(),
			"the bound is checked before any effect runs");

		// The size bound a fresh alignment already has, on the same request.
		auto bounded                       = realignment_request();
		bounded.Limits.MaximumBytesPerSide = 1U;
		blobs.DeclaredBlobBytes            = 4096U;
		const auto refused                 = realigner.realign(bounded, std::stop_token{ });
		expect(
			refused.Refusal == application::agent_realignment_refusal::compute_limit,
			"an oversize side refuses at the alignment's own bound");
		expect(refused.Files.empty(), "a bounded refusal freezes nothing");
		expect(
			blobs.RefusedBeforeAllocation,
			"the bound refuses before the bytes are held");
	}

	void test_pathological_diagnostic_content()
	{
		constexpr auto marker = "BPR_DIAGNOSTIC_PRIVATE_CANARY diff source\n";
		for (const bool oversized : { false, true }) {
			auto limits                = core::diff_compute_limits{ };
			limits.MaximumBytesPerSide = 4096U;
			fixture test{ { }, limits };
			auto    content = std::string{ marker };
			if (oversized) {
				content.append(1024U * 1024U, 'x');
			}
			test.Blobs.LeftBytes  = bytes(content);
			test.Blobs.RightBytes = bytes(content);
			test.request(comparison_key());
			test.run_effect_and_decision();
			expect(oversized ? test.Projections.Failures.size() == 1U : test.Projections.Results.size() == 1U, "hostile source retains one typed diff terminal");
			test.Host->shutdown();
			expect(test.Host->is_quiescent(), "hostile diff source drains completely");
		}
	}

} // namespace

int main()
{
	test_pathological_diagnostic_content();
	test_worker_and_cache_lanes();
	test_exact_cache_key_collisions();
	test_reordered_generation_and_stale_completion();
	test_invalid_replacement_supersedes_pending_work();
	test_reordered_exact_identity_boundaries();
	test_cancellation_and_shutdown_stages();
	test_job_and_cache_bounds();
	test_terminal_queue_is_bounded();
	test_absent_and_empty_sides();
	test_typed_failures_and_exact_echo();
	test_invalidation_and_projection_exception();
	test_exact_epoch_cache_and_pending_invalidation();
	test_projection_reentry_is_deferred();
	test_persistent_projection_feedback_yields_between_turns();
	test_pending_effect_is_safe_after_host_destruction();
	realignment_freezes_the_bound_revisions_own_identities();
	realignment_refuses_when_the_bound_revisions_objects_are_gone();
	realignment_refuses_past_its_bound_rather_than_blocking();
	test_a_prepared_comparison_opens_without_reading_or_computing();
	test_preparation_yields_to_the_reviewer();
	test_opening_the_file_being_prepared_waits_for_it();
	test_preparation_stays_inside_its_share_and_never_evicts_the_reviewer();
	test_preparation_is_cancelled_with_its_authority();
	test_a_large_window_is_bounded();
	return EXIT_SUCCESS;
}
