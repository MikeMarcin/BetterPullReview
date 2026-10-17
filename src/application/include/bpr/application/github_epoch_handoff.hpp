#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_EPOCH_HANDOFF_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_EPOCH_HANDOFF_HPP

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace bpr::application {
	/// \brief Describes the git smart authority binding record.
	struct git_smart_authority_binding
	{
		core::application_session_id     ApplicationSession;
		core::host_profile_id            Profile;
		core::github_profile_revision    ProfileRevision;
		core::git_host_id                Host;
		core::host_authority_fingerprint HostAuthority;
		github_canonical_hostname        Hostname;
		github_server_contract           ServerContract = github_server_contract::github_com_2022_11_28;
		core::github_account_id          Account;
		core::repository_id              Repository;
		core::repository_locator         Locator;
		core::github_identity_generation IdentityGeneration;
		core::git_hash_algorithm         ObjectFormat = core::git_hash_algorithm::invalid;
		// Last and defaulted so every binding written before the mode existed
		// still names the authenticated authority it named, and part of the
		// defaulted comparison so a lease minted for one mode can never agree
		// with a handoff made in the other.
		core::github_access_mode Mode{ };

		auto operator<=>(const git_smart_authority_binding&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_smart_authority_binding& aBinding) noexcept -> bool
	{
		// Two rules, one per mode, matching the authority the lease binds:
		// public preview names no account, the authenticated mode names an
		// exact one. A credential-free transport can therefore never be
		// leased under an account's binding.
		const auto accountAsExpected = aBinding.Mode
				== core::github_access_mode::public_preview
			? aBinding.Account.value() == 0U
			: aBinding.Account.value() != 0U;
		return aBinding.ApplicationSession.value() != 0U
			&& aBinding.Profile.value() != 0U
			&& aBinding.ProfileRevision.value() != 0U
			&& aBinding.Host.value() != 0U && aBinding.HostAuthority.valid()
			&& valid_github_authority_hostname(
				   aBinding.ServerContract, aBinding.Hostname)
			&& accountAsExpected
			&& aBinding.Repository.value() != 0U
			&& core::valid(aBinding.Locator)
			&& aBinding.IdentityGeneration.value() != 0U
			&& (aBinding.ObjectFormat == core::git_hash_algorithm::sha1
				|| aBinding.ObjectFormat == core::git_hash_algorithm::sha256);
	}

	/// \brief Provides the git smart authority lease contract.
	class git_smart_authority_lease
	{
	public:
		git_smart_authority_lease()          = default;
		virtual ~git_smart_authority_lease() = default;

		git_smart_authority_lease(const git_smart_authority_lease&) = delete;
		auto operator=(const git_smart_authority_lease&)
			-> git_smart_authority_lease&                      = delete;
		git_smart_authority_lease(git_smart_authority_lease&&) = delete;
		auto operator=(git_smart_authority_lease&&)
			-> git_smart_authority_lease& = delete;

		[[nodiscard]] virtual auto binding() const noexcept
			-> const git_smart_authority_binding& = 0;
		[[nodiscard]] virtual auto acquire(
			const git_smart_request_context& aContext)
			-> git_smart_transport_result<git_smart_transport> = 0;
		[[nodiscard]] virtual auto supports_sparse_commit_trees() const noexcept -> bool
		{
			return false;
		}
		virtual void               revoke() noexcept                = 0;
		[[nodiscard]] virtual auto revoked() const noexcept -> bool = 0;
	};

	using git_smart_authority = std::unique_ptr<git_smart_authority_lease>;

	// A failed or bounded timeline read cannot suppress the accepted live head.
	enum class epoch_timeline_availability : std::uint8_t {
		unavailable,
		available,
		refused,
		capped,
		failed,
	};

	struct epoch_force_push_observation
	{
		std::uint64_t        EventId = 0U;
		core::git_commit_oid Head;
		std::int64_t         EventUtcMilliseconds = 0;

		auto operator<=>(const epoch_force_push_observation&) const = default;
	};

	/// \brief Bounded count facts from the exact accepted provider snapshot.
	struct accepted_pull_request_shape
	{
		std::size_t                FileCount           = 0U;
		std::size_t                ReturnedCommitCount = 0U;
		std::optional<std::size_t> ReportedCommitCount;
		auto                       operator<=>(const accepted_pull_request_shape&) const = default;
	};

	enum class pull_request_object_loading : std::uint8_t {
		complete,
		selected_objects,
	};

	/// \brief Permits selected-object loading for a validated accepted shape.
	/// \details PR counts do not bound repository closure size. The epoch host
	/// separately requires a capable lease; absent or inconsistent facts never
	/// permit missing-object acquisition.
	[[nodiscard]] auto object_loading_for(
		const std::optional<accepted_pull_request_shape>& aShape) noexcept
		-> pull_request_object_loading;

	// This is the sole accepted GitHub-to-epoch revision handoff. It intentionally
	// has no pull_request_source_revision, accepted_pull_request_revision, Source,
	// or Observation member. The epoch host mints the source observation only
	// after accepting this bounded, provider-free record.
	/// \brief Describes the accepted pull request handoff record.
	struct accepted_pull_request_handoff
	{
		core::application_session_id       ApplicationSession;
		core::host_profile_id              Profile;
		core::github_profile_revision      ProfileRevision;
		core::git_host_id                  Host;
		core::host_authority_fingerprint   HostAuthority;
		github_canonical_hostname          Hostname;
		github_server_contract             ServerContract = github_server_contract::github_com_2022_11_28;
		core::github_account_id            Account;
		core::github_viewer_actor_id       Viewer;
		core::repository_id                Repository;
		core::repository_locator           Locator;
		core::pull_request_id              PullRequest;
		core::pull_request_number          Number;
		core::provider_snapshot_id         Snapshot;
		core::github_identity_generation   IdentityGeneration;
		core::request_generation           PullRequestGeneration;
		core::pull_request_revision_origin Origin       = core::pull_request_revision_origin::live;
		core::git_hash_algorithm           ObjectFormat = core::git_hash_algorithm::invalid;
		core::base_branch_id               BaseBranch;
		git_base_branch_ref                BaseRef;
		/// [[BPR-1560]]. The comparison base: the merge base GitHub named for
		/// the recorded base and head under `comparison_base_basis::merge_base`,
		/// otherwise the recorded base itself, which Whole PR then refuses.
		core::git_commit_oid Base;
		core::git_commit_oid Head;
		// Last of the identity fields and defaulted so every handoff written
		// before the mode existed still names the authenticated authority it
		// named. Under public preview Account and Viewer are zero by rule.
		core::github_access_mode                             Mode{ };
		std::optional<core::current_viewer_review_candidate> CurrentViewerReview;
		// A missing lease is a valid authenticated observation with smart Git
		// unavailable. It never grants an alternate transport.
		git_smart_authority                        SmartGit;
		epoch_timeline_availability                TimelineAvailability = epoch_timeline_availability::unavailable;
		std::vector<epoch_force_push_observation>  ForcePushes;
		std::optional<accepted_pull_request_shape> Shape;
		/// [[BPR-1560]]. The base commit GitHub records for the pull request
		/// (`baseRefOid`, REST `base.sha`). It is what a publication checks
		/// the pull request against; it is never compared from. Unset means
		/// the producer recorded no separate base and `Base` is that commit.
		core::git_commit_oid        RecordedBase;
		core::comparison_base_basis BaseBasis = core::comparison_base_basis::merge_base;
	};

	/// \brief [[BPR-1560]]. The base commit GitHub records for the pull request.
	[[nodiscard]] inline auto recorded_base(const accepted_pull_request_handoff& aHandoff) noexcept
		-> const core::git_commit_oid&
	{
		return aHandoff.RecordedBase.valid() ? aHandoff.RecordedBase : aHandoff.Base;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const core::pull_request_revision_origin aOrigin) noexcept -> bool
	{
		switch (aOrigin) {
		case core::pull_request_revision_origin::live:
		case core::pull_request_revision_origin::explicit_historical_import:
			return true;
		}
		return false;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const core::github_review_state aState) noexcept -> bool
	{
		switch (aState) {
		case core::github_review_state::pending:
		case core::github_review_state::approved:
		case core::github_review_state::changes_requested:
		case core::github_review_state::commented:
		case core::github_review_state::dismissed:
			return true;
		}
		return false;
	}

	[[nodiscard]] inline auto valid(
		const core::current_viewer_review_candidate& aReview,
		const core::github_viewer_actor_id           aViewer,
		const core::git_hash_algorithm               aObjectFormat) noexcept -> bool
	{
		return aReview.Review.value() != 0U && aReview.Actor == aViewer
			&& core::valid(aReview.Order) && valid(aReview.State)
			&& aReview.State != core::github_review_state::pending
			&& aReview.Commit.valid()
			&& aReview.Commit.algorithm() == aObjectFormat;
	}

	[[nodiscard]] inline auto authority_binding(
		const accepted_pull_request_handoff& aHandoff)
		-> git_smart_authority_binding
	{
		return {
			.ApplicationSession = aHandoff.ApplicationSession,
			.Profile            = aHandoff.Profile,
			.ProfileRevision    = aHandoff.ProfileRevision,
			.Host               = aHandoff.Host,
			.HostAuthority      = aHandoff.HostAuthority,
			.Hostname           = aHandoff.Hostname,
			.ServerContract     = aHandoff.ServerContract,
			.Account            = aHandoff.Account,
			.Repository         = aHandoff.Repository,
			.Locator            = aHandoff.Locator,
			.IdentityGeneration = aHandoff.IdentityGeneration,
			.ObjectFormat       = aHandoff.ObjectFormat,
			.Mode               = aHandoff.Mode,
		};
	}

	[[nodiscard]] inline auto agrees(
		const git_smart_authority_binding&   aBinding,
		const accepted_pull_request_handoff& aHandoff) noexcept -> bool
	{
		return aBinding.ApplicationSession == aHandoff.ApplicationSession
			&& aBinding.Profile == aHandoff.Profile
			&& aBinding.ProfileRevision == aHandoff.ProfileRevision
			&& aBinding.Host == aHandoff.Host
			&& aBinding.HostAuthority == aHandoff.HostAuthority
			&& aBinding.Hostname == aHandoff.Hostname
			&& aBinding.ServerContract == aHandoff.ServerContract
			&& aBinding.Account == aHandoff.Account
			&& aBinding.Repository == aHandoff.Repository
			&& aBinding.Locator == aHandoff.Locator
			&& aBinding.IdentityGeneration == aHandoff.IdentityGeneration
			&& aBinding.ObjectFormat == aHandoff.ObjectFormat
			&& aBinding.Mode == aHandoff.Mode;
	}

	[[nodiscard]] inline auto valid(
		const accepted_pull_request_handoff& aHandoff) noexcept -> bool
	{
		// Two rules, one per mode, matching `core::pull_request_authority`:
		// the authenticated handoff names an exact reader, the public-preview
		// handoff names none. A public-preview handoff also carries no current
		// viewer review, because there is no viewer who could have left one.
		const auto readerAsExpected = aHandoff.Mode
				== core::github_access_mode::public_preview
			? (aHandoff.Account.value() == 0U && aHandoff.Viewer.value() == 0U
			   && !aHandoff.CurrentViewerReview)
			: (aHandoff.Account.value() != 0U && aHandoff.Viewer.value() != 0U);
		const auto validFacts       = aHandoff.ApplicationSession.value() != 0U
			&& aHandoff.Profile.value() != 0U
			&& aHandoff.ProfileRevision.value() != 0U
			&& aHandoff.Host.value() != 0U && aHandoff.HostAuthority.valid()
			&& valid_github_authority_hostname(
									aHandoff.ServerContract, aHandoff.Hostname)
			&& readerAsExpected
			&& aHandoff.Repository.value() != 0U
			&& core::valid(aHandoff.Locator)
			&& aHandoff.PullRequest.value() != 0U && aHandoff.Number.valid()
			&& aHandoff.Snapshot.value() != 0U
			&& aHandoff.IdentityGeneration.value() != 0U
			&& aHandoff.PullRequestGeneration.value() != 0U
			&& application::valid(aHandoff.Origin)
			&& (aHandoff.ObjectFormat == core::git_hash_algorithm::sha1
				|| aHandoff.ObjectFormat == core::git_hash_algorithm::sha256)
			&& aHandoff.BaseBranch.valid() && application::valid(aHandoff.BaseRef)
			&& aHandoff.Base.valid() && aHandoff.Head.valid()
			&& aHandoff.Base.algorithm() == aHandoff.ObjectFormat
			&& aHandoff.Head.algorithm() == aHandoff.ObjectFormat
			&& (!aHandoff.RecordedBase.valid() || aHandoff.RecordedBase.algorithm() == aHandoff.ObjectFormat)
			&& (aHandoff.BaseBasis == core::comparison_base_basis::merge_base
				|| (core::retained_recorded_base(aHandoff.BaseBasis) && aHandoff.Base == recorded_base(aHandoff)))
			&& (!aHandoff.CurrentViewerReview
				|| application::valid(
					*aHandoff.CurrentViewerReview,
					aHandoff.Viewer,
					aHandoff.ObjectFormat));
		if (!validFacts || aHandoff.ForcePushes.size() > core::maximum_epoch_occurrence_count
			|| (aHandoff.TimelineAvailability != epoch_timeline_availability::available
				&& !aHandoff.ForcePushes.empty())) {
			return false;
		}
		for (const auto& event : aHandoff.ForcePushes) {
			if (event.EventId == 0U || !event.Head.valid()
				|| event.Head.algorithm() != aHandoff.ObjectFormat
				|| event.EventUtcMilliseconds <= 0) {
				return false;
			}
		}
		if (aHandoff.SmartGit == nullptr) {
			return validFacts;
		}

		const auto& binding = aHandoff.SmartGit->binding();
		return !aHandoff.SmartGit->revoked()
			&& application::valid(binding) && agrees(binding, aHandoff);
	}

	/// \brief Describes the pull request authority invalidation record.
	struct pull_request_authority_invalidation
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
		core::github_identity_generation IdentityGeneration;
		// Last and defaulted, matching the handoff this invalidation revokes.
		core::github_access_mode Mode{ };

		auto operator<=>(const pull_request_authority_invalidation&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const pull_request_authority_invalidation& aInvalidation) noexcept -> bool
	{
		// Two rules, one per mode, matching the authority key: public preview
		// carries no reader and the authenticated mode carries an exact one.
		const auto readerAsExpected = aInvalidation.Mode
				== core::github_access_mode::public_preview
			? (aInvalidation.Account.value() == 0U
			   && aInvalidation.Viewer.value() == 0U)
			: (aInvalidation.Account.value() != 0U
			   && aInvalidation.Viewer.value() != 0U);
		return aInvalidation.ApplicationSession.value() != 0U
			&& aInvalidation.Profile.value() != 0U
			&& aInvalidation.ProfileRevision.value() != 0U
			&& aInvalidation.Host.value() != 0U
			&& aInvalidation.HostAuthority.valid()
			&& readerAsExpected
			&& aInvalidation.Repository.value() != 0U
			&& aInvalidation.PullRequest.value() != 0U
			&& aInvalidation.IdentityGeneration.value() != 0U;
	}

	/// \brief Provides the github epoch handoff port contract.
	class github_epoch_handoff_port
	{
	public:
		virtual ~github_epoch_handoff_port() = default;

		virtual void accept(accepted_pull_request_handoff aHandoff) = 0;
		virtual void invalidate(
			pull_request_authority_invalidation aInvalidation) = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_EPOCH_HANDOFF_HPP
