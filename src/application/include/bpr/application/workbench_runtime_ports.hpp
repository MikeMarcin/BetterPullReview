#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP

#include "bpr/core/workbench_interaction.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace bpr::application {
	class epoch_application_host;

	inline constexpr std::size_t   maximum_workbench_actionable_files     = 262'144U;
	inline constexpr std::size_t   maximum_workbench_outstanding_requests = 64U;
	inline constexpr std::uint32_t minimum_workbench_drawer_width         = 64U;
	inline constexpr std::uint32_t maximum_workbench_drawer_width         = 4'096U;

	/// \brief Describes the workbench host limits record.
	///
	/// \details `ActionableFiles` bounds the ordered actionable set the host will
	/// retain for one workbench, `OutstandingRequests` bounds concurrently
	/// submitted effect work, and `DrawerWidth` is the largest drawer width the
	/// host will retain or persist.
	struct workbench_host_limits
	{
		std::size_t   ActionableFiles     = 4'096U;
		std::size_t   OutstandingRequests = 8U;
		std::uint32_t DrawerWidth         = 1'024U;

		auto operator<=>(const workbench_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const workbench_host_limits& aLimits) noexcept -> bool
	{
		return aLimits.ActionableFiles > 0U
			&& aLimits.ActionableFiles <= maximum_workbench_actionable_files
			&& aLimits.OutstandingRequests > 0U
			&& aLimits.OutstandingRequests <= maximum_workbench_outstanding_requests
			&& aLimits.DrawerWidth >= minimum_workbench_drawer_width
			&& aLimits.DrawerWidth <= maximum_workbench_drawer_width;
	}

	enum class workbench_file_content_kind : std::uint8_t {
		unknown,
		text,
		binary,
	};

	/// \brief Retains the provider-free change fact attached to one file row.
	enum class workbench_file_change_kind : std::uint8_t {
		uninitialized,
		added,
		modified,
		removed,
		renamed,
		copied,
		type_changed,
		unchanged,
	};

	/// \brief Retains one bounded provider-free workbench file observation.
	struct workbench_file_observation_entry
	{
		core::workbench_file_id              File;
		std::string                          PresentationPath;
		std::filesystem::path                CurrentPath;
		std::optional<std::filesystem::path> PreviousPath;
		workbench_file_change_kind           Change      = workbench_file_change_kind::uninitialized;
		workbench_file_content_kind          ContentKind = workbench_file_content_kind::unknown;

		auto operator<=>(const workbench_file_observation_entry&) const = default;
	};

	/// \brief Checks whether one bounded file observation is exact and usable.
	[[nodiscard]] auto valid(
		const workbench_file_observation_entry& aEntry) noexcept -> bool;

	/// \brief Describes the workbench request context record.
	///
	/// \details `Request` is unique for the host lifetime and is the exactly-once
	/// terminal key: a second delivery of the same request finds no outstanding
	/// record. `Session` names the host that issued the request, so a terminal
	/// from a foreign host cannot be mistaken for a local one. `Generation` names
	/// the retained workbench incarnation the request was issued under; it
	/// advances whenever the host adopts a new observation or abandons its
	/// outstanding work. A locally decided change carries a zero `Request`,
	/// because no effect produced it.
	struct workbench_request_context
	{
		core::request_id             Request;
		core::application_session_id Session;
		core::request_generation     Generation;

		auto operator<=>(const workbench_request_context&) const = default;
	};

	/// \brief Names one accepted GitHub pull-request observation exactly.
	///
	/// \details Workbench files come from the accepted provider snapshot, not
	/// from the local Git cache. This authority therefore carries the complete
	/// provider/session identity and deliberately has no cache or epoch-catalog
	/// scope. Using `core::pull_request_authority` here would require inventing
	/// those local identities before the epoch host has opened either resource.
	struct workbench_pull_request_authority
	{
		core::application_session_id     ApplicationSession;
		core::host_profile_id            Profile;
		core::github_profile_revision    ProfileRevision;
		core::git_host_id                Host;
		core::host_authority_fingerprint HostAuthority;
		core::github_account_id          Account;
		core::github_viewer_actor_id     Viewer;
		core::repository_id              Repository;
		core::pull_request_id            PullRequest;
		core::pull_request_number        Number;
		core::provider_snapshot_id       Snapshot;
		core::github_identity_generation IdentityGeneration;
		core::request_generation         PullRequestGeneration;

		auto operator<=>(const workbench_pull_request_authority&) const = default;
	};

	/// \brief Checks whether the workbench authority is complete and usable.
	[[nodiscard]] constexpr auto valid(
		const workbench_pull_request_authority& aAuthority) noexcept -> bool
	{
		return aAuthority.ApplicationSession.value() != 0U
			&& aAuthority.Profile.value() != 0U
			&& aAuthority.ProfileRevision.value() != 0U
			&& aAuthority.Host.value() != 0U && aAuthority.HostAuthority.valid()
			&& aAuthority.Account.value() != 0U
			&& aAuthority.Viewer.value() != 0U
			&& aAuthority.Repository.value() != 0U
			&& aAuthority.PullRequest.value() != 0U && aAuthority.Number.valid()
			&& aAuthority.Snapshot.value() != 0U
			&& aAuthority.IdentityGeneration.value() != 0U
			&& aAuthority.PullRequestGeneration.value() != 0U;
	}

	/// \brief Describes the workbench observation request record.
	///
	/// \details `MaximumFiles` is the bound the host will accept. A port that
	/// cannot answer within it must report `limit_exceeded` rather than truncate,
	/// because a truncated order would give the core false neighbour facts.
	struct workbench_observation_request
	{
		workbench_request_context        Context;
		workbench_pull_request_authority Authority;
		std::size_t                      MaximumFiles = 0U;

		auto operator<=>(const workbench_observation_request&) const = default;
	};

	enum class workbench_observation_status : std::uint8_t {
		available,
		cancelled,
		unavailable,
		limit_exceeded,
	};

	/// \brief Describes the workbench file observation record.
	///
	/// \details `Actionable` is the filtered actionable order in presentation
	/// order. The host owns it; the core never sees it and receives only the
	/// bounded cursor facts derived from it. `Context` and `Authority` must repeat
	/// the request exactly, so a misrouted answer cannot be adopted. The port owns
	/// uniqueness within `Actionable`; the host resolves the first matching entry.
	struct workbench_file_observation
	{
		workbench_request_context                     Context;
		workbench_pull_request_authority              Authority;
		workbench_observation_status                  Status = workbench_observation_status::unavailable;
		std::vector<workbench_file_observation_entry> Actionable;

		auto operator<=>(const workbench_file_observation&) const = default;
	};

	/// \brief Describes the exact accepted comparison used for file resolution.
	struct workbench_selected_diff_context
	{
		core::application_session_id  ApplicationSession;
		core::pull_request_authority  Authority;
		core::comparison_pair         Comparison;
		core::comparison_revision     ComparisonRevision;
		core::diff_options            Options;
		core::diff_options_digest     OptionsDigest;
		core::diff_algorithm_revision Algorithm;

		auto operator<=>(const workbench_selected_diff_context&) const = default;
	};

	/// \brief Describes one exact selected-file tree lookup request.
	struct workbench_selected_diff_request
	{
		workbench_request_context        Context;
		workbench_pull_request_authority WorkbenchAuthority;
		workbench_selected_diff_context  Diff;
		workbench_file_observation_entry File;

		auto operator<=>(const workbench_selected_diff_request&) const = default;
	};

	/// \brief Opaque provenance for one endpoint resolved by the epoch host.
	///
	/// \details The value repeats the complete selected-diff request, side,
	/// blob, and repository path that the epoch cache actor resolved. Production
	/// code outside that actor can copy and validate the value but cannot mint a
	/// replacement for an arbitrary presentation or caller-supplied path.
	class workbench_selected_diff_endpoint final
	{
	public:
		workbench_selected_diff_endpoint(
			const workbench_selected_diff_endpoint&)                                  = default;
		workbench_selected_diff_endpoint(workbench_selected_diff_endpoint&&) noexcept = default;
		auto operator=(const workbench_selected_diff_endpoint&)
			-> workbench_selected_diff_endpoint& = default;
		auto operator=(workbench_selected_diff_endpoint&&) noexcept
			-> workbench_selected_diff_endpoint& = default;
		~workbench_selected_diff_endpoint()      = default;

		[[nodiscard]] auto request() const noexcept
			-> const workbench_selected_diff_request&
		{
			return Request;
		}

		[[nodiscard]] auto side() const noexcept -> core::diff_side
		{
			return Side;
		}

		[[nodiscard]] auto blob() const noexcept -> const core::git_blob_oid&
		{
			return Blob;
		}

		[[nodiscard]] auto path() const noexcept
			-> const std::filesystem::path&
		{
			return Path;
		}

		auto operator<=>(const workbench_selected_diff_endpoint&) const = default;

	private:
		workbench_selected_diff_endpoint(
			workbench_selected_diff_request aRequest,
			const core::diff_side           aSide,
			core::git_blob_oid              aBlob,
			std::filesystem::path           aPath)
			: Request{ std::move(aRequest) }
			, Side{ aSide }
			, Blob{ std::move(aBlob) }
			, Path{ std::move(aPath) }
		{
		}

		friend class epoch_application_host;
#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
		friend auto make_workbench_selected_diff_endpoint_for_testing(
			workbench_selected_diff_request,
			core::diff_side,
			core::git_blob_oid,
			std::filesystem::path) -> workbench_selected_diff_endpoint;
#endif

		workbench_selected_diff_request Request;
		core::diff_side                 Side = core::diff_side::left;
		core::git_blob_oid              Blob;
		std::filesystem::path           Path;
	};

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
	/// \brief Mints endpoint provenance for application-host fixtures only.
	[[nodiscard]] auto make_workbench_selected_diff_endpoint_for_testing(
		workbench_selected_diff_request aRequest,
		core::diff_side                 aSide,
		core::git_blob_oid              aBlob,
		std::filesystem::path           aPath)
		-> workbench_selected_diff_endpoint;
#endif

	/// \brief Checks exact epoch endpoint provenance against a publication.
	[[nodiscard]] inline auto valid(
		const workbench_selected_diff_endpoint& aEndpoint,
		const workbench_selected_diff_request&  aRequest,
		const core::diff_side                   aSide,
		const core::git_blob_oid&               aBlob,
		const std::filesystem::path&            aPath) noexcept -> bool
	{
		return aEndpoint.request() == aRequest && aEndpoint.side() == aSide
			&& aEndpoint.blob() == aBlob && aEndpoint.path() == aPath;
	}

	enum class workbench_selected_diff_status : std::uint8_t {
		available,
		binary,
		missing_left,
		missing_right,
		both_missing,
		ambiguous_left,
		ambiguous_right,
		ambiguous_both,
		cancelled,
		stale,
		failure,
	};

	/// \brief Describes one exact selected-file tree lookup terminal.
	struct workbench_selected_diff_observation
	{
		workbench_selected_diff_request                 Request;
		workbench_selected_diff_status                  Status = workbench_selected_diff_status::failure;
		std::optional<core::git_blob_oid>               LeftBlob;
		std::optional<core::git_blob_oid>               RightBlob;
		std::optional<std::filesystem::path>            LeftPath;
		std::optional<std::filesystem::path>            RightPath;
		std::optional<workbench_selected_diff_endpoint> LeftEndpoint;
		std::optional<workbench_selected_diff_endpoint> RightEndpoint;

		auto operator<=>(const workbench_selected_diff_observation&) const = default;
	};

	/// \brief Resolves selected files through the retained epoch cache actor.
	class workbench_selected_diff_port
	{
	public:
		virtual ~workbench_selected_diff_port() = default;

		[[nodiscard]] virtual auto context(
			const workbench_pull_request_authority& aAuthority) const
			-> std::optional<workbench_selected_diff_context> = 0;
		[[nodiscard]] virtual auto resolve(
			const workbench_selected_diff_request& aRequest,
			std::stop_token                        aStop)
			-> workbench_selected_diff_observation = 0;
	};

	/// \brief Describes an exact resolved file-comparison request.
	struct workbench_selected_diff_publication
	{
		workbench_selected_diff_request                 Request;
		core::file_comparison_key                       Key;
		std::optional<std::filesystem::path>            LeftPath;
		std::optional<std::filesystem::path>            RightPath;
		std::optional<workbench_selected_diff_endpoint> LeftEndpoint;
		std::optional<workbench_selected_diff_endpoint> RightEndpoint;

		auto operator<=>(const workbench_selected_diff_publication&) const = default;
	};

	/// \brief Presents selected-file resolution without performing its effects.
	class workbench_selected_diff_projection_port
	{
	public:
		virtual ~workbench_selected_diff_projection_port() = default;

		virtual void begin(const workbench_selected_diff_request& aRequest) = 0;
		virtual void publish(
			const workbench_selected_diff_publication& aPublication) = 0;
		virtual void fail(
			const workbench_selected_diff_observation& aFailure) = 0;
	};

	/// \brief Provides the workbench observation port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane. An implementation must honour the supplied stop token and must repeat
	/// the request context and authority in its answer.
	class workbench_observation_port
	{
	public:
		virtual ~workbench_observation_port() = default;

		[[nodiscard]] virtual auto assemble(
			const workbench_observation_request& aRequest,
			std::stop_token                      aStop)
			-> workbench_file_observation = 0;
	};

	/// \brief Describes the workbench layout settings record.
	///
	/// \details This is the whole durable workbench layout: the semantic drawer
	/// states the core owns while a workbench is active, and the drawer widths the
	/// host owns outright because they carry no semantic meaning.
	struct workbench_layout_settings
	{
		core::workbench_drawer_state Files              = core::workbench_drawer_state::expanded;
		core::workbench_drawer_state Conversations      = core::workbench_drawer_state::expanded;
		std::uint32_t                FilesWidth         = 320U;
		std::uint32_t                ConversationsWidth = 320U;

		auto operator<=>(const workbench_layout_settings&) const = default;
	};

	enum class workbench_settings_failure : std::uint8_t {
		unavailable,
		cancelled,
	};

	/// \brief Describes the workbench settings error record.
	struct workbench_settings_error
	{
		workbench_settings_failure Failure = workbench_settings_failure::unavailable;

		auto operator<=>(const workbench_settings_error&) const = default;
	};

	template <typename Value>
	using workbench_settings_result = std::expected<Value, workbench_settings_error>;

	/// \brief Provides the workbench settings port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane, and marshals the answer back as a typed terminal. An absent durable
	/// record is not a failure: an implementation with nothing stored returns the
	/// default settings.
	class workbench_settings_port
	{
	public:
		virtual ~workbench_settings_port() = default;

		[[nodiscard]] virtual auto load(std::stop_token aStop)
			-> workbench_settings_result<workbench_layout_settings> = 0;
		[[nodiscard]] virtual auto store(
			const workbench_layout_settings& aSettings,
			std::stop_token                  aStop)
			-> workbench_settings_result<void> = 0;
	};

	/// \brief Describes the workbench layout delta record.
	///
	/// \details Drawer widths only. Collapsed and expanded are semantic and travel
	/// in the core's drawer delta instead, so one meaning never has two carriers.
	struct workbench_layout_delta
	{
		std::uint32_t FilesWidth         = 0U;
		std::uint32_t ConversationsWidth = 0U;

		auto operator<=>(const workbench_layout_delta&) const = default;
	};

	/// \brief Describes one file-drawer row by opaque semantic identity.
	struct workbench_file_projection
	{
		core::workbench_file_id File;
		std::string             Path;

		auto operator<=>(const workbench_file_projection&) const = default;
	};

	/// \brief Describes the workbench projection record.
	///
	/// \details Every domain is engaged only when its published meaning changed,
	/// so a bounded change never forces presentation to rebuild the whole
	/// workbench. The host publishes nothing when no domain changed.
	struct workbench_projection
	{
		workbench_request_context                             Context;
		std::optional<core::workbench_selection_delta>        Selection;
		std::optional<core::workbench_drawer_delta>           Drawers;
		std::optional<core::workbench_command_delta>          Commands;
		std::optional<workbench_layout_delta>                 Layout;
		std::optional<std::vector<workbench_file_projection>> Files;

		auto operator<=>(const workbench_projection&) const = default;
	};

	/// \brief Checks whether the supplied projection carries no changed domain.
	[[nodiscard]] constexpr auto empty(
		const workbench_projection& aProjection) noexcept -> bool
	{
		return !aProjection.Selection.has_value()
			&& !aProjection.Drawers.has_value()
			&& !aProjection.Commands.has_value()
			&& !aProjection.Layout.has_value()
			&& !aProjection.Files.has_value();
	}

	enum class workbench_host_failure : std::uint8_t {
		observation_unavailable,
		observation_limit,
		settings_unavailable,
		selected_file_binary,
		selected_file_missing_left,
		selected_file_missing_right,
		selected_file_missing_both,
		selected_file_ambiguous,
		selected_file_cancelled,
		selected_file_stale,
		host_failure,
	};

	/// \brief Describes the workbench failure publication record.
	struct workbench_failure_publication
	{
		workbench_request_context Context;
		workbench_host_failure    Failure = workbench_host_failure::host_failure;

		auto operator<=>(const workbench_failure_publication&) const = default;
	};

	/// \brief Provides the workbench projection port contract.
	///
	/// \details Both operations run on the decision lane inside the host's
	/// exception boundary. A stale, superseded, cancelled, duplicate, or foreign
	/// terminal reaches neither operation.
	class workbench_projection_port
	{
	public:
		virtual ~workbench_projection_port() = default;

		virtual void publish(const workbench_projection& aProjection)    = 0;
		virtual void fail(const workbench_failure_publication& aFailure) = 0;
	};

	enum class workbench_terminal_rejection : std::uint8_t {
		none,
		unknown_request,
		inexact_terminal,
		superseded,
		cancelled,
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP
