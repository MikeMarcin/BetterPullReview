#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_REGISTRY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_REGISTRY_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/profile_window.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace bpr::application {
	enum class profile_root_mode : std::uint8_t { legacy,
												  isolated };

	/// \brief Root authority is derived from profile identity, never from its label.
	struct profile_registry_entry
	{
		core::bpr_profile_record             Profile;
		profile_root_mode                    RootMode = profile_root_mode::isolated;
		std::string                          RootIdentity;
		std::vector<core::bpr_window_record> RetainedWindows{ };
		auto                                 operator==(const profile_registry_entry&) const -> bool = default;
	};

	/// \brief Complete bounded registry; adoption records no movement of existing domain data.
	struct profile_registry_snapshot
	{
		std::uint64_t                       Revision       = 1U;
		std::uint64_t                       LastProfileId  = 1U;
		bool                                DefaultAdopted = true;
		std::vector<profile_registry_entry> Profiles{ { { core::default_bpr_profile_id, "Default" }, profile_root_mode::legacy, "legacy" } };
		std::uint64_t                       LastWindowId                                               = 0U;
		auto                                operator==(const profile_registry_snapshot&) const -> bool = default;
	};

	/// \brief Returns the only root authority token permitted for this identity.
	[[nodiscard]] auto profile_root_identity(core::bpr_profile_id aProfile) -> std::string;
	/// \brief Validates core catalog facts and Default-only legacy root authority.
	[[nodiscard]] auto valid(const profile_registry_snapshot& aSnapshot) -> bool;
	/// \brief Checks forward revision, immutable existing identities and monotonic allocation.
	[[nodiscard]] auto valid_profile_registry_update(const profile_registry_snapshot& aBefore,
													 const profile_registry_snapshot& aAfter) -> bool;

	enum class profile_registry_failure : std::uint8_t {
		unavailable,
		cancelled,
		schema_mismatch,
		corrupt,
		conflict,
		busy,
		not_loaded,
		closing,
		invalid_label,
		unknown_profile,
		profile_limit,
		identity_exhausted
	};
	template <typename TValue>
	using profile_registry_result = std::expected<TValue, profile_registry_failure>;

	/// \brief Serialized effect-lane persistence; save is an atomic revision compare-and-set.
	class profile_registry_port
	{
	public:
		virtual ~profile_registry_port()                                                                                                              = default;
		[[nodiscard]] virtual auto load(std::stop_token aStop) -> profile_registry_result<profile_registry_snapshot>                                  = 0;
		[[nodiscard]] virtual auto save(const profile_registry_snapshot& aCandidate,
										std::uint64_t aExpectedRevision, std::stop_token aStop) -> profile_registry_result<profile_registry_snapshot> = 0;
	};

	/// \brief Explicit scope for one terminal; create/load failures have no preexisting profile ID.
	struct profile_registry_publication
	{
		core::application_session_id            Session;
		core::request_id                        Request;
		std::optional<core::bpr_profile_id>     Profile;
		std::optional<profile_registry_failure> Failure;
	};

	class profile_registry_projection_port
	{
	public:
		virtual ~profile_registry_projection_port()                            = default;
		virtual void publish(const profile_registry_publication& aPublication) = 0;
	};

	/// \brief Decision-lane registry owner with one outstanding effect and durable-only acceptance.
	/// \details Public methods and inspection run on the decision lane. Ports and executors
	/// must outlive quiescence. Cancellation clears acceptance and requires reload because
	/// a commit may have completed before cancellation was observed. No default fallback occurs.
	class profile_registry_host final
	{
	public:
		profile_registry_host(profile_registry_port& aStore, effect_executor& aEffects,
							  decision_executor& aDecision, profile_registry_projection_port& aProjection,
							  core::application_session_id aSession);
		~profile_registry_host();
		profile_registry_host(const profile_registry_host&)                                  = delete;
		auto               operator=(const profile_registry_host&) -> profile_registry_host& = delete;
		[[nodiscard]] auto reload() -> profile_registry_result<void>;
		[[nodiscard]] auto create(std::string_view aLabel) -> profile_registry_result<void>;
		[[nodiscard]] auto rename(core::bpr_profile_id aProfile, std::string_view aLabel) -> profile_registry_result<void>;
		/// \brief Saves only validated window metadata and allocation high-water, preserving profile roots and labels.
		[[nodiscard]] auto save_window_state(const core::profile_window_state& aState) -> profile_registry_result<void>;
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto accepted() const noexcept -> const std::optional<profile_registry_snapshot>&;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
