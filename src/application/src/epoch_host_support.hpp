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

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
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
	};

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
