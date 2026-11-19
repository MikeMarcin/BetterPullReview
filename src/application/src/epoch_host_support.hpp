#ifndef BPR_SRC_APPLICATION_SRC_EPOCH_HOST_SUPPORT_HPP
#define BPR_SRC_APPLICATION_SRC_EPOCH_HOST_SUPPORT_HPP
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_pack_stream.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/profile_epoch_actor_registry.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/epoch_model.hpp"

#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

#if defined(BPR_APPLICATION_TESTING)
	#include <atomic>
#endif

// Helpers the epoch application host's sources share.
namespace bpr::application {
#if defined(BPR_APPLICATION_TESTING)
	inline std::atomic<int> ArmedAdmissionStage{ -1 };
#endif
	inline constexpr std::size_t maximum_identity_attempts = 32U;

	[[nodiscard]] auto validated_limits(const epoch_host_limits aLimits)
		-> epoch_host_limits;

	using cache_actor_key = profile_epoch_actor_key;

	[[nodiscard]] auto actor_key(
		const accepted_pull_request_handoff& aHandoff) noexcept -> cache_actor_key;

	[[nodiscard]] auto actor_key(
		const diff_blob_request& aRequest) noexcept
		-> std::optional<cache_actor_key>;

	[[nodiscard]] auto actor_key(
		const agent_workspace_materialization_request& aRequest) noexcept
		-> std::optional<cache_actor_key>;

	[[nodiscard]] auto actor_key(
		const owned_agent_workspace& aWorkspace) noexcept
		-> std::optional<cache_actor_key>;

	[[nodiscard]] auto actor_key(
		const workbench_selected_diff_request& aRequest) noexcept
		-> std::optional<cache_actor_key>;

	template <typename Identity>
	[[nodiscard]] auto mint_identity(epoch_entropy_port& aEntropy)
		-> std::optional<Identity>
	{
		const auto bytes = aEntropy.generate();
		if (!bytes || !application::valid(*bytes)) {
			return std::nullopt;
		}
		return Identity{ *bytes };
	}

	// Captured by one bounded worker job; no host or presentation lifetime is retained.
	struct epoch_diagnostics
	{
		std::shared_ptr<diagnostic_log_port> Log;
		std::uint64_t                        Request       = 0U;
		std::uint64_t                        WindowSession = 0U;
		std::uint64_t                        Generation    = Log ? Log->context_generation() : 0U;

		void record(diagnostic_epoch_stage aStage, diagnostic_outcome aOutcome,
					diagnostic_epoch_failure               aFailure = { },
					std::optional<git_pack_stream_failure> aPack    = { },
					git_smart_transport_step               aStep    = git_smart_transport_step::none) const noexcept;
		template <typename Failure>
		void failed(diagnostic_epoch_stage aStage, const Failure aFailure) const noexcept
		{
			record(aStage, diagnostic_outcome::failed, aFailure);
		}
		void failed(diagnostic_epoch_stage aStage, const git_cache_error& aFailure) const noexcept;
		void failed(diagnostic_epoch_stage aStage, const git_smart_transport_error& aFailure) const noexcept;
		void failed(diagnostic_epoch_stage aStage, const git_protocol_v2_error& aFailure) const noexcept;
		template <typename... Failure>
		void failed(diagnostic_epoch_stage aStage, const std::variant<Failure...>& aFailure) const noexcept
		{
			std::visit([&](const auto& aValue) { failed(aStage, aValue); }, aFailure);
		}
		/// \brief [[BPR-2290]]. As `perform`, and the ending line's detail is
		/// `aSummary(result)` with the effect's duration: enums and counts only.
		template <typename Effect, typename Summary>
		auto perform(diagnostic_epoch_stage aStage, Effect&& aEffect, Summary&& aSummary) const -> decltype(aEffect())
		{
			const auto began  = std::chrono::steady_clock::now();
			auto       result = perform_detailed(aStage, std::forward<Effect>(aEffect), [&](const auto& aResult) {
				return aSummary(aResult) + " elapsed_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count());
			});
			return result;
		}
		template <typename Effect>
		auto perform(diagnostic_epoch_stage aStage, Effect&& aEffect) const -> decltype(aEffect())
		{
			record(aStage, diagnostic_outcome::started);
			try {
				auto result = aEffect();
				if (result) {
					record(aStage, diagnostic_outcome::succeeded);
				} else {
					failed(aStage, result.error());
				}
				return result;
			} catch (...) {
				failed(aStage, core::epoch_failure::unavailable);
				throw;
			}
		}

