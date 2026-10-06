#ifndef BPR_SRC_APP_SRC_GITHUB_PUBLIC_PATH_FIXTURE_HPP
#define BPR_SRC_APP_SRC_GITHUB_PUBLIC_PATH_FIXTURE_HPP

#if !defined(BPR_BUILD_TESTING)
	#error "The GitHub public-path fixture is test-build only"
#endif

#include "github_application_fixture_access.hpp"

#include "github_host_profile.hpp"
#include "github_provider.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_synchronization.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace bpr::app::detail {
	/// \brief Exercises renewal inside the unlocked historical script compilation window.
	[[nodiscard]] auto verify_large_review_history_session_interleaving() -> bool;
	class github_public_path_fixture_components final
	{
	public:
		github_public_path_fixture_components(
			github_public_path_fixture                             aFixture,
			const adapters::github::validated_github_host_profile& aProfile,
			application::effect_executor&                          aVaultEffects,
			github_public_path_fixture_options                     aOptions);
		~github_public_path_fixture_components();

		github_public_path_fixture_components(
			const github_public_path_fixture_components&) = delete;
		auto operator=(const github_public_path_fixture_components&)
			-> github_public_path_fixture_components& = delete;
		github_public_path_fixture_components(
			github_public_path_fixture_components&&) = delete;
		auto operator=(github_public_path_fixture_components&&)
			-> github_public_path_fixture_components& = delete;

		[[nodiscard]] auto transport_factory() const noexcept
			-> std::shared_ptr<
				adapters::github::detail::github_wire_transport_factory_port>;
		[[nodiscard]] auto credential_vault() const noexcept
			-> application::github_credential_vault_port&;
		[[nodiscard]] auto take_enrollment_credential()
			-> application::github_credential_material;
		/// \brief The recorded device-grant transport, for the registered course.
		[[nodiscard]] auto device_authorization_transport() noexcept
			-> application::device_authorization_transport_port&;
		/// \brief The access token the recorded grant issued, to scan for.
		///
		/// \returns Empty until the grant has been exchanged. It is the exact
		/// value the vault should hold and the exact value nothing else should.
		[[nodiscard]] auto device_authorization_access_token() const
			-> std::string;
		[[nodiscard]] auto evidence() const noexcept
			-> github_public_path_fixture_evidence;
		[[nodiscard]] auto last_smart_authority() const noexcept
			-> std::optional<core::pull_request_authority>;
		/// \brief The recorded credential-free Git smart factory ([[BPR-0715]]).
		[[nodiscard]] auto anonymous_smart_transport_factory() const noexcept
			-> std::shared_ptr<application::git_smart_transport_factory_port>;
		[[nodiscard]] auto application_data_root() const
			-> const std::filesystem::path&;
		[[nodiscard]] auto git_executable() const
			-> const std::filesystem::path&;
		void prepare_review_mutation(core::review_mutation_target aTarget);
		/// Change only the fake server's next read, never the accepted host state.
		void stage_server_conversation(std::string aThread, bool aAddedAndResolved);
		/// [[BPR-1085]]. Change one thread's root body, resolution or root author
		/// on the fake server's next read; an absent field keeps its value.
		void               stage_server_root(std::string aThread, std::optional<std::string> aRootBody, std::optional<bool> aResolved,
											 std::optional<std::pair<std::string, std::string>> aRootAuthor = std::nullopt);
		void               report_review_mutation_target(core::review_mutation_target aTarget);
		void               set_private_lineage(std::shared_ptr<application::review_publication_lineage_port>);
		[[nodiscard]] auto review_mutation_provider() noexcept
			-> application::review_mutation_port&;
		[[nodiscard]] auto review_comments() const
			-> std::vector<application::review_comment_projection>;
		[[nodiscard]] auto review_mutation_evidence() const noexcept
			-> review_mutation_fixture_evidence;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APP_SRC_GITHUB_PUBLIC_PATH_FIXTURE_HPP
