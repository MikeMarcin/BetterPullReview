#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP

#include "bpr/application/agent_workspace_lifetime_guard.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/epoch_model.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::application {
	/// \brief Whether an existing checkout has uncommitted work in it.
	///
	/// \details Three states, because "not asked" is not "clean". An agent
	/// reasoning about a reviewer's uncommitted work should be *known* to be
	/// doing so, and a host that could not determine the state must say that
	/// rather than pick the reassuring answer.
	enum class checkout_cleanliness : std::uint8_t {
		unknown,
		clean,
		dirty,
	};

	[[nodiscard]] auto stable_name(checkout_cleanliness aCleanliness) noexcept
		-> std::string_view;

	/// \brief What a host reports about the reviewer's active worktree.
	struct checkout_observation
	{
		std::string          Path;
		checkout_cleanliness Cleanliness = checkout_cleanliness::unknown;

		auto operator<=>(const checkout_observation&) const = default;
	};

	/// \brief Why a workspace could not be prepared.
	enum class workspace_refusal : std::uint8_t {
		none,
		/// The profile asks for the reviewer's checkout and the reviewer did
		/// not choose it for this invocation.
		existing_checkout_not_chosen,
		/// No checkout was observed at all.
		no_checkout_available,
	};

	[[nodiscard]] auto stable_name(workspace_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief The workspace one agent invocation will run in.
	struct prepared_workspace
	{
		core::agent_workspace_policy Policy
			= core::agent_workspace_policy::disposable_review_workspace;
		/// The revision the workspace is materialised at. Always the session's
		/// own head for a disposable workspace; for an existing checkout it is
		/// what the reviewer happens to have.
		std::optional<core::git_commit_oid> PinnedHead;
		std::string                         Path;
		/// Meaningful only for an existing checkout. A disposable workspace is
		/// built from an immutable revision, so it has nothing uncommitted in
		/// it by construction and reports `clean`.
		checkout_cleanliness Cleanliness = checkout_cleanliness::clean;
		/// [[BPR-PENDING-AQ4]]. The workspace borrows its history from the
		/// reviewer's linked clone.
		bool BorrowsHistory = false;
		/// [[BPR-PENDING-AQ5]]. The workspace holds the head without its
		/// history: a sparse review's snapshot whose parents no clone lent.
		bool Shallow = false;

		auto operator<=>(const prepared_workspace&) const = default;
	};

	/// \brief The verdict on preparing one workspace.
	struct workspace_preparation
	{
		std::optional<prepared_workspace> Workspace;
		workspace_refusal                 Refusal = workspace_refusal::none;

		auto operator<=>(const workspace_preparation&) const = default;
	};

	/// \brief One exact request to materialize a disposable agent workspace.
	struct agent_workspace_materialization_request
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		core::agent_session          Session;
		std::filesystem::path        ParentDirectory;
		/// [[BPR-PENDING-AQ4]], RFC-BPR-0092 Q7. The working tree of the
		/// reviewer's linked clone whose history the workspace borrows, read
		/// only, through Git's alternates. Absent: the workspace holds only
		/// what the application's own copy gives it, as before.
		std::optional<std::filesystem::path> HistoryClone;

		auto operator<=>(const agent_workspace_materialization_request&) const
			= default;
	};

	/// \brief Opaque exact directory identity captured at materialization time.
	///
	/// \details The Git adapter owns the platform interpretation. Application
	/// code carries this bounded capability unchanged so cleanup can require the
	/// original directory rather than trusting whatever later occupies its path.
	struct agent_workspace_directory_identity
	{
		std::array<std::uint64_t, 4U> Opaque{ };

		auto operator<=>(const agent_workspace_directory_identity&) const = default;
	};

	/// \brief An exact workspace owned by one accepted invocation.
	///
	/// \details `OwnershipMarker` is created inside the private destination and
	/// must still match before cleanup. A missing or altered marker leaks one
	/// bounded workspace rather than authorizing deletion of an ambiguous path.
	struct owned_agent_workspace
	{
		prepared_workspace                            Workspace;
		core::application_session_id                  ApplicationSession;
		core::pull_request_authority                  Authority;
		core::agent_session_id                        Session;
		std::filesystem::path                         ParentDirectory;
		std::string                                   OwnershipMarker;
		agent_workspace_directory_identity            DirectoryIdentity;
		std::optional<agent_workspace_lifetime_guard> LifetimeGuard;

		auto operator<=>(const owned_agent_workspace&) const = default;
	};

	[[nodiscard]] constexpr auto valid(
		const agent_workspace_directory_identity& aIdentity) noexcept -> bool
	{
		return aIdentity.Opaque[0] != 0U
			&& (aIdentity.Opaque[1] != 0U || aIdentity.Opaque[2] != 0U
				|| aIdentity.Opaque[3] != 0U);
	}

	enum class agent_workspace_failure : std::uint8_t {
		invalid_request,
		unavailable,
		cancelled,
		stale,
		cleanup_failed,
	};

	/// \brief [[BPR-1738]]. Why an `unavailable` workspace could not be
	/// made, in terms a reviewer can act on.
	///
	/// \details A closed set, so the presentation can say what happened and
	/// what to do instead of naming the outcome and showing Git's own log.
	enum class agent_workspace_unavailable_reason : std::uint8_t {
		/// The review's local copy could not be copied into a workspace.
		copy_failed,
		/// The review holds only the files it reads, and the complete head
		/// the agent needs could not be downloaded.
		download_failed,
		/// As `download_failed`, because GitHub could not be reached.
		connection_failed,
		/// As `download_failed`, because the account was refused.
		sign_in_refused,
		/// As `download_failed`, because the head is larger than a
		/// workspace's snapshot may be.
		too_large,
	};

	[[nodiscard]] auto stable_name(agent_workspace_unavailable_reason aReason) noexcept
		-> std::string_view;

	/// \brief Carries a workspace failure and any safe cache explanation.
	struct agent_workspace_error
	{
		agent_workspace_failure            Code = agent_workspace_failure::invalid_request;
		std::string                        Detail;
		agent_workspace_unavailable_reason Reason = agent_workspace_unavailable_reason::copy_failed;

		agent_workspace_error() = default;

		// Implicit on purpose: a bare failure code is still a complete error.
		agent_workspace_error(const agent_workspace_failure aCode)
			: Code{ aCode }
		{
		}

		agent_workspace_error(
			const agent_workspace_failure aCode,
			decltype(Detail)              aDetail)
			: Code{ aCode }
			, Detail{ std::move(aDetail) }
		{
		}

		agent_workspace_error(
			const agent_workspace_failure            aCode,
			decltype(Detail)                         aDetail,
			const agent_workspace_unavailable_reason aReason)
			: Code{ aCode }
			, Detail{ std::move(aDetail) }
			, Reason{ aReason }
		{
		}

		auto operator<=>(const agent_workspace_error&) const = default;
	};

	/// \brief Compares a workspace error with its primary failure code.
	[[nodiscard]] inline auto operator==(
		const agent_workspace_error&  aError,
		const agent_workspace_failure aCode) noexcept -> bool
	{
		return aError.Code == aCode;
	}

	/// \brief Materializes and cleans workspaces on the retained cache actor.
	class agent_workspace_port
	{
	public:
		virtual ~agent_workspace_port() = default;

		[[nodiscard]] virtual auto materialize_agent_workspace(
			const agent_workspace_materialization_request& aRequest,
			std::stop_token                                aStop)
			-> std::expected<owned_agent_workspace, agent_workspace_error> = 0;
		[[nodiscard]] virtual auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			std::stop_token              aStop)
			-> std::expected<void, agent_workspace_error> = 0;
	};

	/// \brief Prepares the workspace one invocation runs in.
	///
	/// \details `aExistingCheckoutChosen` is the reviewer's explicit choice for
	/// **this invocation**, and a profile whose policy is `existing_checkout`
	/// is refused without it. That is the shape the card asks for: the
	/// dangerous mode cannot be reached by configuration alone, only by
	/// someone deciding it now.
	///
	/// Selecting the existing checkout for this invocation overrides a profile's
	/// safe disposable default; without that per-run choice, a profile may not
	/// silently enter the checkout. A disposable workspace pins the session's
	/// head. It is materialised from an immutable revision, so nothing the
	/// reviewer does to the active worktree afterwards can change what the agent
	/// sees — that is why the pin is the session's `HeadOid` and never the
	/// checkout's.
	[[nodiscard]] auto prepare_agent_workspace(
		const core::agent_profile&                 aProfile,
		const core::agent_session&                 aSession,
		const std::optional<checkout_observation>& aCheckout,
		bool                                       aExistingCheckoutChosen,
		const std::string&                         aDisposableRoot) -> workspace_preparation;

	/// \brief Validates the complete exact materialization identity.
	[[nodiscard]] auto valid(
		const agent_workspace_materialization_request& aRequest) noexcept -> bool;
	/// \brief Validates one cleanup authority without touching the filesystem.
	[[nodiscard]] auto valid(const owned_agent_workspace& aWorkspace) noexcept
		-> bool;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP
