#ifndef BPR_SRC_APPLICATION_SRC_REVIEW_MUTATION_EXECUTION_AUTHORITY_HPP
#define BPR_SRC_APPLICATION_SRC_REVIEW_MUTATION_EXECUTION_AUTHORITY_HPP

#include "bpr/application/review_mutation_application_host.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application::detail {
	/// \brief Issues a short-lived grant for one accepted normal Publish gesture.
	class review_mutation_user_authority_issuer final
	{
	public:
		review_mutation_user_authority_issuer() = delete;
		[[nodiscard]] static auto issue(core::application_session_id aSession,
										core::request_generation aGeneration, review_mutation_target_binding aTarget,
										review_mutation_acting_identity aActor, review_mutation_publication_request aRequest)
			-> review_mutation_execution_grant;
		/// \brief [[BPR-0657]]. The one-shot grant for one exact thread resolution.
		[[nodiscard]] static auto issue_thread(core::application_session_id aSession,
											   core::request_generation aGeneration, review_mutation_target_binding aTarget,
											   review_mutation_acting_identity aActor, review_thread_resolution_publication_request aRequest)
			-> review_mutation_execution_grant;
		/// \brief [[BPR-PENDING-AR1]]. The one-shot grant for one exact reaction change.
		[[nodiscard]] static auto issue_reaction(core::application_session_id aSession,
												 core::request_generation aGeneration, review_mutation_target_binding aTarget,
												 review_mutation_acting_identity aActor, review_reaction_publication_request aRequest)
			-> review_mutation_execution_grant;
	};

	enum class designated_review_bootstrap_failure : std::uint8_t {
		invalid_configuration,
		artifact_absent,
		unsafe_parent,
		unsafe_artifact,
		artifact_limit,
		malformed_approval,
		approval_mismatch,
		approval_digest_mismatch,
		executable_unavailable,
		executable_limit,
		public_observation,
		artifact_substituted,
		artifact_replayed,
		spent_state_failure,
		file_sync_failure,
		directory_sync_failure,
		consume_failure,
		replay_state_absent,
		unsafe_replay_state,
		replay_state_mismatch,
		replay_state_capacity,
		replay_state_substituted,
		platform_unavailable,
	};

	enum class designated_review_public_observation_failure : std::uint8_t {
		network,
		timeout,
		http,
		redirect,
		malformed,
		response_limit,
		identity_mismatch,
		unavailable,
	};

	class designated_review_public_observation_port
	{
	public:
		virtual ~designated_review_public_observation_port() = default;

		[[nodiscard]] virtual auto observe(
			const review_mutation_target_binding& aExpected)
			-> std::expected<void, designated_review_public_observation_failure>
			= 0;
	};

	inline constexpr auto designated_review_replay_schema
		= std::string_view{ "bpr.designated_live_review_replay.v1" };
	inline constexpr auto designated_review_replay_namespace_child_name
		= std::string_view{ "designated-live-review-replay-state-v1" };
	inline constexpr auto designated_review_replay_marker_capacity
		= std::size_t{ 128U };

	/// \brief Exact schema, normalized path, and random identity of one replay namespace.
	struct designated_review_replay_configuration
	{
		std::filesystem::path NamespacePath;
		std::string           NamespaceId;

		auto operator<=>(const designated_review_replay_configuration&) const = default;
	};

	struct designated_review_bootstrap_request
	{
		std::filesystem::path ApprovalPath;
		std::filesystem::path ExecutablePath;
		std::string           SourceSha256;
		std::string           BuildConfiguration;
		/// \brief The stage-one supervisor this application was launched by.
		///
		/// \details BPR-0262 fact_5. Without it an approval authorizes an
		/// application digest while saying nothing about which supervisor it
		/// was granted against, so the same approval would be equally valid
		/// under a different one. The application computes this itself, from
		/// the fixed position the supervisor occupies inside its own capsule;
		/// it is not told, because being told would make it a claim.
		std::string SupervisorSha256;
		/// \brief The capsule inventory the supervisor verified.
		///
		/// \details The other half of fact_5. The inventory cannot live inside
		/// the capsule it describes — it would have to cover itself — so the
		/// application cannot compute this one and the supervisor supplies it.
		/// That is safe in the direction that matters: a supervisor that
		/// supplied the wrong digest would render a subject the operator never
		/// approved, and the approval would be refused. It can cause a refusal,
		/// never an acceptance.
		std::string                            InventorySha256;
		designated_review_replay_configuration Replay;
		bool                                   BuildTesting = true;

		auto operator<=>(const designated_review_bootstrap_request&) const = default;
	};

	/// \brief Owns explicit creation/reset and read-only loading of replay state.
	class designated_review_replay_initializer final
	{
	public:
		designated_review_replay_initializer() = delete;

		/// \brief Creates missing configured directories or resets to a fresh identity.
		[[nodiscard]] static auto initialize_or_reset(
			const std::filesystem::path& aNamespacePath)
			-> std::expected<
				designated_review_replay_configuration,
				designated_review_bootstrap_failure>;

		/// \brief Loads safe existing state without creating or repairing it.
		[[nodiscard]] static auto load(
			const std::filesystem::path& aNamespacePath)
			-> std::expected<
				designated_review_replay_configuration,
				designated_review_bootstrap_failure>;
	};

	[[nodiscard]] auto designated_review_approval_subject(
		const designated_review_bootstrap_request& aRequest,
		std::string_view                           aApplicationSha256,
		std::string_view                           aApprovalId)
		-> std::expected<std::string, designated_review_bootstrap_failure>;

	class designated_review_authority_bootstrap final
	{
	public:
		designated_review_authority_bootstrap() = delete;

		[[nodiscard]] static auto run(
			const designated_review_bootstrap_request& aRequest,
			designated_review_public_observation_port& aObservation)
			-> std::expected<
				review_mutation_execution_grant,
				designated_review_bootstrap_failure>;
	};

