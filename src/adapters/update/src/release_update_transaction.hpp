#ifndef BPR_SRC_ADAPTERS_UPDATE_SRC_RELEASE_UPDATE_TRANSACTION_HPP
#define BPR_SRC_ADAPTERS_UPDATE_SRC_RELEASE_UPDATE_TRANSACTION_HPP

#include "bpr/application/release_update_port.hpp"

#include <filesystem>
#include <functional>

namespace bpr::adapters::update::detail {

	/// \brief Native transaction facts. Paths never cross the application-facing port.
	struct install_material
	{
		std::filesystem::path Path;
		std::string           Identity;
	};
	[[nodiscard]] auto install_source(std::shared_ptr<const application::release_update_verified_bundle> aBundle,
									  std::stop_token                                                    aStop) -> std::optional<install_material>;
	[[nodiscard]] auto install_tree_identity(const std::filesystem::path& aPath) -> std::optional<std::string>;
	[[nodiscard]] auto validate_install_tree(const std::filesystem::path& aPath, const std::filesystem::path& aInstalled,
											 const core::release_update_candidate& aCandidate, std::stop_token aStop) -> core::release_update_reason;
	[[nodiscard]] auto trusted_install_helper(const std::filesystem::path& aBundle, const std::filesystem::path& aHelper) -> bool;

	/// \brief Effect capability used by the journal engine. Production binds native trust and exact launch.
	class install_effects
	{
	public:
		virtual ~install_effects()                                                                                                               = default;
		[[nodiscard]] virtual auto validate(const std::filesystem::path& aBundle,
											const std::filesystem::path& aTrustSource, const core::release_update_candidate& aCandidate) -> bool = 0;
		[[nodiscard]] virtual auto launch(const std::filesystem::path& aExecutable, std::string_view aTransaction) -> bool                       = 0;
	};

	enum class install_boundary : std::uint8_t {
		prepared,
		before_old_rename,
		after_old_rename,
		before_new_rename,
		after_new_rename,
		before_launch,
		after_launch,
		healthy,
		before_cleanup,
		complete,
		before_rollback,
		recovered,
		cleanup_entry
	};

	/// \brief Journaled same-volume replacement; all operations run on an effect lane.
	/// \details An observer may stop at any durable boundary for crash qualification.
	/// It never changes validation or ownership. A missing observer always continues.
	class install_transaction final
	{
	public:
		using boundary_observer = std::function<bool(install_boundary)>;
		install_transaction(std::filesystem::path aInstalled, install_effects& aEffects,
							boundary_observer aObserver = { });
		~install_transaction();
		install_transaction(const install_transaction&)                                  = delete;
		auto               operator=(const install_transaction&) -> install_transaction& = delete;
		[[nodiscard]] auto prepare(const std::filesystem::path& aVerified, const core::release_update_candidate& aCandidate, const std::function<bool()>& aAuthorized = [] { return true; }, const std::filesystem::path& aApplicationRoot = { }) -> core::release_update_reason;
		[[nodiscard]] auto replace() -> core::release_update_reason;
		[[nodiscard]] auto resume(std::string_view aTransaction) -> bool;
		[[nodiscard]] auto abort_prepared() -> core::release_update_reason;
		[[nodiscard]] auto recover() -> core::release_update_reason;
		[[nodiscard]] auto finish() -> core::release_update_reason;
		[[nodiscard]] auto identity() const -> std::string;
		[[nodiscard]] auto control_path() const -> std::filesystem::path;
		[[nodiscard]] auto transaction_path() const -> std::filesystem::path;

	private:
		struct implementation;
		std::unique_ptr<implementation> Impl;
	};
}
#endif
