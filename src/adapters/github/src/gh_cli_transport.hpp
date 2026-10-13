#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_TRANSPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_TRANSPORT_HPP

#include "bpr/application/git_smart_transport_port.hpp"
#include "github_cli_process.hpp"
#include "github_host_profile.hpp"
#include "github_mutation_contract.hpp"
#include "github_wire_transport.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	struct gh_cli_macos_environment_facts
	{
		std::filesystem::path HomeDirectory;

		auto operator==(const gh_cli_macos_environment_facts&) const -> bool = default;
	};

	struct gh_cli_linux_environment_facts
	{
		std::filesystem::path HomeDirectory;
		std::filesystem::path RuntimeDirectory;
		std::string           SessionBusAddress;

		auto operator==(const gh_cli_linux_environment_facts&) const -> bool = default;
	};

	struct gh_cli_windows_environment_facts
	{
		std::filesystem::path UserProfileDirectory;
		std::filesystem::path RoamingApplicationDataDirectory;
		std::filesystem::path LocalApplicationDataDirectory;
		std::filesystem::path SystemRootDirectory;

		auto operator==(const gh_cli_windows_environment_facts&) const -> bool = default;
	};

	using gh_cli_platform_environment_facts = std::variant<
		gh_cli_macos_environment_facts,
		gh_cli_linux_environment_facts,
		gh_cli_windows_environment_facts>;

	struct gh_cli_https_environment_facts
	{
		std::optional<std::string>           Proxy;
		std::optional<std::string>           NoProxy;
		std::optional<std::filesystem::path> CertificateFile;
		std::optional<std::filesystem::path> CertificateDirectory;

		auto operator==(const gh_cli_https_environment_facts&) const -> bool = default;
	};

	struct gh_cli_configuration_candidate
	{
		std::filesystem::path             Executable;
		std::filesystem::path             NeutralWorkingDirectory;
		gh_cli_platform_environment_facts Platform;
		gh_cli_https_environment_facts    Https;
	};

	enum class gh_cli_invocation_failure : std::uint8_t {
		profile_transport_mismatch,
		profile_correlation_mismatch,
		server_contract_mismatch,
		invalid_wire_request,
		invalid_route,
		request_limit,
	};

	enum class gh_cli_configuration_failure : std::uint8_t {
		executable_not_absolute,
		executable_not_found,
		executable_not_regular,
		executable_not_executable,
		executable_name_mismatch,
		working_directory_not_absolute,
		working_directory_not_found,
		working_directory_not_directory,
		working_directory_is_repository,
		irrelevant_platform_facts,
		invalid_platform_environment,
		environment_limit,
		invalid_https_proxy,
		credential_bearing_https_proxy,
		invalid_no_proxy,
		invalid_certificate_file,
		invalid_certificate_directory,
	};

	/// \brief The closed set of local `gh` help topics the Git-object path checks.
	/// \details Each names one `gh` operation that path invokes: the REST read
	/// shape, the exact-revision repository clone and the credential helper that
	/// clone launches. A probe runs only `gh <topic> --help`.
	enum class gh_cli_help_topic : std::uint8_t {
		api,
		repo_clone,
		auth_git_credential,
		auth_token,
	};

	inline constexpr std::array gh_cli_required_help_topics{
		gh_cli_help_topic::api,
		gh_cli_help_topic::repo_clone,
		gh_cli_help_topic::auth_git_credential,
		gh_cli_help_topic::auth_token,
	};
	/// \brief Distinguishes a completed named lookup refusal from tool/probe failures.
	struct gh_cli_credential_error
	{
		github_cli_process_failure Failure               = github_cli_process_failure::invalid_request;
		bool                       CredentialUnavailable = false;
	};
	using gh_cli_credential_result = std::expected<cli_transient_credential, gh_cli_credential_error>;

	/// \brief The one `gh` account a pinned configuration's launches use.
	/// \details [[BPR-1373]]. Carries no credential. Every `gh` launch of a
	/// pinned configuration runs through `run_gh_cli`, which hands the child a
	/// token-free private configuration naming only this host and login. One
	/// operation acquires the named credential separately and passes it through
	/// the dedicated process port; the pin itself never holds credential bytes.
	struct gh_cli_account_pin
	{
		std::string Hostname;
		std::string Login;

		auto operator==(const gh_cli_account_pin&) const -> bool = default;
	};

	class validated_gh_cli_configuration final
	{
	public:
		validated_gh_cli_configuration(
			const validated_gh_cli_configuration&) = default;
		validated_gh_cli_configuration(
			validated_gh_cli_configuration&&) = delete;
		auto operator=(const validated_gh_cli_configuration&)
			-> validated_gh_cli_configuration& = default;
		auto operator=(validated_gh_cli_configuration&&)
			-> validated_gh_cli_configuration& = delete;

		[[nodiscard]] auto executable() const noexcept
			-> const std::filesystem::path&;
		[[nodiscard]] auto neutral_working_directory() const noexcept
			-> const std::filesystem::path&;
		/// \brief The account every `gh` launch is pinned to; empty when unpinned.
		[[nodiscard]] auto pin() const noexcept -> const std::optional<gh_cli_account_pin>&;

	private:
		friend class gh_cli_transport;
		validated_gh_cli_configuration(
			std::filesystem::path                             aExecutable,
			std::filesystem::path                             aNeutralWorkingDirectory,
			std::vector<github_cli_process_environment_entry> aEnvironment);

		std::filesystem::path                             Executable;
		std::filesystem::path                             NeutralWorkingDirectory;
		std::vector<github_cli_process_environment_entry> Environment;
		std::optional<gh_cli_account_pin>                 Pin;

		friend auto pin_gh_cli_configuration(
			const validated_gh_cli_configuration& aConfiguration,
			std::string_view aHostname, std::string_view aLogin)
			-> std::optional<validated_gh_cli_configuration>;
		friend auto run_gh_cli(
			github_cli_process_port& aProcess, const validated_gh_cli_configuration& aConfiguration,
			github_cli_process_request aRequest, std::stop_token aStop, cli_transient_credential aCredential)
			-> github_cli_process_result;
		friend auto acquire_gh_cli_credential(github_cli_process_port&, const validated_gh_cli_configuration&, std::stop_token) -> gh_cli_credential_result;
		friend auto compile_gh_cli_auth_inventory_request(
			const validated_gh_cli_configuration& aConfiguration)
			-> github_cli_process_request;
		friend auto validate_gh_cli_configuration(
			const gh_cli_configuration_candidate& aCandidate)
			-> std::expected<
				validated_gh_cli_configuration,
				gh_cli_configuration_failure>;
		friend auto compile_gh_cli_process_request(
			const validated_gh_cli_configuration& aConfiguration,
			const validated_github_host_profile&  aProfile,
			const github_wire_request&            aRequest)
			-> std::expected<
				github_cli_process_request,
				gh_cli_invocation_failure>;
		friend auto compile_gh_cli_version_request(
			const validated_gh_cli_configuration& aConfiguration)
			-> github_cli_process_request;
		friend auto compile_gh_cli_auth_status_request(
			const validated_gh_cli_configuration& aConfiguration)
			-> github_cli_process_request;
		friend auto compile_gh_cli_help_request(
			const validated_gh_cli_configuration& aConfiguration,
			gh_cli_help_topic                     aTopic)
			-> github_cli_process_request;
		friend auto compile_gh_cli_exact_blob_request(
			const validated_gh_cli_configuration&, const validated_github_host_profile&,
			const application::git_smart_exact_blob_request&)
			-> std::optional<github_cli_process_request>;
		friend auto compile_gh_cli_exact_blobs_request(
			const validated_gh_cli_configuration&, const validated_github_host_profile&,
			const application::git_smart_exact_blobs_request&)
			-> std::optional<github_cli_process_request>;
		friend auto compile_gh_cli_git_clone_request(
			const validated_gh_cli_configuration&, const validated_github_host_profile&,
			const application::git_smart_request_context&, core::git_commit_oid,
			const std::filesystem::path&, const std::filesystem::path&, std::size_t)
			-> std::optional<github_cli_process_request>;
	};

	using gh_cli_configuration_result = std::expected<
		validated_gh_cli_configuration,
		gh_cli_configuration_failure>;

	[[nodiscard]] auto validate_gh_cli_configuration(
		const gh_cli_configuration_candidate& aCandidate)
		-> gh_cli_configuration_result;

	/// \brief The same configuration, pinned to one account. [[BPR-1373]]
	/// \details Refuses a hostname that is not canonical or a login outside
	/// `valid_cli_account_login`, and refuses to re-pin a pinned one.
	[[nodiscard]] auto pin_gh_cli_configuration(
		const validated_gh_cli_configuration& aConfiguration,
		std::string_view aHostname, std::string_view aLogin)
		-> std::optional<validated_gh_cli_configuration>;

	/// \brief The name of a private configuration directory under the
	/// configuration's neutral working directory.
	inline constexpr std::string_view gh_cli_account_directory_prefix = "bpr-gh-account-";

	/// \brief Runs one `gh` launch compiled from \p aConfiguration.
	/// \details [[BPR-1373]]. Unpinned, it is `aProcess.run`. Pinned, it first
	/// creates a fresh directory named `gh_cli_account_directory_prefix` and six
	/// random characters under the neutral working directory, writes
	/// `config.yml` (`version: "1"`) and a `hosts.yml` naming only the pinned
	/// host and login, makes both files 0400 and the directory 0500, and adds
	/// `GH_CONFIG_DIR` to this child's environment alone. It runs the child and
	/// then removes the directory. A launch whose directory cannot be made is
	/// refused as `start_failure` without running. No token variable is ever
	/// set, and an existing `GH_CONFIG_DIR` entry refuses the launch.
	[[nodiscard]] auto run_gh_cli(
		github_cli_process_port& aProcess, const validated_gh_cli_configuration& aConfiguration,
		github_cli_process_request aRequest, std::stop_token aStop, cli_transient_credential aCredential = { }) -> github_cli_process_result;

	/// \brief Acquires only the named account under token-free private configuration.
	[[nodiscard]] auto acquire_gh_cli_credential(github_cli_process_port&, const validated_gh_cli_configuration&, std::stop_token) -> gh_cli_credential_result;
	/// \brief Stock-gh hostname classification, independent of BPR's server-contract enum.
	[[nodiscard]] auto gh_cli_credential_environment(std::string_view) noexcept -> cli_credential_environment;

	using gh_cli_invocation_result = std::expected<
		github_cli_process_request,
		gh_cli_invocation_failure>;

	[[nodiscard]] auto compile_gh_cli_process_request(
		const validated_gh_cli_configuration& aConfiguration,
		const validated_github_host_profile&  aProfile,
		const github_wire_request&            aRequest) -> gh_cli_invocation_result;

	/// \brief Compiles a closed, write-denied GET for one exact repository blob.
	[[nodiscard]] auto compile_gh_cli_exact_blob_request(
		const validated_gh_cli_configuration&, const validated_github_host_profile&,
		const application::git_smart_exact_blob_request&)
		-> std::optional<github_cli_process_request>;

	/// \brief Compiles one closed GraphQL read of several exact repository
	/// blobs by object ID. [[BPR-1327]]
	///
	/// \details The owner, name and object IDs travel as variables in the
	/// standard-input body, never in the query text. The query is fixed apart
	/// from how many aliased objects it names. It also selects
	/// `viewer { login databaseId }`, so the answer names the account that
	/// performed the read ([[BPR-1370]]).
	[[nodiscard]] auto compile_gh_cli_exact_blobs_request(
		const validated_gh_cli_configuration&, const validated_github_host_profile&,
		const application::git_smart_exact_blobs_request&)
		-> std::optional<github_cli_process_request>;

	/// Local noncredential probe, using the same executable and child environment.
	[[nodiscard]] auto compile_gh_cli_version_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request;

	/// \brief One active authenticated account reported by the configured gh.
	/// \details This deliberately carries no token, scopes or credential source.
	struct gh_cli_authenticated_account
	{
		std::string Hostname;
		std::string Login;

		auto operator==(const gh_cli_authenticated_account&) const -> bool = default;
	};

	enum class gh_cli_auth_status_failure : std::uint8_t {
		response_limit,
		malformed_response,
	};

	/// \brief One account in `gh`'s full inventory. [[BPR-1373]]
	/// \details No token, scopes or credential. `SignedIn` is `gh`'s own
	/// `state` of `success`; `KeychainHeld` is a `tokenSource` of `keyring`,
	/// the only kind a private configuration can reach.
	struct gh_cli_inventory_account
	{
		std::string Hostname;
		std::string Login;
		bool        Active       = false;
		bool        SignedIn     = false;
		bool        KeychainHeld = false;

		auto operator==(const gh_cli_inventory_account&) const -> bool = default;
	};

	/// \brief Minimum `gh` release, RFC-BPR-0015's account-discovery and API floor.
	/// \details 2.96 is where the structured `auth status --json hosts` inventory
	/// that admits a CLI profile was established. No later operation the app runs
	/// needs a newer release; those are checked by local help probes instead.
	inline constexpr std::array<unsigned, 3U> minimum_gh_cli_version{ 2U, 96U, 0U };

	/// \brief Tests a bounded `gh --version` banner against the minimum.
	/// \details Admits `gh version MAJOR.MINOR.PATCH` followed by a space, newline,
	/// `-` or the end, at or above the minimum, including later major versions.
	/// Malformed or overflowing numbers are refused.
	[[nodiscard]] auto supported_gh_cli_version(std::string_view aOutput) -> bool;

	/// \brief The account-inventory admission; the same banner policy.
	[[nodiscard]] auto supported_gh_cli_auth_status_version(std::string_view aOutput)
		-> bool;

	/// \brief Compiles one closed, write-denied, credential-free `gh <topic> --help`.
	/// \details No network, request body or repository input. Output is bounded to
	/// 32 KiB and the run to 10 seconds; only a completed zero exit with no
	/// standard error returns output, so a failed probe never exposes partial text.
	[[nodiscard]] auto compile_gh_cli_help_request(
		const validated_gh_cli_configuration& aConfiguration,
		gh_cli_help_topic                     aTopic)
		-> github_cli_process_request;

	/// \brief Tests completed help output for the flags and usage the app invokes.
	[[nodiscard]] auto gh_cli_help_admits(gh_cli_help_topic aTopic, std::string_view aHelp)
		-> bool;

	/// \brief Compiles the explicit, token-free active-account inventory request.
	[[nodiscard]] auto compile_gh_cli_auth_status_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request;

	/// \brief Parses the bounded JSON returned by `gh auth status`.
	/// \details Only active successful host/login pairs cross this boundary.
	[[nodiscard]] auto parse_gh_cli_auth_status(
		std::string_view aOutput)
		-> std::expected<
			std::vector<gh_cli_authenticated_account>,
			gh_cli_auth_status_failure>;

	/// \brief Compiles `gh auth status --json hosts`, every account. [[BPR-1373]]
	/// \details Write-denied and token-free like the active inventory. It
	/// always reads `gh`'s own configuration: it never runs pinned.
	[[nodiscard]] auto compile_gh_cli_auth_inventory_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request;

	/// \brief Parses every account of the full inventory, in host then
	/// listed order. A login listed twice for one host is malformed.
	[[nodiscard]] auto parse_gh_cli_auth_inventory(std::string_view aOutput)
		-> std::expected<std::vector<gh_cli_inventory_account>, gh_cli_auth_status_failure>;

	/// \brief Compiles one exact-commit clone into an owned, write-confined store.
	[[nodiscard]] auto compile_gh_cli_git_clone_request(
		const validated_gh_cli_configuration&         aConfiguration,
		const validated_github_host_profile&          aProfile,
		const application::git_smart_request_context& aContext, core::git_commit_oid aCommit,
		const std::filesystem::path& aGit, const std::filesystem::path& aRoot,
		std::size_t aIndex) -> std::optional<github_cli_process_request>;

	/// \brief Names the bare store that clone \p aIndex writes under \p aRoot.
	[[nodiscard]] auto gh_cli_git_clone_store(const std::filesystem::path& aRoot, std::size_t aIndex)
		-> std::filesystem::path;

	/// \brief Compiles one credential-free local Git request over an owned store.
	/// \details The environment is built from nothing. No network, lazy object
	/// fetch, ambient Git configuration, or repository executable configuration
	/// participates, and `PATH` holds only the directory of \p aGit.
	[[nodiscard]] auto compile_gh_cli_local_git_request(
		const std::filesystem::path& aGit, const std::filesystem::path& aRoot,
		std::vector<std::string> aArguments, std::string aInput,
		std::size_t aStandardOutputLimit) -> github_cli_process_request;

	enum class gh_cli_availability_reason : std::uint8_t {
		opt_in_product_scope_in_progress,
		executable_identity_unverifiable,
		cross_authority_redirect_unverifiable,
		ambient_configuration_unverifiable,
		credential_identity_binding_unverifiable,
		working_directory_identity_unverifiable,
		descendant_process_containment_unverifiable,
		numeric_process_identity_reuse_unverifiable,
	};

	class gh_cli_transport final : public github_wire_transport_port, public github_mutation_wire_transport
	{
	public:
		gh_cli_transport(
			const validated_github_host_profile&  aProfile,
			const validated_gh_cli_configuration& aConfiguration,
			github_cli_process_port& aProcess, cli_transient_credential aCredential = { });

		[[nodiscard]] auto begin_operation(std::stop_token) -> github_wire_operation override;
		[[nodiscard]] auto begin_mutation_operation(std::stop_token) -> github_mutation_operation override;
		[[nodiscard]] auto strict_cli() const noexcept -> bool override { return true; }

		[[nodiscard]] auto availability_reasons() const noexcept
			-> std::array<gh_cli_availability_reason, 8>;

		[[nodiscard]] auto perform(
			const github_wire_request& aRequest,
			std::stop_token            aStop) -> github_wire_completion override;

		[[nodiscard]] auto perform_capability_probe(
			const github_capability_probe_wire_request& aRequest,
			std::stop_token                             aStop) -> github_capability_probe_wire_completion override;

		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request& aRequest, std::stop_token aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion override;

		[[nodiscard]] auto configuration_unchanged() const -> bool;

	private:
		[[nodiscard]] auto acquire_operation(std::stop_token aStop)
			-> std::expected<std::shared_ptr<gh_cli_transport>, gh_cli_credential_error>;
		struct configuration_metadata
		{
			std::filesystem::path           Canonical;
			std::filesystem::file_type      Type = std::filesystem::file_type::not_found;
			std::uintmax_t                  Size = 0U;
			std::filesystem::file_time_type Modified{ };
			auto                            operator==(const configuration_metadata&) const -> bool = default;
		};
		[[nodiscard]] auto observe_configuration_files() const
			-> std::optional<std::array<configuration_metadata, 3>>;
		std::filesystem::path                                ConfigurationDirectory;
		std::optional<std::array<configuration_metadata, 3>> ConfigurationFiles;
		std::uintmax_t                                       ExecutableSize = 0U;
		std::filesystem::file_time_type                      ExecutableWriteTime{ };
		bool                                                 ConfigurationObserved = false;
		validated_github_host_profile                        Profile;
		validated_gh_cli_configuration                       Configuration;
		github_cli_process_port&                             Process;
		cli_transient_credential                             Credential;
	};
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_TRANSPORT_HPP