#if defined(BPR_APPLICATION_TESTING)
	void fail_designated_review_fsync_after_for_testing(std::uint32_t aCalls) noexcept;
	/// \brief Post-sync record mutation injected by the bootstrap test target.
	enum class designated_review_marker_mutation_for_testing : std::uint8_t {
		none,
		unlink,
		substitute,
		hard_link,
	};
	void mutate_designated_review_marker_after_sync_for_testing(
		designated_review_marker_mutation_for_testing aMutation) noexcept;
#endif
} // namespace bpr::application::detail

namespace bpr::application {
	class review_mutation_execution_authority final
	{
	public:
		review_mutation_execution_authority(
			const review_mutation_execution_authority&) = delete;
		auto operator=(const review_mutation_execution_authority&)
			-> review_mutation_execution_authority& = delete;
		~review_mutation_execution_authority()      = default;

	private:
		enum class kind : std::uint8_t {
			testing_reusable,
			attended_one_shot,
			user_one_shot,
		};

		explicit review_mutation_execution_authority(
			review_mutation_execution_scope                    aScope,
			std::optional<review_mutation_publication_request> aRequest = std::nullopt);
		review_mutation_execution_authority(
			review_mutation_target_binding        aTarget,
			std::chrono::steady_clock::time_point aExpiresAt) noexcept;

		[[nodiscard]] auto bind(
			const review_mutation_execution_scope& aScope) noexcept -> bool;
		[[nodiscard]] auto permits(
			const review_mutation_execution_scope&     aScope,
			const review_mutation_publication_request& aRequest) noexcept -> bool;
		// [[BPR-0657]]. A thread resolution binds and is permitted on the same
		// session, generation, target and actor as a publication; a user
		// one-shot grant permits only the exact request it was issued for.
		explicit review_mutation_execution_authority(
			review_mutation_thread_scope                 aScope,
			review_thread_resolution_publication_request aRequest);
		[[nodiscard]] auto bind_thread(const review_mutation_thread_scope& aScope) noexcept -> bool;
		[[nodiscard]] auto permits_thread(
			const review_mutation_thread_scope&                 aScope,
			const review_thread_resolution_publication_request& aRequest) noexcept -> bool;
		// [[BPR-PENDING-AR1]]. A reaction binds and is permitted like a
		// resolution, its subject in the scope's `Thread`; a user one-shot grant
		// permits only the exact change it was issued for.
		explicit review_mutation_execution_authority(
			review_mutation_thread_scope        aScope,
			review_reaction_publication_request aRequest);
		[[nodiscard]] auto permits_reaction(
			const review_mutation_thread_scope&        aScope,
			const review_reaction_publication_request& aRequest) noexcept -> bool;
		void revoke() noexcept;

		friend class review_mutation_application_host;
		friend class detail::designated_review_authority_bootstrap;
		friend class detail::review_mutation_user_authority_issuer;
#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
		friend auto make_review_mutation_testing_grant(
			core::application_session_id,
			core::request_generation,
			review_mutation_target_binding,
			review_mutation_acting_identity,
			std::optional<review_mutation_publication_request>)
			-> std::shared_ptr<review_mutation_execution_authority>;
		friend auto make_review_mutation_one_shot_testing_grant(
			review_mutation_target_binding,
			std::chrono::steady_clock::time_point)
			-> std::shared_ptr<review_mutation_execution_authority>;
#endif

		std::optional<review_mutation_publication_request>          Request;
		std::optional<review_thread_resolution_publication_request> ThreadRequest;
		std::optional<review_reaction_publication_request>          ReactionRequest;
		mutable std::mutex                                          Mutex;
		review_mutation_execution_scope                             Scope;
		review_mutation_target_binding                              Target;
		std::optional<review_mutation_acting_identity>              ActingIdentity;
		std::chrono::steady_clock::time_point                       ExpiresAt{ };
		kind                                                        Kind     = kind::testing_reusable;
		bool                                                        Bound    = false;
		std::atomic_bool                                            Consumed = false;
		std::atomic_bool                                            Revoked  = false;
	};
} // namespace bpr::application

#endif
