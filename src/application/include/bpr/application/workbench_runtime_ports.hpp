#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_RUNTIME_PORTS_HPP

#include "bpr/application/home_view_state.hpp"
#include "bpr/core/agent_wrapper.hpp"
#include "bpr/core/feedback_label_columns.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/core/workbench_interaction.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
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
		/// [[BPR-0972]]. The file's position in the port's complete catalogue,
		/// so a withheld file restored later returns to its place without a
		/// new observation. Absent from a port that does not number it.
		std::optional<std::size_t> Ordinal;

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

	/// \brief Reports whether the workbench authority is the signed-out one.
	///
	/// \details [[BPR-0715]]. A public-preview read names no account and no
	/// viewer; both zeroes together are that authority's discriminant, the
	/// same rule `core::pull_request_authority` applies. One zero without the
	/// other is a malformed authority, not a signed-out one.
	[[nodiscard]] constexpr auto anonymous(
		const workbench_pull_request_authority& aAuthority) noexcept -> bool
	{
		return aAuthority.Account.value() == 0U && aAuthority.Viewer.value() == 0U;
	}

	/// \brief Checks whether the workbench authority is complete and usable.
	[[nodiscard]] constexpr auto valid(
		const workbench_pull_request_authority& aAuthority) noexcept -> bool
	{
		return aAuthority.ApplicationSession.value() != 0U
			&& aAuthority.Profile.value() != 0U
			&& aAuthority.ProfileRevision.value() != 0U
			&& aAuthority.Host.value() != 0U && aAuthority.HostAuthority.valid()
			&& (anonymous(aAuthority)
				|| (aAuthority.Account.value() != 0U
					&& aAuthority.Viewer.value() != 0U))
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
		/// [[BPR-0972]]. Catalogue files the port left out of the order because
		/// an exact-material receipt makes them not actionable. A port that
		/// names them lets the host restore one without observing again.
		std::vector<workbench_file_observation_entry> Withheld;

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

	/// \brief [[BPR-1277]]. Whether `aAsked` asks for the same diff as `aShown`:
	/// the same authority, comparison context and file entry. Only the request
	/// context (its generation) may differ, as when a refresh asks again.
	[[nodiscard]] inline auto same_selected_diff(
		const workbench_selected_diff_request& aShown,
		const workbench_selected_diff_request& aAsked) noexcept -> bool
	{
		return aShown.WorkbenchAuthority == aAsked.WorkbenchAuthority
			&& aShown.Diff == aAsked.Diff
			&& aShown.File == aAsked.File;
	}

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

	/// \brief Asks which of a review's files differ between two exact commits.
	///
	/// \details [[BPR-0654]]. The overview's "changed since my last review"
	/// column: `Left` is the head I last reviewed, `Right` the accepted head,
	/// and `Paths` the pull request's files as the snapshot names them. The
	/// host answers beside the reviewer's selected comparison, never through
	/// it, so what the reviewer is looking at is not disturbed.
	struct changed_paths_request
	{
		workbench_pull_request_authority   WorkbenchAuthority;
		core::git_commit_oid               Left;
		core::git_commit_oid               Right;
		std::vector<std::filesystem::path> Paths;

		auto operator<=>(const changed_paths_request&) const = default;
	};

	enum class changed_paths_status : std::uint8_t {
		/// `Changed` names exactly the paths whose blob differs.
		resolved,
		/// The retained cache no longer serves the accepted review.
		stale,
		cancelled,
		failure,
	};

	/// \brief One exact answer to a `changed_paths_request`.
	struct changed_paths_observation
	{
		changed_paths_request              Request;
		changed_paths_status               Status = changed_paths_status::failure;
		std::vector<std::filesystem::path> Changed;

		auto operator<=>(const changed_paths_observation&) const = default;
	};

	using changed_paths_completion = std::function<void(const changed_paths_observation&)>;

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

	/// \brief Which modality a pull request opens in ([[BPR-0661]]).
	///
	/// \details [[BPR-1295]]. Read merged into Review on 2026-10-06. The
	/// stored value 1 keeps its number and now names Review, so a reviewer
	/// who chose to open in Read opens in Review.
	enum class review_open_modality : std::uint8_t {
		overview,
		review,
		/// The modality the reviewer last used in this session.
		last_used,
	};

	/// \brief When a file is marked read ([[BPR-0661]]).
	enum class review_mark_read_behaviour : std::uint8_t {
		by_hand,
		when_fully_seen,
	};

	/// \brief The comparison selected on return to a pull request ([[BPR-0661]]).
	enum class review_return_comparison : std::uint8_t {
		whole_pr,
		since_last_review,
		since_last_upload,
	};

	/// \brief The comments drawer's first filter ([[BPR-0661]]).
	enum class review_comments_filter : std::uint8_t {
		all,
		unread,
		unresolved,
	};

	/// \brief Whether a drawer carries its own controls ([[BPR-0721]]).
	///
	/// \details Hidden, the owner's direction and the default: a collapsed
	/// drawer leaves no column at all and an expanded one has no Hide
	/// button, so the strip's toggles and their shortcuts are the only way to
	/// open or collapse a drawer. Shown restores the 18-pixel rail and the
	/// Hide button. Presentation only; the drawer state is the core's either
	/// way.
	enum class review_drawer_controls : std::uint8_t {
		hidden,
		shown,
	};

	/// \brief How Conventional Comments label marks are drawn ([[BPR-1083]]).
	///
	/// \details The owner, 2026-10-05: "i think settings should also have a
	/// toggle to switch between the icons and the shortcode icons, because i
	/// liked both of them." Icons, the default, are BPR-1080's twelve glyphs;
	/// letters are the label's two-letter shortcode in the same mark, with the
	/// same tints, blocking fill, size and names. Presentation only.
	enum class review_feedback_marks : std::uint8_t {
		icons,
		letters,
	};

	/// \brief Whether the comment editor offers the label helper ([[BPR-1088]]).
	///
	/// \details The helper is the label chip's completion and the leading
	/// `:xx:` shortcodes (RFC-BPR-0048's convention-assisted composition).
	/// Off by default (owner, BPR-1079 Settings v1): a label typed out in
	/// full is recognized and counted either way.
	enum class review_label_helper : std::uint8_t {
		off,
		on,
	};

	/// \brief Where the Overview's label counts start ([[BPR-1088]]).
	///
	/// \details Open is `core::feedback_resolution_mode::unresolved`, the
	/// default (RFC-BPR-0049); All is `feedback_resolution_mode::all`. This
	/// is the default a pull request's Overview opens with. The mode a
	/// reviewer switches to on an Overview is that view's own state and is
	/// never written back here.
	enum class review_overview_counts : std::uint8_t {
		open,
		all,
	};

	/// \brief What reading the stored label columns left out ([[BPR-1088]]).
	///
	/// \details A load observation, never a preference: no store writes it,
	/// and a reviewer's edit to the review behaviour clears it. Both counts
	/// are `core::read_feedback_label_columns`'s; every valid stored label
	/// kept its place in `LabelColumns`.
	struct review_label_columns_recovery
	{
		std::uint8_t Unknown  = 0U;
		std::uint8_t Repeated = 0U;

		auto operator<=>(const review_label_columns_recovery&) const = default;
	};

	/// \brief The reviewer's durable review-behaviour preferences.
	///
	/// \details [[BPR-0661]]. RFC-BPR-0028's Reviews & queue rows. Stored
	/// beside the layout in the workbench settings record, validated by the
	/// host, honoured by the window: the modality on open, whether a file
	/// marks itself read once fully seen, the
	/// comparison on return, the comments drawer's first filter and, since
	/// [[BPR-0721]], whether the drawers carry their own controls. Every
	/// default is the behaviour the window had before the preference existed,
	/// except the drawer controls, which the owner asked to have off.
	struct review_behaviour_settings
	{
		review_open_modality OpenModality = review_open_modality::overview;
		/// [[BPR-1295]]. "Continue review enters" is gone with Read: Start
		/// and Continue review both enter Review.
		review_mark_read_behaviour MarkRead         = review_mark_read_behaviour::by_hand;
		review_return_comparison   ReturnComparison = review_return_comparison::whole_pr;
		review_comments_filter     CommentsFilter   = review_comments_filter::all;
		review_drawer_controls     DrawerControls   = review_drawer_controls::hidden;
		/// [[BPR-1083]]. Icons by default.
		review_feedback_marks FeedbackMarks = review_feedback_marks::icons;
		/// [[BPR-1088]]. The comment editor's label helper, off by default.
		review_label_helper LabelHelper = review_label_helper::off;
		/// [[BPR-1088]]. The Overview's starting count, Open by default.
		review_overview_counts OverviewCounts = review_overview_counts::open;
		/// [[BPR-1088]]. The Overview's dedicated label chips, in order. None
		/// by default, so the catch-all counts every conversation.
		core::feedback_label_columns LabelColumns{ };
		/// [[BPR-1088]]. Set only on a record read from a store whose label
		/// columns named an unknown or repeated label; see the type.
		std::optional<review_label_columns_recovery> LabelColumnsRecovery;

		auto operator<=>(const review_behaviour_settings&) const = default;
	};

	/// \brief The record a reviewer's edit asks for: the same preferences,
	/// with no load observation carried into the write.
	[[nodiscard]] inline auto edited(review_behaviour_settings aSettings) noexcept -> review_behaviour_settings
	{
		aSettings.LabelColumnsRecovery.reset();
		return aSettings;
	}

	/// \brief Checks whether every preference names a value of its vocabulary.
	///
	/// \details [[BPR-1088]]. The label columns must be a selection
	/// `core::check_feedback_label_columns` accepts: no unknown or repeated
	/// label is ever admitted, so a store never has one to write.
	[[nodiscard]] inline auto valid(const review_behaviour_settings& aSettings) noexcept -> bool
	{
		return static_cast<std::uint8_t>(aSettings.OpenModality)
			<= static_cast<std::uint8_t>(review_open_modality::last_used)
			&& static_cast<std::uint8_t>(aSettings.MarkRead)
			<= static_cast<std::uint8_t>(review_mark_read_behaviour::when_fully_seen)
			&& static_cast<std::uint8_t>(aSettings.ReturnComparison)
			<= static_cast<std::uint8_t>(review_return_comparison::since_last_upload)
			&& static_cast<std::uint8_t>(aSettings.CommentsFilter)
			<= static_cast<std::uint8_t>(review_comments_filter::unresolved)
			&& static_cast<std::uint8_t>(aSettings.DrawerControls)
			<= static_cast<std::uint8_t>(review_drawer_controls::shown)
			&& static_cast<std::uint8_t>(aSettings.FeedbackMarks)
			<= static_cast<std::uint8_t>(review_feedback_marks::letters)
			&& static_cast<std::uint8_t>(aSettings.LabelHelper)
			<= static_cast<std::uint8_t>(review_label_helper::on)
			&& static_cast<std::uint8_t>(aSettings.OverviewCounts)
			<= static_cast<std::uint8_t>(review_overview_counts::all)
			&& core::check_feedback_label_columns(aSettings.LabelColumns).has_value();
	}

	/// Window-local normal geometry in logical display coordinates. Zero size
	/// means no saved placement. Qt observes screens and fits it for presentation.
	struct workbench_window_geometry
	{
		std::int32_t X = 0, Y = 0, Width = 0, Height = 0;
		bool         Maximized                                           = false;
		auto         operator<=>(const workbench_window_geometry&) const = default;
	};
	[[nodiscard]] inline auto valid(const workbench_window_geometry& aGeometry) noexcept -> bool
	{
		return aGeometry == workbench_window_geometry{ }
		|| (aGeometry.X >= -1'000'000 && aGeometry.X <= 1'000'000
			&& aGeometry.Y >= -1'000'000 && aGeometry.Y <= 1'000'000
			&& aGeometry.Width >= 64 && aGeometry.Width <= 16'384
			&& aGeometry.Height >= 64 && aGeometry.Height <= 16'384);
	}

	/// \brief Describes the workbench layout settings record.
	///
	/// \details This is the whole durable workbench layout: the semantic drawer
	/// states and source-pane numbering the core owns while a workbench is
	/// active, and the drawer widths the host owns outright because they carry
	/// no semantic meaning.
	struct workbench_layout_settings
	{
		core::workbench_drawer_state Files         = core::workbench_drawer_state::expanded;
		core::workbench_drawer_state Conversations = core::workbench_drawer_state::expanded;
		/// [[BPR-0651]]. The repositories drawer, left of the files.
		core::workbench_drawer_state Repositories       = core::workbench_drawer_state::expanded;
		core::workbench_line_numbers LineNumbers        = core::workbench_line_numbers::shown;
		core::workbench_one_pane     OnePane            = core::workbench_one_pane::disabled;
		std::uint32_t                FilesWidth         = 320U;
		std::uint32_t                ConversationsWidth = 320U;
		std::uint32_t                RepositoriesWidth  = 300U;
		/// [[BPR-0661]]. The review-behaviour preferences, durable beside the
		/// layout because they are one reviewer's, not one review's.
		review_behaviour_settings Behaviour;
		/// [[BPR-0783]]. This window's Home view. It belongs to the window
		/// alone: the profile's defaults do not store it.
		home_view_state           Home;
		workbench_window_geometry Geometry;
		/// [[BPR-1130]]. This window's Updates line, folded or open. Like
		/// Home it belongs to the window alone: the profile's defaults never
		/// store it, so every new window starts folded.
		core::workbench_updates_line UpdatesLine  = core::workbench_updates_line::folded;
		core::writing_hook_settings  WritingHooks = { };
		/// [[BPR-1301]]. The profile's Claude process wrapper, carried here
		/// only because this record is what the store writes.
		std::string ClaudeWrapper = { };
		/// [[BPR-1313]]. This window's preferred summary share in ten-thousandths.
		std::optional<std::uint32_t> SummaryFraction = { };

		auto operator<=>(const workbench_layout_settings&) const = default;
	};

	/// Profile defaults are separate from each admitted window's geometry.
	struct workbench_window_layout
	{
		core::workbench_drawer_state Files         = core::workbench_drawer_state::expanded;
		core::workbench_drawer_state Conversations = core::workbench_drawer_state::expanded;
		core::workbench_drawer_state Repositories  = core::workbench_drawer_state::expanded;
		core::workbench_line_numbers LineNumbers   = core::workbench_line_numbers::shown;
		core::workbench_one_pane     OnePane       = core::workbench_one_pane::disabled;
		std::uint32_t                FilesWidth = 320U, ConversationsWidth = 320U, RepositoriesWidth = 300U;
		/// [[BPR-0783]].
		home_view_state           Home;
		workbench_window_geometry Geometry;
		/// [[BPR-1130]].
		core::workbench_updates_line UpdatesLine                                       = core::workbench_updates_line::folded;
		std::optional<std::uint32_t> SummaryFraction                                   = { };
		auto                         operator<=>(const workbench_window_layout&) const = default;
	};
	struct workbench_profile_preferences
	{
		review_behaviour_settings   Behaviour;
		workbench_window_layout     NewWindowDefaults;
		core::writing_hook_settings WritingHooks = { };
		/// [[BPR-1301]]. One absolute path run in place of `claude` for every
		/// Claude launch in this profile; empty runs `claude` as before. An
		/// agent-free build carries it unread, so its saves keep it.
		std::string ClaudeWrapper                                           = { };
		auto        operator<=>(const workbench_profile_preferences&) const = default;
	};
	[[nodiscard]] inline auto window_layout(const workbench_layout_settings& aSettings) -> workbench_window_layout
	{
		return { aSettings.Files, aSettings.Conversations, aSettings.Repositories, aSettings.LineNumbers, aSettings.OnePane,
				 aSettings.FilesWidth, aSettings.ConversationsWidth, aSettings.RepositoriesWidth, aSettings.Home, aSettings.Geometry,
				 aSettings.UpdatesLine, aSettings.SummaryFraction };
	}
	[[nodiscard]] inline auto combined_layout(const workbench_window_layout&   aLayout,
											  const review_behaviour_settings& aBehaviour) -> workbench_layout_settings
	{
		auto result            = workbench_layout_settings{ aLayout.Files, aLayout.Conversations, aLayout.Repositories, aLayout.LineNumbers, aLayout.OnePane,
															aLayout.FilesWidth, aLayout.ConversationsWidth, aLayout.RepositoriesWidth, aBehaviour, aLayout.Home, aLayout.Geometry,
															aLayout.UpdatesLine };
		result.SummaryFraction = aLayout.SummaryFraction;
		return result;
	}
	[[nodiscard]] inline auto valid(const workbench_window_layout& aLayout) noexcept -> bool
	{
		const auto drawer = [](const core::workbench_drawer_state aValue) { return aValue == core::workbench_drawer_state::expanded || aValue == core::workbench_drawer_state::collapsed; };
		const auto width  = [](const std::uint32_t aValue) { return aValue >= minimum_workbench_drawer_width && aValue <= maximum_workbench_drawer_width; };
		return drawer(aLayout.Files) && drawer(aLayout.Conversations) && drawer(aLayout.Repositories)
			&& (aLayout.LineNumbers == core::workbench_line_numbers::shown || aLayout.LineNumbers == core::workbench_line_numbers::hidden)
			&& (aLayout.OnePane == core::workbench_one_pane::disabled || aLayout.OnePane == core::workbench_one_pane::enabled)
			&& width(aLayout.FilesWidth) && width(aLayout.ConversationsWidth) && width(aLayout.RepositoriesWidth)
			&& valid(aLayout.Home) && valid(aLayout.Geometry)
			&& (!aLayout.SummaryFraction || (*aLayout.SummaryFraction > 0 && *aLayout.SummaryFraction < 10000))
			&& (aLayout.UpdatesLine == core::workbench_updates_line::folded || aLayout.UpdatesLine == core::workbench_updates_line::open);
	}
	[[nodiscard]] inline auto valid(const workbench_profile_preferences& aPreferences) noexcept -> bool
	{
		return core::valid(aPreferences.WritingHooks) && core::storable_agent_wrapper(aPreferences.ClaudeWrapper) && valid(aPreferences.Behaviour) && valid(aPreferences.NewWindowDefaults)
			&& aPreferences.NewWindowDefaults.Geometry == workbench_window_geometry{ }
		&& aPreferences.NewWindowDefaults.UpdatesLine == core::workbench_updates_line::folded
			&& !aPreferences.NewWindowDefaults.SummaryFraction;
	}
	inline constexpr std::size_t maximum_workbench_window_layouts = 32U;
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
		/// An adapter binds this legacy surface to an explicitly admitted window.
		[[nodiscard]] virtual auto window() const noexcept -> core::bpr_window_id { return { }; }
		[[nodiscard]] virtual auto profile_scoped() const noexcept -> bool { return false; }
		[[nodiscard]] virtual auto load_preferences(std::stop_token aStop) -> workbench_settings_result<workbench_profile_preferences>
		{
			auto loaded = load(aStop);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			// [[BPR-0783]]. A window's Home view is never a profile default. The
			// durable adapter strips it; stripping it here too makes that
			// structural, so no legacy window-bound port can carry one through.
			auto defaults     = window_layout(*loaded);
			defaults.Home     = { };
			defaults.Geometry = { };
			return workbench_profile_preferences{ loaded->Behaviour, defaults, loaded->WritingHooks, loaded->ClaudeWrapper };
		}
		[[nodiscard]] virtual auto store_preferences(const workbench_profile_preferences& aPreferences,
													 std::stop_token                      aStop) -> workbench_settings_result<void>
		{
			// Compatibility for legacy in-memory ports. Durable adapters override
			// this operation; window stores never invoke it.
			auto settings          = combined_layout(aPreferences.NewWindowDefaults, aPreferences.Behaviour);
			settings.WritingHooks  = aPreferences.WritingHooks;
			settings.ClaudeWrapper = aPreferences.ClaudeWrapper;
			return store(settings, aStop);
		}
		/// Retains only explicitly authoritative stable window IDs; empty removes all layout rows.
		[[nodiscard]] virtual auto retain_windows(std::span<const core::bpr_window_id>, std::stop_token) -> workbench_settings_result<void>
		{
			return std::unexpected(workbench_settings_error{ });
		}

		[[nodiscard]] virtual auto erase_window(core::bpr_window_id, std::stop_token) -> workbench_settings_result<void>
		{
			return std::unexpected(workbench_settings_error{ });
		}

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
		std::uint32_t RepositoriesWidth  = 0U;
		/// Absent until the window layout is loaded; the nested optional carries Reset.
		std::optional<std::optional<std::uint32_t>> SummaryFraction = { };

		auto operator<=>(const workbench_layout_delta&) const = default;
	};

	/// \brief Describes one file-drawer row by opaque semantic identity.
	struct workbench_file_projection
	{
		core::workbench_file_id File;
		std::string             Path;

		auto operator<=>(const workbench_file_projection&) const = default;
	};

	/// \brief [[BPR-0972]]. A bounded change to the published file order.
	///
	/// \details Removals name files that left; insertions carry each joining
	/// file at its position in the new order, ascending. A consumer removes
	/// first, then inserts.
	struct workbench_files_delta
	{
		std::vector<core::workbench_file_id>                           Removed;
		std::vector<std::pair<std::size_t, workbench_file_projection>> Inserted;

		auto operator<=>(const workbench_files_delta&) const = default;
	};

	/// \brief Describes the workbench projection record.
	///
	/// \details Every domain is engaged only when its published meaning changed,
	/// so a bounded change never forces presentation to rebuild the whole
	/// workbench. The host publishes nothing when no domain changed.
	struct workbench_projection
	{
		workbench_request_context                      Context;
		std::optional<core::workbench_selection_delta> Selection;
		std::optional<core::workbench_drawer_delta>    Drawers;
		/// [[BPR-1295]]. What showing the drawers would open, published when
		/// it changes, so the hide-and-show command can name it.
		std::optional<core::workbench_drawer_restore_delta> DrawerRestore;
		std::optional<core::workbench_line_numbers_delta>   LineNumbers;
		std::optional<core::workbench_one_pane_delta>       OnePane;
		/// [[BPR-1130]]. The Updates line's fold, published when it changes
		/// and once when this window's choice is restored.
		std::optional<core::workbench_updates_line_delta>     UpdatesLine;
		std::optional<core::workbench_command_delta>          Commands;
		std::optional<workbench_layout_delta>                 Layout;
		std::optional<std::vector<workbench_file_projection>> Files;
		/// [[BPR-0972]]. A bounded change to the last published file order,
		/// published instead of `Files` when a retained receipt moves one file.
		std::optional<workbench_files_delta> FilesDelta;
		/// [[BPR-0661]]. Published when the preferences change, when they are
		/// first loaded, and again when a change was refused so the page
		/// reverts to what the host holds.
		std::optional<review_behaviour_settings> Behaviour;
		/// [[BPR-0727]]. The reason the core refused the gesture this
		/// publication answers, when it refused one. A refusal is an event,
		/// not a retained state, so it is published each time it happens and
		/// never carried into a later publication.
		std::optional<core::workbench_reason_code> Refusal;
		/// [[BPR-0783]]. The window's Home view: published when the layout is
		/// first loaded and whenever the accepted view changes, and again when a
		/// view was refused so Home reverts to what the host holds.
		std::optional<home_view_state> HomeView;
		/// Published once after this window's load, never as a sibling preference.
		std::optional<workbench_window_geometry> Geometry;

		auto operator<=>(const workbench_projection&) const = default;
	};

	/// \brief Checks whether the supplied projection carries no changed domain.
	[[nodiscard]] constexpr auto empty(
		const workbench_projection& aProjection) noexcept -> bool
	{
		return !aProjection.Selection.has_value()
			&& !aProjection.Drawers.has_value()
			&& !aProjection.DrawerRestore.has_value()
			&& !aProjection.LineNumbers.has_value()
			&& !aProjection.OnePane.has_value()
			&& !aProjection.UpdatesLine.has_value()
			&& !aProjection.Commands.has_value()
			&& !aProjection.Layout.has_value()
			&& !aProjection.Files.has_value()
			&& !aProjection.FilesDelta.has_value()
			&& !aProjection.Behaviour.has_value()
			&& !aProjection.Refusal.has_value()
			&& !aProjection.HomeView.has_value()
			&& !aProjection.Geometry.has_value();
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
		selected_file_comparison_unavailable,
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