		template <typename Effect, typename Detail>
		auto perform_detailed(diagnostic_epoch_stage aStage, Effect&& aEffect, Detail&& aDetail) const -> decltype(aEffect())
		{
			record(aStage, diagnostic_outcome::started);
			try {
				auto result   = aEffect();
				PendingDetail = aDetail(result);
				if (result) {
					record(aStage, diagnostic_outcome::succeeded);
				} else {
					failed(aStage, result.error());
				}
				PendingDetail.clear();
				return result;
			} catch (...) {
				PendingDetail.clear();
				failed(aStage, core::epoch_failure::unavailable);
				throw;
			}
		}

		/// [[BPR-2290]]. The ending line's detail while `perform_detailed`
		/// records it; empty otherwise.
		mutable std::string PendingDetail = { };
	};

	/// \brief [[BPR-2290]]. A historical acquisition's line detail: its mode,
	/// where its objects came from, its haves and the pack's size.
	[[nodiscard]] auto historical_acquisition_summary(const git_smart_request_context& aContext, const git_smart_transport_port& aTransport,
													  std::size_t aHaves, const git_protocol_v2_client_result<git_protocol_v2_acquisition_result>& aResult)
		-> std::string;

	/// \brief [[BPR-2290]]. A size or time wall: trying the next background
	/// historical import would start the same kind of clone again.
	[[nodiscard]] constexpr auto historical_wall(const git_smart_transport_failure aFailure) noexcept -> bool
	{
		return aFailure == git_smart_transport_failure::timeout || aFailure == git_smart_transport_failure::response_limit;
	}

	[[nodiscard]] auto catalog_failure(
		const epoch_catalog_failure aFailure) noexcept -> core::epoch_failure;

	[[nodiscard]] auto cache_failure(const git_cache_error& aFailure) noexcept
		-> core::epoch_failure;

	[[nodiscard]] auto evidenced_live_update(const core::epoch_cause& aCause) noexcept -> bool;

	// One exact bounded scan detects previously observed heads. Failed or
	// capped history cannot prove that an observed-head key is unused.
	[[nodiscard]] auto read_occurrence_epochs(epoch_catalog_session& aCatalog,
											  const std::stop_token  aStop) -> std::optional<std::vector<core::review_epoch_summary>>;

	[[nodiscard]] auto record_live_occurrence(
		const accepted_pull_request_handoff&              aHandoff,
		core::epoch_occurrence_history                    aHistory,
		const core::review_epoch_summary&                 aEpoch,
		const std::span<const core::review_epoch_summary> aEpochs)
		-> core::epoch_occurrence_history;

	[[nodiscard]] auto reconcile_local_occurrences(const accepted_pull_request_handoff& aHandoff,
												   core::epoch_occurrence_history aHistory, epoch_catalog_session& aCatalog,
												   const std::optional<core::review_epoch_summary>& aCurrent, const std::stop_token aStop)
		-> core::epoch_occurrence_history;

	void enrich_commit_dates(core::epoch_occurrence_history& aHistory,
							 git_cache_session* const aCache, const std::stop_token aStop);

	[[nodiscard]] auto invalidation_identity(
		const accepted_pull_request_handoff& aHandoff)
		-> pull_request_authority_invalidation;

	[[nodiscard]] auto recovery_request(const append_operation_record& aRecord)
		-> std::optional<git_operation_recovery_request>;

	[[nodiscard]] auto expected_retained_pair(
		const append_operation_record& aRecord) -> git_retained_ref_pair;

	[[nodiscard]] auto same_append_identity(
		const append_operation_record& aPrior,
		const append_operation_record& aNext) noexcept -> bool;

	[[nodiscard]] auto exact_catalog_commit(
		const catalog_commit_result&               aResult,
		const core::epoch_catalog_identity         aPrior,
		const std::optional<core::review_epoch_id> aInserted) noexcept -> bool;
}
#endif
